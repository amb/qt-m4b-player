cmake_minimum_required(VERSION 3.24)
project(qt-m4b-player VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Sql Concurrent)
qt_standard_project_setup(REQUIRES 6.5)
if(Qt6_VERSION VERSION_GREATER_EQUAL 6.8)
    qt_policy(SET QTP0004 NEW)
endif()

if(ANDROID)
    message(FATAL_ERROR "Android packaging is a planned milestone; see docs/ARCHITECTURE.md. This first prototype builds on Linux.")
endif()
find_package(PkgConfig REQUIRED)
pkg_check_modules(MPV REQUIRED IMPORTED_TARGET mpv>=2.0)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat>=60)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec>=60)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil>=58)

add_library(player-core STATIC
    src/MediaData.h src/MediaData.cpp
    src/MetadataReader.h src/MetadataReader.cpp
    src/Library.h src/Library.cpp
    src/MpvEngine.h src/MpvEngine.cpp
)
target_include_directories(player-core PUBLIC src)
target_link_libraries(player-core PUBLIC Qt6::Core Qt6::Gui Qt6::Sql
    PkgConfig::MPV PkgConfig::AVFORMAT PkgConfig::AVCODEC PkgConfig::AVUTIL)
target_compile_options(player-core PRIVATE -Wall -Wextra -Wpedantic)
add_executable(media-inspect tools/InspectMedia.cpp)
target_link_libraries(media-inspect PRIVATE player-core)

qt_add_executable(qt-m4b-player src/main.cpp src/WindowPreferences.h src/WindowPreferences.cpp)
qt_add_qml_module(qt-m4b-player
    URI AudiobookPlayer
    VERSION 1.0
    # Ubuntu supplies shared QML plugins, which are loaded dynamically.
    NO_IMPORT_SCAN
    QML_FILES qml/Main.qml qml/TransportButton.qml qml/TransportIcon.qml qml/MenuCommand.qml qml/CoverArtwork.qml qml/ProgressHeader.qml qml/InfoPopup.qml
    SOURCES src/PlayerController.h src/PlayerController.cpp src/AppPreferences.h src/AppPreferences.cpp
)
file(GLOB TRANSPORT_ICON_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.svg")
qt_add_resources(qt-m4b-player transport-icons
    PREFIX "/icons" BASE "assets/icons" FILES ${TRANSPORT_ICON_FILES})
qt_add_resources(qt-m4b-player third-party-notices
    PREFIX "/licenses" BASE "assets/icons" FILES assets/icons/LICENSE.phosphor)
target_link_libraries(qt-m4b-player PRIVATE player-core Qt6::Quick Qt6::QuickControls2 Qt6::Concurrent)
target_compile_options(qt-m4b-player PRIVATE -Wall -Wextra -Wpedantic)

file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/dependency-versions.txt" CONTENT
"Project: ${PROJECT_NAME} ${PROJECT_VERSION}\nCompiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}\nQt: ${Qt6_VERSION}\nlibmpv client API (pkg-config): ${MPV_VERSION}\nlibavformat: ${AVFORMAT_VERSION}\nlibavcodec: ${AVCODEC_VERSION}\nlibavutil: ${AVUTIL_VERSION}\n")
include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    qt_add_executable(core-tests tests/CoreTests.cpp)
    target_link_libraries(core-tests PRIVATE player-core Qt6::Test)
    target_compile_definitions(core-tests PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME core COMMAND core-tests)
    qt_add_executable(player-tests tests/PlayerTests.cpp src/PlayerController.h src/PlayerController.cpp)
    add_dependencies(player-tests qt-m4b-player)
    set_target_properties(player-tests PROPERTIES QT_QML_MODULE_NO_IMPORT_SCAN TRUE)
    target_link_libraries(player-tests PRIVATE player-core Qt6::Qml Qt6::Concurrent Qt6::Test)
    target_compile_definitions(player-tests PRIVATE FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    add_test(NAME player COMMAND player-tests)
    qt_add_executable(window-tests tests/WindowTests.cpp src/WindowPreferences.h src/WindowPreferences.cpp)
    add_dependencies(window-tests qt-m4b-player)
    target_include_directories(window-tests PRIVATE src)
    target_link_libraries(window-tests PRIVATE Qt6::Gui Qt6::Test)
    add_test(NAME window COMMAND window-tests)
    set_tests_properties(core player window PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
install(TARGETS qt-m4b-player RUNTIME DESTINATION bin)
install(FILES packaging/io.github.amb.QtM4bPlayer.desktop DESTINATION share/applications)
install(FILES packaging/flatpak/io.github.amb.QtM4bPlayer.metainfo.xml DESTINATION share/metainfo)
install(FILES packaging/icons/io.github.amb.QtM4bPlayer.svg
        DESTINATION share/icons/hicolor/scalable/apps)
foreach(icon_size 16 32 48 64 128 256 512)
    install(FILES packaging/icons/${icon_size}x${icon_size}/io.github.amb.QtM4bPlayer.png
            DESTINATION share/icons/hicolor/${icon_size}x${icon_size}/apps)
endforeach()

install(FILES LICENSE assets/icons/LICENSE.phosphor DESTINATION share/licenses/qt-m4b-player)
