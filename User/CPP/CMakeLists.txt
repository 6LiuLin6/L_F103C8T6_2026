cmake_minimum_required(VERSION 3.22)

get_filename_component(MODULE_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)


file(GLOB_RECURSE MODULE_SOURCES
    CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
)


add_library(${MODULE_NAME}
        ${MODULE_SOURCES}
)


target_link_libraries(${MODULE_NAME} PRIVATE
    stm32cubemx
)
target_include_directories(${MODULE_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)