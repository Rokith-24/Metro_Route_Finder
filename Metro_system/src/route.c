
#include <stdio.h>
#include <stdlib.h>

#include "route.h"

/* ---------- Internal Helpers ---------- */

static int stationExists(const StationNode *stations, int stationId)
{
    return searchStation((StationNode *)stations, stationId) != NULL;
}

static int connectionIndex(
    const RouteGraph *graph,
    int stationA,
    int stationB
)
{
    size_t i;

    if (graph == NULL) {
        return -1;
    }

    for (i = 0; i < graph->count; i++) {
        if ((graph->connections[i].stationA == stationA &&
             graph->connections[i].stationB == stationB) ||
            (graph->connections[i].stationA == stationB &&
             graph->connections[i].stationB == stationA)) {
            return (int)i;
        }
    }

    return -1;
}

static void collectStationIds(
    const StationNode *root,
    int *ids,
    size_t *index
)
{
    if (root == NULL) {
        return;
    }

    collectStationIds(root->left, ids, index);
    ids[*index] = root->station.id;
    (*index)++;
    collectStationIds(root->right, ids, index);
}

static int findStationIndex(
    const int *ids,
    size_t count,
    int stationId
)
{
    size_t i;

    for (i = 0; i < count; i++) {
        if (ids[i] == stationId) {
            return (int)i;
        }
    }

    return -1;
}

/* ---------- Graph Lifecycle ---------- */

void initRouteGraph(RouteGraph *graph)
{
    if (graph == NULL) {
        return;
    }

    graph->connections = NULL;
    graph->count = 0;
    graph->capacity = 0;
}

void freeRouteGraph(RouteGraph *graph)
{
    if (graph == NULL) {
        return;
    }

    free(graph->connections);

    graph->connections = NULL;
    graph->count = 0;
    graph->capacity = 0;
}

/* ---------- Connection Management ---------- */

int routeConnectionExists(
    const RouteGraph *graph,
    int stationA,
    int stationB
)
{
    return connectionIndex(graph, stationA, stationB) >= 0;
}

AppStatus addRouteConnection(
    RouteGraph *graph,
    const StationNode *stations,
    int stationA,
    int stationB
)
{
    RouteConnection *newConnections;
    size_t newCapacity;

    if (graph == NULL || stations == NULL ||
        stationA <= 0 || stationB <= 0) {
        return APP_INVALID_INPUT;
    }

    if (stationA == stationB) {
        return APP_INVALID_INPUT;
    }

    if (!stationExists(stations, stationA) ||
        !stationExists(stations, stationB)) {
        return APP_NOT_FOUND;
    }

    if (routeConnectionExists(graph, stationA, stationB)) {
        return APP_DUPLICATE;
    }

    if (graph->count == graph->capacity) {
        newCapacity = graph->capacity == 0
                    ? 4
                    : graph->capacity * 2;

        if (newCapacity < graph->capacity ||
            newCapacity > (size_t)-1 / sizeof(RouteConnection)) {
            return APP_ERROR;
        }

        newConnections = realloc(
            graph->connections,
            newCapacity * sizeof(RouteConnection)
        );

        if (newConnections == NULL) {
            return APP_ERROR;
        }

        graph->connections = newConnections;
        graph->capacity = newCapacity;
    }

    graph->connections[graph->count].stationA = stationA;
    graph->connections[graph->count].stationB = stationB;
    graph->count++;

    return APP_SUCCESS;
}

AppStatus removeRouteConnection(
    RouteGraph *graph,
    int stationA,
    int stationB
)
{
    int index;

    if (graph == NULL || stationA <= 0 || stationB <= 0 ||
        stationA == stationB) {
        return APP_INVALID_INPUT;
    }

    index = connectionIndex(graph, stationA, stationB);

    if (index < 0) {
        return APP_NOT_FOUND;
    }

    graph->connections[index] =
        graph->connections[graph->count - 1];

    graph->count--;

    return APP_SUCCESS;
}

size_t removeStationConnections(
    RouteGraph *graph,
    int stationId
)
{
    size_t i;
    size_t removed = 0;

    if (graph == NULL || stationId <= 0) {
        return 0;
    }

    i = 0;

    while (i < graph->count) {
        if (graph->connections[i].stationA == stationId ||
            graph->connections[i].stationB == stationId) {

            graph->connections[i] =
                graph->connections[graph->count - 1];

            graph->count--;
            removed++;
        } else {
            i++;
        }
    }

    return removed;
}

/* ---------- Connection Display ---------- */

void displayConnections(
    const RouteGraph *graph,
    const StationNode *stations
)
{
    size_t i;
    StationNode *nodeA;
    StationNode *nodeB;

    if (graph == NULL || stations == NULL) {
        printf("Cannot display connections: invalid input.\n");
        return;
    }

    printf("\n========== STATION CONNECTIONS ==========\n");
    printf("%-12s %-28s | %-12s %-28s\n",
           "Station A", "Name", "Station B", "Name");

    printf("------------------------------------------------"
           "-----------------------------\n");

    if (graph->count == 0) {
        printf("No connections available.\n");
        return;
    }

    for (i = 0; i < graph->count; i++) {
        nodeA = searchStation(
            (StationNode *)stations,
            graph->connections[i].stationA
        );

        nodeB = searchStation(
            (StationNode *)stations,
            graph->connections[i].stationB
        );

        if (nodeA == NULL || nodeB == NULL) {
            printf("%-12d %-28s | %-12d %-28s\n",
                   graph->connections[i].stationA,
                   "[MISSING STATION]",
                   graph->connections[i].stationB,
                   "[MISSING STATION]");
            continue;
        }

        printf("%-12d %-28s | %-12d %-28s\n",
               nodeA->station.id,
               nodeA->station.name,
               nodeB->station.id,
               nodeB->station.name);
    }

    printf("=========================================\n");
}

/* ---------- Network Map ---------- */

void displayNetworkMap(
    const RouteGraph *graph,
    const StationNode *stations
)
{
    int *stationIds;
    size_t totalStations;
    size_t index = 0;
    size_t i;
    int foundConnection;

    if (graph == NULL || stations == NULL) {
        printf("Cannot display network: invalid input.\n");
        return;
    }

    totalStations = (size_t)countStations(stations);

    printf("\n========== RAILWAY NETWORK MAP ==========\n");

    if (totalStations == 0) {
        printf("No stations available.\n");
        return;
    }

    stationIds = malloc(totalStations * sizeof(int));

    if (stationIds == NULL) {
        printf("Unable to allocate memory for network map.\n");
        return;
    }

    collectStationIds(stations, stationIds, &index);

    printf("\nDirect connections:\n");

    if (graph->count == 0) {
        printf("No railway connections are configured.\n");
    } else {
        for (i = 0; i < graph->count; i++) {
            StationNode *a = searchStation(
                (StationNode *)stations,
                graph->connections[i].stationA
            );

            StationNode *b = searchStation(
                (StationNode *)stations,
                graph->connections[i].stationB
            );

            if (a != NULL && b != NULL) {
                printf("[%d %s] <----> [%d %s]\n",
                       a->station.id,
                       a->station.name,
                       b->station.id,
                       b->station.name);
            }
        }
    }

    printf("\nStations without any connections:\n");
    foundConnection = 0;

    for (i = 0; i < totalStations; i++) {
        size_t j;
        int connected = 0;

        for (j = 0; j < graph->count; j++) {
            if (graph->connections[j].stationA == stationIds[i] ||
                graph->connections[j].stationB == stationIds[i]) {
                connected = 1;
                break;
            }
        }

        if (!connected) {
            StationNode *node = searchStation(
                (StationNode *)stations,
                stationIds[i]
            );

            if (node != NULL) {
                printf("[%d %s] (ISOLATED)\n",
                       node->station.id,
                       node->station.name);
                foundConnection = 1;
            }
        }
    }

    if (!foundConnection) {
        printf("None.\n");
    }

    free(stationIds);

    printf("==========================================\n");
}

/* ---------- Breadth-First Search ---------- */

AppStatus findRouteBFS(
    const RouteGraph *graph,
    const StationNode *stations,
    int sourceId,
    int destinationId,
    int *routeIds,
    size_t routeCapacity,
    size_t *routeLength
)
{
    int *ids = NULL;
    int *queue = NULL;
    int *parent = NULL;
    int *path = NULL;
    unsigned char *visited = NULL;

    size_t totalStations;
    size_t stationIndex = 0;
    size_t front = 0;
    size_t rear = 0;
    size_t pathLength = 0;
    size_t i;
    int sourceIndex;
    int destinationIndex;
    int current;
    AppStatus result = APP_ERROR;

    if (routeLength != NULL) {
        *routeLength = 0;
    }

    if (graph == NULL || stations == NULL ||
        routeIds == NULL || routeLength == NULL ||
        sourceId <= 0 || destinationId <= 0) {
        return APP_INVALID_INPUT;
    }

    if (!stationExists(stations, sourceId) ||
        !stationExists(stations, destinationId)) {
        return APP_NOT_FOUND;
    }

    totalStations = (size_t)countStations(stations);

    if (totalStations == 0) {
        return APP_NOT_FOUND;
    }

    if (routeCapacity == 0 ||
        totalStations > (size_t)-1 / sizeof(int)) {
        return APP_INVALID_INPUT;
    }

    ids = malloc(totalStations * sizeof(int));
    queue = malloc(totalStations * sizeof(int));
    parent = malloc(totalStations * sizeof(int));
    path = malloc(totalStations * sizeof(int));
    visited = calloc(totalStations, sizeof(unsigned char));

    if (ids == NULL || queue == NULL || parent == NULL ||
        path == NULL || visited == NULL) {
        goto cleanup;
    }

    collectStationIds(stations, ids, &stationIndex);

    sourceIndex = findStationIndex(
        ids, totalStations, sourceId
    );

    destinationIndex = findStationIndex(
        ids, totalStations, destinationId
    );

    if (sourceIndex < 0 || destinationIndex < 0) {
        result = APP_NOT_FOUND;
        goto cleanup;
    }

    for (i = 0; i < totalStations; i++) {
        parent[i] = -1;
    }

    visited[sourceIndex] = 1;
    queue[rear++] = sourceIndex;

    while (front < rear) {
        current = queue[front++];

        if (current == destinationIndex) {
            break;
        }

        for (i = 0; i < graph->count; i++) {
            int neighbourId = -1;
            int neighbourIndex;

            if (graph->connections[i].stationA == ids[current]) {
                neighbourId = graph->connections[i].stationB;
            } else if (
                graph->connections[i].stationB == ids[current]
            ) {
                neighbourId = graph->connections[i].stationA;
            }

            if (neighbourId < 0) {
                continue;
            }

            neighbourIndex = findStationIndex(
                ids, totalStations, neighbourId
            );

            /*
             * Ignore stale connections to stations that no
             * longer exist in the AVL tree.
             */
            if (neighbourIndex < 0 ||
                visited[neighbourIndex]) {
                continue;
            }

            visited[neighbourIndex] = 1;
            parent[neighbourIndex] = current;
            queue[rear++] = neighbourIndex;
        }
    }

    if (!visited[destinationIndex]) {
        result = APP_NOT_FOUND;
        goto cleanup;
    }

    current = destinationIndex;

    while (current != -1) {
        path[pathLength++] = ids[current];
        current = parent[current];
    }

    if (pathLength > routeCapacity) {
        result = APP_INVALID_INPUT;
        goto cleanup;
    }

    for (i = 0; i < pathLength; i++) {
        routeIds[i] = path[pathLength - 1 - i];
    }

    *routeLength = pathLength;
    result = APP_SUCCESS;

cleanup:
    free(ids);
    free(queue);
    free(parent);
    free(path);
    free(visited);

    return result;
}
