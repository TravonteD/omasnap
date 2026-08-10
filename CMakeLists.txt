cmake_minimum_required(VERSION 3.24)
project(omarchy-capture-editor LANGUAGES CXX)

find_package(Qt6 6.8 REQUIRED COMPONENTS Concurrent Core Gui Test Widgets)

qt_add_executable(omarchy-capture-editor
  main.cpp
  capture.cpp
  capture.hpp
  editor.cpp
  editor.hpp
)

target_compile_features(omarchy-capture-editor PRIVATE cxx_std_23)
target_compile_options(omarchy-capture-editor PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(omarchy-capture-editor PRIVATE
  Qt6::Concurrent
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
)

qt_add_executable(omarchy-capture-editor-smoke
  editor-smoke.cpp
  capture.cpp
  capture.hpp
  editor.cpp
  editor.hpp
)
target_compile_features(omarchy-capture-editor-smoke PRIVATE cxx_std_23)
target_compile_options(omarchy-capture-editor-smoke PRIVATE -Wall -Wextra -Wpedantic)
target_link_libraries(omarchy-capture-editor-smoke PRIVATE
  Qt6::Concurrent
  Qt6::Core
  Qt6::Gui
  Qt6::Test
  Qt6::Widgets
)

install(TARGETS omarchy-capture-editor RUNTIME DESTINATION bin)
