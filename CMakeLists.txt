cmake_minimum_required(VERSION 3.6)

project(comanche)

set(APP_VERSION_MAJOR 0)
set(APP_VERSION_MINOR 2)
set(CMAKE_CXX_STANDARD 11)

add_subdirectory(lib/glew-2.1.0/build/cmake)
add_subdirectory(lib/glfw)

add_library(
    imgui STATIC
    lib/imgui/imconfig.h
    lib/imgui/imgui.cpp
    lib/imgui/imgui.h
    lib/imgui/imgui_demo.cpp
    lib/imgui/imgui_draw.cpp
    lib/imgui/examples/imgui_impl_glfw.cpp
    lib/imgui/examples/imgui_impl_glfw.h
    lib/imgui/examples/imgui_impl_opengl3.cpp
    lib/imgui/examples/imgui_impl_opengl3.h
    lib/imgui/imgui_internal.h
    lib/imgui/imgui_widgets.cpp
    lib/imgui/imstb_rectpack.h
    lib/imgui/imstb_textedit.h
    lib/imgui/imstb_truetype.h
)
target_compile_definitions(imgui PRIVATE GLEW_STATIC IMGUI_IMPL_OPENGL_LOADER_GLEW)
include_directories(
    lib/imgui
    lib/imgui/examples
)

include_directories(
    lib/glew-2.1.0/include
    lib/glfw/include
    lib/glm
    lib/lodepng
    lib/OpenSimplexCPP/include
)

add_executable(
    comanche
    main.cpp
    lib/lodepng/lodepng.cpp
)
#set(CMAKE_EXE_LINKER_FLAGS " -static")
target_link_libraries(
    comanche
    #-static-libgcc
    imgui
    glew_s
    glfw
    #opengl32
)
configure_file(textures.png textures.png COPYONLY)
