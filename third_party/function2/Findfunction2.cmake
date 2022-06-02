# Makes it possible to find function2 through find_package(function2 REQUIRED)
# when this source directory was added to the CMake module path.
# For instance it could be done through adding this to the CMakeLists.txt
# file in the parent directory:
# ```cmake
# list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/function2")
# ```

set(FU2_IS_FIND_INCLUDED ON)
include("${CMAKE_CURRENT_LIST_DIR}/CMakeLists.txt")
