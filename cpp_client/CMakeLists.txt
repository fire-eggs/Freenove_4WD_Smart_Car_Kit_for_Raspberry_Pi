cmake_minimum_required(VERSION 3.0)

project(vidtest2)

include_directories(${PROJECT_SOURCE_DIR} /home/pi/fltk/fltk-1.4.x-20220204-e73f9d46 .)

set(LINK_FLAGS "-pthread")

add_executable(vidtest2 main.cpp vidThread.cpp recvThread.cpp powerThread.cpp)

target_link_libraries(vidtest2 fltk fltk_images jpeg m X11 Xext pthread Xinerama
                      Xfixes Xcursor Xft Xrender m fontconfig dl ${LINK_FLAGS})

#install(TARGETS test RUNTIME DESTINATION bin)

