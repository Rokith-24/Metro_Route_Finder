#include <gtest/gtest.h>
extern "C" {
#include "common.h"
#include "auth.h"
}

TEST(AuthPermissionTest, AdminCanPerformAdministrativeActions) {
    EXPECT_TRUE(hasPermission(ROLE_ADMIN, "ADD"));
    EXPECT_TRUE(hasPermission(ROLE_ADMIN, "UPDATE"));
    EXPECT_TRUE(hasPermission(ROLE_ADMIN, "DELETE"));
}

TEST(AuthPermissionTest, ViewerCannotPerformAdministrativeActions) {
    EXPECT_FALSE(hasPermission(ROLE_VIEWER, "ADD"));
    EXPECT_FALSE(hasPermission(ROLE_VIEWER, "UPDATE"));
    EXPECT_FALSE(hasPermission(ROLE_VIEWER, "DELETE"));
}

TEST(AuthPermissionTest, UnknownPermissionIsDenied) {
    EXPECT_FALSE(hasPermission(ROLE_VIEWER, "SUPERUSER"));
    EXPECT_FALSE(hasPermission(ROLE_ADMIN, "SUPERUSER"));
}

TEST(AuthTest, InvalidCredentialsDoNotAuthenticate) {
    User user = {};
    EXPECT_NE(authenticateUser("gtest_user_that_does_not_exist", "not-a-real-password", &user), APP_SUCCESS);
}
