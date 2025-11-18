cmake_minimum_required(VERSION 3.16)
project(CamDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt5 COMPONENTS Widgets REQUIRED)
find_package(VTK REQUIRED)

include(${VTK_USE_FILE})
include_directories(${OpenCAMLib_INCLUDE_DIR} ${BOOST_LIBRARY_DIR})
link_directories(${OpenCAMLib_LIBRARY_DIR})

add_executable(CamDemo
    main.cpp
    MainWindow.cpp
    MainWindow.h
)

target_link_libraries(CamDemo
    Qt5::Widgets
    ${VTK_LIBRARIES}
    ocl
)

add_custom_command(TARGET CamDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${VTK_DLL_DIR}"
    $<TARGET_FILE_DIR:CamDemo>
)

add_custom_command(TARGET CamDemo POST_BUILD
    COMMAND "${QT_BIN_DIR}/windeployqt.exe"
            --$<IF:$<CONFIG:Debug>,debug,release>
            --compiler-runtime
            "$<TARGET_FILE:CamDemo>"
    COMMENT "Running windeployqt to deploy Qt runtime and plugins"
    VERBATIM
)

add_custom_command(TARGET CamDemo POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${OpenCAMLib_BIN_DIR}"
            $<TARGET_FILE_DIR:CamDemo>
    COMMENT "Copying OpenCAMLib DLLs..."
)