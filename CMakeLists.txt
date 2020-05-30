cmake_minimum_required (VERSION 3.6)

project (Ducker LANGUAGES CXX)

find_package(Qt5 COMPONENTS Gui Widgets REQUIRED)
link_libraries(Qt5::Gui Qt5::Widgets)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

include_directories(
    "src"
)

set (PLUGIN_SRC
    "src/config_ducking_client.h"
    "src/config_ducking_client.cpp"
    "src/config_ducking_tabs.h"
    "src/config_ducking_tabs.cpp"
    "src/mod_ducker_channel.h"
    "src/mod_ducker_channel.cpp"
    "src/mod_ducker_global.h"
    "src/mod_ducker_global.cpp"
    "src/plugin.h"
    "src/plugin.cpp"
    "src/plugin_qt.h"
    "src/plugin_qt.cpp"
    "src/settings_duck.h"
    "src/settings_duck.cpp"

    "ducker.qrc"
)

# Create named folders for the sources within the .vcproj
# Empty name lists them directly under the .vcproj
source_group("plugin" FILES ${PLUGIN_SRC})

# Turn on the ability to create folders to organize projects (.vcproj)
# It creates "CMakePredefinedTargets" folder by default and adds CMake
# defined projects like INSTALL.vcproj and ZERO_CHECK.vcproj
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Set compiler flags and options. 
# Here it is setting the Visual Studio warning level to 4
# set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")

# Command to output information to the console
# Useful for displaying errors, warnings, and debugging
# message ("cxx Flags:" ${CMAKE_CXX_FLAGS})

# Sub-directories where more CMakeLists.txt exist
#add_subdirectory(app)
#add_subdirectory (math)
set(WITH_VOLUME true)
set(WITH_VOLUME_WIDGETS true)
#add_subdirectory(../teamspeak-plugin-qt-common common_out)
include(deps/teamspeak-plugin-qt-common/CMakeLists.txt)

add_library(Ducker SHARED
    ${PLUGIN_SRC} ${TS_QT_CORE} ${TS_QT_VOLUME})
target_compile_features(Ducker PRIVATE cxx_std_17)
