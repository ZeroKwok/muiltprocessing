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

project(muiltprocessing VERSION 0.1.2)
option(MUILTPROCESSING_BUILD_EXAMPLE "Compile the example" ON)

find_package(cppzmq CONFIG REQUIRED)

add_library(${PROJECT_NAME} INTERFACE)
set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include
    INTERFACE_LINK_LIBRARIES cppzmq)
set(MUILTPROCESSING_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

if(MUILTPROCESSING_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()