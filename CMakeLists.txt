# This CMake Template is based on the pamplejuce project: https://github.com/sudara/pamplejuce/
# JUCE requires a CMake Version of 3.15, 3.20 or higher is recommended
cmake_minimum_required(VERSION 3.15)

# Function to add a git submodule through CMake, 
# seen at https://www.scivision.dev/cmake-git-submodule/ and
# https://cliutils.gitlab.io/modern-cmake/chapters/projects/submodule.html
function(add_git_submodule dir)
# add a Git submodule directory to CMake, assuming the
# Git submodule directory is a CMake project.
#
# Usage: in CMakeLists.txt
# 
# include(AddGitSubmodule.cmake)
# add_git_submodule(mysubmod_dir)
find_package(Git REQUIRED)
if(NOT EXISTS ${dir}/CMakeLists.txt)
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive -- ${dir}
                    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
                    COMMAND_ERROR_IS_FATAL ANY)

endif()
add_subdirectory(${dir})

endfunction(add_git_submodule)

# Name of the plugin
# This cannot have spaces (but PRODUCT_NAME can)
set(PROJECT_NAME "CQTAnalyzer")

# Set the plugin formats you'll be building here.
# Valid formats: AAX Unity VST AU AUv3 Standalone
set(FORMATS VST3 Standalone)

# For simplicity, the name of the project is also the name of the target
project(${PROJECT_NAME} 
        VERSION 0.4.5
        LANGUAGES CXX C
        HOMEPAGE_URL "https://iem.kug.ac.at/")

# By default we don't want Xcode schemes to be made for modules, etc
set(CMAKE_XCODE_GENERATE_SCHEME OFF)

# Enable to build universal binaries on macOS, increasing build time
# This only affects local builds, GitHub actions always builds Universals
# set(CMAKE_OSX_ARCHITECTURES arm64 x86_64)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Support macOS down to High Sierra")

# Adds all the module sources so they appear correctly in the IDE
# Must be set before JUCE is added as a sub-dir (or any targets are made)
# https://github.com/juce-framework/JUCE/commit/6b1b4cf7f6b1008db44411f2c8887d71a3348889
set_property(GLOBAL PROPERTY USE_FOLDERS YES)

# Create a /Modules directory in the IDE with the JUCE Module code
option(JUCE_ENABLE_MODULE_SOURCE_GROUPS "Show all module sources in IDE projects" ON)

# Add git submodule of JUCE
add_git_submodule(JUCE)

# Add directory of JUCE
add_subdirectory(JUCE)

# Copy VST-Plugins into default directory
set_directory_properties (PROPERTIES JUCE_COPY_PLUGIN_AFTER_BUILD ON)

# set copy paths for built plug-ins
# TODO: Add paths for other plugin formats
if (CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_directory_properties (PROPERTIES JUCE_VST_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST/IEM")
    set_directory_properties (PROPERTIES JUCE_VST3_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/VST3/IEM")

elseif (CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set_directory_properties (PROPERTIES JUCE_VST_COPY_DIR "$ENV{ProgramW6432}/Steinberg/Vstplugins/IEM")
        set (prefix "$ENV{CommonProgramW6432}")
    else()
        set_directory_properties (PROPERTIES JUCE_VST_COPY_DIR "$ENV{programfiles\(x86\)}/Steinberg/Vstplugins/IEM")
        set (prefix "$ENV{CommonProgramFiles\(x86\)}")
    endif()
    set_directory_properties (PROPERTIES JUCE_VST3_COPY_DIR "${prefix}/VST3/IEM")

elseif ((CMAKE_SYSTEM_NAME STREQUAL "Linux") OR (CMAKE_SYSTEM_NAME MATCHES ".*BSD"))
    set_directory_properties (PROPERTIES JUCE_VST_COPY_DIR "$ENV{HOME}/.vst/IEM")
    set_directory_properties (PROPERTIES JUCE_VST3_COPY_DIR "$ENV{HOME}/.vst3/IEM")
endif()

# Check the readme at `docs/CMake API.md` in the JUCE repo for full config
juce_add_plugin("${PROJECT_NAME}"
    ICON_BIG resources/Standalone/IEMPluginSuiteSmall.png
    ICON_SMALL resources/Standalone/IEMPluginSuiteSmall.png
    COMPANY_NAME "IEM"
    PLUGIN_MANUFACTURER_CODE Iema               # This has to be one uppercase, rest lower for AU formats
    PLUGIN_CODE CQTA                            # A unique four-character plugin id with at least one upper-case character
    FORMATS "${FORMATS}"
    PRODUCT_NAME "${PROJECT_NAME}")        # The name of the final executable, which can differ from the target name

include_directories (resources/ /usr/local/include)
add_compile_definitions (DONT_SET_USING_JUCE_NAMESPACE=1
                         JUCE_MODAL_LOOPS_PERMITTED=1)

juce_add_binary_data (LAF_fonts SOURCES
    resources/lookAndFeel/Roboto-Bold.ttf
    resources/lookAndFeel/Roboto-Light.ttf
    resources/lookAndFeel/Roboto-Medium.ttf
    resources/lookAndFeel/Roboto-Regular.ttf)

# Manually list all .h and .cpp files for the plugin
set(SourceFiles
    Source/PluginEditor.h
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginProcessor.cpp

    Source/CQTVisualizer.h
    Source/CQTVisualizer.cpp
    Source/CQTThread.h
    Source/CQTThread.cpp

    Source/Utilities/BufferQueue.h
    Source/Utilities/OverlappingSampleCollector.h
    Source/Utilities/Parula.h
    
    resources/OSC/OSCInputStream.h
    resources/OSC/OSCParameterInterface.cpp
    resources/OSC/OSCParameterInterface.h
    resources/OSC/OSCStatus.cpp
    resources/OSC/OSCStatus.h
    resources/OSC/OSCUtilities.h)
target_sources("${PROJECT_NAME}" PRIVATE ${SourceFiles})

# No, we don't want our source buried in extra nested folders
set_target_properties("${PROJECT_NAME}" PROPERTIES FOLDER "")

# The Xcode source tree should uhhh, still look like the source tree, yo
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/Source PREFIX "" FILES ${SourceFiles})

# Required for Linux happiness:
# See https://forum.juce.com/t/loading-pytorch-model-using-binarydata/39997/2
set_target_properties(Assets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

# This cleans up the folder organization on Xcode.
# It tucks the Plugin varieties into a "Targets" folder and generate an Xcode Scheme manually
# Xcode scheme generation is turned off globally to limit noise from other targets
# The non-hacky way of doing this is via the global PREDEFINED_TARGETS_FOLDER property
# However that doesn't seem to be working in Xcode
# Not all plugin types (au, vst) available on each build type (win, macos, linux)
foreach(target ${FORMATS} "All")
    if(TARGET ${PROJECT_NAME}_${target})
        set_target_properties(${PROJECT_NAME}_${target} PROPERTIES
            # Tuck the actual plugin targets into a folder where they won't bother us
            FOLDER "Targets"

            # MacOS only: Sets the default executable that Xcode will open on build
            # For this exact path to to work, manually build the AudioPluginHost.xcodeproj in the JUCE subdir
            XCODE_SCHEME_EXECUTABLE "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/extras/AudioPluginHost/Builds/MacOSX/build/Debug/AudioPluginHost.app"

            # Let us build the target in Xcode
            XCODE_GENERATE_SCHEME ON)
    endif()
endforeach()
set_target_properties(Assets PROPERTIES FOLDER "Targets")

# We'll need to link to these from our plugin as well as our tests
set(JUCE_DEPENDENCIES
    juce::juce_audio_utils)


target_compile_definitions("${PROJECT_NAME}" PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_include_directories("${PROJECT_NAME}" PRIVATE Source)

target_link_libraries("${PROJECT_NAME}" PRIVATE
    LAF_fonts
    ${JUCE_DEPENDENCIES}
    juce::juce_audio_plugin_client
    juce::juce_osc
    juce::juce_dsp
    juce::juce_opengl
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Color our warnings and errors
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
   add_compile_options (-fdiagnostics-color=always)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
   add_compile_options (-fcolor-diagnostics)
endif ()
    