cmake_minimum_required(VERSION 3.16)
project(pixpin_linux VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Svg)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TESS REQUIRED tesseract lept)

include_directories(
    src
    ${TESS_INCLUDE_DIRS}
)

set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/SystemTray.cpp
    src/capture/ScreenCapturer.cpp
    src/capture/CaptureOverlay.cpp
    src/capture/MagnifierWidget.cpp
    src/ui/ToolBarWidget.cpp
    src/ui/AnnotationLayer.cpp
    src/ui/PinWindow.cpp
    src/ocr/OcrEngine.cpp
    src/ocr/OcrResultDialog.cpp
    src/utils/ImageUtils.cpp
)

set(HEADERS
    src/app/Application.h
    src/app/SystemTray.h
    src/capture/ScreenCapturer.h
    src/capture/CaptureOverlay.h
    src/capture/MagnifierWidget.h
    src/ui/ToolBarWidget.h
    src/ui/AnnotationLayer.h
    src/ui/PinWindow.h
    src/ocr/OcrEngine.h
    src/ocr/OcrResultDialog.h
    src/utils/ImageUtils.h
)

add_executable(pixpin_linux ${SOURCES} ${HEADERS})

target_link_libraries(pixpin_linux PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Qt6::Svg
    ${TESS_LIBRARIES}
)

target_compile_definitions(pixpin_linux PRIVATE
    PIXPIN_VERSION="${PROJECT_VERSION}"
)
