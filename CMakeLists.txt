# Windows(MinGW) 와 Linux/WSL 양쪽에서 빌드된다. 차이는 FFmpeg 를 찾는 방식뿐이다.

cmake_minimum_required(VERSION 3.14)

project(smart_parking_qt_client VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Quick QuickWidgets Qml Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Quick QuickWidgets Qml Network)

# Qt Mqtt 는 기본 설치에 포함되지 않는 add-on 이다. 팀원 PC 에서 아직 설치하지
# 않았더라도 빌드가 깨지지 않도록 선택적으로 찾는다. 모듈이 없으면 실시간
# 알림만 꺼진 채로 빌드된다 (MaintenanceTool > Additional Libraries > Qt Mqtt).
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Mqtt)
if (Qt${QT_VERSION_MAJOR}Mqtt_FOUND)
    message(STATUS "Qt Mqtt found: real-time MQTT alerts enabled")
else()
    message(WARNING
        "Qt Mqtt module not found. Building without real-time MQTT alerts. "
        "Install it with the Qt Maintenance Tool (Additional Libraries > Qt Mqtt).")
endif()

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

# FFmpeg 는 두 가지 방식으로 들어온다.
#   1) Windows: 팀 공용 shared build 를 third_party/ffmpeg 에 풀어둔 self-contained 트리
#   2) Linux: 배포판 개발 패키지. 멀티아치 경로라 include/ + lib/ 짝이 안 맞으므로 pkg-config 로 찾는다
add_library(smart_parking_ffmpeg INTERFACE)

if(FFMPEG_AVFORMAT_LIBRARY AND FFMPEG_AVCODEC_LIBRARY
        AND FFMPEG_AVUTIL_LIBRARY AND FFMPEG_SWSCALE_LIBRARY)
    message(STATUS "FFmpeg found in FFMPEG_ROOT=${FFMPEG_ROOT}")
    target_include_directories(smart_parking_ffmpeg INTERFACE "${FFMPEG_INCLUDE_DIR}")
    target_link_libraries(smart_parking_ffmpeg
        INTERFACE
            "${FFMPEG_AVFORMAT_LIBRARY}"
            "${FFMPEG_AVCODEC_LIBRARY}"
            "${FFMPEG_AVUTIL_LIBRARY}"
            "${FFMPEG_SWSCALE_LIBRARY}")
else()
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(FFMPEG_PC IMPORTED_TARGET
            libavformat libavcodec libavutil libswscale)
    endif()

    if(FFMPEG_PC_FOUND)
        message(STATUS "FFmpeg found via pkg-config (libavformat ${FFMPEG_PC_libavformat_VERSION})")
        target_link_libraries(smart_parking_ffmpeg INTERFACE PkgConfig::FFMPEG_PC)
    else()
        message(FATAL_ERROR
            "FFmpeg not found.\n"
            "  Windows: extract the shared build so that ${FFMPEG_ROOT}/include and "
            "${FFMPEG_ROOT}/lib exist, or pass -DFFMPEG_ROOT=<path>.\n"
            "  Linux:   sudo apt install libavformat-dev libavcodec-dev "
            "libavutil-dev libswscale-dev pkg-config")
    endif()
endif()

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
    src/services/mqttserviceclient.cpp
    src/services/mqttserviceclient.h
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
        smart_parking_ffmpeg
)

target_include_directories(smart_parking_qt_client
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/rtsp_legacy"
)

target_compile_definitions(smart_parking_qt_client
    PRIVATE SMART_PARKING_CONFIG_DIR="${CMAKE_CURRENT_SOURCE_DIR}/config"
)

if (Qt${QT_VERSION_MAJOR}Mqtt_FOUND)
    target_link_libraries(smart_parking_qt_client
        PRIVATE Qt${QT_VERSION_MAJOR}::Mqtt)
    target_compile_definitions(smart_parking_qt_client
        PRIVATE SMART_PARKING_HAS_QT_MQTT)
endif()

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
        src/services/mqttserviceclient.cpp
        src/services/mqttserviceclient.h
    )
    target_include_directories(parkingcontroller_retry_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
    )
    if (Qt${QT_VERSION_MAJOR}Mqtt_FOUND)
        target_link_libraries(parkingcontroller_retry_test PRIVATE
            Qt${QT_VERSION_MAJOR}::Mqtt)
        target_compile_definitions(parkingcontroller_retry_test PRIVATE
            SMART_PARKING_HAS_QT_MQTT)
    endif()
    target_link_libraries(parkingcontroller_retry_test PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Network
    )
    add_test(NAME parkingcontroller_retry COMMAND parkingcontroller_retry_test)
endif()