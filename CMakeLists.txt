project(echo)

cmake_minimum_required(VERSION 3.5.1)

set(CMAKE_BUILD_TYPE Debug)

set(SRC src)
set(SERVER_SOURCE ${SRC}/doepoll.c ${SRC}/server.c ${SRC}/bipbuffer.c)
set(TESTER_SOURCE ${SRC}/doepoll.c ${SRC}/tester.c ${SRC}/bipbuffer.c)

include_directories(src)

add_executable(server ${SERVER_SOURCE})
add_executable(tester ${TESTER_SOURCE})
