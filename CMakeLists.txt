set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=missing-field-initializers -Wno-error")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-error=missing-field-initializers -Wno-error")

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(MicroClimaEdge)
