# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

# Add HomeKit ADK, u8g2, dht
set(EXTRA_COMPONENT_DIRS ${CMAKE_CURRENT_LIST_DIR}/esp-apple-homekit-adk
                         ${CMAKE_CURRENT_LIST_DIR}/components/
                         ${CMAKE_CURRENT_LIST_DIR}/components/esp-idf-lib/components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
if(CONFIG_HAP_MFI_HW_AUTH)
    add_definitions(-DHAVE_MFI_HW_AUTH=1)
endif()
project(Lightbulb)


