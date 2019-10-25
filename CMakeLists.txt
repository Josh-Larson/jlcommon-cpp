cmake_minimum_required(VERSION 3.7)

project(jlcommon LANGUAGES CXX VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)

file(GLOB HEADERS include/*.h)
file(GLOB SOURCES src/*.cpp)
add_library(jlcommon SHARED ${SOURCES} ../services/DefenseDaemonManager.cpp ../services/DefenseDaemonManager.h ../services/DefenseDaemonRadarService.cpp ../services/DefenseDaemonRadarService.h)
set_target_properties(jlcommon PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties(jlcommon PROPERTIES SOVERSION 1)
target_include_directories(jlcommon PRIVATE include)
target_include_directories(jlcommon PRIVATE src)
set(CMAKE_BUILD_TYPE Debug)

if(MSVC)
	target_compile_options(jlcommon PRIVATE /W4 /WX)
else()
	target_compile_options(jlcommon PRIVATE -Wall -Wextra -pedantic -Werror)
endif()

include(GNUInstallDirs)
install(TARGETS jlcommon
		LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/jlcommon)
configure_file(jlcommon.pc.in jlcommon.pc @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/jlcommon.pc DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig)

# Google Test
set(CMAKE_BINARY_DIR bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR})
include_directories(include)
include_directories(src)

add_executable(tests test/tests.cpp)
target_link_libraries(tests gtest pthread m jlcommon)

