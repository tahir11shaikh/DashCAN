cmake_minimum_required(VERSION 3.16)

project(DashCAN VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# Find and set-up packages
# ----------------------------------------------------------------------------
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS
  Widgets
  LinguistTools
  Concurrent
  Charts
  QuickWidgets
  Quick
  Quick3D
)

set(TS_FILES DashCAN_en_IO.ts)

# ----------------------------------------------------------------------------
# Set all sources
# ----------------------------------------------------------------------------
set(PROJECT_SOURCES

    main.cpp
    mainWindow.cpp
    mainWindow.h
    mainWindow.ui
    resources.qrc
    ${TS_FILES}
)

# ----------------------------------------------------------------------------
# Include executable/header
# ----------------------------------------------------------------------------
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(DashCAN
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
        mainApp.cpp
        loginDialog.h loginDialog.cpp loginDialog.ui
        app/CanDbc.h 
        app/CanApp.h app/CanApp.cpp
        deviceDialog.h deviceDialog.cpp deviceDialog.ui
    )
    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(DashCAN SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(DashCAN
            ${PROJECT_SOURCES}
        )
    endif()
    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

# ----------------------------------------------------------------------------
# Include library folder
# ----------------------------------------------------------------------------
target_include_directories(DashCAN
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/PCAN
        ${CMAKE_SOURCE_DIR}/app
)

target_link_directories(DashCAN
    PRIVATE
        ${CMAKE_SOURCE_DIR}/thirdparty/PCAN
        ${CMAKE_SOURCE_DIR}/app
)

target_link_libraries(DashCAN
    PRIVATE
        Qt${QT_VERSION_MAJOR}::Widgets
        Qt${QT_VERSION_MAJOR}::Concurrent
        Qt${QT_VERSION_MAJOR}::Charts
        Qt${QT_VERSION_MAJOR}::QuickWidgets
        Qt${QT_VERSION_MAJOR}::Quick
        Qt${QT_VERSION_MAJOR}::Quick3D
)

# ----------------------------------------------------------------------------
# Link library
# ----------------------------------------------------------------------------
target_link_libraries(DashCAN
    PRIVATE
        PCANBasic
        CanDBC
)

# ----------------------------------------------------------------------------
# Bundle/Install settings
# ----------------------------------------------------------------------------
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.DashCAN)
endif()

set_target_properties(DashCAN PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS DashCAN
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(DashCAN)
endif()
