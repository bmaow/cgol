cmake_minimum_required(VERSION 3.5...3.24)

set(CMAKE_CXX_STANDARD 17)

project(cgol)

# Subdirectories ---------------------------------------- /

# GLFW
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_SOURCE_DIR}/src/dependencies/glfw)


set(BUILD_SRC
	src/main.cpp
	src/ogls.h
	src/ogls.cpp

	# glad
	src/dependencies/glad/include/glad/glad.h
	src/dependencies/glad/src/glad.c
	
	# imgui
	src/dependencies/imgui/imconfig.h
	src/dependencies/imgui/imgui.cpp
	src/dependencies/imgui/imgui.h
	src/dependencies/imgui/imgui_demo.cpp
	src/dependencies/imgui/imgui_draw.cpp
	src/dependencies/imgui/imgui_impl_glfw.cpp
	src/dependencies/imgui/imgui_impl_glfw.h
	src/dependencies/imgui/imgui_impl_opengl3.cpp
	src/dependencies/imgui/imgui_impl_opengl3.h
	src/dependencies/imgui/imgui_impl_opengl3_loader.h
	src/dependencies/imgui/imgui_internal.h
	src/dependencies/imgui/imgui_tables.cpp
	src/dependencies/imgui/imgui_widgets.cpp
	src/dependencies/imgui/imstb_rectpack.h
	src/dependencies/imgui/imstb_textedit.h
	src/dependencies/imgui/imstb_truetype.h
)

add_executable(cgol ${BUILD_SRC})

target_include_directories(cgol
	PUBLIC
	${CMAKE_SOURCE_DIR}/src/dependencies
	${CMAKE_SOURCE_DIR}/src/dependencies/glad/include
	${CMAKE_SOURCE_DIR}/src/dependencies/glfw/include
)

target_link_libraries(cgol
	PRIVATE
	glfw
)
