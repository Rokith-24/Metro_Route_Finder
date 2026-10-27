
#ifndef ROUTE_H
#define ROUTE_H

#include <stddef.h>
#include "bst.h"

/*
 * Each connection represents a bidirectional railway link
 * between two existing stations.
 */
typedef struct {
    int stationA;
    int stationB;
} RouteConnection;

/*
 * Dynamic array of railway connections.
 * Stations themselves remain managed by the AVL tree.
 */
typedef struct {
    RouteConnection *connections;
    size_t count;
    size_t capacity;
} RouteGraph;

/* Graph lifecycle */
void initRouteGraph(RouteGraph *graph);
void freeRouteGraph(RouteGraph *graph);

/* Connection management */
AppStatus addRouteConnection(
    RouteGraph *graph,
    const StationNode *stations,
    int stationA,
    int stationB
);

AppStatus removeRouteConnection(
    RouteGraph *graph,
    int stationA,
    int stationB
);

int routeConnectionExists(
    const RouteGraph *graph,
    int stationA,
    int stationB
);

/* Remove every connection involving a station */
size_t removeStationConnections(
    RouteGraph *graph,
    int stationId
);

/* Network display */
void displayConnections(
    const RouteGraph *graph,
    const StationNode *stations
);

void displayNetworkMap(
    const RouteGraph *graph,
    const StationNode *stations
);

/* BFS route finding */
AppStatus findRouteBFS(
    const RouteGraph *graph,
    const StationNode *stations,
    int sourceId,
    int destinationId,
    int *routeIds,
    size_t routeCapacity,
    size_t *routeLength
);

#endif
