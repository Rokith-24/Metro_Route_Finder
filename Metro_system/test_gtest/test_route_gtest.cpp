#include <gtest/gtest.h>
#include <vector>
extern "C" {
#include "common.h"
#include "station.h"
#include "bst.h"
#include "route.h"
}

class RouteGraphTest : public ::testing::Test {
protected:
    StationNode* root = nullptr;
    RouteGraph graph;
    Station station(int id, const char* name) {
        Station s = {};
        s.id = id; std::snprintf(s.name, sizeof(s.name), "%s", name);
        s.distance = 1.0f; s.fare = 2.0f; s.platform = 1;
        return s;
    }
    void SetUp() override {
        initRouteGraph(&graph);
        ASSERT_EQ(insertStation(&root, station(1, "A")), APP_SUCCESS);
        ASSERT_EQ(insertStation(&root, station(2, "B")), APP_SUCCESS);
        ASSERT_EQ(insertStation(&root, station(3, "C")), APP_SUCCESS);
        ASSERT_EQ(insertStation(&root, station(4, "D")), APP_SUCCESS);
    }
    void TearDown() override { freeRouteGraph(&graph); freeStationTree(root); }
};

TEST_F(RouteGraphTest, AddConnectionAndRejectDuplicate) {
    EXPECT_EQ(addRouteConnection(&graph, root, 1, 2), APP_SUCCESS);
    EXPECT_TRUE(routeConnectionExists(&graph, 1, 2));
    EXPECT_TRUE(routeConnectionExists(&graph, 2, 1));
    EXPECT_EQ(addRouteConnection(&graph, root, 2, 1), APP_DUPLICATE);
    EXPECT_EQ(graph.count, 1u);
}

TEST_F(RouteGraphTest, RejectInvalidOrMissingStations) {
    EXPECT_EQ(addRouteConnection(&graph, root, 1, 1), APP_INVALID_INPUT);
    EXPECT_EQ(addRouteConnection(&graph, root, 1, 999), APP_NOT_FOUND);
    EXPECT_EQ(addRouteConnection(&graph, root, 0, 2), APP_INVALID_INPUT);
}

TEST_F(RouteGraphTest, RemoveConnection) {
    ASSERT_EQ(addRouteConnection(&graph, root, 1, 2), APP_SUCCESS);
    EXPECT_EQ(removeRouteConnection(&graph, 1, 2), APP_SUCCESS);
    EXPECT_FALSE(routeConnectionExists(&graph, 1, 2));
    EXPECT_EQ(removeRouteConnection(&graph, 1, 2), APP_NOT_FOUND);
}

TEST_F(RouteGraphTest, BreadthFirstSearchFindsShortestPath) {
    ASSERT_EQ(addRouteConnection(&graph, root, 1, 2), APP_SUCCESS);
    ASSERT_EQ(addRouteConnection(&graph, root, 2, 3), APP_SUCCESS);
    ASSERT_EQ(addRouteConnection(&graph, root, 1, 4), APP_SUCCESS);
    ASSERT_EQ(addRouteConnection(&graph, root, 4, 3), APP_SUCCESS);
    int routeIds[4] = {0}; size_t routeLength = 0;
    EXPECT_EQ(findRouteBFS(&graph, root, 1, 3, routeIds, 4, &routeLength), APP_SUCCESS);
    EXPECT_EQ(routeLength, 3u);
    EXPECT_EQ(routeIds[0], 1);
    EXPECT_EQ(routeIds[2], 3);
}

TEST_F(RouteGraphTest, NoRouteReturnsNotFound) {
    int routeIds[4] = {0}; size_t routeLength = 0;
    EXPECT_EQ(findRouteBFS(&graph, root, 1, 4, routeIds, 4, &routeLength), APP_NOT_FOUND);
}
