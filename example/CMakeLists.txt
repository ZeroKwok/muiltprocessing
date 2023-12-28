# This file is part of the muiltprocessing distribution.
# Copyright (c) 2023 zero.kwok@foxmail.com
# 
# This is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 3 of
# the License, or (at your option) any later version.
# 
# This software is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
# Lesser General Public License for more details.
# 
# You should have received a copy of the GNU Lesser General Public
# License along with this software; 
# If not, see <http://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.10)

project(example_for_muiltprocessing)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8)
endif()

set(Boost_USE_STATIC_LIBS ON)
add_compile_definitions(BOOST_LIB_DIAGNOSTIC=1)
add_compile_definitions(AUTO_LINK_LIB_DIAGNOSTIC=1)

find_package(utility REQUIRED)
find_package(Boost REQUIRED thread date_time filesystem)

include_directories(${utility_INCLUDE_DIRS})
link_directories   (${utility_LIBRARY_DIRS})
include_directories(${Boost_INCLUDE_DIRS})
link_directories   (${Boost_LIBRARY_DIRS})

add_executable(${PROJECT_NAME} example.cpp)
target_compile_definitions(${PROJECT_NAME} PRIVATE UTILITY_SUPPORT_BOOST)
target_link_libraries(${PROJECT_NAME} muiltprocessing)
