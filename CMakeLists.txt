cmake_minimum_required(VERSION 3.20)
project(MuvixoClient CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    add_compile_options(/utf-8 /EHsc /MP)
endif()

include(FetchContent)

FetchContent_Declare(minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG        master)
FetchContent_MakeAvailable(minhook)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        docking)
FetchContent_MakeAvailable(imgui)

add_library(MuvixoClient SHARED
    src/dllmain.cpp
    src/clickgui.cpp
    src/hooks.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)
target_include_directories(MuvixoClient PRIVATE
    src ${imgui_SOURCE_DIR} ${imgui_SOURCE_DIR}/backends
    ${minhook_SOURCE_DIR}/include)
target_link_libraries(MuvixoClient PRIVATE minhook opengl32 dwmapi)
set_target_properties(MuvixoClient PROPERTIES PREFIX "" OUTPUT_NAME "MuvixoClient")

add_executable(Injector src/injector.cpp)
target_compile_definitions(Injector PRIVATE UNICODE _UNICODE)
set_target_properties(Injector PROPERTIES OUTPUT_NAME "Injector")

add_custom_command(TARGET Injector POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:MuvixoClient> $<TARGET_FILE_DIR:Injector>)
