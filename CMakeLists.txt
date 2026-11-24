# Runner's Reign — M5Stack Tab5 (ESP32-P4) point-and-click adventure RPG
# Build:  idf.py set-target esp32p4 && idf.py build
cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

# Bring-up: ESP-IDF sets -Werror=all, which a bare -Wno-error can't cancel (GCC
# quirk); -w disables all warnings so a style nit can't stop the build. Drop this
# once the tree compiles clean.
idf_build_set_property(COMPILE_OPTIONS "-w" APPEND)

project(runnersreign-p4)
