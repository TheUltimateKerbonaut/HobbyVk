#pragma once
#ifndef WINDOW_H
#define WINDOW_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

class Window
{
public:
	Window(const uint32_t width, const uint32_t height, const std::string& title);
	~Window();

	inline bool ShouldClose() { return glfwWindowShouldClose(m_Window); }
	inline void Update() { glfwPollEvents(); }

	std::pair<uint32_t, const char**> GetExtensions();

	GLFWwindow* m_Window;
};

#endif