
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "storage.h"

static Station makeStation(int id, const char *name)
{
    Station s = {0};

    s.id = id;
    snprintf(s.name, sizeof(s.name), "%s", name);
    s.distance = 1.0f;
    s.fare = 10.0f;
    s.platform = 1;

    return s;
}

int main(void)
{
    StationNode *stations = NULL;
    RouteGraph graph;
    RouteGraph loadedGraph;
    FILE *file;

    initRouteGraph(&graph);
    initRouteGraph(&loadedGraph);

    assert(insertStation(&stations,
        makeStation(101, "Central")) == APP_SUCCESS);
    assert(insertStation(&stations,
        makeStation(102, "Park Avenue")) == APP_SUCCESS);
    assert(insertStation(&stations,
        makeStation(103, "Airport")) == APP_SUCCESS);

    /* Create a temporary CSV fixture. */
    file = fopen("build/test_connections.csv", "w");
    assert(file != NULL);

    fprintf(file, "station_a,station_b\n");
    fprintf(file, "101,102\n");
    fprintf(file, "102,103\n");

    assert(fclose(file) == 0);

    /* Test loading. */
    assert(loadConnectionsFromCSV(
        &graph, stations, "build/test_connections.csv"
    ) == APP_SUCCESS);

    assert(graph.count == 2);
    assert(routeConnectionExists(&graph, 101, 102));
    assert(routeConnectionExists(&graph, 102, 103));

    puts("Connection CSV loading test passed.");

    /* Test saving. */
    assert(saveConnectionsToCSV(
        &graph, "build/test_connections_saved.csv"
    ) == APP_SUCCESS);

    puts("Connection CSV saving test passed.");

    /* Test round-trip: load the saved file into another graph. */
    assert(loadConnectionsFromCSV(
        &loadedGraph, stations, "build/test_connections_saved.csv"
    ) == APP_SUCCESS);

    assert(loadedGraph.count == 2);
    assert(routeConnectionExists(&loadedGraph, 101, 102));
    assert(routeConnectionExists(&loadedGraph, 102, 103));

    puts("Connection CSV round-trip test passed.");

    /* Invalid station IDs must be rejected. */
    file = fopen("build/test_connections_invalid.csv", "w");
    assert(file != NULL);

    fprintf(file, "station_a,station_b\n");
    fprintf(file, "101,999\n");

    assert(fclose(file) == 0);

    assert(loadConnectionsFromCSV(
        &loadedGraph, stations, "build/test_connections_invalid.csv"
    ) == APP_NOT_FOUND);

    /* Failed load must preserve the previous graph. */
    assert(loadedGraph.count == 2);
    assert(routeConnectionExists(&loadedGraph, 101, 102));

    puts("Invalid connection and rollback test passed.");

    freeRouteGraph(&graph);
    freeRouteGraph(&loadedGraph);
    freeStationTree(stations);

    puts("\nAll connection storage tests passed!");
    return 0;
}
