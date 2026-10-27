#include <gtest/gtest.h>
extern "C" {
#include "common.h"
#include "station.h"
#include "bst.h"
}

class StationTreeTest : public ::testing::Test {
protected:
    StationNode* root = nullptr;
    Station makeStation(int id, const char* name) {
        Station s = {};
        s.id = id;
        std::snprintf(s.name, sizeof(s.name), "%s", name);
        s.distance = static_cast<float>(id) * 1.25f;
        s.fare = static_cast<float>(id) * 2.0f;
        s.platform = 1;
        return s;
    }
    void TearDown() override { freeStationTree(root); root = nullptr; }
};

TEST_F(StationTreeTest, EmptyTreeHasZeroStations) {
    EXPECT_EQ(countStations(root), 0);
    int id = -1;
    EXPECT_FALSE(findMinimumStationId(root, &id));
    EXPECT_FALSE(findMaximumStationId(root, &id));
}

TEST_F(StationTreeTest, InsertSearchAndCount) {
    EXPECT_EQ(insertStation(&root, makeStation(20, "Central")), APP_SUCCESS);
    EXPECT_EQ(insertStation(&root, makeStation(10, "Park")), APP_SUCCESS);
    EXPECT_EQ(insertStation(&root, makeStation(30, "Museum")), APP_SUCCESS);
    EXPECT_EQ(countStations(root), 3);
    ASSERT_NE(searchStation(root, 10), nullptr);
    EXPECT_STREQ(searchStation(root, 10)->station.name, "Park");
    EXPECT_EQ(searchStation(root, 999), nullptr);
}

TEST_F(StationTreeTest, DuplicateIdIsRejected) {
    ASSERT_EQ(insertStation(&root, makeStation(10, "Park")), APP_SUCCESS);
    EXPECT_EQ(insertStation(&root, makeStation(10, "Duplicate")), APP_DUPLICATE);
    EXPECT_EQ(countStations(root), 1);
}

TEST_F(StationTreeTest, MinimumAndMaximumIdsAreCorrect) {
    ASSERT_EQ(insertStation(&root, makeStation(40, "D")), APP_SUCCESS);
    ASSERT_EQ(insertStation(&root, makeStation(5, "A")), APP_SUCCESS);
    ASSERT_EQ(insertStation(&root, makeStation(22, "B")), APP_SUCCESS);
    int minId = 0, maxId = 0;
    ASSERT_TRUE(findMinimumStationId(root, &minId));
    ASSERT_TRUE(findMaximumStationId(root, &maxId));
    EXPECT_EQ(minId, 5);
    EXPECT_EQ(maxId, 40);
}

TEST_F(StationTreeTest, UpdateAndDeleteStation) {
    Station s = makeStation(7, "OldName");
    ASSERT_EQ(insertStation(&root, s), APP_SUCCESS);
    s = makeStation(7, "NewName");
    ASSERT_EQ(updateStation(root, 7, s), APP_SUCCESS);
    ASSERT_NE(searchStation(root, 7), nullptr);
    EXPECT_STREQ(searchStation(root, 7)->station.name, "NewName");
    EXPECT_EQ(deleteStation(&root, 7), APP_SUCCESS);
    EXPECT_EQ(searchStation(root, 7), nullptr);
    EXPECT_EQ(countStations(root), 0);
}
