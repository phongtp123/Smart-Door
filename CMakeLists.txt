# Define CMake minimum version required to build project.
cmake_minimum_required(VERSION 3.20.0)

# Set path to custom module, when build project, Zephyr compile modules and find this module too when compile and 
# provide header file of the custom module.
set(ZEPHYR_EXTRA_MODULES "${CMAKE_SOURCE_DIR}/../../modules/drivers/bh1750" 
                         "${CMAKE_SOURCE_DIR}/../../modules/drivers/rc522")
# Include Zephyr package when compile project.
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
# Provide project name.
project(smart_door)

# Target to main application
target_sources(app PRIVATE src/main.c)