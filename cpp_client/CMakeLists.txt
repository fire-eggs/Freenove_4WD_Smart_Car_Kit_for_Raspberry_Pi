cmake_minimum_required(VERSION 3.0)

project(test LANGUAGES C CXX)

include_directories(${PROJECT_SOURCE_DIR} /home/kevin/fltk .)

find_library(FLTK fltk /home/kevin/fltk/lib)

add_executable(test main.cpp)

target_link_libraries(test ${FLTK} m X11 Xext pthread Xinerama
                      Xfixes Xcursor Xft Xrender m fontconfig dl)

#install(TARGETS test RUNTIME DESTINATION bin)

