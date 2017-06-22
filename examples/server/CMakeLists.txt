cmake_minimum_required (VERSION 2.8)

project (lwm2mserver)

if(DTLS)
    message(FATAL_ERROR "DTLS option is not supported." )
endif()

include(${CMAKE_CURRENT_LIST_DIR}/../../core/wakaama.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../shared/shared.cmake)

add_definitions(-DLWM2M_SERVER_MODE)
add_definitions(${SHARED_DEFINITIONS} ${WAKAAMA_DEFINITIONS})

include_directories (${WAKAAMA_SOURCES_DIR} ${SHARED_INCLUDE_DIRS})

execute_process(COMMAND git clone git@github.com:vslapik/ugeneric.git)

SET(SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/lwm2mserver.c
    ${CMAKE_CURRENT_LIST_DIR}/db.c
    ${CMAKE_CURRENT_LIST_DIR}/rest.c
    ${CMAKE_CURRENT_LIST_DIR}/poller.c
    )

SET(CMAKE_C_FLAGS "-O0 -g")

add_library(ugeneric SHARED
    ugeneric/generic.c
    ugeneric/vector.c
    ugeneric/dict.c
    ugeneric/htbl.c
    ugeneric/mem.c
    ugeneric/bst.c
    ugeneric/sort.c
    ugeneric/stack.c
    ugeneric/backtrace.c
    ugeneric/string_utils.c
    ugeneric/file_utils.c
)
include_directories(ugeneric)
target_compile_definitions(ugeneric PRIVATE ENABLE_UASSERT_INPUT)

add_executable(${PROJECT_NAME} ${SOURCES} ${WAKAAMA_SOURCES} ${SHARED_SOURCES})
target_link_libraries(lwm2mserver microhttpd sqlite3 ugeneric pthread)


# Add WITH_LOGS to debug variant
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY COMPILE_DEFINITIONS $<$<CONFIG:Debug>:WITH_LOGS>)

SOURCE_GROUP(wakaama FILES ${WAKAAMA_SOURCES})
