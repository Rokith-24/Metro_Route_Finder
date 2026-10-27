#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "common.h"
#include "station.h"
#include "bst.h"
#include "storage.h"
#include "auth.h"
#include "audit_log.h"
#include "route.h"
#include "audit_worker.h"

#define INPUT_MAX 128
#define CONNECTIONS_FILE "data/connections.csv"

static int auditWorkerStarted = 0;

static void trimInput(char *text)
{
    size_t n;
    if (text == NULL) return;
    n = strlen(text);
    while (n > 0 && (text[n - 1] == '\n' || text[n - 1] == '\r'))
        text[--n] = '\0';
}

/* Returns 1 for input, 0 for EOF, -1 for invalid/too-long input. */
static int readLine(const char *prompt, char *buffer, size_t size)
{
    size_t n;
    int ch;
    if (prompt != NULL) printf("%s", prompt);
    if (buffer == NULL || size < 2) return -1;
    if (fgets(buffer, (int)size, stdin) == NULL) return 0;
    n = strlen(buffer);
    if (n > 0 && buffer[n - 1] != '\n') {
        ch = getchar();
        if (ch != '\n' && ch != EOF) {
            while (ch != '\n' && ch != EOF) ch = getchar();
            printf("Input is too long. Please try again.\n");
            return -1;
        }
    }
    trimInput(buffer);
    return 1;
}

static int readInteger(const char *prompt, int *value)
{
    char input[INPUT_MAX], *end;
    long number;
    int result = readLine(prompt, input, sizeof(input));
    if (result <= 0) return result;
    errno = 0;
    number = strtol(input, &end, 10);
    if (input[0] == '\0' || errno != 0 || *end != '\0' ||
        number < -2147483647L - 1L || number > 2147483647L) {
        printf("Invalid integer. Please try again.\n");
        return -1;
    }
    *value = (int)number;
    return 1;
}

static int readFloatValue(const char *prompt, float *value)
{
    char input[INPUT_MAX], *end;
    float number;
    int result = readLine(prompt, input, sizeof(input));
    if (result <= 0) return result;
    errno = 0;
    number = strtof(input, &end);
    if (input[0] == '\0' || errno != 0 || *end != '\0' || !isfinite(number)) {
        printf("Invalid number. Please try again.\n");
        return -1;
    }
    *value = number;
    return 1;
}

static int readStationName(char *name, size_t size)
{
    int result = readLine("Station name (no commas): ", name, size);
    if (result <= 0) return result;
    if (name[0] == '\0' || strchr(name, ',') != NULL) {
        printf("Name cannot be empty or contain a comma.\n");
        return -1;
    }
    return 1;
}

static int readStationDetails(Station *station, int includeId)
{
    int result;
    if (includeId) {
        result = readInteger("Station ID: ", &station->id);
        if (result <= 0) return result;
    }
    result = readStationName(station->name, sizeof(station->name));
    if (result <= 0) return result;
    result = readFloatValue("Distance: ", &station->distance);
    if (result <= 0) return result;
    result = readFloatValue("Fare: ", &station->fare);
    if (result <= 0) return result;
    result = readInteger("Platform number: ", &station->platform);
    if (result <= 0) return result;
    if (station->id <= 0 || station->distance < 0.0f ||
        station->fare < 0.0f || station->platform <= 0) {
        printf("ID/platform must be positive; distance/fare cannot be negative.\n");
        return -1;
    }
    return 1;
}

static void logEvent(const char *username,
                     const char *action,
                     const char *details,
                     AppStatus result)
{
    AppStatus status;

    status = queueAuditEvent(username, action, details, result);

    if (status != APP_SUCCESS) {
        if (writeAuditLog(username, action, details, result) != APP_SUCCESS) {
            fprintf(stderr, "Warning: could not write to the audit log.\n");
        }
    }
}

static void showStationSummary(const StationNode *root)
{
    int id;
    printf("\nTotal stations: %d\n", countStations(root));
    if (findMinimumStationId(root, &id)) printf("Minimum station ID: %d\n", id);
    else printf("Minimum station ID: unavailable (tree is empty)\n");
    if (findMaximumStationId(root, &id)) printf("Maximum station ID: %d\n", id);
    else printf("Maximum station ID: unavailable (tree is empty)\n");
}

static void viewStation(const StationNode *root)
{
    int id, result;
    StationNode *node;
    result = readInteger("Enter station ID to search: ", &id);
    if (result <= 0) return;
    node = searchStation((StationNode *)root, id);
    if (node == NULL) printf("Station ID %d was not found.\n", id);
    else displayStation(&node->station);
}

static void addStation(StationNode **root, const User *user)
{
    Station station = {0};
    AppStatus status;
    if (!hasPermission(user->role, "ADD")) {
        printf("Permission denied: Admin access is required.\n");
        logEvent(user->username, "ADD_STATION", "Permission denied", APP_PERMISSION_DENIED);
        return;
    }
    if (readStationDetails(&station, 1) <= 0) return;
    status = insertStation(root, station);
    if (status != APP_SUCCESS) {
        printf(status == APP_DUPLICATE ? "A station with that ID already exists.\n" : "Could not add station.\n");
        logEvent(user->username, "ADD_STATION", "Insertion failed", status);
        return;
    }
    status = saveStationsToCSV(*root, STATIONS_FILE);
    if (status != APP_SUCCESS) {
        printf("WARNING: station added in memory, but CSV save failed. Restart only after checking data/stations.csv.\n");
        logEvent(user->username, "ADD_STATION", "CSV save failed", status);
        return;
    }
    printf("Station added and saved. It remains disconnected until you add a connection.\n");
    logEvent(user->username, "ADD_STATION", "Station added successfully", APP_SUCCESS);
}

static void updateStationDetails(StationNode **root, const User *user)
{
    int id, result;
    StationNode *node;
    Station updated;
    AppStatus status;
    char details[INPUT_MAX];
    if (!hasPermission(user->role, "UPDATE")) {
        printf("Permission denied: Admin access is required.\n");
        logEvent(user->username, "UPDATE_STATION", "Permission denied", APP_PERMISSION_DENIED);
        return;
    }
    result = readInteger("Enter station ID to update: ", &id);
    if (result <= 0) return;
    node = searchStation(*root, id);
    if (node == NULL) {
        printf("Station not found.\n");
        logEvent(user->username, "UPDATE_STATION", "Station not found", APP_NOT_FOUND);
        return;
    }
    updated = node->station;
    printf("Enter updated details for station %d.\n", id);
    if (readStationDetails(&updated, 0) <= 0) return;
    status = updateStation(*root, id, updated);
    if (status != APP_SUCCESS) {
        printf("Could not update station.\n");
        logEvent(user->username, "UPDATE_STATION", "Update failed", status);
        return;
    }
    status = saveStationsToCSV(*root, STATIONS_FILE);
    if (status != APP_SUCCESS) {
        printf("WARNING: station updated in memory, but CSV save failed.\n");
        logEvent(user->username, "UPDATE_STATION", "CSV save failed", status);
        return;
    }
    snprintf(details, sizeof(details), "Station ID %d updated", id);
    printf("Station updated and saved.\n");
    logEvent(user->username, "UPDATE_STATION", details, APP_SUCCESS);
}

static void deleteStationById(StationNode **root, RouteGraph *graph, const User *user)
{
    int id, confirm;
    size_t removedEdges;
    AppStatus status;
    char details[INPUT_MAX];
    StationNode *node;
    if (!hasPermission(user->role, "DELETE")) {
        printf("Permission denied: Admin access is required.\n");
        logEvent(user->username, "DELETE_STATION", "Permission denied", APP_PERMISSION_DENIED);
        return;
    }
    if (readInteger("Enter station ID to delete: ", &id) <= 0) return;
    node = searchStation(*root, id);
    if (node == NULL) {
        printf("Station not found.\n");
        logEvent(user->username, "DELETE_STATION", "Station not found", APP_NOT_FOUND);
        return;
    }
    printf("Station: %s (ID %d)\n", node->station.name, id);
    printf("Direct connections that will be removed:\n");
    {
        size_t i, shown = 0;
        for (i = 0; i < graph->count; ++i) {
            if (graph->connections[i].stationA == id || graph->connections[i].stationB == id) {
                printf("  %d <-> %d\n", graph->connections[i].stationA, graph->connections[i].stationB);
                ++shown;
            }
        }
        if (shown == 0) printf("  (none)\n");
    }
    if (readInteger("Confirm delete? Enter 1 for yes, 0 to cancel: ", &confirm) <= 0 || confirm != 1) {
        printf("Station deletion cancelled.\n");
        return;
    }
    /* Save the graph snapshot first so an invalid station ID is not left in connections.csv. */
    removedEdges = removeStationConnections(graph, id);
    status = saveConnectionsToCSV(graph, CONNECTIONS_FILE);
    if (status != APP_SUCCESS) {
        /* Best-effort rollback by restoring the removed edges from a backup list is not available here. */
        printf("ERROR: could not save connections.csv after removing links. Do not continue; restore the CSV backup and restart.\n");
        logEvent(user->username, "DELETE_STATION", "Connection CSV save failed; manual recovery required", status);
        return;
    }
    status = deleteStation(root, id);
    if (status != APP_SUCCESS) {
        printf("ERROR: connection file was updated but station deletion failed. Check both CSV files.\n");
        logEvent(user->username, "DELETE_STATION", "Tree deletion failed after connection save", status);
        return;
    }
    status = saveStationsToCSV(*root, STATIONS_FILE);
    if (status != APP_SUCCESS) {
        printf("ERROR: station removed in memory, but stations.csv save failed. Check both CSV files before restarting.\n");
        logEvent(user->username, "DELETE_STATION", "Station CSV save failed; manual recovery required", status);
        return;
    }
    snprintf(details, sizeof(details), "Deleted station %d; removed %lu links", id, (unsigned long)removedEdges);
    printf("Station deleted. Removed %lu direct connection(s).\n", (unsigned long)removedEdges);
    logEvent(user->username, "DELETE_STATION", details, APP_SUCCESS);
}

static void findAndDisplayRoute(const RouteGraph *graph, const StationNode *root)
{
    int sourceId, destinationId;
    size_t routeLength = 0, i, totalStations = (size_t)countStations(root);
    int *routeIds;
    AppStatus status;
    if (readInteger("Enter source station ID: ", &sourceId) <= 0) return;
    if (readInteger("Enter destination station ID: ", &destinationId) <= 0) return;
    if (totalStations == 0) { printf("No stations are available.\n"); return; }
    routeIds = (int *)malloc(totalStations * sizeof(*routeIds));
    if (routeIds == NULL) { printf("Unable to allocate route memory.\n"); return; }
    status = findRouteBFS(graph, root, sourceId, destinationId, routeIds, totalStations, &routeLength);
    if (status == APP_SUCCESS) {
        printf("\n========== BFS ROUTE =========\n");
        for (i = 0; i < routeLength; ++i) {
            StationNode *node = searchStation((StationNode *)root, routeIds[i]);
            if (i > 0) printf(" -> ");
            if (node != NULL) printf("%s (ID: %d)", node->station.name, node->station.id);
            else printf("Station ID %d", routeIds[i]);
        }
        printf("\nStations in route: %lu\n", (unsigned long)routeLength);
    } else if (status == APP_NOT_FOUND) {
        printf("No route exists between those stations, or a station ID was not found.\n");
    } else printf("Could not calculate the route.\n");
    free(routeIds);
}

static void addConnection(RouteGraph *graph, const StationNode *root, const User *user)
{
    int a, b, confirm;
    AppStatus status;
    if (!hasPermission(user->role, "ADD")) {
        printf("Permission denied: Admin access is required.\n");
        logEvent(user->username, "ADD_CONNECTION", "Permission denied", APP_PERMISSION_DENIED);
        return;
    }
    if (readInteger("Enter first station ID: ", &a) <= 0) return;
    if (readInteger("Enter second station ID: ", &b) <= 0) return;
    printf("Proposed bidirectional connection: %d <-> %d\n", a, b);
    if (readInteger("Confirm? Enter 1 for yes, 0 for no: ", &confirm) <= 0 || confirm != 1) {
        printf("Connection creation cancelled.\n");
        return;
    }
    status = addRouteConnection(graph, root, a, b);
    if (status != APP_SUCCESS) {
        printf("Could not add connection. Check station IDs and whether it already exists.\n");
        logEvent(user->username, "ADD_CONNECTION", "Creation failed", status);
        return;
    }
    status = saveConnectionsToCSV(graph, CONNECTIONS_FILE);
    if (status != APP_SUCCESS) {
        (void)removeRouteConnection(graph, a, b);
        printf("Could not save connections.csv; change rolled back in memory. Check the CSV file.\n");
        logEvent(user->username, "ADD_CONNECTION", "CSV save failed", status);
        return;
    }
    printf("Connection added and saved successfully.\n");
    logEvent(user->username, "ADD_CONNECTION", "Connection added successfully", APP_SUCCESS);
}

static void removeConnection(RouteGraph *graph, const StationNode *root, const User *user)
{
    int a, b, confirm;
    AppStatus status;
    if (!hasPermission(user->role, "DELETE")) {
        printf("Permission denied: Admin access is required.\n");
        logEvent(user->username, "REMOVE_CONNECTION", "Permission denied", APP_PERMISSION_DENIED);
        return;
    }
    if (readInteger("Enter first station ID: ", &a) <= 0) return;
    if (readInteger("Enter second station ID: ", &b) <= 0) return;
    if (!routeConnectionExists(graph, a, b)) {
        printf("That direct connection does not exist.\n");
        return;
    }
    if (readInteger("Confirm removal? Enter 1 for yes, 0 for no: ", &confirm) <= 0 || confirm != 1) {
        printf("Connection removal cancelled.\n");
        return;
    }
    status = removeRouteConnection(graph, a, b);
    if (status != APP_SUCCESS) { printf("Could not remove connection.\n"); return; }
    status = saveConnectionsToCSV(graph, CONNECTIONS_FILE);
    if (status != APP_SUCCESS) {
        /* Restore with the actual station tree, never NULL. */
        (void)addRouteConnection(graph, root, a, b);
        printf("Could not save connections.csv. In-memory connection restored; verify the file.\n");
        logEvent(user->username, "REMOVE_CONNECTION", "CSV save failed", status);
        return;
    }
    printf("Connection removed and saved successfully.\n");
    logEvent(user->username, "REMOVE_CONNECTION", "Connection removed successfully", APP_SUCCESS);
}

static void showMenu(UserRole role)
{
    printf("\n========== METRO MANAGEMENT SYSTEM ==========\n");
    printf("1. Display all stations\n2. Search station by ID\n3. Station summary\n");
    printf("4. Find route using BFS\n5. Display direct connections\n6. Display network map\n");
    if (role == ROLE_ADMIN) {
        printf("7. Add station\n8. Update station\n9. Delete station\n");
        printf("10. Add connection\n11. Remove connection\n");
    }
    printf("12. Logout\n0. Exit application\n=============================================\n");
}

static int runUserSession(StationNode **root, RouteGraph *graph, const User *user)
{
    int choice, result;
    for (;;) {
        showMenu(user->role);
        result = readInteger("Choose an option: ", &choice);
        if (result == 0) return 0;
        if (result < 0) continue;
        switch (choice) {
        case 1: displayStationsInOrder(*root); break;
        case 2: viewStation(*root); break;
        case 3: showStationSummary(*root); break;
        case 4: findAndDisplayRoute(graph, *root); break;
        case 5: displayConnections(graph, *root); break;
        case 6: displayNetworkMap(graph, *root); break;
        case 7:
            if (user->role == ROLE_ADMIN) addStation(root, user);
            else { printf("Permission denied.\n"); logEvent(user->username, "ADD_STATION", "Permission denied", APP_PERMISSION_DENIED); }
            break;
        case 8:
            if (user->role == ROLE_ADMIN) updateStationDetails(root, user);
            else { printf("Permission denied.\n"); logEvent(user->username, "UPDATE_STATION", "Permission denied", APP_PERMISSION_DENIED); }
            break;
        case 9:
            if (user->role == ROLE_ADMIN) deleteStationById(root, graph, user);
            else { printf("Permission denied.\n"); logEvent(user->username, "DELETE_STATION", "Permission denied", APP_PERMISSION_DENIED); }
            break;
        case 10:
            if (user->role == ROLE_ADMIN) addConnection(graph, *root, user);
            else { printf("Permission denied.\n"); logEvent(user->username, "ADD_CONNECTION", "Permission denied", APP_PERMISSION_DENIED); }
            break;
        case 11:
            if (user->role == ROLE_ADMIN) removeConnection(graph, *root, user);
            else { printf("Permission denied.\n"); logEvent(user->username, "REMOVE_CONNECTION", "Permission denied", APP_PERMISSION_DENIED); }
            break;
        case 12:
            logEvent(user->username, "LOGOUT", "User logged out", APP_SUCCESS);
            printf("Logged out successfully.\n");
            return 1;
        case 0:
            logEvent(user->username, "EXIT", "Application exit requested", APP_SUCCESS);
            return 0;
        default: printf("Invalid option. Please choose again.\n"); break;
        }
    }
}

int main(void)
{
    StationNode *root = NULL;
    RouteGraph graph;
    AppStatus status;
    int running = 1;

    initRouteGraph(&graph);
    printf("=============================================\nMETRO MANAGEMENT SYSTEM\n=============================================\n");
    status = loadStationsFromCSV(&root, STATIONS_FILE);
    if (status != APP_SUCCESS) {
        fprintf(stderr, "Error loading station data. Check data/stations.csv.\n");
        freeRouteGraph(&graph);
        freeStationTree(root);
        return EXIT_FAILURE;
    }
    printf("Loaded %d stations.\n", countStations(root));
    status = loadConnectionsFromCSV(&graph, root, CONNECTIONS_FILE);
    if (status != APP_SUCCESS) {
        fprintf(stderr, "Error loading railway connections. Check data/connections.csv.\n");
        freeRouteGraph(&graph);
        freeStationTree(root);
        return EXIT_FAILURE;
    }
    printf("Loaded %lu connections.\n", (unsigned long)graph.count);

    /*
     * Start the background audit worker only after application data has
     * loaded successfully. If startup fails, logEvent() falls back to
     * synchronous audit logging.
     */
    status = startAuditWorker();
    if (status == APP_SUCCESS) {
        auditWorkerStarted = 1;
        printf("Background audit logging started.\n");
    } else {
        fprintf(stderr,
                "Warning: background audit worker could not start; "
                "using synchronous audit logging.\n");
    }

    while (running) {
        char username[USERNAME_MAX], password[PASSWORD_MAX];
        int result;
        User user = {0};
        printf("\n--------------- LOGIN ---------------\n");
        result = readLine("Username: ", username, sizeof(username));
        if (result == 0) break;
        if (result < 0) continue;
        result = readLine("Password: ", password, sizeof(password));
        if (result == 0) break;
        if (result < 0) continue;
        status = authenticateUser(username, password, &user);
        if (status != APP_SUCCESS) {
            printf("Login failed. Check your credentials.\n");
            logEvent(username, "LOGIN", "Login failed", status);
            continue;
        }
        logEvent(user.username, "LOGIN", "Login successful", APP_SUCCESS);
        printf("\nWelcome, %s!\nRole: %s\n", user.username,
               user.role == ROLE_ADMIN ? "Admin" : "Viewer");
        running = runUserSession(&root, &graph, &user);
        if (running) printf("You can now sign in with another account.\n");
    }
    /*
     * Drain and stop the audit worker before releasing application resources
     * or exiting, so queued events (including the final session event)
     * have a chance to be written.
     */
    if (auditWorkerStarted) {
        stopAuditWorker();
        auditWorkerStarted = 0;
    }

    freeRouteGraph(&graph);
    freeStationTree(root);
    printf("\nMetro Management System closed.\n");
    return EXIT_SUCCESS;
}
