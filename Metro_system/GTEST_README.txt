METRO MANAGEMENT SYSTEM - GOOGLE TEST SUITE

Contents:
- test_bst_gtest.cpp: station AVL/tree operations
- test_route_gtest.cpp: connection management and BFS routes
- test_storage_gtest.cpp: station/connection CSV persistence
- test_auth_gtest.cpp: role permissions and invalid credentials
- test_audit_log_gtest.cpp: audit log output/invalid arguments

IMPORTANT
These are GoogleTest C++ tests around your C modules. They use your project's current API names (StationNode, Station, RouteGraph, AppStatus, etc.). Put these files in test_gtest/ and adjust only if your local headers differ.
Run tests from the project root because data paths are relative.

BUILDING ON WINDOWS / MINGW
GoogleTest must be installed/built for the same MinGW toolchain. A typical approach is to compile each C source with gcc as C, then link the resulting object files with g++ and GoogleTest libraries. Do not compile .c source files directly as C++.

Example after GoogleTest is available as a library:

mkdir build\gtest_obj 2>nul

gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude -c src\bst.c -o build\gtest_obj\bst.o
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude -c src\route.c -o build\gtest_obj\route.o
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude -c src\storage.c -o build\gtest_obj\storage.o
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude -c src\auth.c -o build\gtest_obj\auth.o
gcc -std=c11 -Wall -Wextra -Wpedantic -Iinclude -c src\audit_log.c -o build\gtest_obj\audit_log.o

Then link the five C++ test files with the objects and your GoogleTest libraries, for example:

g++ -std=c++17 -Iinclude -I<gtest_include> test_gtest\test_bst_gtest.cpp test_gtest\test_route_gtest.cpp test_gtest\test_storage_gtest.cpp test_gtest\test_auth_gtest.cpp test_gtest\test_audit_log_gtest.cpp build\gtest_obj\bst.o build\gtest_obj\route.o build\gtest_obj\storage.o build\gtest_obj\auth.o build\gtest_obj\audit_log.o -L<gtest_lib_dir> -lgtest_main -lgtest -pthread -o build\metro_gtests.exe

Replace <gtest_include> and <gtest_lib_dir> with the actual GoogleTest paths. On Windows, ensure the library was built with the same MinGW compiler. The audit_worker.c module is Windows-specific; it can be added to a separate worker test target when you want threading covered in GTest too.

NOTES
1. Tests use temporary files under data/ and remove them during teardown. Ensure data/ exists.
2. The audit log test removes data/audit.log before its test; back up any existing audit history first, or change it to use a dedicated test path if audit_log.c supports configurable paths.
3. If your auth module expects a particular users.csv format, the invalid-user test should still return a non-success status; it assumes that file is readable or missing-user failure is reported as non-success.
4. Some GoogleTest assertions in SetUp helpers may report failures but execution continues; that is acceptable for this initial suite, and can be refined later.
