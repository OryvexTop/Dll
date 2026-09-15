cmake_minimum_required(VERSION 3.20)
project(MuvixoClient CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

if(MSVC)
    add_compile_options(/utf-8 /EHsc /MP /W3)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

if(NOT WIN32)
    message(FATAL_ERROR "Windows only.")
endif()

include(FetchContent)

FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        v1.3.4
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(minhook)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5-docking
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(imgui)

# ── ImGui static lib ────────────────────────────────────────────────────
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp)
target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends)
target_link_libraries(imgui PUBLIC opengl32 dwmapi)

# ── MuvixoClient.dll ────────────────────────────────────────────────────
add_library(MuvixoClient SHARED
    src/dllmain.cpp
    src/clickgui.cpp
    src/hooks.cpp
    src/modules.cpp)
target_include_directories(MuvixoClient PRIVATE
    src ${minhook_SOURCE_DIR}/include)
target_link_libraries(MuvixoClient PRIVATE imgui minhook opengl32 dwmapi)
set_target_properties(MuvixoClient PROPERTIES PREFIX "" OUTPUT_NAME "MuvixoClient")

# ── Injector.exe (GUI subsystem - no console window) ────────────────────
add_executable(Injector WIN32 src/injector.cpp)
target_compile_definitions(Injector PRIVATE UNICODE _UNICODE)
target_link_libraries(Injector PRIVATE comctl32)
set_target_properties(Injector PROPERTIES OUTPUT_NAME "Injector")

# ── Copy DLL next to Injector ───────────────────────────────────────────
add_custom_command(TARGET Injector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:MuvixoClient> $<TARGET_FILE_DIR:Injector>
    COMMENT "Copying MuvixoClient.dll next to Injector.exe")
