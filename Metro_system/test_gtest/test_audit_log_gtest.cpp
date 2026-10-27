#include <gtest/gtest.h>
#include <cstdio>
#include <fstream>
#include <string>
extern "C" {
#include "common.h"
#include "audit_log.h"
}

TEST(AuditLogTest, WritesEventWithExpectedFields) {
    const char* path = AUDIT_FILE;
    std::remove(path);
    ASSERT_EQ(writeAuditLog("gtest_user", "GTEST_ACTION", "unit test detail", APP_SUCCESS), APP_SUCCESS);
    std::ifstream in(path);
    ASSERT_TRUE(in.good());
    std::string line; std::getline(in, line);
    EXPECT_NE(line.find("user=gtest_user"), std::string::npos);
    EXPECT_NE(line.find("action=GTEST_ACTION"), std::string::npos);
    EXPECT_NE(line.find("details=unit test detail"), std::string::npos);
    EXPECT_NE(line.find("result=0"), std::string::npos);
}

TEST(AuditLogTest, RejectsNullArguments) {
    EXPECT_EQ(writeAuditLog(nullptr, "ACTION", "detail", APP_SUCCESS), APP_INVALID_INPUT);
    EXPECT_EQ(writeAuditLog("user", nullptr, "detail", APP_SUCCESS), APP_INVALID_INPUT);
    EXPECT_EQ(writeAuditLog("user", "ACTION", nullptr, APP_SUCCESS), APP_INVALID_INPUT);
}
