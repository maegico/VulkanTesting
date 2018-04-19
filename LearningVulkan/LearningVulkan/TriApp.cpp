#define NOMINMAX
#include <Windows.h>

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE	//force depth range of glm from [-1, 1] to [0, 1]
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "std_image.h"

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>
#include <set>
#include <algorithm>
#include <fstream>
#include <array>
#include <chrono>

#define THROW(x) { throw std::runtime_error(x); }

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static VkVertexInputBindingDescription getBindingDescription()
	{
		//describes the rate to load data from memory throughout the vertices
		//specifies number of bytes between data entries
		//specifies whether to move to the next data entry after each vertex or each instance
		VkVertexInputBindingDescription bindingDescription = {};
		bindingDescription.binding = 0;
		bindingDescription.stride = sizeof(Vertex);
		//inputRate can be one of 2 things:
			//VK_VERTEX_INPUT_RATE_VERTEX - move to the next data entry after each vertex
			//VK_VERTEX_INPUT_RATE_INSTANCE - move to the next data entry after each instance
		bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);

		attributeDescriptions[2].binding = 0;
		attributeDescriptions[2].location = 2;
		attributeDescriptions[2].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[2].offset = offsetof(Vertex, texCoord);

		return attributeDescriptions;
	}
};

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
	VkDescriptorSetLayout descriptorSetLayout;
	VkPipelineLayout pipelineLayout;
	VkPipeline graphicsPipeline;
	std::vector<VkFramebuffer> swapChainFramebuffers;
	VkCommandPool commandPool;
	std::vector<VkCommandBuffer> commandBuffers;
	VkSemaphore imageAvailableSemaphore;
	VkSemaphore renderFinishedSemaphore;
	VkBuffer vertexBuffer;
	VkDeviceMemory vertexBufferMemory;
	VkBuffer indexBuffer;
	VkDeviceMemory indexBufferMemory;
	VkBuffer uniformBuffer;
	VkDeviceMemory uniformBufferMemory;
	VkDescriptorPool descriptorPool;
	VkDescriptorSet descriptorSet;
	VkImage textureImage;
	VkImageView textureImageView;
	VkSampler textureSampler;
	VkDeviceMemory textureImageMemory;
	VkImage depthImage;
	VkDeviceMemory depthImageMemory;
	VkImageView depthImageView;
	
	const std::vector<Vertex> vertices = {
		{ { -0.5f, -0.5f, 0.0f },{ 1.0f, 0.0f, 0.0f },{ 0.0f, 0.0f } },
		{ { 0.5f, -0.5f, 0.0f },{ 0.0f, 1.0f, 0.0f },{ 1.0f, 0.0f } },
		{ { 0.5f, 0.5f, 0.0f },{ 0.0f, 0.0f, 1.0f },{ 1.0f, 1.0f } },
		{ { -0.5f, 0.5f, 0.0f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 1.0f } },

		{ { -0.5f, -0.5f, -0.5f },{ 1.0f, 0.0f, 0.0f },{ 0.0f, 0.0f } },
		{ { 0.5f, -0.5f, -0.5f },{ 0.0f, 1.0f, 0.0f },{ 1.0f, 0.0f } },
		{ { 0.5f, 0.5f, -0.5f },{ 0.0f, 0.0f, 1.0f },{ 1.0f, 1.0f } },
		{ { -0.5f, 0.5f, -0.5f },{ 1.0f, 1.0f, 1.0f },{ 0.0f, 1.0f } }
	};

	const std::vector<uint16_t> indices = {
		0, 1, 2, 2, 3, 0,
		4, 5, 6, 6, 7, 4
	};

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

	struct UniformBufferObject
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
	};

#pragma region Primary functions

	void initWindow()
	{
		glfwInit();

		//don't create OpenGL context
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		//disable window resizing
		//glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

		//allows us to store an arbitrary pointer in the window object
		glfwSetWindowUserPointer(window, this);
		glfwSetWindowSizeCallback(window, TriApp::onWindowResized);
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
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createCommandPool();
		createDepthResources();
		createFramebuffers();
		createTextureImage();
		createTextureImageView();
		createTextureSampler();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffer();
		createDescriptorPool();
		createDescriptorSet();
		createCommandBuffers();
		createSemaphores();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();

			updateUniformBuffer();
			drawFrame();
		}

		//idles the program until drawing is done and the semaphores are released
		vkDeviceWaitIdle(device);
	}

	void cleanup()
	{
		cleanupSwapChain();

		vkDestroySampler(device, textureSampler, nullptr);
		vkDestroyImageView(device, textureImageView, nullptr);
		vkDestroyImage(device, textureImage, nullptr);
		vkFreeMemory(device, textureImageMemory, nullptr);

		vkDestroyDescriptorPool(device, descriptorPool, nullptr);

		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
		vkDestroyBuffer(device, uniformBuffer, nullptr);
		vkFreeMemory(device, uniformBufferMemory, nullptr);

		vkDestroyBuffer(device, indexBuffer, nullptr);
		vkFreeMemory(device, indexBufferMemory, nullptr);

		vkDestroyBuffer(device, vertexBuffer, nullptr);
		vkFreeMemory(device, vertexBufferMemory, nullptr);

		vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
		vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);

		vkDestroyCommandPool(device, commandPool, nullptr);

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

	static void onWindowResized(GLFWwindow* window, int width, int height)
	{
		//since recreateswapchain is not a static function we need to get an instance of this class to call it
		TriApp* app = reinterpret_cast<TriApp*>(glfwGetWindowUserPointer(window));
		app->recreateSwapChain();
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
		//so, for the staging queue you need a queue family that supports transfer ops
			//any queue family that supports graphics or comput already implicitly support it
		//we won't make a new queue, but if you want to
		/*
		Modify QueueFamilyIndices and findQueueFamilies to explicitly look for a queue family with the VK_QUEUE_TRANSFER bit, but not the VK_QUEUE_GRAPHICS_BIT.
		Modify createLogicalDevice to request a handle to the transfer queue
		Create a second command pool for command buffers that are submitted on the transfer queue family
		Change the sharingMode of resources to be VK_SHARING_MODE_CONCURRENT and specify both the graphics and transfer queue families
		Submit any transfer commands like vkCmdCopyBuffer (which we'll be using in this chapter) to the transfer queue instead of the graphics queue
		*/

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
		VkPhysicalDeviceFeatures supportedFeatures;
		QueueFamilyIndices indices = findQueueFamilies(device);
		bool extensionsSupported = checkDeviceExtensionSupport(device);
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

		//after these you can determine which graphics card to use
		//for now I will force it to use a dedicated GPU
		//any GPU is fine, but I want to use my dedicate GPU

		bool swapChainAdequate = false;
		if (extensionsSupported)
		{
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}

		return indices.isComplete() && extensionsSupported
			&& swapChainAdequate && supportedFeatures.samplerAnisotropy;
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
		deviceFeatures.samplerAnisotropy = VK_TRUE;

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

	void cleanupSwapChain()
	{
		vkDestroyImageView(device, depthImageView, nullptr);
		vkDestroyImage(device, depthImage, nullptr);
		vkFreeMemory(device, depthImageMemory, nullptr);

		for (auto framebuffer : swapChainFramebuffers)
		{
			vkDestroyFramebuffer(device, framebuffer, nullptr);
		}

		//instead of recreating the command pool, they are just cleaning up the existing command buffers
		//this way we just use the existing pool to allocate the new buffers
		vkFreeCommandBuffers(device, commandPool, static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());

		vkDestroyPipeline(device, graphicsPipeline, nullptr);
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
		vkDestroyRenderPass(device, renderPass, nullptr);

		//since we create the image views we have to destroy them
		for (auto imageView : swapChainImageViews)
		{
			vkDestroyImageView(device, imageView, nullptr);
		}

		vkDestroySwapchainKHR(device, swapChain, nullptr);
	}

	void recreateSwapChain()
	{
		int width, height;
		glfwGetWindowSize(window, &width, &height);
		if (width == 0 || height == 0) return;

		//wait until all resources are free
		vkDeviceWaitIdle(device);

		cleanupSwapChain();

		createSwapChain();
		createImageViews();
		createRenderPass();
		//we could avoid recreating the pipeline by using dynamic state for viewports and scissor rects
		createGraphicsPipeline();
		createDepthResources();
		createFramebuffers();
		createCommandBuffers();
	}

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
			int width, height;
			glfwGetWindowSize(window, &width, &height);

			VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };
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
			swapChainImageViews[i] = createImageView(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT);
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

		VkAttachmentDescription depthAttachment = {};
		depthAttachment.format = findDepthFormat();
		depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depthAttachmentRef = {};
		depthAttachmentRef.attachment = 1;
		depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		//Subpasses reference one or more attachments
		//the below is the reference
		VkAttachmentReference colorAttachmentRef = {};
		//index into attachment descriptions array
		colorAttachmentRef.attachment = 0;
		//specifies which layout we want the attachment to have during a subpass
		colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		//subpass can only use a single depth(+stencil) attachment
		VkSubpassDescription subpass = {};
		//since Vulkan may support comput subpasses in the future,
		//we have to be explicit about this being a graphics subpass
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &colorAttachmentRef;
		subpass.pDepthStencilAttachment = &depthAttachmentRef;
		//the index of the attachment in this array is directly referenced from the fragment shader
		//by "layout(location = 0) out vec4 outColor" directive

		//subpass dependencies
		//subpasses in a render pass automatically take care of image layout transitions
		//transitions are controlled by subpass dependencies
		VkSubpassDependency dependency = {};
		//srcSubpass refers to the dependent subpass
		//VK_SUBPASS_EXTERNAL refers to the implicit subpass before or after the render pass
		//depends on whether it is srcSubpass or dstSubpass
		//dstSubpass mush always be higher than srcSubpass
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;

		//access mask is the op to wait on
		//stage mask is the stages in which these operations occur
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.srcAccessMask = 0;
		//wait on either reading or writing the color attachment in the color attachment stage
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		std::array<VkAttachmentDescription, 2> attachments = { colorAttachment, depthAttachment };
		VkRenderPassCreateInfo renderPassInfo = {};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
		renderPassInfo.pAttachments = attachments.data();
		renderPassInfo.subpassCount = 1;
		renderPassInfo.pSubpasses = &subpass;
		renderPassInfo.dependencyCount = 1;
		renderPassInfo.pDependencies = &dependency;

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

		auto bindingDescription = Vertex::getBindingDescription();
		auto attributeDescription = Vertex::getAttributeDescriptions();

		//describes format of vertex data that will be passed to the vertex shader
			//can do this in 2 ways:
				//Bindings - spacing between data and whether the data is per-vertex or per-instance
				//Attribute descriptions - type of the attributes passed to the vertex shader, which binding to load them from and at which offset
		//for now we will specify that there is not vertex data to load
		VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
		vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInputInfo.vertexBindingDescriptionCount = 1;
		vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
		vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
		vertexInputInfo.pVertexAttributeDescriptions = attributeDescription.data();

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
		//editted frontFace, since the y-flip in the proj matrix the matrices are being drawn in clockwise order
			//this causing backface culling to kick in, preventing any geometry from being drawn
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;	//the order that triangles are created
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

		VkPipelineDepthStencilStateCreateInfo depthStencil = {};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		//whether or not new fragments should be compared to the depth buffer to see if they should be discarded
		depthStencil.depthTestEnable = VK_TRUE;
		//specifies if the new depth of fragments that pass the depth test should be written to the depth buffer
			//good for drawing transparent objects,
			//since they should be compared to prev rendered opaque objects, but not cause further away transparent objects to not be drawn
		depthStencil.depthWriteEnable = VK_TRUE;
		//specifies the comparison performed to keep or discard fragments
			//doing lower depth = closer
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
		//below is used for optional depth bound test
			//allows us to only keep fragments falling within specified depth range
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.minDepthBounds = 0.0f;	//Optional
		depthStencil.maxDepthBounds = 1.0f;	//Optional
		//rest are for a stencil component

		//you need to specify uniform values during pipeline creation
		VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
		pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutInfo.setLayoutCount = 1;
		pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
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
		pipelineInfo.pDepthStencilState = &depthStencil;
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
			std::array<VkImageView, 2> attachments = { swapChainImageViews[i], depthImageView };

			VkFramebufferCreateInfo framebufferInfo = {};
			framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebufferInfo.renderPass = renderPass;
			framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
			framebufferInfo.pAttachments = attachments.data();
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
			std::array<VkClearValue, 2> clearValues = {};
			clearValues[0].color = { 0.0f, 0.0f, 0.0f, 1.0f };
			clearValues[1].depthStencil = { 1.0f, 0 };

			renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
			renderPassInfo.pClearValues = clearValues.data();

			//all functions that record commands can be recognized by "vkCmd" prefix
			//the final parameter controls how the drawing commands within the render pass will be provided
				//VK_SUBPASS_CONTENTS_INLINE - render pass commands will be embedded in the primary command buffer itself, no secondary buffers will be executed
				//VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS - render pass commands will be executed from secondary command buffers
			vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

			//bind the graphics pipeline
			//second parameter tells whether the pipeline is graphics or compute
			vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

			VkBuffer vertexBuffers[] = { vertexBuffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);

			vkCmdBindIndexBuffer(commandBuffers[i], indexBuffer, 0, VK_INDEX_TYPE_UINT16);

			//not unique to graphics pipelines, so we need to specify
			vkCmdBindDescriptorSets(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

			//the fourth parameter is the offset into the vertex buffer
				//defines lowest value of Gl_VertexIndex
			//the last one is the offset for instanced rendering
				//defines lowest value of gl_InstanceIndex
			//vkCmdDraw(commandBuffers[i], static_cast<uint32_t>(vertices.size()), 1, 0, 0);

			//now using indices
			//not using instancing so we say only 1 instance
			vkCmdDrawIndexed(commandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

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
		VkResult result = vkAcquireNextImageKHR(device, swapChain, std::numeric_limits<uint64_t>::max(), imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

		//if the swap chain is out of date, recreate it and try again next frame
		//we are ignoring the suboptimal case
		if (result == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
		{
			THROW("failed to acquire swap chain image!")
		}

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

		//present the images
			//submitting the result to the swap chain to have it eventually show up on the screen

		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = signalSemaphores;
		VkSwapchainKHR swapChains[] = { swapChain };
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;	//Optional: allows you to specify an array of VkResult values
			//good for checking every individual swap chain if presentation is successful

		//submits the request to present an image to the swap chain
		result = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
		{
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS)
		{
			THROW("failed to present swap chain image!")
		}

		if (enableValidationLayers)
		{
			//wait for presentation to finish before starting to draw the next frame
			vkQueueWaitIdle(presentQueue);
		}
	}

#pragma region Buffer Functions

	void createVertexBuffer()
	{
		VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		//src means the buffer can be used as source in a mem transfer op
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		//copy vertex data to the staging buffer
			//done by mapping the buffer memory into CPU accessible memory with vkMapMemory
		void* data;
		//acces a region of the specified mem resource defined by an offset and size
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		//now we just copy the memory over, but the driver may not immediately do this (could be because of caching, etc.)
			//two ways of handling it:
				//use a mem heap that is host coherent (we use this one)
				//call vkFlushMappedMemoryRanges after writing to the mapped memory
					//then call vkInvalidateMappedMemoryRanges before reading from the mapped memory
		memcpy(data, vertices.data(), (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		//dst means buffer can be used as destination in a mem transfer op
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vertexBuffer, vertexBufferMemory);

		//we can't use vkMapMemory since the memory is device local
		//so we use this function
		copyBuffer(stagingBuffer, vertexBuffer, bufferSize);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	void createIndexBuffer()
	{
		VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, indices.data(), (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, indexBuffer, indexBufferMemory);

		copyBuffer(stagingBuffer, indexBuffer, bufferSize);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	void createUniformBuffer()
	{
		VkDeviceSize bufferSize = sizeof(UniformBufferObject);
		createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			uniformBuffer, uniformBufferMemory);
	}

	uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		//the struct has two arrays memoryTypes and memoryHeaps
			//Memory heaps are distinct memory resources like dedicated VRAM and swap space in RAM for when VRAM runs out
			//Memory types consist of an array of VkMemoryType structs that specify heap and properties of each type
		VkPhysicalDeviceMemoryProperties memProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			//typefilter acts a bit field of memory types that are suitable
			//the below acts as a mask to only look at the one bit
				//if that bit is on the if statement should return true
			//for memProperties we are checking the properties of the mem against the passed in properties bit field
			if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		THROW("failed to find suitable memory type!")
	}

	void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
		VkBuffer& buffer, VkDeviceMemory& bufferMemory)
	{
		VkBufferCreateInfo bufferInfo = {};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		//tell it what type of buffer is being created
		bufferInfo.usage = usage;
		//just like images in the swap chain, buffers can also be owned by a specific queue family
			//can also be shared by multiple queue families
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

		if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
		{
			THROW("failed to create buffer!")
		}

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		//in a real world app, shouldn't call this for every individual buffer
			//there is a max that the physical device labels as maxMemoryAllocationCount
		//should use a custom allocator that splits up a single allocation among many different objects by using the offset parameters
			//or you could use the VulkanMemoryAllocator library\
				//https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
		if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
		{
			THROW("failed to allocate buffer memory!")
		}

		//fourth param is offset within the region of memory
			//if it is non-zero it must be divisible by memRequirements.alignment
		vkBindBufferMemory(device, buffer, bufferMemory, 0);
	}

	void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		VkBufferCopy copyRegion = {};
		copyRegion.srcOffset = 0;	//Optional
		copyRegion.dstOffset = 0;	//Optional
		copyRegion.size = size;
		
		//below takes an array of regions to copy(regions are made of VkBufferCopy structs)
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
		
		endSingleTimeCommands(commandBuffer);
	}

	VkCommandBuffer beginSingleTimeCommands()
	{
		//Mem transfer ops are executed using command buffers, just like drawing commands
			//So first allocate a temp command buffer
				//we could create a separate command pool for these temporary short-lived buffers
				//should use VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag for this case
		VkCommandBufferAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		allocInfo.commandPool = commandPool;
		//level specifies if the allocate buffers are primary or secondary buffers
			//primary can be submitted to a queue for execution. but can't be called from other command buffers
			//secondary can't be submitted directly, but can be called from primary command buffers
				//apparently can be used to reuse common ops from primary buffers??
		allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		allocInfo.commandBufferCount = 1;

		VkCommandBuffer commandBuffer;
		if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS)
		{
			THROW("failed to allocate single command buffer!")
		}

		//start recording the command buffer
		VkCommandBufferBeginInfo beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		//says we will only use the buffer once
			//good practice to tell the drive about our intent using one_time_submit_bit
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		return commandBuffer;
	}

	void endSingleTimeCommands(VkCommandBuffer commandBuffer)
	{
		vkEndCommandBuffer(commandBuffer);

		VkSubmitInfo submitInfo = {};
		submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &commandBuffer;

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		//we want the transfer on buffers immediately, so there are 2 ways
			//use a fence and wait with vkWaitForFences()
				//would allow you to schedule multiple transfers simultaneously and wait for them all to complete
			//or wait for transfer queue to become idle with vkQueueWaitIdle()
		vkQueueWaitIdle(graphicsQueue);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}

	void createDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding uboLayoutBinding = {};
		//binding specifies the binding variable in the shader
		uboLayoutBinding.binding = 0;
		uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1;
		//below specifies in which shader stage this is going to be used
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		//only for image sampling related descriptors
		uboLayoutBinding.pImmutableSamplers = nullptr;	//Optional

		VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
		samplerLayoutBinding.binding = 1;
		samplerLayoutBinding.descriptorCount = 1;
		samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		samplerLayoutBinding.pImmutableSamplers = nullptr;
		samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
		VkDescriptorSetLayoutCreateInfo layoutInfo = {};
		layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
		layoutInfo.pBindings = bindings.data();

		if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
		{
			THROW("failed to create descriptor set layout!")
		}
	}

	void updateUniformBuffer()
	{
		//the most efficient way to pass frequently changing values to the shader is push constants

		static auto startTime = std::chrono::high_resolution_clock::now();

		auto currentTime = std::chrono::high_resolution_clock::now();
		float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo = {};
		//glm::mat4(1.0f) gives the 4x4 identity matrix
		ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		//looking at the geometry from above at a 45 degree angle
			//takes eye position, center position, and up axis parameters
		ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
		//using 45 degree vertical field-of-view
		//then aspect ratio, then near and far view planes
		ubo.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f, 10.0f);
		//since glm was made for OpenGl where the Y coordinate of the clip coordinates is inverted,
			//easiest way to compensat is to flip the sign on the scaling factor of the Y axis in the projection matrix
		ubo.proj[1][1] *= -1;

		void* data;
		vkMapMemory(device, uniformBufferMemory, 0, sizeof(ubo), 0, &data);
		memcpy(data, &ubo, sizeof(ubo));
		vkUnmapMemory(device, uniformBufferMemory);
	}

#pragma endregion

	void createDescriptorPool()
	{
		//need to describe which descriptor types our descriptor set are going to contain and how many
		std::array<VkDescriptorPoolSize, 2> poolSizes = {};
		poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		poolSizes[0].descriptorCount = 1;

		poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		poolSizes[1].descriptorCount = 1;

		VkDescriptorPoolCreateInfo poolInfo = {};
		poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
		poolInfo.pPoolSizes = poolSizes.data();
		poolInfo.maxSets = 1;

		if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
		{
			THROW("failed to create descriptor pool!")
		}
	}

	void createDescriptorSet()
	{
		//don't need to explicitly clean up descriptor sets,
			//because they are freed when the descriptor pool is destroyed

		//allocate the descriptor set
		VkDescriptorSetLayout layouts[] = { descriptorSetLayout };
		VkDescriptorSetAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = descriptorPool;
		allocInfo.descriptorSetCount = 1;
		allocInfo.pSetLayouts = layouts;

		if (vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet) != VK_SUCCESS)
		{
			THROW("failed to allocate descriptor set!")
		}

		//configure inner descriptors
		VkDescriptorBufferInfo bufferInfo = {};
		bufferInfo.buffer = uniformBuffer;
		bufferInfo.offset = 0;
		bufferInfo.range = sizeof(UniformBufferObject);

		VkDescriptorImageInfo imageInfo = {};
		imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		imageInfo.imageView = textureImageView;
		imageInfo.sampler = textureSampler;

		std::array<VkWriteDescriptorSet, 2> descriptorWrites = {};
		descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[0].dstSet = descriptorSet;
		descriptorWrites[0].dstBinding = 0;	//binding index in shader
		descriptorWrites[0].dstArrayElement = 0;	//first index in the array to update
		descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptorWrites[0].descriptorCount = 1;
		descriptorWrites[0].pBufferInfo = &bufferInfo;	//used for descriptors with buffer data
		descriptorWrites[0].pImageInfo = nullptr;	//Optional: used for descriptors using image data
		descriptorWrites[0].pTexelBufferView = nullptr;	//Optional: used for descriptors using buffer views

		descriptorWrites[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptorWrites[1].dstSet = descriptorSet;
		descriptorWrites[1].dstBinding = 1;	//binding index in shader
		descriptorWrites[1].dstArrayElement = 0;	//first index in the array to update
		descriptorWrites[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptorWrites[1].descriptorCount = 1;
		descriptorWrites[1].pImageInfo = &imageInfo;

		//can take a VkWriteDescriptorSet or VkCopyDescriptorSet
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
	}

#pragma region Texture Functions

	void createTextureImage()
	{
		int texWidth, texHeight, texChannels;
		stbi_uc* pixels = stbi_load("textures/texture.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		//pixels are laid out row by row with 4 bytes per pixel with STBI_rgb_alpha
		VkDeviceSize imageSize = texWidth * texHeight * 4;

		if (!pixels)
		{
			THROW("failed to load texture image")
		}

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;

		createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			stagingBuffer, stagingBufferMemory);

		void* data;
		vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
		memcpy(data, pixels, static_cast<size_t>(imageSize));
		vkUnmapMemory(device, stagingBufferMemory);

		stbi_image_free(pixels);

		createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

		transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		//image was created with VK_IMAGE_LAYOUT_UNDEFINED layout, so we use that as the old layout
		copyBufferToImage(stagingBuffer, textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

		//To start sampling from the texture image in the shader
			//transition to prepare it for shader access
		transitionImageLayout(textureImage, VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		vkDestroyBuffer(device, stagingBuffer, nullptr);
		vkFreeMemory(device, stagingBufferMemory, nullptr);
	}

	void createTextureImageView()
	{
		textureImageView = createImageView(textureImage, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_ASPECT_COLOR_BIT);
	}

	void createTextureSampler()
	{
		VkSamplerCreateInfo samplerInfo = {};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		//how to interpolate pixels that are magnified or minified
			//magnified matters for oversampling
			//minification matters for undersampling
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		//addressing mode can be specified per axis using addressMode fields
			//we currently say repeat texture when going beyond the image dimensions
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		//whether to use anisotropy or not
			//should use unless performance is a concern
		//maxAnisotropy field limits amount of texel samples used to calculate final color
			//16 is close to the highest you would use
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = 16;
		//the color returned when sampling beyond image with clamp to border addressing mode
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		//specifies which coordinate system to use for addressing texels in an image
			//true makes ranges of uv coordinates: [0, texWidth) and [0, texHeight)
			//false makes ranges of uv coordinates:[0,1)
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		//if a comparison function is enabled, then texels will first be compared to a value
			//the result of that comparison is used in filtering operations
			//mainly used for percentage-closer filtering on shadow maps
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		//All of these fields apply to mipmapping
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		if (vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS)
		{
			THROW("failed to create texture sampler!")
		}
	}

	void createImage(uint32_t width, uint32_t height, VkFormat format,
		VkImageTiling tiling, VkImageUsageFlags usage,
		VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory)
	{
		VkImageCreateInfo imageInfo = {};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = width;
		imageInfo.extent.height = height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		//should use same format for the texels as the pixels in the buffer
		//otherwise the copy operation will fail
		imageInfo.format = format;
		//tiling mode can't be changed at a later time
		//since we are using a staging buffer, we can't use tiling_linear
		imageInfo.tiling = tiling;
		//images can be one of two possible values:
		//layout_undefined - not usable by GPU and first transition will discard texels
		//layout_preinitialized - not usable by GPU and first transition will preserve texels
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = usage;
		//will only be used by one queue family
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		//only used for images utilized as attachments
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0;	//Optional

		if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS)
		{
			THROW("failed to create image!")
		}

		VkMemoryRequirements memRequirements;
		vkGetImageMemoryRequirements(device, image, &memRequirements);

		VkMemoryAllocateInfo allocInfo = {};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size;
		allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

		if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS)
		{
			THROW("failed to allocate image memory!")
		}

		vkBindImageMemory(device, image, imageMemory, 0);
	}

	void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		//common way to perform layout transitions is using an image memory barrier
			//pipeline barrier generally used to synchronize access to resources
			//can be used to transition image layouts and transfer queue family ownership when VK_SHARING_MODE_EXCLUSIVE is used
		VkImageMemoryBarrier barrier = {};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		//these two specify layout transition
			//if you don't care about the existing contents you can use VK_IMAGE_LAYOUT_UNDEFINED
		barrier.oldLayout = oldLayout;
		barrier.newLayout = newLayout;
		//if using a barrier to transfer queue family ownership, then these should be the indicies of the queue families
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		//specifies the image that is affected and the specific part of the image
		barrier.image = image;

		if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

			if (hasStencilComponent(format))
			{
				barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
			}
		}
		else
		{
			barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		}

		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = 1;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.srcAccessMask = 0;	//TODO
		barrier.dstAccessMask = 0;	//TODO

		VkPipelineStageFlags sourceStage;
		VkPipelineStageFlags destinationStage;

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
			newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			//since we aren't waiting on anything
				//we can have a NULL access mask
				//earliest stage to access is the top of the pipeline
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
			newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
			newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
				VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else
		{
			throw std::invalid_argument("unsupported layout transition!");
		}

		//since barriers are synchronization primitives
		//must specify which types of operations involing the resource happen before and after the barrier
			//the right values depend on the old and new layout
		vkCmdPipelineBarrier(commandBuffer,
			sourceStage, destinationStage,
			0, 0, nullptr, 0, nullptr, 1, &barrier);

		endSingleTimeCommands(commandBuffer);
	}

	void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height)
	{
		VkCommandBuffer commandBuffer = beginSingleTimeCommands();

		//specify which part of the buffer is going to be copied to which part of the image
		VkBufferImageCopy region = {};
		region.bufferOffset = 0;
		//two below specify how the image is laid out in memory
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		//below indicate which part of the image to copy
		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0,0,0 };
		region.imageExtent = { width, height,1 };

		vkCmdCopyBufferToImage(commandBuffer, buffer, image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		endSingleTimeCommands(commandBuffer);
	}

	VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags)
	{
		VkImageViewCreateInfo viewInfo = {};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;

		//subresourceRange describes the image's purpose and what part should be accessed
		//if you were doing stereoscopic 3d then your swap chain would have multiple layers and have multiple image views (one for each eye)
		viewInfo.subresourceRange.aspectMask = aspectFlags;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		VkImageView imageView;
		if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS)
		{
			THROW("failed to create texture image view!");
		}

		return imageView;
	}

#pragma endregion

	void createDepthResources()
	{
		VkFormat depthFormat = findDepthFormat();

		createImage(swapChainExtent.width, swapChainExtent.height,
			depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depthImage, depthImageMemory);

		depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT);

		//we could do this in the render pass, but since we are only doing this once
			//we will just use a pipeline barrier
		transitionImageLayout(depthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
	}

	bool hasStencilComponent(VkFormat format)
	{
		return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
			format == VK_FORMAT_D24_UNORM_S8_UINT;
	}

	VkFormat findDepthFormat()
	{
		return findSupportedFormat(
		{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
		);
	}

	VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates,
		VkImageTiling tiling, VkFormatFeatureFlags features)
	{
		for (VkFormat format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features)
			{
				return format;
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features)
			{
				return format;
			}

			THROW("failed to find supported format!")
		}
	}

};

#pragma region Helpful Advice for Real Apps

/*
All of the helper functions that submit commands so far have been set up to execute synchronously
by waiting for the queue to become idle. For practical applications it is recommended to combine
these operations in a single command buffer and execute them asynchronously for higher throughput,
especially the transitions and copy in the createTextureImage function. Try to experiment
with this by creating a setupCommandBuffer that the helper functions record commands into,
and add a flushSetupCommands to execute the commands that have been recorded so far.
It's best to do this after the texture mapping works to check if the texture resources are still set up correctly.
*/

//In a game with the state changin every frame the most efficient way is:

/*
void drawFrame() {
	updateAppState();

	vkQueueWaitIdle(presentQueue);

	vkAcquireNextImageKHR(...)

	submitDrawCommands();

	vkQueuePresentKHR(presentQueue, &presentInfo);
}
//This allows you to update the state of the app while the previous frame is being rendered
*/

#pragma endregion

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