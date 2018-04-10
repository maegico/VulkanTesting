#define NOMINMAX
#include <Windows.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <set>
#include <algorithm>
#include <fstream>

#define THROW(x) { throw std::runtime_error(x); }

//we will be compiling glsl into SPIR-V with
	//glslangValidator.exe

class TriApp
{
public:
	const int WIDTH = 800;
	const int HEIGHT = 600;

	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

private:
	GLFWwindow* window;	//GLFW's window variable that has all the properties of a window
	VkInstance instance;	//the instance of vulkan
	//physical device will be auto destroyed when instance is destroyed
	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;	//the hardware GPU to be used
	VkDevice device;
	VkQueue graphicsQueue;	//the queue used for drawing the graphics
	VkQueue presentQueue;	//the queue used to present images
	VkDebugReportCallbackEXT callback;	//the callback function to access details about errors
	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;	//the swap chain
	std::vector<VkImage> swapChainImages;	//handles to the images in the swap chain
	//To use an image in the swap chain it needs a corresponding image view
	std::vector<VkImageView> swapChainImageViews;	//the image views for the images in the swap chain
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;
	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;

	//the validation layers we want enabled
	const std::vector<const char*> validationLayers = {
		"VK_LAYER_LUNARG_standard_validation"
	};
	//the device extensions that we need/want
	const std::vector<const char*> deviceExtensions = {
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

#ifdef NDEBUG
	const bool enableValidationLayers = false;
#else
	const bool enableValidationLayers = true;
#endif

	struct QueueFamilyIndices
	{
		int graphicsFamily = -1;
		int presentFamily = -1;

		bool isComplete()
		{
			return graphicsFamily >= 0 && presentFamily >= 0;
		}
	};

	struct SwapChainSupportDetails
	{
		VkSurfaceCapabilitiesKHR capabilities;
		std::vector<VkSurfaceFormatKHR> formats;
		std::vector<VkPresentModeKHR> presentModes;
	};

#pragma region Primary functions

	void initWindow()
	{
		glfwInit();

		//don't create OpenGL context
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		//disable window resizing
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
	}

	void initVulkan()
	{
		createInstance();
		setupDebugCallback();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createRenderPass();
		createGraphicsPipeline();
		createFramebuffers();
		createCommandPool();
		createCommandBuffers();
		createSemaphores();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			drawFrame();
		}
	}

	void cleanup()
	{
		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

		vkDestroyCommandPool(device, commandPool, nullptr);

		for (auto framebuffer : swapChainFramebuffers)
		{
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}

		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);

		//since we create the image views we have to destroy them
		for (auto imageView : swapChainImageViews)
		{
			vkDestroyImageView(device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);
		vkDestroyDevice(device, nullptr);

		if (enableValidationLayers)
		{
			DestroyDebugReportCallbackEXT(instance, callback, nullptr);
		}

		vkDestroySurfaceKHR(instance, surface, nullptr);

		vkDestroyInstance(instance, nullptr);

		glfwDestroyWindow(window);

		glfwTerminate();
	}

#pragma endregion

#pragma region Debug Callback Functions

	//debugCallback called four times on the same object
		//not sure why

	//the debug report flags can be any combo of:
		//info
		//warning
		//performance warning
		//error
		//debug
	//user data is to pass your own data to the callback
	static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
		VkDebugReportFlagsEXT flags,
		VkDebugReportObjectTypeEXT objType,
		uint64_t obj,
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* msg,
		void* userData)
	{
		//just prints the msg to the standard error buffer
		std::cerr << "validation layer: " << msg << std::endl;

		//returning true is normally only used to test validation layers
		return VK_FALSE;
	}

	void setupDebugCallback()
	{
		if (!enableValidationLayers) return;

		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		//flags are filtering which types of messages to see
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = debugCallback;
		
		//if we wanted to, we could pass a pointer to pUserData field
		//this is passed to the callback function's userData parameter
			//we can use this to pass a pointer to the TriApp class

		//normally you would pass this createInfo to vkCreateDebugReportCallbackEXT function,
			//but this is an extension function and is not automatically loaded
		//So, we manually load it with this function
		if (CreateDebugReportCallbackEXT(instance, &createInfo, nullptr, &callback) != VK_SUCCESS)
		{
			THROW("failed to set up debug callback!");
		}
	}

	VkResult CreateDebugReportCallbackEXT(VkInstance instance,
		const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugReportCallbackEXT* pCallback)
	{
		auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pCallback);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	void DestroyDebugReportCallbackEXT(VkInstance instance,
		VkDebugReportCallbackEXT callback,
		const VkAllocationCallbacks* pAllocator)
	{
		auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
		if (func != nullptr)
		{
			func(instance, callback, pAllocator);
		}
	}

#pragma endregion

	void createInstance()
	{
		if (enableValidationLayers && !checkValidationLayerSupport())
		{
			THROW("not all validation layers requested are supported!")
		}

		auto extensions = getRequiredExtensions();

		//check required extensions against available extensions
		if (!checkExtensionSupport(extensions))
		{
			THROW("not all required extensions supported!");
		}

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_1;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		createInfo.flags = 0;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
		{
			THROW("failed to create instance!");
		}
	}

#pragma region Validation/Extension Functions

	std::vector<const char*> getRequiredExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

		if (enableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}

		return extensions;
	}

	//Go through all available extensions and compare them against the required extensions
	bool checkExtensionSupport(std::vector<const char*> requiredExtensions)
	{
		uint32_t extCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
		std::vector<VkExtensionProperties> extensions(extCount);

		vkEnumerateInstanceExtensionProperties(nullptr, &extCount, extensions.data());

		std::cout << "available extensions: " << std::endl;
		for (const auto& extension : extensions)
		{
			std::cout << "\t" << extension.extensionName << std::endl;
		}

		std::cout << "required extensions: " << std::endl;
		for (const auto& requiredExtension : requiredExtensions)
		{
			std::cout << "\t" << requiredExtension << std::endl;
		}

		//compare the required extensions against available extensions
		uint32_t count = 0;
		for (const auto& requiredExtension : requiredExtensions)
		{
			for (const auto& extension : extensions)
			{
				if (strcmp(requiredExtension, extension.extensionName) == 0)
					count++;
			}
		}

		if (count == requiredExtensions.size())
			return true;

		return false;
	}

	bool checkValidationLayerSupport()
	{
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : validationLayers)
		{
			bool layerFound = false;

			for (const auto& layerProperties : availableLayers)
			{
				if (strcmp(layerName, layerProperties.layerName) == 0)
				{
					layerFound = true;
					break;
				}
			}

			if (!layerFound)
				return false;
		}

		return true;
	}

#pragma endregion

#pragma region Physical Device Functions

	//pick a graphics card to use
	//we can use more than one, but we won't
	void pickPhysicalDevice()
	{
		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			THROW("failed to find GPUs with Vulkan support!")
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				physicalDevice = device;
				break;
			}
		}

		if (physicalDevice == VK_NULL_HANDLE)
		{
			THROW("failed to find a suitable GPU!");
		}
	}

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
	{
		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}

			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (queueFamily.queueCount > 0 && presentSupport)
			{
				indices.presentFamily = i;
			}

			if (indices.isComplete())
			{
				break;
			}

			i++;
		}

		return indices;
	}

	bool isDeviceSuitable(VkPhysicalDevice device)
	{
		//properties are things like name, type, and supported vulkan version
		VkPhysicalDeviceProperties deviceProperties;
		//features are things texture compression, 64 bit floats, and multi viewport rendering(VR)
		VkPhysicalDeviceFeatures deviceFeatures;
		QueueFamilyIndices indices = findQueueFamilies(device);
		bool extensionsSupported = checkDeviceExtensionSupport(device);
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		//after these you can determine which graphics card to use
		//for now I will force it to use a dedicated GPU
		//any GPU is fine, but I want to use my dedicate GPU

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
			&& indices.isComplete() && extensionsSupported && swapChainAdequate;
	}

	bool checkDeviceExtensionSupport(VkPhysicalDevice device)
	{
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		std::vector<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

		for (const auto& extension : availableExtensions)
		{
			requiredExtensions.erase(extension.extensionName);
		}

		return requiredExtensions.empty();
	}

#pragma endregion

	void createLogicalDevice()
	{
		float queuePriority = 1.0f;
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<int> uniqueQueueFamilies = { indices.graphicsFamily, indices.presentFamily };

		for (int queueFamily : uniqueQueueFamilies)
		{
			VkDeviceQueueCreateInfo queueCreateInfo = {};
			queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queueCreateInfo.queueFamilyIndex = queueFamily;
			queueCreateInfo.queueCount = 1;
			//this priority allows us to influence scheduling of command buffer execution
			queueCreateInfo.pQueuePriorities = &queuePriority;
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
		createInfo.pQueueCreateInfos = queueCreateInfos.data();
		createInfo.pEnabledFeatures = &deviceFeatures;
		createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		createInfo.ppEnabledExtensionNames = deviceExtensions.data();
		if (enableValidationLayers)
		{
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
		{
			THROW("failed to create logical device!")
		}

		vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
		vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
	}

	void createSurface()
	{
		//how to do it manually
		//Also a lot of this isn't being found
		/*VkWin32SurfaceCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		createInfo.hwnd = glfwGetWin32Window(window);
		createInfo.hinstance = GetModuleHandle(nullptr);

		auto CreateWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR");

		if (!CreateWin32SurfaceKHR || CreateWin32SurfaceKHR(instance, &createInfo, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("failed to create window surface!");
		}*/

		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
		{
			THROW("failed to create window surface!")
		}
	}

#pragma region Swap Chain Functions

	void createSwapChain()
	{
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);
		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
		//maxImageCount == 0, it means there is no limit besides memory requirements
		if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
		{
			imageCount = swapChainSupport.capabilities.maxImageCount;
		}

		VkSwapchainCreateInfoKHR createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		createInfo.surface = surface;
		createInfo.minImageCount = imageCount;
		createInfo.imageFormat = surfaceFormat.format;
		createInfo.imageColorSpace = surfaceFormat.colorSpace;
		createInfo.imageExtent = extent;
		//imageArrayLayers says number of layers for each image
		//always 1 unless you are doing stereoscopic 3D
		createInfo.imageArrayLayers = 1;
		//imageUsage says what operations we are using the swap chain for
		//color attachment means we will render directly to them
		//for stuff like post-procession use VK_IMAGE_USAGE_TRANSFER_DST_BIT
		//for this you will use a memory operation to transfer the rendered image to a swap chain image
		createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
		uint32_t queueFamilyIndices[] = { (uint32_t)indices.graphicsFamily, (uint32_t)indices.presentFamily };
		//if the queueFamily being used for graphics isn't the same as the one used to present images
		//we need to handle interaction between multiple queues
		//in this case draw images in the swap chain from the graphics queue and then submit them on the presentation queue
		if (indices.graphicsFamily != indices.presentFamily)
		{
			//VK_SHARING_MODE_CONCURRENT means images can be used across multiple queue families without transfering ownership
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		}
		else
		{
			//VK_SHARING_MODE_EXCLUSIVE means an image is owned by one queue family at a time and ownership must be transfered
			//offers the best performance
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			createInfo.queueFamilyIndexCount = 0;	//optional
			createInfo.pQueueFamilyIndices = nullptr;	//optional
		}

		//we can specify that images in the swap chain can be transformed if it is supported
		//we can find supported transforms in capabilities.supportedTransforms
		createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

		//saying we don't want the alpha channel to be used for blending with other windows in the window system
		createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		createInfo.presentMode = presentMode;
		//if clipped == true that means we don't care about the color of obscured pixels
		createInfo.clipped = true;
		//oldSwapchain used when you need to recreate the swapchain
		createInfo.oldSwapchain = VK_NULL_HANDLE;

		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
		{
			THROW("failed to create swapchain!");
		}

		//retrieve handles to the images in the swap chain
		//we query again since the implementation is allowed to create more images
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;
	}

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
	{
		SwapChainSupportDetails details;

		//query basic surface capabilities
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		//query supported surface formats
		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
		if (formatCount != 0)
		{
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		//query supported presentation modes
		uint32_t presentCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, nullptr);
		if (presentCount != 0)
		{
			details.presentModes.resize(presentCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, details.presentModes.data());
		}

		return details;
	}

	//find the right surface format/color depth
	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
	{
		//A great describtion of the differences between RGB and sRGB
		//https://stackoverflow.com/questions/12524623/what-are-the-practical-differences-when-working-with-colors-in-a-linear-vs-a-no

		//check if we are free to choose any format
		if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED)
		{
			return{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
		}

		for (const auto& availableFormat : availableFormats)
		{
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				return availableFormat;
			}
		}

		//otherwise we could rank formats, but for now we will just use the first available format
		return availableFormats[0];
	}

	//look for the best present mode
	//this determines how the swap chain presents images/switches buffers
	VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> availablePresentModes)
	{
		//the only guaranteed available mode is VK_PRESENT_MODE_FIFO_KHR
		//so as a last resort we will return it
		VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;


		for (const auto& availablePresentMode : availablePresentModes)
		{
			//try to find triple buffering if possible
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				return availablePresentMode;
			}
			//but some drivers might not support VK_PRESENT_MODE_FIFO_KHR yet,
			//so query for immediate swapping
			else if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR)
			{
				bestMode = availablePresentMode;
			}
		}

		return bestMode;
	}

	//swap extent is the resolution of the swap chain images
	//need to understand this function better
	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
	{
		//not entirely sure about why we are doing this
		if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
		{
			return capabilities.currentExtent;
		}
		else
		{
			VkExtent2D actualExtent = { WIDTH, HEIGHT };
			//NOMINMAX makes it so the window.h macros min and max don't interfere with other versions
			actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
			actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

			return actualExtent;
		}
	}

#pragma endregion

	void createImageViews()
	{
		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); i++)
		{
			VkImageViewCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			createInfo.image = swapChainImages[i];
			createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			createInfo.format = swapChainImageFormat;
			//createInfo.components allows use to swizzle color channels
			createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

			//subresourceRange describes the image's purpose and what part should be accessed
			//if you were doing stereoscopic 3d then your swap chain would have multiple layers and have multiple image views (one for each eye)
			createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			createInfo.subresourceRange.baseMipLevel = 0;
			createInfo.subresourceRange.levelCount = 1;
			createInfo.subresourceRange.baseArrayLayer = 0;
			createInfo.subresourceRange.layerCount = 1;

			if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS)
			{
				THROW("failed to create image views!");
			}
		}
	}

#pragma region Graphics Pipeline Functions

	//create a render pass object to describe:
		//number of color and depth buffers
		//number of samples to use for each
		//how content should be handled throughout rendering ops
	void createRenderPass()
	{
		//will create a single color buffer attachment
		//represented by one of the images from the swap chain
		VkAttachmentDescription colorAttachment = {};
		colorAttachment.format = swapChainImageFormat;
		//no multisampling, so we will only use 1 sample
		colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		//loadOp determines what to do with attachment before rendering
		//currently we will clear the values to a constant at the start
		colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		//storeOp determines what to do with attachment after rendering
		//currently we are going to store into memory for reading later
		colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		//initialLayout specifies which layout image before the render pass begins
		//don't care what the previous layout the image was in
		//contents of image are not guaranteed to be preserved, but we are going to clear it anyway
		colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		//finalLayout specifies which layout image after the render pass ends
		//tells the image to be ready for presentation using the swap chain after rendering
		colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		//Subpasses reference one or more attachments
		//the below is the reference
		VkAttachmentReference colorAttachmentRef = {};
		//index into attachment descriptions array
		colorAttachmentRef.attachment = 0;
		//specifies which layout we want the attachment to have during a subpass
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass = {};
		//since Vulkan may support comput subpasses in the future,
		//we have to be explicit about this being a graphics subpass
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		//the index of the attachment in this array is directly referenced from the fragment shader
		//by "layout(location = 0) out vec4 outColor" directive

		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = 1;
		renderPassInfo.pAttachments = &colorAttachment;
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;

		if (vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
		{
			THROW("failed to create a render pass!")
		}
	}

	void createGraphicsPipeline()
	{
		//only needed during pipeline creation
		VkShaderModule vertShaderModule;
		VkShaderModule fragShaderModule;

		auto vertShaderCode = readFile("shaders/vert.spv");
		auto fragShaderCode = readFile("shaders/frag.spv");

		vertShaderModule = createShaderModule(vertShaderCode);
		fragShaderModule = createShaderModule(fragShaderCode);

		VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
		vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
		vertShaderStageInfo.module = vertShaderModule;
		vertShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
		fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragShaderStageInfo.module = fragShaderModule;
		fragShaderStageInfo.pName = "main";

		VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

		//describes format of vertex data that will be passed to the vertex shader
			//can do this in 2 ways:
				//Bindings - spacing between data and whether the data is per-vertex or per-instance
				//Attribute descriptions - type of the attributes passed to the vertex shader, which binding to load them from and at which offset
		//for now we will specify that there is not vertex data to load
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 0;
		vertexInputInfo.pVertexBindingDescriptions = nullptr;
		vertexInputInfo.vertexAttributeDescriptionCount = 0;
		vertexInputInfo.pVertexAttributeDescriptions = nullptr;

		//describes two things: what kind of geometry will be drawn and if primitive restart should be enabled
		VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		//describes the region of the framebuffer that the output will be rendered to
		VkViewport viewport = {};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = (float)swapChainExtent.width;
		viewport.height = (float)swapChainExtent.height;
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;

		//Scissor rectangles define in which regions pixels will actually be stored
			//any pixels outside the scissor rectangle will be discarded by the rasterizer
		VkRect2D scissor = {};
		scissor.offset = { 0,0 };
		scissor.extent = swapChainExtent;

		VkPipelineViewportStateCreateInfo viewportState = {};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.pViewports = &viewport;
		viewportState.scissorCount = 1;
		viewportState.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo rasterizer = {};
		rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterizer.depthClampEnable = VK_FALSE;	//whether to clamp things past the near and far planes to those planes
		rasterizer.rasterizerDiscardEnable = VK_FALSE;	//whether geometry should never pass through the rasterizer stage (basically disable output to framebuffer)
		rasterizer.polygonMode = VK_POLYGON_MODE_FILL;	//whether to do fill, line, or point mode
		rasterizer.lineWidth = 1.0f;	//thickness of lines in terms of # of fragments(any line wider than 1 requires a feature)
		rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;	//which faces to cull
		rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;	//the order that triangles are created
		rasterizer.depthBiasEnable = VK_FALSE;
		rasterizer.depthBiasConstantFactor = 0.0f;	//optional
		rasterizer.depthBiasClamp = 0.0f;	//optional
		rasterizer.depthBiasSlopeFactor = 0.0f;	//optional

		//a form of anti-aliasing
			//combines the fragment shader results of multiple polygons that rasterize to the same pixel
			//since it doesn't run the fragment shader multiple times if only one polygon maps to a pixel,
				//it is much less expensive than simply rendering a higher res and then downscaling
		//we won't use this for the tutorial
		VkPipelineMultisampleStateCreateInfo multisampling = {};
		multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisampling.sampleShadingEnable = VK_FALSE;
		multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisampling.minSampleShading = 1.0f;	//optional
		multisampling.pSampleMask = nullptr;	//optional
		multisampling.alphaToCoverageEnable = VK_FALSE;	//optional
		multisampling.alphaToOneEnable = VK_FALSE;	//optional

		//we might also need to deal with the depth or stencil buffer
		//don't have one right now, so we will ignore it

		//Two ways to colorblend:
			//1) mix the old and new value to produce the final color
			//2) combine the old and the new value using a bitwise op

		//contains configuration per attached framebuffer
			//typically used for alpha blending
		VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		colorBlendAttachment.blendEnable = VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;	//Optional
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;	//Optional
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;	//Optional
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;	//Optional
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;	//Optional
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;	//Optional

		/*	The above struct is best mimiced by the below code
		if (blendEnable) {
			finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
			finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
		} else {
			finalColor = newColor;
		}

		finalColor = finalColor & colorWriteMask;
		*/
		//above is sudo code for the struct which is used for color blending
		//Most common use is alpha blending in which case final color needs to be computed like:
		/*
		finalColor.rgb = newAlpha * newColor + (1 - newAlpha) * oldColor;
		finalColor.a = newAlpha.a;
		
		In terms of struct params:

		colorBlendAttachment.blendEnable = VK_TRUE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		*/

		//references the array of structures for all of the framebuffers
		//also allows the setting of blend constants that can be used as blend factors in the above calculations
			//if you want to use the bitwise combination methode set logicOpEnable to true
				//this will automatically disable the first method
		VkPipelineColorBlendStateCreateInfo colorBlending = {};
		colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlending.logicOpEnable = VK_FALSE;
		colorBlending.logicOp = VK_LOGIC_OP_COPY;	//Optional
		colorBlending.attachmentCount = 1;
		colorBlending.pAttachments = &colorBlendAttachment;
		colorBlending.blendConstants[0] = 0.0f;	//Optional
		colorBlending.blendConstants[1] = 0.0f;	//Optional
		colorBlending.blendConstants[2] = 0.0f;	//Optional
		colorBlending.blendConstants[3] = 0.0f;	//Optional

		//if we need to change something like size of the viewport need to create the below
		/*VkDynamicState dynamicStates[] = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_LINE_WIDTH
		};

		VkPipelineDynamicStateCreateInfo dynamicState = {};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;*/

		//you need to specify uniform values during pipeline creation
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 0;	//Optional
		pipelineLayoutInfo.pSetLayouts = nullptr;	//Optional
		pipelineLayoutInfo.pushConstantRangeCount = 0;	//Optional
		pipelineLayoutInfo.pPushConstantRanges = nullptr;	//Optional

		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
		{
			THROW("failed to create pipeline layout!")
		}

		VkGraphicsPipelineCreateInfo pipelineInfo = {};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = shaderStages;
		pipelineInfo.pVertexInputState = &vertexInputInfo;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterizer;
		pipelineInfo.pMultisampleState = &multisampling;
		pipelineInfo.pDepthStencilState = nullptr;	//Optional
		pipelineInfo.pColorBlendState = &colorBlending;
		pipelineInfo.pDynamicState = nullptr;	//Optional
		pipelineInfo.layout = pipelineLayout;
		pipelineInfo.renderPass = renderPass;
		pipelineInfo.subpass = 0;	//subpass index (for this graphics pipeline)
		//basPipeline variables allow us to create a new graphics pipeline by deriving from an existing pipeline
			//so you don't have to reset the whole pipeline
			//these are only used if VK_PIPELINE_CREATE_DERIVATIVE_BIT flag is set in the flags field
		pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
		pipelineInfo.basePipelineIndex = -1;

		//the below function can create multiple createinfo objects and create multiple VkPipeline objects in one call
		if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
		{
			THROW("failed to create graphics pipeline!")
		}

		//the modules need to be cleaned up, so we will do it at the end
		vkDestroyShaderModule(device, fragShaderModule, nullptr);
		vkDestroyShaderModule(device, vertShaderModule, nullptr);
	}

	static std::vector<char> readFile(const std::string& filename)
	{
		//ate says to read starting at the end and binary says to read it as a binary file
		//by starting at the end we can use the read position to determine size of the file
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			THROW("failed to open file!")
		}

		size_t fileSize = (size_t)file.tellg();
		std::vector<char> buffer(fileSize);
		file.seekg(0);	//seek to the beginning
		file.read(buffer.data(), fileSize);
		file.close();
		return buffer;
	}

	//take a buffer of bytecode and create a VkShaderModule from it
	//the bytecode pointer is a uint32_t not a char pointer
		//we need to perform an reinterpret_cast, but we must ensure the data satisfies the alignment requirements of uint32_t
		//we do this by storing it into an std::vector
	VkShaderModule createShaderModule(const std::vector<char>& code)
	{
		VkShaderModuleCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		VkShaderModule shaderModule;
		if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
		{
			THROW("failed to create shader module!")
		}

		return shaderModule;
	}

#pragma endregion

	//Set up the render pass to expect a single framebuffer with same format as the swap chain images
	//the attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer obj
		//A framebuffer obj references all VkImageView objects that represent attachments ?????Confused on this point?????
			//means there will only be a single one (color attachment)
			//the image we use depends on which image the swap chain returns when we retrieve one for presentation
				//So we have to create a framebuffer for all images in swap chain and choose the correct one at drawing time
	void createFramebuffers()
	{
		swapChainFramebuffers.resize(swapChainImageViews.size());

		for (size_t i = 0; i < swapChainImageViews.size(); i++)
		{
			VkImageView attachments[] = { swapChainImageViews[i] };

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = 1;
			framebufferInfo.pAttachments = attachments;
			framebufferInfo.width = swapChainExtent.width;
			framebufferInfo.height = swapChainExtent.height;
			framebufferInfo.layers = 1;

			if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
			{
				THROW("failed to create framebuffer!")
			}
		}
	}

	void createCommandPool()
	{
		//Command buffers are executed by submitting them on one of the device queues
		//Each command pool can only allocate command buffers that are submitted on a single type of queue
			//since we are recording commands for drawing we will use the graphics queue family

		QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		VkCommandPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
		poolInfo.flags = 0;

		if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
		{
			THROW("failed to create command pool!")
		}
	}

	void createCommandBuffers()
	{
		commandBuffers.resize(swapChainFramebuffers.size());

		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		//level specifies if the allocate buffers are primary or secondary buffers
			//primary can be submitted to a queue for execution. but can't be called from other command buffers
			//secondary can't be submitted directly, but can be called from primary command buffers
				//apparently can be used to reuse common ops from primary buffers??
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

		if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
		{
			THROW("failed to allocate command buffers")
		}

		//recording a command buffer
		for (size_t i = 0; i < commandBuffers.size(); i++)
		{
			VkCommandBufferBeginInfo beginInfo = {};
			beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			//simultaneous use means the buffer can be resubmitted while it is also already pending execution
				//we use simultaneous flag since we may already be scheduling the drawing commands for the next frame while the last frame isn't done
			beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
			beginInfo.pInheritanceInfo = nullptr;	//Optional

			//if the buffer was already recorded once, then a call to the below function will implicitly reset it
			vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

			VkRenderPassBeginInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			renderPassInfo.renderPass = renderPass;
			//we created a framebuffer for each swap chain image that specifies it as a color attachment
			renderPassInfo.framebuffer = swapChainFramebuffers[i];

			//The below define the size of the render area
			renderPassInfo.renderArea.offset = { 0,0 };
			renderPassInfo.renderArea.extent = swapChainExtent;

			//These are the clear values to use for VK_ATTACHMENT_LOAD_OP_CLEAR
				//we use this as the load operation for the color attachment
			VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
			renderPassInfo.clearValueCount = 1;
			renderPassInfo.pClearValues = &clearColor;

			//all functions that record commands can be recognized by "vkCmd" prefix
			//the final parameter controls how the drawing commands within the render pass will be provided
				//VK_SUBPASS_CONTENTS_INLINE - render pass commands will be embedded in the primary command buffer itself, no secondary buffers will be executed
				//VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS - render pass commands will be executed from secondary command buffers
			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			//bind the graphics pipeline
			//second parameter tells whether the pipeline is graphics or compute
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			//the fourth parameter is the offset into the vertex buffer
				//defines lowest value of Gl_VertexIndex
			//the last one is the offset for instanced rendering
				//defines lowest value of gl_InstanceIndex
			vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

			//end the render pass
			vkCmdEndRenderPass(commandBuffers[i]);

			if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS)
			{
				THROW("failed to record command buffer!")
			}
		}
	}

	void createSemaphores()
	{
		VkSemaphoreCreateInfo semaphoreInfo = {};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS
			|| vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) != VK_SUCCESS)
		{
			THROW("failed to create semaphores!")
		}
	}

	void drawFrame()
	{
		//will acquire an image from the swap chain
		//execute the command buffer with that image as attachment in the framebuffer
		//return the image to the swap chain for presentation

		//all the above operations are run asynchronously and will return before the operation is finished
		//can either use fences or semaphores to synchronize
			//fences are designed to sync rendering with app itself
			//semaphores are used to sync ops within or accross command queues

		//acquire image from swap chain
		uint32_t imageIndex;
		//third parameter specifies a timeout in nanoseconds for an image to become available
			//using max of 64bit uint disables this
		//the fourth and fifth params are for the semaphore and fence
			//we are only using a semaphore
		//final param the out variable to store index of the newly avaiable swap chain image
		vkAcquireNextImageKHR(device, swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

		//submit the command buffer
		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
		VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = waitSemaphores;
		submitInfo.pWaitDstStageMask = waitStages;	//what stage(s) of the pipeline to wait
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

		VkSemaphore signalSemaphores[] = { renderFinishedSemaphore };
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = signalSemaphores;

		//can take an array of VkSubmitInfo structs for when the workload is much larger
		if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS)
		{
			THROW("failed to submit draw command buffer!")
		}

		//s
	}
};

int main()
{
	TriApp app;
	bool result = EXIT_SUCCESS;

	try
	{
		app.run();
	}
	catch (const std::runtime_error& e)
	{
		std::cerr << e.what() << std::endl;
		result = EXIT_FAILURE;
	}

	getchar();
	return result;
}