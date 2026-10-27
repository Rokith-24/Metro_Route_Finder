#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>
#include <string>
extern "C" {
#include "common.h"
#include "station.h"
#include "bst.h"
#include "storage.h"
#include "route.h"
}

class StorageTest : public ::testing::Test {
protected:
    StationNode* root = nullptr;
    RouteGraph graph;
    const char* stationsPath = "data/gtest_stations_tmp.csv";
    const char* connectionsPath = "data/gtest_connections_tmp.csv";
    void SetUp() override { initRouteGraph(&graph); }
    void TearDown() override {
        freeStationTree(root); root = nullptr; freeRouteGraph(&graph);
        std::remove(stationsPath); std::remove(connectionsPath);
    }
    Station makeStation(int id, const char* name) {
        Station s = {};
        s.id=id; std::snprintf(s.name, sizeof(s.name), "%s", name);
        s.distance=1.5f; s.fare=3.0f; s.platform=2; return s;
    }
};

TEST_F(StorageTest, SaveAndLoadStationsRoundTrip) {
    ASSERT_EQ(insertStation(&root, makeStation(1, "Central")), APP_SUCCESS);
    ASSERT_EQ(insertStation(&root, makeStation(2, "Park")), APP_SUCCESS);
    ASSERT_EQ(saveStationsToCSV(root, stationsPath), APP_SUCCESS);
    freeStationTree(root); root = nullptr;
    EXPECT_EQ(loadStationsFromCSV(&root, stationsPath), APP_SUCCESS);
    EXPECT_EQ(countStations(root), 2);
    ASSERT_NE(searchStation(root, 1), nullptr);
    EXPECT_STREQ(searchStation(root, 1)->station.name, "Central");
}

TEST_F(StorageTest, MissingStationFileReturnsError) {
    EXPECT_NE(loadStationsFromCSV(&root, "data/definitely_missing_gtest_file.csv"), APP_SUCCESS);
}

TEST_F(StorageTest, SaveAndLoadConnectionsRoundTrip) {
    ASSERT_EQ(insertStation(&root, makeStation(1, "Central")), APP_SUCCESS);
    ASSERT_EQ(insertStation(&root, makeStation(2, "Park")), APP_SUCCESS);
    ASSERT_EQ(addRouteConnection(&graph, root, 1, 2), APP_SUCCESS);
    ASSERT_EQ(saveConnectionsToCSV(&graph, connectionsPath), APP_SUCCESS);
    freeRouteGraph(&graph); initRouteGraph(&graph);
    EXPECT_EQ(loadConnectionsFromCSV(&graph, root, connectionsPath), APP_SUCCESS);
    EXPECT_EQ(graph.count, 1u);
    EXPECT_TRUE(routeConnectionExists(&graph, 1, 2));
}
