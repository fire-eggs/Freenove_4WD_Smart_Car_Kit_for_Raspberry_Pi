cmake_minimum_required(VERSION 3.0)

project(vidtest2)

include_directories(${PROJECT_SOURCE_DIR} /home/pi/fltk/fltk-1.4.x-20220204-e73f9d46 .)

set(LINK_FLAGS "-pthread")

add_executable(test main.cpp vidThread.cpp)

target_link_libraries(test fltk m X11 Xext pthread Xinerama
                      Xfixes Xcursor Xft Xrender m fontconfig dl ${LINK_FLAGS})

#install(TARGETS test RUNTIME DESTINATION bin)

