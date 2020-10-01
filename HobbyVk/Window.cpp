#include "Window.h"

Window::Window(const uint32_t width, const uint32_t height, const std::string& title)
{
	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_Window = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
}

std::pair<uint32_t, const char**> Window::GetExtensions()
{
	uint32_t glfwExtensionCount = 0;
	const char** extensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
	return std::pair<uint32_t, const char**>(glfwExtensionCount, extensions);
}

Window::~Window()
{
	glfwDestroyWindow(m_Window);
	glfwTerminate();
}