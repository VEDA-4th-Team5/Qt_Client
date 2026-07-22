cmake_minimum_required(VERSION 3.14)

project(smart_parking_qt_client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Quick QuickWidgets Qml Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Quick QuickWidgets Qml Network)

set(DEFAULT_FFMPEG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ffmpeg")

if(DEFINED ENV{FFMPEG_ROOT})
    set(DEFAULT_FFMPEG_ROOT "$ENV{FFMPEG_ROOT}")
endif()

set(FFMPEG_ROOT "${DEFAULT_FFMPEG_ROOT}" CACHE PATH "FFmpeg shared build root")
set(FFMPEG_INCLUDE_DIR "${FFMPEG_ROOT}/include")
set(FFMPEG_LIBRARY_DIR "${FFMPEG_ROOT}/lib")
set(FFMPEG_BINARY_DIR "${FFMPEG_ROOT}/bin")

find_library(FFMPEG_AVFORMAT_LIBRARY NAMES avformat libavformat PATHS "${FFMPEG_LIBRARY_DIR}" NO_DEFAULT_PATH)
find_library(FFMPEG_AVCODEC_LIBRARY NAMES avcodec libavcodec PATHS "${FFMPEG_LIBRARY_DIR}" NO_DEFAULT_PATH)
find_library(FFMPEG_AVUTIL_LIBRARY NAMES avutil libavutil PATHS "${FFMPEG_LIBRARY_DIR}" NO_DEFAULT_PATH)
find_library(FFMPEG_SWSCALE_LIBRARY NAMES swscale libswscale PATHS "${FFMPEG_LIBRARY_DIR}" NO_DEFAULT_PATH)

foreach(required_library
        FFMPEG_AVFORMAT_LIBRARY
        FFMPEG_AVCODEC_LIBRARY
        FFMPEG_AVUTIL_LIBRARY
        FFMPEG_SWSCALE_LIBRARY)
    if(NOT ${required_library})
        message(FATAL_ERROR "Missing FFmpeg library: ${required_library}. Check FFMPEG_ROOT=${FFMPEG_ROOT}")
    endif()
endforeach()

add_executable(smart_parking_qt_client
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/controllers/parkingcontroller.cpp
    src/controllers/parkingcontroller.h
    src/dialogs/slotevidencedialog.cpp
    src/dialogs/slotevidencedialog.h
    src/models/parkingstate.cpp
    src/models/parkingstate.h
    src/models/parkingzonelayout.cpp
    src/models/parkingzonelayout.h
    src/pages/dashboardpage.cpp
    src/pages/dashboardpage.h
    src/pages/debugpage.cpp
    src/pages/debugpage.h
    src/pages/eventspage.cpp
    src/pages/eventspage.h
    src/pages/parkingmappage.cpp
    src/pages/parkingmappage.h
    src/pages/settingspage.cpp
    src/pages/settingspage.h
    src/services/camerasettings.cpp
    src/services/camerasettings.h
    src/services/notificationcenter.cpp
    src/services/notificationcenter.h
    src/simulation/parkingmockdata.cpp
    src/simulation/parkingmockdata.h
    src/simulation/parkingsimulationservice.cpp
    src/simulation/parkingsimulationservice.h
    src/api/apiclient.cpp
    src/api/apiclient.h
    src/api/imageloader.cpp
    src/api/imageloader.h
    src/api/parkingmodels.h
    src/api/parkingresponseparser.cpp
    src/api/parkingresponseparser.h
    rtsp_legacy/RtspVideoItem.cpp
    rtsp_legacy/RtspVideoItem.h
)

target_link_libraries(smart_parking_qt_client
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::QuickWidgets
        Qt${QT_VERSION_MAJOR}::Qml
        Qt${QT_VERSION_MAJOR}::Network
        "${FFMPEG_AVFORMAT_LIBRARY}"
        "${FFMPEG_AVCODEC_LIBRARY}"
        "${FFMPEG_AVUTIL_LIBRARY}"
        "${FFMPEG_SWSCALE_LIBRARY}"
)

target_include_directories(smart_parking_qt_client
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${FFMPEG_INCLUDE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/rtsp_legacy"
)

target_compile_definitions(smart_parking_qt_client
    PRIVATE SMART_PARKING_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/config"
)

if (QT_VERSION_MAJOR EQUAL 6)
    qt_add_resources(smart_parking_qt_client rtsp_qml
        PREFIX "/"
        FILES
            qml/RtspChannel.qml
    )
else()
    qt5_add_resources(RTSP_QML_RESOURCES qml/rtsp_qml.qrc)
    target_sources(smart_parking_qt_client PRIVATE ${RTSP_QML_RESOURCES})
endif()

if (EXISTS "${FFMPEG_BINARY_DIR}")
    add_custom_command(TARGET smart_parking_qt_client POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FFMPEG_BINARY_DIR}"
            "$<TARGET_FILE_DIR:smart_parking_qt_client>"
    )
endif()

set_target_properties(smart_parking_qt_client PROPERTIES
    WIN32_EXECUTABLE TRUE
)

include(CTest)
if (BUILD_TESTING)
    add_executable(parkingcontroller_retry_test
        tests/parkingcontroller_retry_test.cpp
        src/controllers/parkingcontroller.cpp
        src/controllers/parkingcontroller.h
        src/models/parkingstate.cpp
        src/models/parkingstate.h
        src/api/apiclient.cpp
        src/api/apiclient.h
        src/api/imageloader.cpp
        src/api/imageloader.h
        src/api/parkingmodels.h
        src/api/parkingresponseparser.cpp
        src/api/parkingresponseparser.h
    )
    target_include_directories(parkingcontroller_retry_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(parkingcontroller_retry_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
    )
    add_test(NAME parkingcontroller_retry COMMAND parkingcontroller_retry_test)

    add_executable(parkingsimulationservice_test
        tests/parkingsimulationservice_test.cpp
        src/controllers/parkingcontroller.cpp
        src/controllers/parkingcontroller.h
        src/models/parkingstate.cpp
        src/models/parkingstate.h
        src/simulation/parkingmockdata.cpp
        src/simulation/parkingmockdata.h
        src/simulation/parkingsimulationservice.cpp
        src/simulation/parkingsimulationservice.h
        src/api/apiclient.cpp
        src/api/apiclient.h
        src/api/imageloader.cpp
        src/api/imageloader.h
        src/api/parkingmodels.h
        src/api/parkingresponseparser.cpp
        src/api/parkingresponseparser.h
    )
    target_include_directories(parkingsimulationservice_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(parkingsimulationservice_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
    )
    set_target_properties(parkingsimulationservice_test PROPERTIES
        AUTOMOC_COMPILER_PREDEFINES OFF
    )
    add_test(NAME parking_simulation_service COMMAND parkingsimulationservice_test)

    add_executable(parkingmappage_add_slot_test
        tests/parkingmappage_add_slot_test.cpp
        src/pages/parkingmappage.cpp
        src/pages/parkingmappage.h
        src/models/parkingstate.cpp
        src/models/parkingstate.h
        src/models/parkingzonelayout.cpp
        src/models/parkingzonelayout.h
    )
    target_include_directories(parkingmappage_add_slot_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    target_link_libraries(parkingmappage_add_slot_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
    )
    set_target_properties(parkingmappage_add_slot_test PROPERTIES
        AUTOMOC_COMPILER_PREDEFINES OFF
    )
    add_test(NAME parkingmappage_add_slot COMMAND parkingmappage_add_slot_test)
    set(QT_GUI_DIR_VARIABLE "Qt${QT_VERSION_MAJOR}Gui_DIR")
    get_filename_component(QT_RUNTIME_ROOT
        "${${QT_GUI_DIR_VARIABLE}}/../../.." ABSOLUTE)
    set_tests_properties(parkingmappage_add_slot PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;QT_QPA_PLATFORM_PLUGIN_PATH=${QT_RUNTIME_ROOT}/plugins/platforms"
        TIMEOUT 30
    )
endif()
