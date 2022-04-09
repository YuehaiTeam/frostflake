cmake_minimum_required(VERSION  3.20)
set(CMAKE_GENERATOR_PLATFORM x64)
project(cocogoat-control)
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_subdirectory("${PROJECT_SOURCE_DIR}/lib/MfcClass")
include_directories("${PROJECT_SOURCE_DIR}/lib/MfcClass")
include_directories("${PROJECT_SOURCE_DIR}/lib")
add_executable(cocogoat-control src/main.cpp src/server.cpp src/ws.cpp src/ui.cpp src/utils.cpp src/cvautotrack.cpp src/download.cpp src/yas.cpp lib/json11.cpp ${SOURCE} app.rc)
set_target_properties(cocogoat-control PROPERTIES WIN32_EXECUTABLE TRUE)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

find_package(websocketpp CONFIG REQUIRED)
include_directories (${websocketpp_INCLUDE_DIRS})

find_package(unofficial-brotli CONFIG REQUIRED)
include_directories (${unofficial-brotli_INCLUDE_DIRS})
target_link_libraries(cocogoat-control Rpcrt4.lib shlwapi.lib wininet.lib crypt32.lib websocketpp::websocketpp unofficial::brotli::brotlidec unofficial::brotli::brotlienc unofficial::brotli::brotlicommon)

set_target_properties(cocogoat-control PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:WINDOWS")