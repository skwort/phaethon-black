# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(base)

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

zephyr_nanopb_sources(app ${CMAKE_CURRENT_SOURCE_DIR}/../../../shared/phaethon.proto)

FILE(GLOB app_sources src/*.c)
FILE(GLOB lib_sources ../../lib/*.c)
target_sources(app PRIVATE ${app_sources} ${lib_sources})
target_include_directories(app PRIVATE ../../include)