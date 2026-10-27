
#ifndef STORAGE_H
#define STORAGE_H

#include "bst.h"
#include "route.h"

AppStatus loadStationsFromCSV(
    StationNode **root,
    const char *filename
);

AppStatus saveStationsToCSV(
    const StationNode *root,
    const char *filename
);

/* New: connection persistence */
AppStatus loadConnectionsFromCSV(
    RouteGraph *graph,
    const StationNode *stations,
    const char *filename
);

AppStatus saveConnectionsToCSV(
    const RouteGraph *graph,
    const char *filename
);

#endif
