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
	CreateGraphicsPipeline();
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
	m_Instance = vk::createInstanceUnique(createInfo, nullptr);
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

	m_Device = m_PhysicalDevice.createDeviceUnique(createInfo, nullptr);

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
	createInfo.clipped = VK_TRUE; // Just clip pixels outside the window, we don't need to sample them, and I do like a good bit of performance
	//createInfo.oldSwapchain = vk::SwapchainKHR(...); // Our swapchain can become invalid, if a window is resized for example, but what to do?...

	m_Swapchain = m_Device.get().createSwapchainKHRUnique(createInfo, nullptr);
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
	inputAssembly.primitiveRestartEnable = static_cast<vk::Bool32>(false);

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
	rasterizer.depthClampEnable = static_cast<vk::Bool32>(false); // If true, clamp fragments too near or far apart instead of discarding them
	rasterizer.rasterizerDiscardEnable = static_cast<vk::Bool32>(false); // Basically disables any output to the framebufer
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
	multisampling.sampleShadingEnable = static_cast<vk::Bool32>(false);
	multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
	multisampling.minSampleShading = 1.0f; // Optional
	multisampling.pSampleMask = nullptr;
	multisampling.alphaToCoverageEnable = static_cast<vk::Bool32>(false);
	multisampling.alphaToOneEnable = static_cast<vk::Bool32>(false);
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
	vk::UniqueShaderModule shaderModule = m_Device.get().createShaderModuleUnique(createInfo, nullptr);
	return vk::UniqueShaderModule();
}

void Renderer::PrepareFrame()
{
	m_Window.Update();
}

Renderer::~Renderer()
{
	if (bEnableValidationLayers) DestroyDebugUtilsMessangerEXT(m_Instance.get(), m_DebugMessenger, nullptr);
}
