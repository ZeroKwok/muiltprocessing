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

project(muiltprocessing VERSION 0.1.2.0)
option(MUILTPROCESSING_BUILD_EXAMPLE "Compile the example" ON)

find_package(cppzmq CONFIG REQUIRED)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME} INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_PREFIX}/include>)

set_target_properties(${PROJECT_NAME} PROPERTIES INTERFACE_LINK_LIBRARIES cppzmq)
set(MUILTPROCESSING_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include PARENT_SCOPE)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_INSTALL_PREFIX}/cmake/${PROJECT_NAME}ConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMinorVersion)

install(TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}Config
    ARCHIVE DESTINATION lib COMPONENT Development
    LIBRARY DESTINATION lib COMPONENT Runtime
    RUNTIME DESTINATION bin COMPONENT Runtime
    BUNDLE  DESTINATION bin COMPONENT Runtime
    PUBLIC_HEADER DESTINATION include COMPONENT Development
)
install(EXPORT ${PROJECT_NAME}Config DESTINATION cmake)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include DESTINATION ./)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/example DESTINATION ./)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/LICENSE DESTINATION ./ OPTIONAL)
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/README.md DESTINATION ./ OPTIONAL)

if (NOT ${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/build/templates.in" 
                   "${CMAKE_CURRENT_SOURCE_DIR}/build/build_dynamic_x86_vc14.2.py")
endif()

if(MUILTPROCESSING_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()