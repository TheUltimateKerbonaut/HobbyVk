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

	void DrawFrame();
	void WaitIdle();

private:

	// Main functions
	void InitVulkan();
	void CreateInstance();
	void SetupDebugMessanger();
	bool CheckValidationLayerSupport();
	void CreateSurface();
	void CreateSwapChain();
	void CreateImageViews();
	void CreateRenderPass();
	void CreateGraphicsPipeline();
	void CreateFramebuffers();
	void CreateCommandPool();
	void CreateCommandBuffers();
	void CreateSyncObjects();

	// Picking and creating devices
	void PickPhysicalDevice();
	bool IsDeviceSuitable(vk::PhysicalDevice device);
	bool CheckDeviceExtensionSupport(vk::PhysicalDevice device);
	void CreateLogicalDevice();

	// Validation layers and debugging
	std::vector<const char*> GetRequiredExtensions(); // Changes based on bEnableValidationLayers
	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
														VkDebugUtilsMessageTypeFlagsEXT messageType,
														VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData,
														void* pUserData);
	static VkResult CreateDebugUtilsMessengerEXT(	VkInstance instance, 
													const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, 
													const VkAllocationCallbacks* pAllocator, 
													VkDebugUtilsMessengerEXT* pDebugMessenger);
	static void DestroyDebugUtilsMessangerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessanger, const VkAllocationCallbacks* pAllocator);
	void PopulateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo);
	VkDebugUtilsMessengerEXT m_DebugMessenger;

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
	vk::UniqueShaderModule CreateShaderModule(const std::vector<char>& vCode);

	const uint32_t m_Width;
	const uint32_t m_Height;

	Window m_Window;

	vk::UniqueInstance m_Instance;

	// Validation layers and device extensions //"VK_LAYER_LUNARG_api_dump"
	const std::vector<const char*> m_vValidationLayers	 =  { "VK_LAYER_KHRONOS_validation" };
	const std::vector<const char*> m_DeviceExtensions	 =  { VK_KHR_SWAPCHAIN_EXTENSION_NAME	};
	
	// Devices, queue, surface
	vk::PhysicalDevice m_PhysicalDevice;
	vk::UniqueDevice m_Device; // Logical device
	vk::Queue m_GraphicsQueue;
	vk::Queue m_PresentQueue;
	vk::UniqueSurfaceKHR m_Surface;

	// Swapchains
	vk::UniqueSwapchainKHR m_Swapchain;
	std::vector<vk::Image> m_SwapchainImages;
	vk::Format m_SwapchainImageFormat;
	vk::Extent2D m_SwapChainExtent;
	std::vector<vk::UniqueImageView> m_SwapchainImageViews;
	std::vector<vk::UniqueFramebuffer> m_SwapchainFramebuffers;

	// Pipeline and render pass
	vk::UniquePipelineLayout m_PipelineLayout;
	vk::UniqueRenderPass m_RenderPass;
	vk::UniquePipeline m_GraphicsPipeline;

	// Command buffers
	vk::UniqueCommandPool m_CommandPool;
	std::vector<vk::UniqueCommandBuffer> m_CommandBuffers; // SHould be automatically freed when their commands pools are destroyed

	// Sephamores
	const int MAX_FRAMES_IN_FLIGHT = 2;
	std::vector<vk::UniqueSemaphore> m_vImageAvailableSemaphores;
	std::vector<vk::UniqueSemaphore> m_vRenderFinishedSemaphores;
	std::vector<vk::Fence> m_vInFlightFences; // We do some funky stuff,
	std::vector<vk::Fence> m_vImagesInFlight; // best manage memory ourselves
	size_t m_nCurrentFrame;

};

#endif