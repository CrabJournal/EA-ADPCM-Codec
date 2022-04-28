cmake_minimum_required(VERSION 3.21)
project(EA_ADPCM_codec)

set(CMAKE_CXX_STANDARD 17)

add_executable(EA_ADPCM_codec
        exe/exe.cpp
        EA-ADPCM-codec/EA-ADPCM-codec.cpp
        EA-ADPCM-codec/vector_SIMD.h)

SET(TARGET_SRC
        EA-ADPCM-codec/EA-ADPCM-codec.cpp
        )

file(GLOB SOURCES
        EA-ADPCM-codec/EA-ADPCM-codec.cpp
        )