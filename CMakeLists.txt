cmake_minimum_required(VERSION  3.2)
project(cocogoat-control)
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_subdirectory("${PROJECT_SOURCE_DIR}/MfcClass")
include_directories("${PROJECT_SOURCE_DIR}/MfcClass")
include_directories("${PROJECT_SOURCE_DIR}/lib")
add_executable(cocogoat-control src/main.cpp src/server.cpp src/ui.cpp lib/json11.cpp ${SOURCE} app.rc)
set_target_properties(cocogoat-control PROPERTIES WIN32_EXECUTABLE TRUE)
target_link_libraries(cocogoat-control Rpcrt4.lib)
ADD_DEFINITIONS(-DUNICODE)
ADD_DEFINITIONS(-D_UNICODE)

set_target_properties(cocogoat-control PROPERTIES LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator' uiAccess='false'\" /SUBSYSTEM:WINDOWS")