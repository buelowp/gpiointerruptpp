cmake_minimum_required (VERSION 3.5.0)
project (gpiointerruptpp)

FILE (GLOB SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
FILE (GLOB HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/*.h")

include_directories (BEFORE "${CMAKE_CURRENT_SOURCE_DIR}")

SET (CMAKE_INCLUDE_CURRENT_DIR ON)
SET (CMAKE_CXX_STANDARD 14)
SET (THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Threads REQUIRED)

add_library (${PROJECT_NAME} SHARED ${SOURCES} ${HEADERS})
target_link_libraries (${PROJECT_NAME} Threads::Threads)

set_target_properties (${PROJECT_NAME} PROPERTIES VERSION 1.0 SOVERSION 1)

install (TARGETS ${PROJECT_NAME} DESTINATION /lib)
install (FILES ${HEADERS} DESTINATION /include)

