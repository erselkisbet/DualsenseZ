cmake_minimum_required (VERSION 3.10)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Release>:Release>")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(PRODUCTION_BUILD ON CACHE BOOL "Make this a production build" FORCE)
project (DualsenseZ)
file(GLOB_RECURSE MY_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")

set(CORE_SOURCES ${MY_SOURCES})
add_library(DualsenseCore STATIC ${CORE_SOURCES})
target_include_directories(DualsenseCore PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/source")
target_compile_definitions(DualsenseCore PRIVATE RESOURCES_PATH="./resources/" PRODUCTION_BUILD=1 DEVELOPMENT_BUILD=0)
if(WIN32)
  target_compile_definitions(DualsenseCore PRIVATE WINDOWS=1)
elseif(APPLE)
  target_compile_definitions(DualsenseCore PRIVATE APPLE=1)
else()
  target_compile_definitions(DualsenseCore PRIVATE LINUX=1)
endif()
if(MSVC)
  target_compile_options(DualsenseCore PRIVATE /utf-8)
endif()

add_subdirectory(thirdparty/duaLib)
add_subdirectory(thirdparty/json)
add_subdirectory(thirdparty/miniaudio)
add_subdirectory(thirdparty/ViGEmClient)
add_subdirectory(thirdparty/asio-cmake)
add_subdirectory(thirdparty/PlatformFolders)

# Linking libraries
target_link_libraries(DualsenseCore PUBLIC duaLib nlohmann_json miniaudio ViGEmClient asio sago::platform_folders)
# MSBuild consumes the same CMake-built static core and its native dependencies.
if(WIN32)
  file(GENERATE OUTPUT "${CMAKE_BINARY_DIR}/DualsenseCore.props" CONTENT
"<Project xmlns=\"http://schemas.microsoft.com/developer/msbuild/2003\"><ItemDefinitionGroup><Link><AdditionalDependencies>$<TARGET_FILE:DualsenseCore>;$<TARGET_FILE:duaLib>;$<TARGET_FILE:miniaudio>;$<TARGET_FILE:ViGEmClient>;$<TARGET_FILE:platform_folders>;$<TARGET_LINKER_FILE:hidapi::hidapi>;%(AdditionalDependencies)</AdditionalDependencies></Link></ItemDefinitionGroup></Project>")
endif()

option(DUALSENSEZ_CORE_TESTS "Build headless core boundary integration test" OFF)
if(DUALSENSEZ_CORE_TESTS)
  add_executable(CoreBoundaryTests tests/coreBoundaryTests.cpp tests/workerExchangeTests.cpp)
  target_include_directories(CoreBoundaryTests PRIVATE include source)
  target_compile_definitions(CoreBoundaryTests PRIVATE WINDOWS=1 PRODUCTION_BUILD=1 DEVELOPMENT_BUILD=0 RESOURCES_PATH="./resources/")
  if(MSVC)
    target_compile_options(CoreBoundaryTests PRIVATE /utf-8)
  endif()
  target_link_libraries(CoreBoundaryTests PRIVATE DualsenseCore)
  add_custom_command(TARGET CoreBoundaryTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/resources $<TARGET_FILE_DIR:CoreBoundaryTests>/resources
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:hidapi::hidapi> $<TARGET_FILE_DIR:CoreBoundaryTests>)
endif()
