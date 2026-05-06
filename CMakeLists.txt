cmake_minimum_required(VERSION 3.23)
project(RITMInventorNumber CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(zint REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GTKMM REQUIRED  IMPORTED_TARGET gtkmm-4.0)
pkg_check_modules(SIGCPP REQUIRED IMPORTED_TARGET sigc++-3.0)
pkg_check_modules(MYSQL REQUIRED IMPORTED_TARGET libmariadb)
pkg_check_modules(CAIRO16 REQUIRED IMPORTED_TARGET cairomm-1.16)

# FIND MySQL++
find_path(MYSQLPP_INCLUDE_DIR
    NAMES mysql++/mysql++.h
    PATH_SUFFIXES mysql++
)

find_library(MYSQLPP_LIBRARY
    NAMES mysqlpp
)

if(NOT MYSQLPP_INCLUDE_DIR OR NOT MYSQLPP_LIBRARY)
    message(FATAL_ERROR "MySQL++ no install.")
endif()

file(GLOB SRC_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h")
file(GLOB SRC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(imanager
    ${SRC_HEADERS}
    ${SRC_SOURCES}
)

target_include_directories(imanager PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include" ${MYSQLPP_INCLUDE_DIR})
target_link_libraries(imanager PRIVATE
    PkgConfig::GTKMM
    PkgConfig::SIGCPP
    PkgConfig::MYSQL
    PkgConfig::CAIRO16
    zint::zint
    ${MYSQLPP_LIBRARY}
)