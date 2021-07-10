cmake_minimum_required(VERSION 3.10)
project(prunejuice_tools)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_FLAGS "-Wall -pthread")

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -Wextra")
if (Linux)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pg")
endif ()

# Release
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

set (Boost_NO_BOOST_CMAKE ON)
find_package(Boost 1.75 REQUIRED)

if (METALL_ROOT)
    find_path(METALL_INCLUDE_DIR metall/metall.hpp PATHS ${METALL_ROOT}/include REQUIRED)
else ()
    find_path(METALL_INCLUDE_DIR metall/metall.hpp REQUIRED)
endif ()

function(add_exe src)
    add_executable(${src} ${src}.cpp)
    target_include_directories(${src} PUBLIC ${Boost_INCLUDE_DIRS})
    target_include_directories(${src} PUBLIC ${METALL_INCLUDE_DIR})
    target_link_libraries(${src} stdc++fs)
endfunction()

add_exe(create_graph_metall)
add_exe(st_path_metall)

#add_exe(create_graph_metall_int_id)
#add_exe(st_path_metall_int_id)