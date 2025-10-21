cmake_minimum_required(VERSION 3.12)

find_package(Qt6 REQUIRED COMPONENTS Core Test)

add_executable(tst_i2pmanager
    i2pmanager/tst_I2PManager.cpp
    ${CMAKE_SOURCE_DIR}/src/I2PManager.cpp
)

target_link_libraries(tst_i2pmanager PRIVATE
    Qt6::Core
    Qt6::Test
)

target_include_directories(tst_i2pmanager PRIVATE
    ${CMAKE_SOURCE_DIR}/src
)

set_target_properties(tst_i2pmanager PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set(TEST_DATA_DIR ${CMAKE_CURRENT_SOURCE_DIR}/i2pmanager)
set(TEST_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR})

configure_file(${TEST_DATA_DIR}/fake_i2pd.py
               ${TEST_OUTPUT_DIR}/fake_i2pd.py COPYONLY
               FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)

add_test(NAME I2PManagerToggle COMMAND tst_i2pmanager)
