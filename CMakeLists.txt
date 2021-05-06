cmake_minimum_required(VERSION 3.4.1)
project(m1lkdr0p LANGUAGES C CXX)


find_package(ZLIB REQUIRED)

if (ZLIB_FOUND)
    include_directories(${ZLIB_INCLUDE_DIRS})
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)



if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE RelWithDebInfo )
endif()


set(CMAKE_VERBOSE_MAKEFILE on)

set(COMMON_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(APP_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/app)
set(EXTERNAL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/external)
set(IMGUI_SOURCE_DIR ${EXTERNAL_DIR}/imgui)
set(LZMA_SOURCE_DIR ${EXTERNAL_DIR}/lzma1801/C)


add_subdirectory(${EXTERNAL_DIR}/hlsl2glslfork)

# option(USE_AES "enables building of aes library" ON)
SET(USE_AES OFF CACHE BOOL "" FORCE)
add_subdirectory(${EXTERNAL_DIR}/minizip)


# set(CMAKE_CXX_FLAGS_DEBUG "-g -DANDROID_CONFIG_DEBUG")
# set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DANDROID_CONFIG_RELEASE")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti -fno-exceptions -Wall -Wno-reorder -Wno-deprecated-register -Wno-self-assign -Wno-c++11-narrowing ")

set(IMGUI_SOURCE
        ${IMGUI_SOURCE_DIR}/imgui.cpp
        ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
        ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
        ${IMGUI_SOURCE_DIR}/imgui_demo.cpp
        ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
        ${COMMON_SOURCE_DIR}/imgui_support.cpp
        )

set(LZMA_SOURCE
    ${LZMA_SOURCE_DIR}/Lzma2Dec.c
    ${LZMA_SOURCE_DIR}/Ppmd7Dec.c
    ${LZMA_SOURCE_DIR}/Aes.c
    ${LZMA_SOURCE_DIR}/Bcj2.c
    ${LZMA_SOURCE_DIR}/LzFind.c
    ${LZMA_SOURCE_DIR}/DllSecur.c
    ${LZMA_SOURCE_DIR}/Sort.c
    ${LZMA_SOURCE_DIR}/Ppmd7.c
    ${LZMA_SOURCE_DIR}/Bra.c
    ${LZMA_SOURCE_DIR}/Bra86.c
    ${LZMA_SOURCE_DIR}/7zArcIn.c
    ${LZMA_SOURCE_DIR}/7zAlloc.c
    ${LZMA_SOURCE_DIR}/Alloc.c
    ${LZMA_SOURCE_DIR}/LzmaDec.c
    ${LZMA_SOURCE_DIR}/Ppmd7Enc.c
    ${LZMA_SOURCE_DIR}/7zDec.c
    ${LZMA_SOURCE_DIR}/7zBuf2.c
    ${LZMA_SOURCE_DIR}/BraIA64.c
    ${LZMA_SOURCE_DIR}/Lzma86Dec.c
    ${LZMA_SOURCE_DIR}/7zStream.c
    ${LZMA_SOURCE_DIR}/Sha256.c
    ${LZMA_SOURCE_DIR}/Delta.c
    ${LZMA_SOURCE_DIR}/7zFile.c
    ${LZMA_SOURCE_DIR}/7zCrc.c
    ${LZMA_SOURCE_DIR}/AesOpt.c
    ${LZMA_SOURCE_DIR}/CpuArch.c
    ${LZMA_SOURCE_DIR}/7zCrcOpt.c
    ${LZMA_SOURCE_DIR}/7zBuf.c
    )


set(MDP_SOURCE
        ${COMMON_SOURCE_DIR}/matrix.cpp
        ${COMMON_SOURCE_DIR}/platform.cpp
        ${COMMON_SOURCE_DIR}/path.cpp
        ${COMMON_SOURCE_DIR}/ArchiveReader.cpp
        ${COMMON_SOURCE_DIR}/ImageReader.cpp
        ${COMMON_SOURCE_DIR}/ImageWriter.cpp
        ${COMMON_SOURCE_DIR}/VizController.cpp
        ${COMMON_SOURCE_DIR}/TProfiler.cpp
        ${COMMON_SOURCE_DIR}/vis_milk2/fft.cpp
        ${COMMON_SOURCE_DIR}/vis_milk2/milkdropfs.cpp
        ${COMMON_SOURCE_DIR}/vis_milk2/plugin.cpp
        ${COMMON_SOURCE_DIR}/vis_milk2/state.cpp
        ${COMMON_SOURCE_DIR}/vis_milk2/AudioAnalyzer.cpp
        ${COMMON_SOURCE_DIR}/audio/RawFileAudioSource.cpp
        ${COMMON_SOURCE_DIR}/audio/WavFileAudioSource.cpp
        ${COMMON_SOURCE_DIR}/audio/NullAudioSource.cpp
        ${COMMON_SOURCE_DIR}/render/context.cpp
        ${COMMON_SOURCE_DIR}/render/context_null.cpp
        ${COMMON_SOURCE_DIR}/render/DrawBuffer.cpp
        ${COMMON_SOURCE_DIR}/script/script-tests.cpp
        ${COMMON_SOURCE_DIR}/script/mdp-eel2/script-compiler.cpp
        ${COMMON_SOURCE_DIR}/script/mdp-eel2/script-caltab.cpp
        ${COMMON_SOURCE_DIR}/script/mdp-eel2/script-preprocessor.cpp
        ${COMMON_SOURCE_DIR}/script/mdp-eel2/script-eval.cpp
        ${COMMON_SOURCE_DIR}/script/mdp-eel2/script-lextab.cpp
        ${COMMON_SOURCE_DIR}/script/mdp-eel2/script-context.cpp
        ${COMMON_SOURCE_DIR}/script/mdp-eel2/script-functype.cpp
        ${COMMON_SOURCE_DIR}/script/mdp-eel2/script-expression.cpp
        ${COMMON_SOURCE_DIR}/script/mdp-eel2/script-yylex.cpp
        ${COMMON_SOURCE_DIR}/script/mdp-eel2/script-kernel.cpp
      )

set(LIBS
        hlsl2glsl
        minizip
         ${ZLIB_LIBRARIES}
    )

set(INCLUDE_DIRS
        ${COMMON_SOURCE_DIR}
        ${COMMON_SOURCE_DIR}/script
        ${COMMON_SOURCE_DIR}/script/mdp-eel2
    )


if (APPLE)

	include_directories(
        ${INCLUDE_DIRS}
    )



	add_executable(${CMAKE_PROJECT_NAME}
	    ${MDP_SOURCE}
	    ${IMGUI_SOURCE}
        ${LZMA_SOURCE}
        ${ZLIB_TEST_SOURCES}
	    ${COMMON_SOURCE_DIR}/render/context_metal.mm
        ${COMMON_SOURCE_DIR}/audio/ALAudioSource.cpp
        ${COMMON_SOURCE_DIR}/audio/AVAudioEngineSource.mm
        ${COMMON_SOURCE_DIR}/audio/CoreAudioSource.mm
	    ${APP_SOURCE_DIR}/OSX/console_main.mm
	    )

    target_link_libraries(
        ${CMAKE_PROJECT_NAME}
        ${LIBS}
        "-framework CoreGraphics"
        "-framework Foundation"
        "-framework AppKit"
        "-framework AudioToolbox"
        "-framework AVFAudio"        
        "-framework CoreAudio"
        "-framework Metal"
        "-framework MetalKit"
        "-framework GameController"
        "-framework OpenAL"
        "-lz"
)
    

    set_property (TARGET ${CMAKE_PROJECT_NAME} APPEND_STRING PROPERTY COMPILE_FLAGS "-fobjc-arc")



elseif (UNIX)

    set(CMAKE_CXX_FLAGS_DEBUG "-g -DANDROID_CONFIG_DEBUG")
    set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DANDROID_CONFIG_RELEASE")


	include_directories(
        ${ANDROID_NDK}/sources/android/native_app_glue
        ${INCLUDE_DIRS}
    )


	add_library(${CMAKE_PROJECT_NAME} SHARED
	            ${MDP_SOURCE}
                ${LZMA_SOURCE}
                ${ZLIB_TEST_SOURCES}
	            ${IMGUI_SOURCE}
                ${COMMON_SOURCE_DIR}/render/context_gles.cpp
                ${COMMON_SOURCE_DIR}/audio/SLESAudioSource.cpp
                ${APP_SOURCE_DIR}/android/android_main.cpp
	          )

	# Include libraries needed for lib
	target_link_libraries(${CMAKE_PROJECT_NAME}
                ${LIBS}
	            android
	            OpenSLES
	            EGL
                GLESv3
	            log
	            m)




elseif (WIN32)
    message(FATAL_ERROR "Target platform not supported")

    
else()
    message(FATAL_ERROR "Target platform not supported")
endif()



