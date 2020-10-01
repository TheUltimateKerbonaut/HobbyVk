#pragma once
#ifndef RENDERER_H
#define RENDERER_H

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#ifndef _DEBUG
#define VULKAN_HPP_NO_EXCEPTIONS 
#endif
#include <vulkan/vulkan.hpp>

#include "Window.h"
#include <optional>

class Renderer
{
public:
	Renderer(const uint32_t width, const uint32_t height);
	~Renderer();

	inline bool ShouldRun() { return !m_Window.ShouldClose(); }

	void PrepareFrame();

private:

	// Main functions
	void InitVulkan();
	void CreateInstance();
	bool CheckValidationLayerSupport();
	void CreateSurface();
	void CreateSwapChain();
	void CreateImageViews();
	void CreateGraphicsPipeline();

	// Picking and creating devices
	void PickPhysicalDevice();
	bool IsDeviceSuitable(vk::PhysicalDevice device);
	bool CheckDeviceExtensionSupport(vk::PhysicalDevice device);
	void CreateLogicalDevice();

	// Swap chains
	struct SwapChainSupportDetails
	{
		vk::SurfaceCapabilitiesKHR capabilities;
		std::vector<vk::SurfaceFormatKHR> formats;
		std::vector<vk::PresentModeKHR> presentModes;
	};
	SwapChainSupportDetails	QuerySwapChainSupport(vk::PhysicalDevice device);
	vk::SurfaceFormatKHR	ChoseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& vAvailableFormats);
	vk::PresentModeKHR		ChoseSwapPresentMode(const std::vector<vk::PresentModeKHR>& vAvailablePresentModes);
	vk::Extent2D			ChoseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

	// Queue families
	struct QueueFamilyIndices
	{
		std::optional<uint32_t> graphicsFamily;
		std::optional<uint32_t> presentFamily;
		bool IsComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
	};
	QueueFamilyIndices FindQueueFamilies(vk::PhysicalDevice device);

	// Shaders
	const std::vector<char> ReadFile(const std::string& sFilename);
	vk::ShaderModule CreateShaderModule(const std::vector<char>& vCode);

	const uint32_t m_Width;
	const uint32_t m_Height;

	Window m_Window;

	vk::Instance m_Instance;

	// Validation layers and device extensions //"VK_LAYER_LUNARG_api_dump"
	const std::vector<const char*> m_vValidationLayers	 =  { "VK_LAYER_KHRONOS_validation"		};
	const std::vector<const char*> m_DeviceExtensions	 =  { VK_KHR_SWAPCHAIN_EXTENSION_NAME	};
	
	// Devices, queue and surface
	vk::PhysicalDevice m_PhysicalDevice;
	vk::Device m_Device; // Logical device
	vk::Queue m_GraphicsQueue;
	vk::Queue m_PresentQueue;
	vk::SurfaceKHR m_Surface;

	// Swapchains
	vk::SwapchainKHR m_Swapchain;
	std::vector<vk::Image> m_SwapchainImages;
	vk::Format m_SwapchainImageFormat;
	vk::Extent2D m_SwapChainExtent;
	std::vector<vk::ImageView> m_SwapchainImageViews;
};

#endif