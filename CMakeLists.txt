cmake_minimum_required(VERSION 3.24)
project(omarchy-capture-editor LANGUAGES C CXX)

find_package(PkgConfig REQUIRED)
find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Core Gui Test Widgets)
find_package(LayerShellQt REQUIRED)
pkg_check_modules(WaylandClient REQUIRED IMPORTED_TARGET wayland-client)
find_program(WAYLAND_SCANNER wayland-scanner REQUIRED)

set(PROTOCOL_XMLS
  /usr/share/wayland-protocols/staging/ext-foreign-toplevel-list/ext-foreign-toplevel-list-v1.xml
  /usr/share/wayland-protocols/staging/ext-image-capture-source/ext-image-capture-source-v1.xml
  /usr/share/wayland-protocols/staging/ext-image-copy-capture/ext-image-copy-capture-v1.xml
)
set(PROTOCOL_SOURCES)
foreach(PROTOCOL_XML IN LISTS PROTOCOL_XMLS)
  get_filename_component(PROTOCOL_NAME "${PROTOCOL_XML}" NAME_WE)
  set(PROTOCOL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/${PROTOCOL_NAME}-client-protocol.h")
  set(PROTOCOL_CODE "${CMAKE_CURRENT_BINARY_DIR}/${PROTOCOL_NAME}-protocol.c")
  add_custom_command(
    OUTPUT "${PROTOCOL_HEADER}" "${PROTOCOL_CODE}"
    COMMAND "${WAYLAND_SCANNER}" client-header "${PROTOCOL_XML}" "${PROTOCOL_HEADER}"
    COMMAND "${WAYLAND_SCANNER}" private-code "${PROTOCOL_XML}" "${PROTOCOL_CODE}"
    DEPENDS "${PROTOCOL_XML}"
    VERBATIM
  )
  list(APPEND PROTOCOL_SOURCES "${PROTOCOL_HEADER}" "${PROTOCOL_CODE}")
endforeach()

qt_add_executable(omarchy-capture-editor
  main.cpp
  capture.cpp
  capture.hpp
  surface-capture.cpp
  ${PROTOCOL_SOURCES}
  editor.cpp
  editor.hpp
)
qt_add_resources(omarchy-capture-editor "capture-fonts"
  PREFIX "/fonts"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/Neucha.ttf"
)

target_include_directories(omarchy-capture-editor PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(omarchy-capture-editor PRIVATE cxx_std_23)
target_compile_options(omarchy-capture-editor PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(omarchy-capture-editor PRIVATE
  Qt6::Concurrent
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  LayerShellQt::Interface
  PkgConfig::WaylandClient
)

qt_add_executable(omarchy-capture-editor-smoke
  editor-smoke.cpp
  capture.cpp
  capture.hpp
  surface-capture.cpp
  ${PROTOCOL_SOURCES}
  editor.cpp
  editor.hpp
)
qt_add_resources(omarchy-capture-editor-smoke "capture-fonts-smoke"
  PREFIX "/fonts"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/assets"
  FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/Neucha.ttf"
)
target_include_directories(omarchy-capture-editor-smoke PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(omarchy-capture-editor-smoke PRIVATE cxx_std_23)
target_compile_options(omarchy-capture-editor-smoke PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(omarchy-capture-editor-smoke PRIVATE
  Qt6::Concurrent
  Qt6::Core
  Qt6::Gui
  Qt6::Test
  Qt6::Widgets
  PkgConfig::WaylandClient
)

install(TARGETS omarchy-capture-editor RUNTIME DESTINATION bin)
install(FILES assets/OFL.txt
  DESTINATION share/licenses/omarchy-capture-editor
  RENAME Neucha-OFL.txt
)
