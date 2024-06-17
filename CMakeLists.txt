cmake_minimum_required(VERSION 3.16)
project(Prismatica)

# Source, external and binary directories

set(SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(RESOURCES_DIR "${SRC_DIR}/resources")
set(EXT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external")
set(GLM_DIR "${EXT_DIR}/glm-master")
set(STB_DIR  "${EXT_DIR}/stb")
set(PROJECT_VERSION "0.0.1")
set(WORKING_DIRECTORY "${EXECUTABLE_OUTPUT_PATH}")
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
set(CMAKE_CXX_STANDARD 17)
set(BENCHMARK_MODE ${BUILD_MQTT})

aux_source_directory(${SRC_DIR} SOURCES)

if (MSVC)
    
else()
    add_compile_options(-Ofast -march=native -flto)
endif()

# Add the exectable to CMAKE
add_executable(${PROJECT_NAME} ${SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${SRC_DIR})
set_target_properties(${PROJECT_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY ${EXECUTABLE_OUTPUT_PATH})

# GLM
file(GLOB glm_src "${GLM_DIR}/glm/*.hpp")
target_include_directories(${PROJECT_NAME} PRIVATE ${GLM_DIR})
# We might want to make this PUBLIC in the future.
#target_precompile_headers(${PROJECT_NAME} PRIVATE ${glm_src})

# STB
file(GLOB stb_src "${STB_DIR}/.h")
target_include_directories(${PROJECT_NAME} PRIVATE ${STB_DIR})
# We might want to make this PUBLIC in the future.
#target_precompile_headers(${PROJECT_NAME} PRIVATE ${stb_src})

# paho

if(BENCHMARK_MODE)
    find_package(PahoMqttCpp REQUIRED)
    target_link_libraries(${PROJECT_NAME} PahoMqttCpp::paho-mqttpp3)
    add_compile_definitions()
else()
    target_link_libraries(${PROJECT_NAME})
endif()