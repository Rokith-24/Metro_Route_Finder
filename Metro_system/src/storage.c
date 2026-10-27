
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "storage.h"

#define CSV_LINE_MAX 256

static int parseStation(char *line, Station *station)
{
    char *token;
    char *end;
    long id;
    long platform;
    float distance;
    float fare;

    if (line == NULL || station == NULL) {
        return 0;
    }

    token = strtok(line, ",");
    if (token == NULL || strcmp(token, "id") == 0) {
        return 0;
    }

    errno = 0;
    id = strtol(token, &end, 10);
    if (errno != 0 || *end != '\0' || id <= 0 || id > 2147483647L) {
        return 0;
    }

    token = strtok(NULL, ",");
    if (token == NULL || strlen(token) == 0 ||
        strlen(token) >= STATION_NAME_MAX) {
        return 0;
    }

    memset(station, 0, sizeof(*station));
    memcpy(station->name, token, strlen(token));

    token = strtok(NULL, ",");
    if (token == NULL) {
        return 0;
    }

    errno = 0;
    distance = strtof(token, &end);
    if (errno != 0 || *end != '\0' || !isfinite(distance) || distance < 0.0f) {
        return 0;
    }

    token = strtok(NULL, ",");
    if (token == NULL) {
        return 0;
    }

    errno = 0;
    fare = strtof(token, &end);
    if (errno != 0 || *end != '\0' || !isfinite(fare) || fare < 0.0f) {
        return 0;
    }

    token = strtok(NULL, ",");
    if (token == NULL) {
        return 0;
    }

    errno = 0;
    platform = strtol(token, &end, 10);
    if (errno != 0 || *end != '\0' || platform <= 0 ||
        platform > 2147483647L) {
        return 0;
    }

    if (strtok(NULL, ",") != NULL) {
        return 0;
    }

    station->id = (int)id;
    station->distance = distance;
    station->fare = fare;
    station->platform = (int)platform;

    return 1;
}

static AppStatus writeStation(FILE *file, const Station *station)
{
    if (fprintf(file, "%d,%s,%.2f,%.2f,%d\n",
                station->id,
                station->name,
                station->distance,
                station->fare,
                station->platform) < 0) {
        return APP_ERROR;
    }

    return APP_SUCCESS;
}

static AppStatus writeTreeInOrder(FILE *file, const StationNode *root)
{
    AppStatus status;

    if (root == NULL) {
        return APP_SUCCESS;
    }

    status = writeTreeInOrder(file, root->left);
    if (status != APP_SUCCESS) {
        return status;
    }

    status = writeStation(file, &root->station);
    if (status != APP_SUCCESS) {
        return status;
    }

    return writeTreeInOrder(file, root->right);
}

AppStatus loadStationsFromCSV(StationNode **root, const char *filename)
{
    FILE *file;
    char line[CSV_LINE_MAX];
    unsigned long lineNumber = 0;
    AppStatus overallStatus = APP_SUCCESS;

    if (root == NULL || filename == NULL) {
        return APP_INVALID_INPUT;
    }

    file = fopen(filename, "r");
    if (file == NULL) {
        return APP_ERROR;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        Station station;
        size_t length = strlen(line);

        lineNumber++;

        if (length > 0 && line[length - 1] == '\n') {
            line[length - 1] = '\0';
        }

        length = strlen(line);
        if (length > 0 && line[length - 1] == '\r') {
            line[length - 1] = '\0';
        }

        if (lineNumber == 1 && strcmp(line, "id,name,distance,fare,platform") == 0) {
            continue;
        }

        if (!parseStation(line, &station)) {
            fprintf(stderr, "Invalid station record at line %lu\n", lineNumber);
            overallStatus = APP_ERROR;
            continue;
        }

        {
            AppStatus status = insertStation(root, station);

            if (status != APP_SUCCESS) {
                fprintf(stderr, "Could not load station ID %d at line %lu\n",
                        station.id, lineNumber);
                overallStatus = status;
            }
        }
    }

    if (ferror(file)) {
        overallStatus = APP_ERROR;
    }

    if (fclose(file) != 0) {
        overallStatus = APP_ERROR;
    }

    return overallStatus;
}

AppStatus saveStationsToCSV(const StationNode *root, const char *filename)
{
    FILE *file;
    AppStatus status;

    if (filename == NULL) {
        return APP_INVALID_INPUT;
    }

    file = fopen(filename, "w");
    if (file == NULL) {
        return APP_ERROR;
    }

    if (fprintf(file, "id,name,distance,fare,platform\n") < 0) {
        fclose(file);
        return APP_ERROR;
    }

    status = writeTreeInOrder(file, root);

    if (fclose(file) != 0) {
        status = APP_ERROR;
    }

    return status;
}



/* ---------- Connection CSV Storage ---------- */

#include <limits.h>

/*
 * Parse one connection record:
 * station_a,station_b
 *
 * Returns 1 for a valid record and 0 otherwise.
 */
static int parseConnection(
    char *line,
    int *stationA,
    int *stationB
)
{
    char *token;
    char *end;
    long a;
    long b;

    if (line == NULL || stationA == NULL || stationB == NULL) {
        return 0;
    }

    token = strtok(line, ",");
    if (token == NULL) {
        return 0;
    }

    errno = 0;
    a = strtol(token, &end, 10);

    if (errno != 0 || end == token || *end != '\0' ||
        a <= 0 || a > INT_MAX) {
        return 0;
    }

    token = strtok(NULL, ",");
    if (token == NULL) {
        return 0;
    }

    errno = 0;
    b = strtol(token, &end, 10);

    if (errno != 0 || end == token || *end != '\0' ||
        b <= 0 || b > INT_MAX) {
        return 0;
    }

    /* Reject unexpected additional columns. */
    if (strtok(NULL, ",") != NULL) {
        return 0;
    }

    if (a == b) {
        return 0;
    }

    *stationA = (int)a;
    *stationB = (int)b;

    return 1;
}


/*
 * Load connections into a temporary graph first.
 * The existing graph is replaced only if every record loads
 * successfully.
 *
 * IMPORTANT: graph must already have been initialized with
 * initRouteGraph().
 */
AppStatus loadConnectionsFromCSV(
    RouteGraph *graph,
    const StationNode *stations,
    const char *filename
)
{
    FILE *file;
    char line[CSV_LINE_MAX];
    unsigned long lineNumber = 0;
    RouteGraph temporaryGraph;
    AppStatus status = APP_SUCCESS;

    if (graph == NULL || stations == NULL || filename == NULL) {
        return APP_INVALID_INPUT;
    }

    file = fopen(filename, "r");
    if (file == NULL) {
        return APP_ERROR;
    }

    initRouteGraph(&temporaryGraph);

    while (fgets(line, sizeof(line), file) != NULL) {
        size_t length;
        int stationA;
        int stationB;

        lineNumber++;
        length = strlen(line);

        /*
         * Reject a line that exceeds the input buffer instead
         * of accidentally treating its remainder as another row.
         */
        if (length > 0 && line[length - 1] != '\n' &&
            !feof(file)) {
            int ch;

            while ((ch = fgetc(file)) != '\n' && ch != EOF) {
                /* Discard the remainder of this oversized line. */
            }

            fprintf(stderr,
                    "Connection record too long at line %lu\n",
                    lineNumber);
            status = APP_ERROR;
            break;
        }

        if (length > 0 && line[length - 1] == '\n') {
            line[--length] = '\0';
        }

        if (length > 0 && line[length - 1] == '\r') {
            line[--length] = '\0';
        }

        if (lineNumber == 1 &&
            strcmp(line, "station_a,station_b") == 0) {
            continue;
        }

        if (!parseConnection(line, &stationA, &stationB)) {
            fprintf(stderr,
                    "Invalid connection record at line %lu\n",
                    lineNumber);
            status = APP_ERROR;
            break;
        }

        status = addRouteConnection(
            &temporaryGraph,
            stations,
            stationA,
            stationB
        );

        if (status != APP_SUCCESS) {
            fprintf(stderr,
                    "Could not load connection %d-%d at line %lu\n",
                    stationA,
                    stationB,
                    lineNumber);
            break;
        }
    }

    if (ferror(file)) {
        status = APP_ERROR;
    }

    if (fclose(file) != 0) {
        status = APP_ERROR;
    }

    if (status == APP_SUCCESS) {
        /*
         * Transfer ownership of the successfully loaded
         * connection array to the caller's graph.
         */
        freeRouteGraph(graph);
        *graph = temporaryGraph;
    } else {
        /* Preserve the caller's original graph on failure. */
        freeRouteGraph(&temporaryGraph);
    }

    return status;
}


/*
 * Save all connections to CSV.
 */
AppStatus saveConnectionsToCSV(
    const RouteGraph *graph,
    const char *filename
)
{
    FILE *file;
    size_t i;
    AppStatus status = APP_SUCCESS;

    if (graph == NULL || filename == NULL) {
        return APP_INVALID_INPUT;
    }

    if (graph->count > 0 && graph->connections == NULL) {
        return APP_INVALID_INPUT;
    }

    file = fopen(filename, "w");
    if (file == NULL) {
        return APP_ERROR;
    }

    if (fprintf(file, "station_a,station_b\n") < 0) {
        status = APP_ERROR;
    }

    for (i = 0; status == APP_SUCCESS && i < graph->count; i++) {
        if (fprintf(file, "%d,%d\n",
                    graph->connections[i].stationA,
                    graph->connections[i].stationB) < 0) {
            status = APP_ERROR;
        }
    }

    if (fclose(file) != 0) {
        status = APP_ERROR;
    }

    return status;
}
