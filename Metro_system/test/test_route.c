
#include <assert.h>
#include <stdio.h>

#include "route.h"

static Station createTestStation(int id, const char *name)
{
    Station station = {0};

    station.id = id;
    snprintf(station.name, sizeof(station.name), "%s", name);
    station.distance = 1.0f;
    station.fare = 10.0f;
    station.platform = 1;

    return station;
}

int main(void)
{
    StationNode *stations = NULL;
    RouteGraph graph;
    int route[10];
    size_t routeLength = 0;

    initRouteGraph(&graph);

    assert(insertStation(&stations,
        createTestStation(101, "Central")) == APP_SUCCESS);
    assert(insertStation(&stations,
        createTestStation(102, "Park Avenue")) == APP_SUCCESS);
    assert(insertStation(&stations,
        createTestStation(103, "Nehru Stadium")) == APP_SUCCESS);
    assert(insertStation(&stations,
        createTestStation(104, "Tech City Hub")) == APP_SUCCESS);
    assert(insertStation(&stations,
        createTestStation(105, "University Campus")) == APP_SUCCESS);
    assert(insertStation(&stations,
        createTestStation(106, "Airport")) == APP_SUCCESS);
    assert(insertStation(&stations,
        createTestStation(107, "Isolated Station")) == APP_SUCCESS);

    /* Add the straight-line demo connections. */
    assert(addRouteConnection(&graph, stations, 101, 102)
           == APP_SUCCESS);
    assert(addRouteConnection(&graph, stations, 102, 103)
           == APP_SUCCESS);
    assert(addRouteConnection(&graph, stations, 103, 104)
           == APP_SUCCESS);
    assert(addRouteConnection(&graph, stations, 104, 105)
           == APP_SUCCESS);
    assert(addRouteConnection(&graph, stations, 105, 106)
           == APP_SUCCESS);

    puts("Connection creation tests passed.");

    /* Duplicate connections must be rejected in either direction. */
    assert(addRouteConnection(&graph, stations, 101, 102)
           == APP_DUPLICATE);
    assert(addRouteConnection(&graph, stations, 102, 101)
           == APP_DUPLICATE);

    /* Self-connections must be rejected. */
    assert(addRouteConnection(&graph, stations, 101, 101)
           == APP_INVALID_INPUT);

    /* Unknown station IDs must be rejected. */
    assert(addRouteConnection(&graph, stations, 101, 999)
           == APP_NOT_FOUND);

    puts("Connection validation tests passed.");

    /* BFS should find the route from 101 to 106. */
    assert(findRouteBFS(&graph, stations, 101, 106,
                        route, 10, &routeLength) == APP_SUCCESS);

    assert(routeLength == 6);
    assert(route[0] == 101);
    assert(route[1] == 102);
    assert(route[2] == 103);
    assert(route[3] == 104);
    assert(route[4] == 105);
    assert(route[5] == 106);

    puts("BFS route test passed.");

    /* Reverse travel should also work. */
    assert(findRouteBFS(&graph, stations, 106, 101,
                        route, 10, &routeLength) == APP_SUCCESS);

    assert(routeLength == 6);
    assert(route[0] == 106);
    assert(route[5] == 101);

    puts("Bidirectional route test passed.");

    /* Station 107 exists but is not connected to the network. */
    assert(findRouteBFS(&graph, stations, 101, 107,
                        route, 10, &routeLength) == APP_NOT_FOUND);

    puts("Disconnected station test passed.");

    /* Remove connection 103 <-> 104. */
    assert(removeRouteConnection(&graph, 103, 104)
           == APP_SUCCESS);

    assert(!routeConnectionExists(&graph, 103, 104));

    assert(findRouteBFS(&graph, stations, 101, 106,
                        route, 10, &routeLength) == APP_NOT_FOUND);

    puts("Connection removal test passed.");

    /* Removing all edges of station 105 removes 104 <-> 105. */
    assert(removeStationConnections(&graph, 105) == 2);

    assert(!routeConnectionExists(&graph, 104, 105));

    puts("Station connection cleanup test passed.");

    displayConnections(&graph, stations);
    displayNetworkMap(&graph, stations);

    freeRouteGraph(&graph);
    freeStationTree(stations);

    puts("\nAll route tests passed!");
    return 0;
}
