cmake_minimum_required(VERSION  3.20)
set(CMAKE_GENERATOR_PLATFORM x64)
project(cocogoat-control)
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/MfcClass")
include_directories("${PROJECT_SOURCE_DIR}/lib/MfcClass")
include_directories("${PROJECT_SOURCE_DIR}/lib")
add_executable(cocogoat-control cocogoat-control.manifest src/main.cpp src/server.cpp src/idleTimer.cpp src/tokenVerify.cpp src/ws.cpp src/tray.cpp src/knownApps.cpp src/ui.cpp src/utils.cpp src/cvautotrack.cpp src/cvautotrackImpl.cpp src/download.cpp src/yas.cpp src/scheme.cpp lib/json11.cpp src/installer.cpp ${SOURCE} app.rc)
set_target_properties(cocogoat-control PROPERTIES WIN32_EXECUTABLE TRUE)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)
# _WIN32_WINNT=0x0601
ADD_DEFINITIONS(-D_WIN32_WINNT=0x0601)
set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

find_package(websocketpp CONFIG REQUIRED)
include_directories (${websocketpp_INCLUDE_DIRS})

find_package(CURL CONFIG REQUIRED)
include_directories (${CURL_INCLUDE_DIRS})

target_link_libraries(cocogoat-control Rpcrt4.lib shlwapi.lib crypt32.lib imagehlp.lib websocketpp::websocketpp ${PROJECT_SOURCE_DIR}/lib/libcurl.lib ${PROJECT_SOURCE_DIR}/lib/libsodium.lib)

set_target_properties(cocogoat-control PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")