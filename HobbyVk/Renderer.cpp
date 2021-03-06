#include "Renderer.h"
#include <iostream>
#include <set>
#include <fstream>

#ifdef _DEBUG
	constexpr bool bEnableValidationLayers = true;
#else
	constexpr bool bEnableValidationLayers = false;
#endif

Renderer::Renderer(const uint32_t width, const uint32_t height) : m_Width(width), m_Height(height), m_Window(width, height, "HobbyVk")
{
	InitVulkan();
}

void Renderer::InitVulkan()
{
	CreateInstance();
	SetupDebugMessanger();
	CheckValidationLayerSupport();
	CreateSurface();
	PickPhysicalDevice();
	CreateLogicalDevice();
	CreateSwapChain();
	CreateImageViews();
	CreateRenderPass();
	CreateGraphicsPipeline();
	CreateFramebuffers();
	CreateCommandPool();
	CreateCommandBuffers();
	CreateSyncObjects();
}

void Renderer::CreateInstance()
{
	// App info
	vk::ApplicationInfo appInfo = vk::ApplicationInfo(
		"HobbyVk",					// Application name
		VK_MAKE_VERSION(1, 0, 0),	// Application version
		"No engine",				// Engine name
		VK_MAKE_VERSION(1, 0, 0),	// Engine version
		VK_API_VERSION_1_2			// API version
	);

	// Vulkan is platform agnostic, so an extension(s) is required to interface with the window system
	auto vRequiredExtensions = GetRequiredExtensions();

	// Create instance info
	vk::InstanceCreateInfo createInfo = vk::InstanceCreateInfo({}, &appInfo);
	createInfo.enabledExtensionCount = static_cast<uint32_t>(vRequiredExtensions.size());
	createInfo.ppEnabledExtensionNames = vRequiredExtensions.data();
	vk::DebugUtilsMessengerCreateInfoEXT debugCreateInfo; // Outside if below so as not to be destroyed before create instance
	if (bEnableValidationLayers) 
	{ 
		createInfo.enabledLayerCount = static_cast<uint32_t>(m_vValidationLayers.size()); 
		createInfo.ppEnabledLayerNames = m_vValidationLayers.data(); 
		
		// Debugging for instance creation
		PopulateDebugMessengerCreateInfo(debugCreateInfo);
		createInfo.pNext = &debugCreateInfo;

	} else createInfo.enabledLayerCount = 0; // No global validation layers

	// Get extensions
	std::vector<vk::ExtensionProperties> vExtensions = vk::enumerateInstanceExtensionProperties();
	std::cout << "Available extensions: " << std::endl;
	for (const auto& extension : vExtensions) { std::cout << "\t" << extension.extensionName << std::endl; }
	std::cout << std::endl;

	// Check that required extensions are within extensions
	int nFoundExtensions = 0;
	for (const auto& extension : vExtensions)
	{
		for (const auto& requiredExtension : vRequiredExtensions)
			if (!std::strcmp(extension.extensionName, requiredExtension)) nFoundExtensions++;
	}
	if (nFoundExtensions != vRequiredExtensions.size()) throw new std::runtime_error("GLFW Vulkan extensions not supported!");

	// Check for validation layers
	if (bEnableValidationLayers && !CheckValidationLayerSupport()) throw new std::runtime_error("Vulkan validation layers requested but not available!");

	// Create Vulkan instance
	m_Instance = vk::createInstanceUnique(createInfo);
}

bool Renderer::CheckValidationLayerSupport()
{
	// Get layers and ensure all that we require are present
	std::vector<vk::LayerProperties> vLayers = vk::enumerateInstanceLayerProperties();
	for (const auto& layerName : m_vValidationLayers)
	{
		bool bFound = false;

		for (const auto& layerProperties : vLayers)
		{
			if (!std::strcmp(layerName, layerProperties.layerName)) { bFound = true; break; }
		}

		if (!bFound) return false;
	}
	
	return true;
}

void Renderer::CreateSurface()
{
	vk::SurfaceKHR surface;
	if (glfwCreateWindowSurface(m_Instance.get(), m_Window.m_Window, nullptr, reinterpret_cast<VkSurfaceKHR*>(&surface)) != VK_SUCCESS)
		throw std::runtime_error("Failed to create window surface!");
	vk::ObjectDestroy<vk::Instance, VULKAN_HPP_DEFAULT_DISPATCHER_TYPE> _deleter(m_Instance.get());
	m_Surface = vk::UniqueSurfaceKHR(surface, _deleter);
}

void Renderer::PickPhysicalDevice()
{
	// List GPUs
	std::vector<vk::PhysicalDevice> vDevices = m_Instance.get().enumeratePhysicalDevices();
	if (vDevices.size() == 0) throw std::runtime_error("Failed to detect any Vulkan compatible GPUs!");

	// Chose GPU
	bool bChoseDevice = false;
	for (const auto& device : vDevices)
	{
		if (IsDeviceSuitable(device)) { m_PhysicalDevice = device; bChoseDevice = true; break; }
	}
	if (!bChoseDevice) throw new std::runtime_error("Failed to find a suitable Vulkan GPU!");
	std::cout << "Chose GPU " << m_PhysicalDevice.getProperties().deviceName << std::endl << std::endl;
}

bool Renderer::IsDeviceSuitable(vk::PhysicalDevice device)
{
	// Get info
	vk::PhysicalDeviceProperties properties = device.getProperties(); // Eg: return properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;
	vk::PhysicalDeviceFeatures features = device.getFeatures(); // Eg: return features.geometryShader;

	bool bExtensionsSupported = CheckDeviceExtensionSupport(device);

	bool bSwapChainAdequate = false;
	if (bExtensionsSupported)
	{
		auto swapChainSupport = QuerySwapChainSupport(device);
		bSwapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
	}

	QueueFamilyIndices indices = FindQueueFamilies(device);
	return indices.IsComplete() && bExtensionsSupported && bSwapChainAdequate;
}

bool Renderer::CheckDeviceExtensionSupport(vk::PhysicalDevice device)
{
	// Query extensions then create a set from required extensions vector
	std::vector<vk::ExtensionProperties> vExtensions = device.enumerateDeviceExtensionProperties();
	std::set<std::string> sRequiredExtensions(m_DeviceExtensions.begin(), m_DeviceExtensions.end());

	// Enumerate through each extension, removing from the set each once it's found
	for (const auto& extension : vExtensions) sRequiredExtensions.erase(extension.extensionName);

	return sRequiredExtensions.empty();
}

Renderer::QueueFamilyIndices Renderer::FindQueueFamilies(vk::PhysicalDevice device)
{
	// Get queue families
	Renderer::QueueFamilyIndices indices;
	std::vector<vk::QueueFamilyProperties> vFamilies = device.getQueueFamilyProperties();
	
	// Check for VK_QUEUE_GRAPHICS_BIT and present support
	int i = 0;
	for (const auto& family : vFamilies)
	{
		if (family.queueFlags & vk::QueueFlagBits::eGraphics) indices.graphicsFamily = i;

		auto bPresentSupport = device.getSurfaceSupportKHR(i, m_Surface.get());
		if (bPresentSupport) indices.presentFamily = i;

		if (indices.IsComplete()) break;
		i++;
	}

	return indices;
}

void Renderer::CreateLogicalDevice()
{
	QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);

	// Create a set of all unique queue families that are nessecary
	std::vector<vk::DeviceQueueCreateInfo> vQueueCreateInfos;
	std::set<uint32_t> uniqueQueueFamilies = { indices.graphicsFamily.value(), indices.presentFamily.value() };

	float fPriority = 1.0f;
	for (uint32_t queueFamily : uniqueQueueFamilies)
	{
		// Specify the queues to be created
		vk::DeviceQueueCreateInfo queueCreateInfo{};
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &fPriority;
		vQueueCreateInfos.push_back(queueCreateInfo);
	}

	// Special GPU features
	vk::PhysicalDeviceFeatures deviceFeatures{};

	// Create logical device
	vk::DeviceCreateInfo createInfo{};
	createInfo.pQueueCreateInfos = vQueueCreateInfos.data();
	createInfo.queueCreateInfoCount = static_cast<uint32_t>(vQueueCreateInfos.size());
	createInfo.pEnabledFeatures = &deviceFeatures;
	createInfo.enabledExtensionCount = static_cast<uint32_t>(m_DeviceExtensions.size());
	createInfo.ppEnabledExtensionNames = m_DeviceExtensions.data();
	if (bEnableValidationLayers)
	{ 
		createInfo.enabledLayerCount = static_cast<uint32_t>(m_vValidationLayers.size());
		createInfo.ppEnabledLayerNames = m_vValidationLayers.data();
	} else createInfo.enabledLayerCount = 0;

	m_Device = m_PhysicalDevice.createDeviceUnique(createInfo);

	// Retrieve queues too - only need one queue from families, so we'll use index 0
	m_GraphicsQueue = m_Device.get().getQueue(indices.graphicsFamily.value(), 0);
	m_PresentQueue = m_Device.get().getQueue(indices.presentFamily.value(), 0);
}

std::vector<const char*> Renderer::GetRequiredExtensions()
{
	std::pair<uint32_t, const char**> glfwExtensions = m_Window.GetExtensions();
	std::vector<const char*> vExtensions(glfwExtensions.second, glfwExtensions.second + glfwExtensions.first);
	if (bEnableValidationLayers) vExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

	return vExtensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL Renderer::DebugCallback(	VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
														VkDebugUtilsMessageTypeFlagsEXT messageType, 
														VkDebugUtilsMessengerCallbackDataEXT const* pCallbackData, 
														void* pUserData)
{
	// Filter out non-important stuff
	if (static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>(messageSeverity) < vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning) return VK_FALSE;

	std::cerr << "--- DEBUG MESSAGE ---" << std::endl << "Message type: " << std::endl;
	
	// Message type
	if (static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(messageType) == vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral)		std::cerr << "general"		<< std::endl;
	if (static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(messageType) == vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)	std::cerr << "performance"	<< std::endl;
	if (static_cast<vk::DebugUtilsMessageTypeFlagBitsEXT>(messageType) == vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation)	std::cerr << "validation"	<< std::endl;
	else std::cerr << "unknown" << std::endl;

	// Message objects
	if (pCallbackData->objectCount > 0)
	{
		std::cerr << "Objects:" << std::endl;
		std::vector vObjects(pCallbackData->pObjects, pCallbackData->pObjects + pCallbackData->objectCount);
		for (auto& object : vObjects) std::cout << "Handle:" << object.objectHandle << std::endl;
	}

	std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
	return VK_FALSE;
}

VkResult Renderer::CreateDebugUtilsMessengerEXT(VkInstance instance, 
												const VkDebugUtilsMessengerCreateInfoEXT * pCreateInfo, 
												const VkAllocationCallbacks * pAllocator, 
												VkDebugUtilsMessengerEXT * pDebugMessenger)
{
	auto function = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (function != nullptr) return function(instance, pCreateInfo, pAllocator, pDebugMessenger);
	else return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void Renderer::DestroyDebugUtilsMessangerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessanger, const VkAllocationCallbacks* pAllocator)
{
	auto function = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (function != nullptr) function(instance, debugMessanger, pAllocator);
}

void Renderer::PopulateDebugMessengerCreateInfo(vk::DebugUtilsMessengerCreateInfoEXT& createInfo)
{
	vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);

	vk::DebugUtilsMessageTypeFlagsEXT messageTypeFlags(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
		vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
		vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);

	createInfo = vk::DebugUtilsMessengerCreateInfoEXT(vk::DebugUtilsMessengerCreateFlagsEXT{}, severityFlags, messageTypeFlags, DebugCallback);
}

void Renderer::SetupDebugMessanger()
{
	if (!bEnableValidationLayers) return;

	vk::DebugUtilsMessengerCreateInfoEXT createInfo;
	PopulateDebugMessengerCreateInfo(createInfo);

	// Call our fake function so that we might load the extension if it's not present
	if (CreateDebugUtilsMessengerEXT(m_Instance.get(), &static_cast<VkDebugUtilsMessengerCreateInfoEXT>(createInfo), nullptr, &m_DebugMessenger))
		throw std::runtime_error("Failed to setup Vulkan debug messanger!");
}

Renderer::SwapChainSupportDetails Renderer::QuerySwapChainSupport(vk::PhysicalDevice device)
{
	SwapChainSupportDetails details;

	// "'operator =' is ambiguous" my arse
	auto capabilities = device.getSurfaceCapabilitiesKHR(m_Surface.get());
	auto formats = device.getSurfaceFormatsKHR(m_Surface.get());
	auto presentModes = device.getSurfacePresentModesKHR(m_Surface.get());
	details.capabilities = capabilities;
	details.formats = formats;
	details.presentModes = presentModes;

	return details;
}

vk::SurfaceFormatKHR Renderer::ChoseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR>& vAvailableFormats)
{
	for (const auto& availableFormat : vAvailableFormats)
	{
		if (availableFormat.format == vk::Format::eB8G8R8A8Srgb && availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) return availableFormat;
	}

	return vAvailableFormats[0]; // Else return next best format.... or panic!
}

vk::PresentModeKHR Renderer::ChoseSwapPresentMode(const std::vector<vk::PresentModeKHR>& vAvailablePresentModes)
{
	// Try to get tripple buffering, but fall back to (basically) v-sync if unavailable
	for (const auto& availablePresentMode : vAvailablePresentModes)
	{
		if (availablePresentMode == vk::PresentModeKHR::eMailbox) return availablePresentMode;
	}

	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D Renderer::ChoseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
	// Vulkan tells us to match the resolution of the window by
	// setting the width and height in the currentExtent member. However,
	// some window managers do allow us to differ here and this is indicated
	// by setting the width and height to UINT32_MAX
	if (capabilities.currentExtent.width != UINT32_MAX) return capabilities.currentExtent;
	else
	{
		// Clamp between min and max allowed values
		vk::Extent2D actualExtent = { m_Width, m_Height };
		actualExtent.width =	std::max(capabilities.minImageExtent.width,	std::min(capabilities.maxImageExtent.width,	 actualExtent.width));
		actualExtent.height =	std::max(capabilities.minImageExtent.height,std::min(capabilities.maxImageExtent.height, actualExtent.height));
		return actualExtent;
	}
}

void Renderer::CreateSwapChain()
{
	SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(m_PhysicalDevice);

	vk::SurfaceFormatKHR surfaceFormat = ChoseSwapSurfaceFormat(swapChainSupport.formats);
	vk::PresentModeKHR presentMode = ChoseSwapPresentMode(swapChainSupport.presentModes);
	vk::Extent2D extent = ChoseSwapExtent(swapChainSupport.capabilities);

	// We know there's a minimum amount of images, but we really want more as
	// that means that we may sometimes be waiting for the driver to complete
	// its stuff, so let's request at least one more than the minimum
	uint32_t nImages = swapChainSupport.capabilities.minImageCount + 1;
	if (swapChainSupport.capabilities.maxImageCount > 0 && nImages > swapChainSupport.capabilities.maxImageCount)
		nImages = swapChainSupport.capabilities.maxImageCount; // Zero is a special number, meaning zero limits ^^^

	// Create the swapchain
	vk::SwapchainCreateInfoKHR createInfo{};
	createInfo.surface = m_Surface.get();
	createInfo.minImageCount = nImages;
	createInfo.imageFormat = surfaceFormat.format;
	createInfo.imageColorSpace = surfaceFormat.colorSpace;
	createInfo.imageExtent = extent;
	createInfo.imageArrayLayers = 1; // Always one unless we're stereoscopic 3D
	createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
	// We're just rendering directly, like a framebuffer with a colour attatchment, but if we're using an FBO then VK_IMAGE_USAGE_TRANSFER_DST_BIT would be wise

	// Decide what to do if a swap chain image is across multiple queue families
	// VK_SHARING_MODE_EXCLUSIVE - nice and fast, VK_SHARING_MODE_CONCURRENT no need for ownership transfers
	QueueFamilyIndices indices = FindQueueFamilies(m_PhysicalDevice);
	uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };
	if (indices.graphicsFamily != indices.presentFamily)
	{
		createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = vk::SharingMode::eExclusive;
		createInfo.queueFamilyIndexCount = 0;		// Optional
		createInfo.pQueueFamilyIndices = nullptr;	// Optional
	}

	createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // We also have the ability to specify transforms (like rotating 90 degrees or flipping)
	createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque; // How should transparrency be treated?..... Ignore it
	createInfo.presentMode = presentMode;
	createInfo.clipped = true; // Just clip pixels outside the window, we don't need to sample them, and I do like a good bit of performance
	//createInfo.oldSwapchain = vk::SwapchainKHR(...); // Our swapchain can become invalid, if a window is resized for example, but what to do?...

	m_Swapchain = m_Device.get().createSwapchainKHRUnique(createInfo);
	auto swapchainImages = m_Device.get().getSwapchainImagesKHR(m_Swapchain.get()); // "ambigious" operator "fix"
	m_SwapchainImages = swapchainImages;
	m_SwapchainImageFormat = surfaceFormat.format;
	m_SwapChainExtent = extent;
}

void Renderer::CreateImageViews()
{
	m_SwapchainImageViews.resize(m_SwapchainImages.size()); // Allocate space
	for (size_t i = 0; i < m_SwapchainImages.size(); ++i)
	{
		vk::ImageViewCreateInfo createInfo{};
		createInfo.image = m_SwapchainImages[i];
		createInfo.viewType = vk::ImageViewType::e2D; // 1D array, 2D texture or 3D texture / cubemap?
		createInfo.format = m_SwapchainImageFormat;
		// If we really want we can swizzle colour channels...
		createInfo.components.r = vk::ComponentSwizzle::eIdentity;	createInfo.components.r = vk::ComponentSwizzle::eIdentity;
		createInfo.components.r = vk::ComponentSwizzle::eIdentity;	createInfo.components.r = vk::ComponentSwizzle::eIdentity;
		// Image purposeand which part will be accessed - mipmaping, etc
		createInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
		createInfo.subresourceRange.baseMipLevel = 0;
		createInfo.subresourceRange.levelCount = 1;
		createInfo.subresourceRange.baseArrayLayer = 0;
		createInfo.subresourceRange.layerCount = 1; // You might want more than 1 layer for stereoscopic 3D for example

		m_SwapchainImageViews[i] = m_Device.get().createImageViewUnique(createInfo);
	}
}

void Renderer::CreateGraphicsPipeline()
{
	auto sVertexShader		= ReadFile("Shaders/vert.spv");
	auto sFragmentShader	= ReadFile("Shaders/frag.spv");

	vk::UniqueShaderModule vertexShaderModule =	CreateShaderModule(sVertexShader);
	vk::UniqueShaderModule fragmentShaderModule = CreateShaderModule(sFragmentShader);

	// Create shaders - one could also put vertexShaderStageInfo.pSpecializationInfo
	// so as to set shader contents, allowing optimisation of if statements and the like
	vk::PipelineShaderStageCreateInfo vertexShaderStageInfo = {};
	vertexShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
	vertexShaderStageInfo.module = vertexShaderModule.get();
	vertexShaderStageInfo.pName = "main"; // main() in shader

	vk::PipelineShaderStageCreateInfo fragmentShaderStageInfo = {};
	fragmentShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
	fragmentShaderStageInfo.module = fragmentShaderModule.get();
	fragmentShaderStageInfo.pName = "main";  // main() in shader
	
	vk::PipelineShaderStageCreateInfo shaderStages[] = { vertexShaderStageInfo, fragmentShaderStageInfo };

	// Vertex input - none for now
	vk::PipelineVertexInputStateCreateInfo vertexInputInfo{};
	vertexInputInfo.vertexBindingDescriptionCount = 0;
	vertexInputInfo.pVertexBindingDescriptions = nullptr; // optional
	vertexInputInfo.vertexAttributeDescriptionCount = 0;
	vertexInputInfo.pVertexAttributeDescriptions = nullptr; // optional

	// Input assembly - what kind of geometry will be drawn and if primitive
	// restart should be enabled
	vk::PipelineInputAssemblyStateCreateInfo inputAssembly{};
	inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
	inputAssembly.primitiveRestartEnable = false;

	// Viewport
	vk::Viewport viewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = (float)m_SwapChainExtent.width;
	viewport.height = (float)m_SwapChainExtent.height;
	viewport.minDepth = 0.0f; // Must be within [0,1], but min
	viewport.maxDepth = 1.0f; // can be higher than max

	// Scissors - region in which pixels will actually be stored
	vk::Rect2D scissor{};
	scissor.offset = { 0, 0 };
	scissor.extent = m_SwapChainExtent;

	// Combine viewport and scissors into a viewport state.
	vk::PipelineViewportStateCreateInfo viewportState{};
	viewportState.viewportCount = 1;
	viewportState.pViewports = &viewport;
	viewportState.scissorCount = 1;
	viewportState.pScissors = &scissor;

	// Rasteris(z)er - takes care of depth testing, face culling, the scissor test,
	// fill mode (wireframe rendering or polygons), and, erm.... rasteris(z)ing
	vk::PipelineRasterizationStateCreateInfo rasterizer{};
	rasterizer.depthClampEnable = false; // If true, clamp fragments too near or far apart instead of discarding them
	rasterizer.rasterizerDiscardEnable = false; // Basically disables any output to the framebufer
	rasterizer.polygonMode = vk::PolygonMode::eFill; // Important: using any other mode than fill requires a GPU feature!
	rasterizer.lineWidth = 1.0f; // Anything thicker than 1.0 requires enabling the wideLines GPU feature
	rasterizer.cullMode = vk::CullModeFlagBits::eBack;
	rasterizer.frontFace = vk::FrontFace::eClockwise;
	rasterizer.depthBiasEnable = VK_FALSE; // The depth bias can add a constant value to depthmaps, useful for shadowmaps
	rasterizer.depthBiasConstantFactor = 0.0f; // Optional
	rasterizer.depthBiasClamp = 0.0f; // Optional
	rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

	// Multisampling - disabled for now
	vk::PipelineMultisampleStateCreateInfo multisampling{};
	multisampling.sampleShadingEnable = false;
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = false;
	multisampling.alphaToOneEnable = false;

	// Todo: Depth and stencil with optional VkPipelineDepthStencilStateCreateInfo

	// Colour blending - configured for alpha blending yet disabled
	vk::PipelineColorBlendAttachmentState colourBlendAttatchment{};
	colourBlendAttatchment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colourBlendAttatchment.blendEnable = false;
	colourBlendAttatchment.srcColorBlendFactor = vk::BlendFactor::eSrcAlpha;
	colourBlendAttatchment.dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha;
	colourBlendAttatchment.colorBlendOp = vk::BlendOp::eAdd;
	colourBlendAttatchment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
	colourBlendAttatchment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
	colourBlendAttatchment.alphaBlendOp = vk::BlendOp::eAdd;

	// More colour blending - constants for blend factors for all the framebuffers
	vk::PipelineColorBlendStateCreateInfo colourBlending{};
	colourBlending.logicOpEnable = false;
	colourBlending.logicOp = vk::LogicOp::eCopy;
	colourBlending.attachmentCount = 1;
	colourBlending.pAttachments = &colourBlendAttatchment;
	colourBlending.blendConstants[0] = 0.0f;	colourBlending.blendConstants[1] = 0.0f;
	colourBlending.blendConstants[2] = 0.0f;	colourBlending.blendConstants[3] = 0.0f;

	// Todo: Dynamic state (if we want) - allows certain things to be changed at draw time,
	// like viewport and line width, in which case the nessecary above values are ignored

	// Pipeline layout - used for uniforms
	vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
	pipelineLayoutInfo.setLayoutCount = 0; // Optional
	pipelineLayoutInfo.pSetLayouts = nullptr; // Optional
	pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
	pipelineLayoutInfo.pPushConstantRanges = 0; // Optional
	m_PipelineLayout = m_Device.get().createPipelineLayoutUnique(pipelineLayoutInfo);

	// Create the graphics pipeline
	vk::GraphicsPipelineCreateInfo pipelineInfo{};
	pipelineInfo.stageCount = 2; // Fragment and vertex
	pipelineInfo.pStages = shaderStages;
	pipelineInfo.pVertexInputState = &vertexInputInfo;
	pipelineInfo.pInputAssemblyState = &inputAssembly;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pRasterizationState = &rasterizer;
	pipelineInfo.pMultisampleState = &multisampling;
	pipelineInfo.pDepthStencilState = nullptr; // Optional
	pipelineInfo.pColorBlendState = &colourBlending;
	pipelineInfo.pDynamicState = nullptr; // Optional
	pipelineInfo.layout = m_PipelineLayout.get();
	pipelineInfo.renderPass = m_RenderPass.get();
	pipelineInfo.subpass = 0;
	pipelineInfo.basePipelineHandle = vk::Pipeline {}; // Optional - these two can be used to create pipelines from others, as switching would be cheaper,
	pipelineInfo.basePipelineIndex = -1; // Optional - as would creating them be too.

	m_GraphicsPipeline = m_Device.get().createGraphicsPipelineUnique(vk::PipelineCache{}, pipelineInfo);
}

const std::vector<char> Renderer::ReadFile(const std::string & sFilename)
{
	// Read from end of file with ate so that determining size is easier
	std::ifstream fFile(sFilename, std::ios::ate | std::ios::binary);
	if (!fFile.is_open()) throw std::runtime_error("Unable to open file " + sFilename + "!");
	size_t fileSize = static_cast<size_t>(fFile.tellg());
	std::vector<char> vBuffer(fileSize);

	fFile.seekg(0);
	fFile.read(vBuffer.data(), fileSize);
	fFile.close();

	return vBuffer;
}

vk::UniqueShaderModule Renderer::CreateShaderModule(const std::vector<char>& vCode)
{
	vk::ShaderModuleCreateInfo createInfo = vk::ShaderModuleCreateInfo({}, vCode.size(), reinterpret_cast<const uint32_t*>(vCode.data()));
	vk::UniqueShaderModule shaderModule = m_Device.get().createShaderModuleUnique(createInfo);
	return shaderModule;
}

void Renderer::CreateRenderPass()
{
	vk::AttachmentDescription colourAttatchment{};
	colourAttatchment.format = m_SwapchainImageFormat;
	colourAttatchment.samples = vk::SampleCountFlagBits::e1; // No multisampling
	colourAttatchment.loadOp = vk::AttachmentLoadOp::eClear; // Clear to black before drawing a new frame
	colourAttatchment.storeOp = vk::AttachmentStoreOp::eStore; // We'd like to read this framebuffer from memory later
	colourAttatchment.initialLayout = vk::ImageLayout::eUndefined; // We don't care what the previous layout was, we're cleaing it anyway
	colourAttatchment.finalLayout = vk::ImageLayout::ePresentSrcKHR; //  We want the image to be ready for presentation to the swap chain later

	// Subpasses and attachment references - only need one
	vk::AttachmentReference colourAttachmentReference{};
	colourAttachmentReference.attachment = 0;
	colourAttachmentReference.layout = vk::ImageLayout::eColorAttachmentOptimal; // Best for colour attachments

	vk::SubpassDescription subpass{};
	subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics; // It's for graphics!
	subpass.colorAttachmentCount = 1; // layout(location = 0) out vec4 outColor, could also mention pInputAttachments, 
	subpass.pColorAttachments = &colourAttachmentReference; // pResolveAttachments, pDepthStencilAttachment, and pPreserveAttachments too.

	// Create the render pass
	vk::RenderPassCreateInfo renderPassInfo{};
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &colourAttatchment;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	m_RenderPass = m_Device.get().createRenderPassUnique(renderPassInfo);

	// Subpass dependency
	vk::SubpassDependency dependency{};
	dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	dependency.dstSubpass = 0;
	dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.srcAccessMask = vk::AccessFlagBits{};
	dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
	dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite;
	renderPassInfo.dependencyCount = 1;
	renderPassInfo.pDependencies = &dependency;
}

void Renderer::CreateFramebuffers()
{
	// Create a framebuffer for each image view in the swapchain
	m_SwapchainFramebuffers.resize(m_SwapchainImageViews.size());

	for (size_t i = 0; i < m_SwapchainImageViews.size(); ++i)
	{
		vk::ImageView attachments[] = { m_SwapchainImageViews[i].get() };
		
		vk::FramebufferCreateInfo framebufferInfo{};
		framebufferInfo.renderPass = m_RenderPass.get();
		framebufferInfo.attachmentCount = 1;
		framebufferInfo.pAttachments = attachments;
		framebufferInfo.width = m_SwapChainExtent.width;
		framebufferInfo.height = m_SwapChainExtent.height;
		framebufferInfo.layers = 1;

		m_SwapchainFramebuffers[i] = m_Device.get().createFramebufferUnique(framebufferInfo);
	}
}

void Renderer::CreateCommandPool()
{
	QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(m_PhysicalDevice);

	vk::CommandPoolCreateInfo poolInfo{};
	poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

	m_CommandPool = m_Device.get().createCommandPoolUnique(poolInfo);
}

void Renderer::CreateCommandBuffers()
{
	m_CommandBuffers.resize(m_SwapchainFramebuffers.size());

	vk::CommandBufferAllocateInfo allocateInfo{};
	allocateInfo.commandPool = m_CommandPool.get();
	allocateInfo.level = vk::CommandBufferLevel::ePrimary; // Primary command buffer, not owned by another
	allocateInfo.commandBufferCount = static_cast<uint32_t>(m_CommandBuffers.size());

	m_CommandBuffers = m_Device.get().allocateCommandBuffersUnique(allocateInfo);

	// Start command buffer recording
	for (size_t i = 0; i < m_CommandBuffers.size(); ++i)
	{
		vk::CommandBufferBeginInfo beginInfo{};
		beginInfo.flags = vk::CommandBufferUsageFlags{};
		beginInfo.pInheritanceInfo = nullptr; // Only needed for secondary command buffers

		m_CommandBuffers[i].get().begin(beginInfo);

		vk::RenderPassBeginInfo renderPassInfo{};
		renderPassInfo.renderPass = m_RenderPass.get();
		renderPassInfo.framebuffer = m_SwapchainFramebuffers[i].get();
		renderPassInfo.renderArea.offset = { 0, 0 };
		renderPassInfo.renderArea.extent = m_SwapChainExtent;

		vk::ClearValue clearColour = vk::ClearColorValue(std::array<float, 4>{ 0.2f, 0.3f, 0.3f, 1.0f });
		renderPassInfo.clearValueCount = 1;
		renderPassInfo.pClearValues = &clearColour;
		
		m_CommandBuffers[i].get().beginRenderPass(renderPassInfo, vk::SubpassContents::eInline); // No secondary command buffers
		
			m_CommandBuffers[i].get().bindPipeline(vk::PipelineBindPoint::eGraphics, m_GraphicsPipeline.get());
			m_CommandBuffers[i].get().draw(3, 1, 0, 0);

		m_CommandBuffers[i].get().endRenderPass();

		m_CommandBuffers[i].get().end();

	}
}

void Renderer::CreateSyncObjects()
{
	m_vImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_vRenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
	m_vInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
	m_vImagesInFlight.resize(m_SwapchainImages.size());

	vk::SemaphoreCreateInfo semaphoreInfo{};
	vk::FenceCreateInfo fenceInfo = vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled);

	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
	{
		m_vImageAvailableSemaphores[i] = m_Device.get().createSemaphoreUnique(semaphoreInfo);
		m_vRenderFinishedSemaphores[i] = m_Device.get().createSemaphoreUnique(semaphoreInfo);
		m_vInFlightFences[i] = m_Device.get().createFence(fenceInfo);
	}
}

void Renderer::DrawFrame()
{
	// Wait for in flight fences
	m_Device.get().waitForFences(m_vInFlightFences[m_nCurrentFrame], true, UINT64_MAX);

	// Acquire next image
	uint32_t nImageIndex = m_Device.get().acquireNextImageKHR(m_Swapchain.get(), UINT64_MAX, m_vImageAvailableSemaphores[m_nCurrentFrame].get(), vk::Fence{});
	
	// Check if a previous frame is using this image
	if (m_vImagesInFlight[nImageIndex] != vk::Fence{})
		m_Device.get().waitForFences(m_vImagesInFlight[nImageIndex], true, UINT64_MAX);
	// Mark the iamge as now being in use by this frame
	m_vImagesInFlight[nImageIndex] = m_vInFlightFences[m_nCurrentFrame];

	// Submit info
	vk::SubmitInfo submitInfo{};

	// Semaphores
	vk::Semaphore waitSemaphores[] = { m_vImageAvailableSemaphores[m_nCurrentFrame].get() };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput }; //
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_CommandBuffers[nImageIndex].get();

	// More sempahores
	vk::Semaphore signalSemaphores[] = { m_vRenderFinishedSemaphores[m_nCurrentFrame].get() };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	// Reset fences
	m_Device.get().resetFences(m_vInFlightFences[m_nCurrentFrame]);

	// Submit commands
	m_GraphicsQueue.submit(submitInfo, m_vInFlightFences[m_nCurrentFrame]);

	vk::PresentInfoKHR presentInfo{};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	vk::SwapchainKHR swapChains[] = { m_Swapchain.get() };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;
	presentInfo.pImageIndices = &nImageIndex;

	m_PresentQueue.presentKHR(presentInfo);

	m_Window.Update();
	m_nCurrentFrame = (m_nCurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::WaitIdle()
{
	m_Device.get().waitIdle();
}

Renderer::~Renderer()
{
	for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) m_Device.get().destroyFence(m_vInFlightFences[i]);

	if (bEnableValidationLayers) DestroyDebugUtilsMessangerEXT(m_Instance.get(), m_DebugMessenger, nullptr);
}
