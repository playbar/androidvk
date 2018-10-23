/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanMain.h"

std::vector<const char*> VulkanPipeLines::args;

VkResult VulkanPipeLines::createInstance(bool enableValidation)
{
	this->settings.validation = enableValidation;

	VkApplicationInfo appInfo = {};
	appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	appInfo.pApplicationName = name.c_str();
	appInfo.pEngineName = name.c_str();
	appInfo.apiVersion = VK_API_VERSION_1_0;

	std::vector<const char*> instanceExtensions = { VK_KHR_SURFACE_EXTENSION_NAME };

	// Enable surface extensions depending on os
#if defined(_WIN32)
	instanceExtensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__ANDROID__)
	instanceExtensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(_DIRECT2DISPLAY)
	instanceExtensions.push_back(VK_KHR_DISPLAY_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	instanceExtensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
	instanceExtensions.push_back(VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = NULL;
	instanceCreateInfo.pApplicationInfo = &appInfo;
	if (instanceExtensions.size() > 0)
	{
		if (settings.validation)
		{
			instanceExtensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}
		instanceCreateInfo.enabledExtensionCount = (uint32_t)instanceExtensions.size();
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	}
	if (settings.validation)
	{
		instanceCreateInfo.enabledLayerCount = vks::debug::validationLayerCount;
		instanceCreateInfo.ppEnabledLayerNames = vks::debug::validationLayerNames;
	}
	return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}

std::string VulkanPipeLines::getWindowTitle()
{
	std::string device(deviceProperties.deviceName);
	std::string windowTitle;
	windowTitle = title + " - " + device;
	if (!enableTextOverlay)
	{
		windowTitle += " - " ;
		windowTitle += frameCounter;
		windowTitle += " fps";
	}
	return windowTitle;
}

const std::string VulkanPipeLines::getAssetPath()
{
#if defined(__ANDROID__)
	return "";
#else
	return "./../data/";
#endif
}

bool VulkanPipeLines::checkCommandBuffers()
{
	for (auto& cmdBuffer : drawCmdBuffers)
	{
		if (cmdBuffer == VK_NULL_HANDLE)
		{
			return false;
		}
	}
	return true;
}

void VulkanPipeLines::createCommandBuffers()
{
	// Create one command buffer for each swap chain image and reuse for rendering
	drawCmdBuffers.resize(swapChain.imageCount);

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
            InitCommandBufferAllocateInfo(
                    cmdPool,
                    VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    static_cast<uint32_t>(drawCmdBuffers.size()));

	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));
}

void VulkanPipeLines::destroyCommandBuffers()
{
	vkFreeCommandBuffers(device, cmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

VkCommandBuffer VulkanPipeLines::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
            InitCommandBufferAllocateInfo(
                    cmdPool,
                    level,
                    1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

	// If requested, also start the new command buffer
	if (begin)
	{
		VkCommandBufferBeginInfo cmdBufInfo = InitCommandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	}

	return cmdBuffer;
}

void VulkanPipeLines::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
{
	if (commandBuffer == VK_NULL_HANDLE)
	{
		return;
	}
	
	VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	VK_CHECK_RESULT(vkQueueWaitIdle(queue));

	if (free)
	{
		vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
	}
}

void VulkanPipeLines::createPipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VulkanPipeLines::prepare()
{
	if (vulkanDevice->enableDebugMarkers)
	{
		vks::debugmarker::setup(device);
	}
	createCommandPool();
	setupSwapChain();
	createCommandBuffers();
	setupDepthStencil();
	setupRenderPass();
	createPipelineCache();
	setupFrameBuffer();

	if (enableTextOverlay)
	{
		// Load the text rendering shaders
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		shaderStages.push_back(loadShader(getAssetPath() + "shaders/base/textoverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		shaderStages.push_back(loadShader(getAssetPath() + "shaders/base/textoverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
		textOverlay = new VulkanTextOverlay(
			vulkanDevice,
			queue,
			frameBuffers,
			swapChain.colorFormat,
			depthFormat,
			&width,
			&height,
			shaderStages
			);
		updateTextOverlay();
	}

	loadAssets();
	prepareUniformBuffers();
	setupDescriptorSetLayout();
	preparePipelines();
	setupDescriptorPool();
	setupDescriptorSet();
	buildCommandBuffers();
	prepared = true;

}

VkPipelineShaderStageCreateInfo VulkanPipeLines::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
#if defined(__ANDROID__)
	shaderStage.module = VksLoadShader(androidApp->activity->assetManager,
												   fileName.c_str(), device, stage);
#else
	shaderStage.module = loadShader(fileName.c_str(), device, stage);
#endif
	shaderStage.pName = "main"; // todo : make param
	assert(shaderStage.module != VK_NULL_HANDLE);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}

void VulkanPipeLines::renderLoop()
{
	destWidth = width;
	destHeight = height;
#if defined(_WIN32)
	MSG msg;
	while (TRUE)
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		if (viewUpdated)
		{
			viewUpdated = false;
			viewChanged();
		}

		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		if (msg.message == WM_QUIT)
		{
			break;
		}

		render();
		frameCounter++;
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		frameTimer = (float)tDiff / 1000.0f;
		camera.update(frameTimer);
		if (camera.moving())
		{
			viewUpdated = true;
		}
		// Convert to clamped timer value
		if (!paused)
		{
			timer += timerSpeed * frameTimer;
			if (timer > 1.0)
			{
				timer -= 1.0f;
			}
		}
		fpsTimer += (float)tDiff;
		if (fpsTimer > 1000.0f)
		{
			if (!enableTextOverlay)
			{
				std::string windowTitle = getWindowTitle();
				SetWindowText(window, windowTitle.c_str());
			}
			lastFPS = static_cast<uint32_t>(1.0f / frameTimer);
			updateTextOverlay();
			fpsTimer = 0.0f;
			frameCounter = 0;
		}
	}
#elif defined(__ANDROID__)
	while (1)
	{
		int ident;
		int events;
		struct android_poll_source* source;
		bool destroy = false;

		focused = true;

		while ((ident = ALooper_pollAll(focused ? 0 : -1, NULL, &events, (void**)&source)) >= 0)
		{
			if (source != NULL)
			{
				source->process(androidApp, source);
			}
			if (androidApp->destroyRequested != 0)
			{
				LOGD("Android app destroy requested");
				destroy = true;
				break;
			}
		}

		// App destruction requested
		// Exit loop, example will be destroyed in application main
		if (destroy)
		{
			break;
		}

		// Render frame
		if (prepared)
		{
			auto tStart = std::chrono::high_resolution_clock::now();
			render();
			frameCounter++;
			auto tEnd = std::chrono::high_resolution_clock::now();
			auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
			frameTimer = tDiff / 1000.0f;
			camera.update(frameTimer);
			// Convert to clamped timer value
			if (!paused)
			{
				timer += timerSpeed * frameTimer;
				if (timer > 1.0)
				{
					timer -= 1.0f;
				}
			}
			fpsTimer += (float)tDiff;
			if (fpsTimer > 1000.0f)
			{
				lastFPS = frameCounter;
				updateTextOverlay();
				fpsTimer = 0.0f;
				frameCounter = 0;
			}

			bool updateView = false;

			// Check touch state (for movement)
			if (touchDown) {
				touchTimer += frameTimer;
			}
			if (touchTimer >= 1.0) {
				camera.keys.up = true;
				viewChanged();
			}

			// Check gamepad state
			const float deadZone = 0.0015f;
			// todo : check if gamepad is present
			// todo : time based and relative axis positions
			if (camera.type != Camera::CameraType::firstperson)
			{
				// Rotate
				if (std::abs(gamePadState.axisLeft.x) > deadZone)
				{
					rotation.y += gamePadState.axisLeft.x * 0.5f * rotationSpeed;
					camera.rotate(glm::vec3(0.0f, gamePadState.axisLeft.x * 0.5f, 0.0f));
					updateView = true;
				}
				if (std::abs(gamePadState.axisLeft.y) > deadZone)
				{
					rotation.x -= gamePadState.axisLeft.y * 0.5f * rotationSpeed;
					camera.rotate(glm::vec3(gamePadState.axisLeft.y * 0.5f, 0.0f, 0.0f));
					updateView = true;
				}
				// Zoom
				if (std::abs(gamePadState.axisRight.y) > deadZone)
				{
					zoom -= gamePadState.axisRight.y * 0.01f * zoomSpeed;
					updateView = true;
				}
				if (updateView)
				{
					viewChanged();
				}
			}
			else
			{
				updateView = camera.updatePad(gamePadState.axisLeft, gamePadState.axisRight, frameTimer);
				if (updateView)
				{
					viewChanged();
				}
			}
		}
	}
#elif defined(_DIRECT2DISPLAY)
	while (!quit)
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		if (viewUpdated)
		{
			viewUpdated = false;
			viewChanged();
		}
		render();
		frameCounter++;
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		frameTimer = tDiff / 1000.0f;
		camera.update(frameTimer);
		if (camera.moving())
		{
			viewUpdated = true;
		}
		// Convert to clamped timer value
		if (!paused)
		{
			timer += timerSpeed * frameTimer;
			if (timer > 1.0)
			{
				timer -= 1.0f;
			}
		}
		fpsTimer += (float)tDiff;
		if (fpsTimer > 1000.0f)
		{
			lastFPS = frameCounter;
			updateTextOverlay();
			fpsTimer = 0.0f;
			frameCounter = 0;
		}
	}
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	while (!quit)
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		if (viewUpdated)
		{
			viewUpdated = false;
			viewChanged();
		}

		while (wl_display_prepare_read(display) != 0)
			wl_display_dispatch_pending(display);
		wl_display_flush(display);
		wl_display_read_events(display);
		wl_display_dispatch_pending(display);

		render();
		frameCounter++;
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		frameTimer = tDiff / 1000.0f;
		camera.update(frameTimer);
		if (camera.moving())
		{
			viewUpdated = true;
		}
		// Convert to clamped timer value
		if (!paused)
		{
			timer += timerSpeed * frameTimer;
			if (timer > 1.0)
			{
				timer -= 1.0f;
			}
		}
		fpsTimer += (float)tDiff;
		if (fpsTimer > 1000.0f)
		{
			if (!enableTextOverlay)
			{
				std::string windowTitle = getWindowTitle();
				wl_shell_surface_set_title(shell_surface, windowTitle.c_str());
			}
			lastFPS = frameCounter;
			updateTextOverlay();
			fpsTimer = 0.0f;
			frameCounter = 0;
		}
	}
#elif defined(__linux__)
	xcb_flush(connection);
	while (!quit)
	{
		auto tStart = std::chrono::high_resolution_clock::now();
		if (viewUpdated)
		{
			viewUpdated = false;
			viewChanged();
		}
		xcb_generic_event_t *event;
		while ((event = xcb_poll_for_event(connection)))
		{
			handleEvent(event);
			free(event);
		}
		render();
		frameCounter++;
		auto tEnd = std::chrono::high_resolution_clock::now();
		auto tDiff = std::chrono::duration<double, std::milli>(tEnd - tStart).count();
		frameTimer = tDiff / 1000.0f;
		camera.update(frameTimer);
		if (camera.moving())
		{
			viewUpdated = true;
		}
		// Convert to clamped timer value
		if (!paused)
		{
			timer += timerSpeed * frameTimer;
			if (timer > 1.0)
			{
				timer -= 1.0f;
			}
		}
		fpsTimer += (float)tDiff;
		if (fpsTimer > 1000.0f)
		{
			if (!enableTextOverlay)
			{
				std::string windowTitle = getWindowTitle();
				xcb_change_property(connection, XCB_PROP_MODE_REPLACE,
					window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
					windowTitle.size(), windowTitle.c_str());
			}
			lastFPS = frameCounter;
			updateTextOverlay();
			fpsTimer = 0.0f;
			frameCounter = 0;
		}
	}
#endif
	// Flush device to make sure all resources can be freed 
	vkDeviceWaitIdle(device);
}

void VulkanPipeLines::updateTextOverlay()
{
	if (!enableTextOverlay)
		return;

	textOverlay->beginTextUpdate();

	textOverlay->addText(title, 5.0f, 5.0f, VulkanTextOverlay::alignLeft);

	std::stringstream ss;
	ss << std::fixed << std::setprecision(3) << (frameTimer * 1000.0f) << "ms (" << lastFPS << " fps)";
	textOverlay->addText(ss.str(), 5.0f, 25.0f, VulkanTextOverlay::alignLeft);

	std::string deviceName(deviceProperties.deviceName);
#if defined(__ANDROID__)	
	deviceName += " (" + androidProduct + ")";
#endif
	textOverlay->addText(deviceName, 5.0f, 45.0f, VulkanTextOverlay::alignLeft);

	getOverlayText(textOverlay);

	textOverlay->endTextUpdate();
}

void VulkanPipeLines::getOverlayText(VulkanTextOverlay *textOverlay)
{
	textOverlay->addText("Phong shading pipeline",(float)width / 6.0f, height - 35.0f, VulkanTextOverlay::alignCenter);
	textOverlay->addText("Toon shading pipeline", (float)width / 2.0f, height - 35.0f, VulkanTextOverlay::alignCenter);
	textOverlay->addText("Wireframe pipeline", width - (float)width / 6.5f, height - 35.0f, VulkanTextOverlay::alignCenter);
	if (!deviceFeatures.fillModeNonSolid) {
		textOverlay->addText("Non solid fill modes not supported!", width - (float)width / 6.5f, (float)height / 2.0f - 7.5f, VulkanTextOverlay::alignCenter);
	}
}


VulkanPipeLines::VulkanPipeLines(bool enableValidation)
{
	settings.validation = enableValidation;

	// Parse command line arguments
	for (size_t i = 0; i < args.size(); i++)
	{
		if (args[i] == std::string("-validation"))
		{
			settings.validation = true;
		}
		if (args[i] == std::string("-vsync"))
		{
			settings.vsync = true;
		}
		if (args[i] == std::string("-fullscreen"))
		{
			settings.fullscreen = true;
		}
		if ((args[i] == std::string("-w")) || (args[i] == std::string("-width")))
		{
			char* endptr;
			uint32_t w = strtol(args[i + 1], &endptr, 10);
			if (endptr != args[i + 1]) { width = w; };
		}
		if ((args[i] == std::string("-h")) || (args[i] == std::string("-height")))
		{
			char* endptr;
			uint32_t h = strtol(args[i + 1], &endptr, 10);
			if (endptr != args[i + 1]) { height = h; };
		}
	}
	
	// Vulkan library is loaded dynamically on Android
	bool libLoaded = loadVulkanLibrary();
	assert(libLoaded);


	zoom = -10.5f;
	rotation = glm::vec3(-25.0f, 15.0f, 0.0f);
	enableTextOverlay = true;
	title = "Pipeline state objects";
}

VulkanPipeLines::~VulkanPipeLines()
{
	// Clean up used Vulkan resources
	// Note : Inherited destructor cleans up resources stored in base class
	vkDestroyPipeline(device, pipelines.phong, nullptr);
	if (deviceFeatures.fillModeNonSolid)
	{
		vkDestroyPipeline(device, pipelines.wireframe, nullptr);
	}
	vkDestroyPipeline(device, pipelines.toon, nullptr);

	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);

	models.cube.destroy();
	uniformBuffer.destroy();
	////////////
	// Clean up Vulkan resources
	swapChain.cleanup();
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	}
	destroyCommandBuffers();
	vkDestroyRenderPass(device, renderPass, nullptr);
	for (uint32_t i = 0; i < frameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
	}

	for (auto& shaderModule : shaderModules)
	{
		vkDestroyShaderModule(device, shaderModule, nullptr);
	}
	vkDestroyImageView(device, depthStencil.view, nullptr);
	vkDestroyImage(device, depthStencil.image, nullptr);
	vkFreeMemory(device, depthStencil.mem, nullptr);

	vkDestroyPipelineCache(device, pipelineCache, nullptr);

	vkDestroyCommandPool(device, cmdPool, nullptr);

	vkDestroySemaphore(device, semaphores.presentComplete, nullptr);
	vkDestroySemaphore(device, semaphores.renderComplete, nullptr);
	vkDestroySemaphore(device, semaphores.textOverlayComplete, nullptr);

	if (enableTextOverlay)
	{
		delete textOverlay;
	}

	delete vulkanDevice;

	if (settings.validation)
	{
		vks::debug::freeDebugCallback(instance);
	}

	vkDestroyInstance(instance, nullptr);

}

void VulkanPipeLines::initVulkan()
{
	VkResult err;

	// Vulkan instance
	err = createInstance(settings.validation);
	if (err)
	{
		VksExitFatal("Could not create Vulkan instance : \n" +
								 VksErrorString(err), "Fatal error");
	}

	loadVulkanFunctions(instance);

	// If requested, we enable the default validation layers for debugging
	if (settings.validation)
	{
		// The report flags determine what type of messages for the layers will be displayed
		// For validating (debugging) an appplication the error and warning bits should suffice
		VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		// Additional flags include performance info, loader and layer debug messages, etc.
		vks::debug::setupDebugging(instance, debugReportFlags, VK_NULL_HANDLE);
	}

	// Physical device
	uint32_t gpuCount = 0;
	// Get number of available physical devices
	VK_CHECK_RESULT(vkEnumeratePhysicalDevices(instance, &gpuCount, nullptr));
	assert(gpuCount > 0);
	// Enumerate devices
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	err = vkEnumeratePhysicalDevices(instance, &gpuCount, physicalDevices.data());
	if (err)
	{
		VksExitFatal("Could not enumerate physical devices : \n" +
								 VksErrorString(err), "Fatal error");
	}

	// GPU selection

	// Select physical device to be used for the Vulkan example
	// Defaults to the first device unless specified by command line
	uint32_t selectedDevice = 0;

	physicalDevice = physicalDevices[selectedDevice];

	// Store properties (including limits), features and memory properties of the phyiscal device (so that examples can check against them)
	vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);
	vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemoryProperties);

	// Derived examples can override this to set actual features (based on above readings) to enable for logical device creation
	getEnabledFeatures();

	// Vulkan device creation
	// This is handled by a separate class that gets a logical device representation
	// and encapsulates functions related to a device
	vulkanDevice = new vks::VulkanDevice(physicalDevice);
	VkResult res = vulkanDevice->createLogicalDevice(enabledFeatures, enabledExtensions);
	if (res != VK_SUCCESS) {
		VksExitFatal(
				"Could not create Vulkan device: \n" + VksErrorString(res),
				"Fatal error");
	}
	device = vulkanDevice->logicalDevice;

	// Get a graphics queue from the device
	vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &queue);

	// Find a suitable depth format
	VkBool32 validDepthFormat = VksGetSupportedDepthFormat(physicalDevice, &depthFormat);
	assert(validDepthFormat);

	swapChain.connect(instance, physicalDevice, device);

	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = InitSemaphoreCreateInfo();
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queu
	VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.presentComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been sumbitted and executed
	VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.renderComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands for the text overlay have been sumbitted and executed
	// Will be inserted after the render complete semaphore if the text overlay is enabled
	VK_CHECK_RESULT(vkCreateSemaphore(device, &semaphoreCreateInfo, nullptr, &semaphores.textOverlayComplete));

	// Set up submit info structure
	// Semaphores will stay the same during application lifetime
	// Command buffer submission info is set by each example
	mSubmitInfo = InitSubmitInfo();
	mSubmitInfo.pWaitDstStageMask = &submitPipelineStages;
	mSubmitInfo.waitSemaphoreCount = 1;
	mSubmitInfo.pWaitSemaphores = &semaphores.presentComplete;
	mSubmitInfo.signalSemaphoreCount = 1;
	mSubmitInfo.pSignalSemaphores = &semaphores.renderComplete;

#if defined(__ANDROID__)
	// Get Android device name and manufacturer (to display along GPU name)
	androidProduct = "";
	char prop[PROP_VALUE_MAX+1];
	int len = __system_property_get("ro.product.manufacturer", prop);
	if (len > 0) {
		androidProduct += std::string(prop) + " ";
	};
	len = __system_property_get("ro.product.model", prop);
	if (len > 0) {
		androidProduct += std::string(prop);
	};
	LOGD("androidProduct = %s", androidProduct.c_str());
#endif	
}

int32_t VulkanPipeLines::handleAppInput(struct android_app* app, AInputEvent* event)
{
	VulkanPipeLines* vulkanExample = reinterpret_cast<VulkanPipeLines*>(app->userData);
	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION)
	{
		int32_t eventSource = AInputEvent_getSource(event);
		switch (eventSource) {
			case AINPUT_SOURCE_JOYSTICK: {
				// Left thumbstick
				vulkanExample->gamePadState.axisLeft.x = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_X, 0);
				vulkanExample->gamePadState.axisLeft.y = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Y, 0);
				// Right thumbstick
				vulkanExample->gamePadState.axisRight.x = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Z, 0);
				vulkanExample->gamePadState.axisRight.y = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RZ, 0);
				break;
			}

			case AINPUT_SOURCE_TOUCHSCREEN: {
				int32_t action = AMotionEvent_getAction(event);

				switch (action) {
					case AMOTION_EVENT_ACTION_UP: {
						vulkanExample->touchTimer = 0.0;
						vulkanExample->touchDown = false;
						vulkanExample->camera.keys.up = false;
						return 1;
						break;
					}
					case AMOTION_EVENT_ACTION_DOWN: {
						vulkanExample->touchPos.x = AMotionEvent_getX(event, 0);
						vulkanExample->touchPos.y = AMotionEvent_getY(event, 0);
						vulkanExample->touchDown = true;
						break;
					}
					case AMOTION_EVENT_ACTION_MOVE: {
						int32_t eventX = AMotionEvent_getX(event, 0);
						int32_t eventY = AMotionEvent_getY(event, 0);

						float deltaX = (float)(vulkanExample->touchPos.y - eventY) * vulkanExample->rotationSpeed * 0.5f;
						float deltaY = (float)(vulkanExample->touchPos.x - eventX) * vulkanExample->rotationSpeed * 0.5f;

						vulkanExample->camera.rotate(glm::vec3(deltaX, 0.0f, 0.0f));
						vulkanExample->camera.rotate(glm::vec3(0.0f, -deltaY, 0.0f));

						vulkanExample->rotation.x += deltaX;				
						vulkanExample->rotation.y -= deltaY;				

						vulkanExample->viewChanged();	

						vulkanExample->touchPos.x = eventX;
						vulkanExample->touchPos.y = eventY;

						break;
					}
					default:
						return 1;
						break;
				}
			}

			return 1;
		}
	}

	if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_KEY)
	{
		int32_t keyCode = AKeyEvent_getKeyCode((const AInputEvent*)event);
		int32_t action = AKeyEvent_getAction((const AInputEvent*)event);
		int32_t button = 0;

		if (action == AKEY_EVENT_ACTION_UP)
			return 0;

		switch (keyCode)
		{
		case AKEYCODE_BUTTON_A:
			vulkanExample->keyPressed(GAMEPAD_BUTTON_A);
			break;
		case AKEYCODE_BUTTON_B:
			vulkanExample->keyPressed(GAMEPAD_BUTTON_B);
			break;
		case AKEYCODE_BUTTON_X:
			vulkanExample->keyPressed(GAMEPAD_BUTTON_X);
			break;
		case AKEYCODE_BUTTON_Y:
			vulkanExample->keyPressed(GAMEPAD_BUTTON_Y);
			break;
		case AKEYCODE_BUTTON_L1:
			vulkanExample->keyPressed(GAMEPAD_BUTTON_L1);
			break;
		case AKEYCODE_BUTTON_R1:
			vulkanExample->keyPressed(GAMEPAD_BUTTON_R1);
			break;
		case AKEYCODE_BUTTON_START:
			vulkanExample->paused = !vulkanExample->paused;
			break;
		};

		LOGD("Button %d pressed", keyCode);
	}

	return 0;
}

void VulkanPipeLines::handleAppCommand(android_app * app, int32_t cmd)
{
	assert(app->userData != NULL);
	VulkanPipeLines* vulkanExample = reinterpret_cast<VulkanPipeLines*>(app->userData);
	switch (cmd)
	{
	case APP_CMD_SAVE_STATE:
		LOGD("APP_CMD_SAVE_STATE");
		/*
		vulkanExample->app->savedState = malloc(sizeof(struct saved_state));
		*((struct saved_state*)vulkanExample->app->savedState) = vulkanExample->state;
		vulkanExample->app->savedStateSize = sizeof(struct saved_state);
		*/
		break;
	case APP_CMD_INIT_WINDOW:
		LOGD("APP_CMD_INIT_WINDOW");
		if (androidApp->window != NULL)
		{
			vulkanExample->initVulkan();
			vulkanExample->initSwapchain();
			vulkanExample->prepare();
			assert(vulkanExample->prepared);
		}
		else
		{
			LOGE("No window assigned!");
		}
		break;
	case APP_CMD_LOST_FOCUS:
		LOGD("APP_CMD_LOST_FOCUS");
		vulkanExample->focused = false;
		break;
	case APP_CMD_GAINED_FOCUS:
		LOGD("APP_CMD_GAINED_FOCUS");
		vulkanExample->focused = true;
		break;
	case APP_CMD_TERM_WINDOW:
		// Window is hidden or closed, clean up resources
		LOGD("APP_CMD_TERM_WINDOW");
		vulkanExample->swapChain.cleanup();
		break;
	}
}

void VulkanPipeLines::render(){
	if (!prepared)
		return;
	draw();
}

void VulkanPipeLines::viewChanged()
{
	updateUniformBuffers();
}

void VulkanPipeLines::keyPressed(uint32_t keyCode)
{
	// Can be overriden in derived class
}

void VulkanPipeLines::buildCommandBuffers()
{
	VkCommandBufferBeginInfo cmdBufInfo = InitCommandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = defaultClearColor;
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = InitRenderPassBeginInfo();
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	for (int32_t i = 0; i < drawCmdBuffers.size(); ++i)
	{
		// Set target frame buffer
		renderPassBeginInfo.framebuffer = frameBuffers[i];

		VK_CHECK_RESULT(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

		vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = InitViewport((float) width, (float) height, 0.0f,
                                                              1.0f);
		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = InitRect2D(width, height, 0, 0);
		vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, NULL);

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(drawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &models.cube.vertices.buffer, offsets);
		vkCmdBindIndexBuffer(drawCmdBuffers[i], models.cube.indices.buffer, 0, VK_INDEX_TYPE_UINT32);

		// Left : Solid colored
		viewport.width = (float)width / 3.0;
		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.phong);

		vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);

		// Center : Toon
		viewport.x = (float)width / 3.0;
		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.toon);
		// Line width > 1.0f only if wide lines feature is supported
		if (deviceFeatures.wideLines) {
			vkCmdSetLineWidth(drawCmdBuffers[i], 2.0f);
		}
		vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);

		if (deviceFeatures.fillModeNonSolid)
		{
			// Right : Wireframe
			viewport.x = (float)width / 3.0 + (float)width / 3.0;
			vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);
			vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.wireframe);
			vkCmdDrawIndexed(drawCmdBuffers[i], models.cube.indexCount, 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(drawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(drawCmdBuffers[i]));
	}
}

void VulkanPipeLines::createCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &cmdPool));
}

void VulkanPipeLines::setupDepthStencil()
{
	VkImageCreateInfo image = {};
	image.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image.pNext = NULL;
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = depthFormat;
	image.extent = { width, height, 1 };
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
	image.flags = 0;

	VkMemoryAllocateInfo mem_alloc = {};
	mem_alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mem_alloc.pNext = NULL;
	mem_alloc.allocationSize = 0;
	mem_alloc.memoryTypeIndex = 0;

	VkImageViewCreateInfo depthStencilView = {};
	depthStencilView.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	depthStencilView.pNext = NULL;
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = depthFormat;
	depthStencilView.flags = 0;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;

	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(device, &image, nullptr, &depthStencil.image));
	vkGetImageMemoryRequirements(device, depthStencil.image, &memReqs);
	mem_alloc.allocationSize = memReqs.size;
	mem_alloc.memoryTypeIndex = vulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(device, &mem_alloc, nullptr, &depthStencil.mem));
	VK_CHECK_RESULT(vkBindImageMemory(device, depthStencil.image, depthStencil.mem, 0));

	depthStencilView.image = depthStencil.image;
	VK_CHECK_RESULT(vkCreateImageView(device, &depthStencilView, nullptr, &depthStencil.view));
}

void VulkanPipeLines::setupFrameBuffer()
{
	VkImageView attachments[2];

	// Depth/Stencil attachment is the same for all frame buffers
	attachments[1] = depthStencil.view;

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = NULL;
	frameBufferCreateInfo.renderPass = renderPass;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = attachments;
	frameBufferCreateInfo.width = width;
	frameBufferCreateInfo.height = height;
	frameBufferCreateInfo.layers = 1;

	// Create frame buffers for every swap chain image
	frameBuffers.resize(swapChain.imageCount);
	for (uint32_t i = 0; i < frameBuffers.size(); i++)
	{
		attachments[0] = swapChain.buffers[i].view;
		VK_CHECK_RESULT(vkCreateFramebuffer(device, &frameBufferCreateInfo, nullptr, &frameBuffers[i]));
	}
}

void VulkanPipeLines::setupRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = swapChain.colorFormat;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	// Depth attachment
	attachments[1].format = depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;
	subpassDescription.inputAttachmentCount = 0;
	subpassDescription.pInputAttachments = nullptr;
	subpassDescription.preserveAttachmentCount = 0;
	subpassDescription.pPreserveAttachments = nullptr;
	subpassDescription.pResolveAttachments = nullptr;

	// Subpass dependencies for layout transitions
	std::array<VkSubpassDependency, 2> dependencies;

	dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[0].dstSubpass = 0;
	dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	dependencies[1].srcSubpass = 0;
	dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
	dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
	renderPassInfo.pAttachments = attachments.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass));
}

void VulkanPipeLines::getEnabledFeatures()
{
	// Fill mode non solid is required for wireframe display
	if (deviceFeatures.fillModeNonSolid) {
		enabledFeatures.fillModeNonSolid = VK_TRUE;
		// Wide lines must be present for line width > 1.0f
		if (deviceFeatures.wideLines) {
			enabledFeatures.wideLines = VK_TRUE;
		}
	};
}

void VulkanPipeLines::windowResize()
{
	if (!prepared)
	{
		return;
	}
	prepared = false;

	// Ensure all operations on the device have been finished before destroying resources
	vkDeviceWaitIdle(device);

	// Recreate swap chain
	width = destWidth;
	height = destHeight;
	setupSwapChain();

	// Recreate the frame buffers

	vkDestroyImageView(device, depthStencil.view, nullptr);
	vkDestroyImage(device, depthStencil.image, nullptr);
	vkFreeMemory(device, depthStencil.mem, nullptr);
	setupDepthStencil();
	
	for (uint32_t i = 0; i < frameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(device, frameBuffers[i], nullptr);
	}
	setupFrameBuffer();

	// Command buffers need to be recreated as they may store
	// references to the recreated frame buffer
	destroyCommandBuffers();
	createCommandBuffers();
	buildCommandBuffers();

	vkDeviceWaitIdle(device);

	if (enableTextOverlay)
	{
		textOverlay->reallocateCommandBuffers();
		updateTextOverlay();
	}

	camera.updateAspectRatio((float)width / (float)height);

	// Notify derived class
	windowResized();
	viewChanged();

	prepared = true;
}

void VulkanPipeLines::windowResized()
{
	// Can be overriden in derived class
}

void VulkanPipeLines::initSwapchain()
{
#if defined(_WIN32)
	swapChain.initSurface(windowInstance, window);
#elif defined(__ANDROID__)	
	swapChain.initSurface(androidApp->window);
#elif defined(_DIRECT2DISPLAY)
	swapChain.initSurface(width, height);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	swapChain.initSurface(display, surface);
#elif defined(__linux__)
	swapChain.initSurface(connection, window);
#endif
}

void VulkanPipeLines::setupSwapChain()
{
	swapChain.create(&width, &height, settings.vsync);
}

void VulkanPipeLines::loadAssets()
{
	models.cube.loadFromFile(getAssetPath() + "models/treasure_smooth.dae", vertexLayout, 1.0f, vulkanDevice, queue);
}

void VulkanPipeLines::setupDescriptorPool()
{
	std::vector<VkDescriptorPoolSize> poolSizes =
			{
                    InitDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1)
			};

	VkDescriptorPoolCreateInfo descriptorPoolInfo =
            InitDescriptorPoolCreateInfo(
                    poolSizes.size(),
                    poolSizes.data(),
                    2);

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void VulkanPipeLines::setupDescriptorSetLayout()
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings =
			{
					// Binding 0 : Vertex shader uniform buffer
                    InitDescriptorSetLayoutBinding(
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VK_SHADER_STAGE_VERTEX_BIT,
                            0)
			};

	VkDescriptorSetLayoutCreateInfo descriptorLayout =
            InitDescriptorSetLayoutCreateInfo(
                    setLayoutBindings.data(),
                    setLayoutBindings.size());

	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
            InitPipelineLayoutCreateInfo(
                    &descriptorSetLayout,
                    1);

	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
}

void VulkanPipeLines::setupDescriptorSet()
{
	VkDescriptorSetAllocateInfo allocInfo =
            InitDescriptorSetAllocateInfo(
                    descriptorPool,
                    &descriptorSetLayout,
                    1);

	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet));

	std::vector<VkWriteDescriptorSet> writeDescriptorSets =
			{
					// Binding 0 : Vertex shader uniform buffer
                    InitWriteDescriptorSet(
                            descriptorSet,
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            0,
                            &uniformBuffer.descriptor)
			};

	vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
}

void VulkanPipeLines::preparePipelines()
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
            InitPipelineInputAssemblyStateCreateInfo(
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                    0,
                    VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationState =
            InitPipelineRasterizationStateCreateInfo(
                    VK_POLYGON_MODE_FILL,
                    VK_CULL_MODE_BACK_BIT,
                    VK_FRONT_FACE_CLOCKWISE,
                    0);

	VkPipelineColorBlendAttachmentState blendAttachmentState =
            InitPipelineColorBlendAttachmentState(
                    0xf,
                    VK_FALSE);

	VkPipelineColorBlendStateCreateInfo colorBlendState =
            InitPipelineColorBlendStateCreateInfo(
                    1,
                    &blendAttachmentState);

	VkPipelineDepthStencilStateCreateInfo depthStencilState =
            InitPipelineDepthStencilStateCreateInfo(
                    VK_TRUE,
                    VK_TRUE,
                    VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo viewportState =
            InitPipelineViewportStateCreateInfo(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisampleState =
            InitPipelineMultisampleStateCreateInfo(VK_SAMPLE_COUNT_1_BIT);

	std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR,
			VK_DYNAMIC_STATE_LINE_WIDTH,
	};
	VkPipelineDynamicStateCreateInfo dynamicState =
            InitPipelineDynamicStateCreateInfo(dynamicStateEnables);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
            InitPipelineCreateInfo(pipelineLayout, renderPass);

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.stageCount = shaderStages.size();
	pipelineCreateInfo.pStages = shaderStages.data();

	// Shared vertex bindings and attributes used by all pipelines

	// Binding description
	std::vector<VkVertexInputBindingDescription> vertexInputBindings = {
            InitVertexInputBindingDescription(VERTEX_BUFFER_BIND_ID,
                                                                 vertexLayout.stride(),
                                                                 VK_VERTEX_INPUT_RATE_VERTEX),
	};

	// Attribute descriptions
	std::vector<VkVertexInputAttributeDescription> vertexInputAttributes = {
            InitVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 0,
                                                                   VK_FORMAT_R32G32B32_SFLOAT, 0),					// Location 0: Position
            InitVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 1,
                                                                   VK_FORMAT_R32G32B32_SFLOAT,
                                                                   sizeof(float) * 3),	// Location 1: Color
            InitVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 2,
                                                                   VK_FORMAT_R32G32_SFLOAT,
                                                                   sizeof(float) * 6),		// Location 2 : Texture coordinates
            InitVertexInputAttributeDescription(VERTEX_BUFFER_BIND_ID, 3,
                                                                   VK_FORMAT_R32G32B32_SFLOAT,
                                                                   sizeof(float) * 8),	// Location 3 : Normal
	};

	VkPipelineVertexInputStateCreateInfo vertexInputState = InitPipelineVertexInputStateCreateInfo();
	vertexInputState.vertexBindingDescriptionCount = static_cast<uint32_t>(vertexInputBindings.size());
	vertexInputState.pVertexBindingDescriptions = vertexInputBindings.data();
	vertexInputState.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributes.size());
	vertexInputState.pVertexAttributeDescriptions = vertexInputAttributes.data();

	pipelineCreateInfo.pVertexInputState = &vertexInputState;

	// Create the graphics pipeline state objects

	// We are using this pipeline as the base for the other pipelines (derivatives)
	// Pipeline derivatives can be used for pipelines that share most of their state
	// Depending on the implementation this may result in better performance for pipeline
	// switchting and faster creation time
	pipelineCreateInfo.flags = VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT;

	// Textured pipeline
	// Phong shading pipeline
	shaderStages[0] = loadShader(getAssetPath() + "shaders/pipelines/phong.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/pipelines/phong.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.phong));

	// All pipelines created after the base pipeline will be derivatives
	pipelineCreateInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
	// Base pipeline will be our first created pipeline
	pipelineCreateInfo.basePipelineHandle = pipelines.phong;
	// It's only allowed to either use a handle or index for the base pipeline
	// As we use the handle, we must set the index to -1 (see section 9.5 of the specification)
	pipelineCreateInfo.basePipelineIndex = -1;

	// Toon shading pipeline
	shaderStages[0] = loadShader(getAssetPath() + "shaders/pipelines/toon.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/pipelines/toon.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.toon));

	// Pipeline for wire frame rendering
	// Non solid rendering is not a mandatory Vulkan feature
	if (deviceFeatures.fillModeNonSolid)
	{
		rasterizationState.polygonMode = VK_POLYGON_MODE_LINE;
		shaderStages[0] = loadShader(getAssetPath() + "shaders/pipelines/wireframe.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
		shaderStages[1] = loadShader(getAssetPath() + "shaders/pipelines/wireframe.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
		VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.wireframe));
	}
}

// Prepare and initialize uniform buffer containing shader uniforms
void VulkanPipeLines::prepareUniformBuffers()
{
	// Create the vertex shader uniform buffer block
	VK_CHECK_RESULT(vulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBuffer,
			sizeof(uboVS)));

	// Map persistent
	VK_CHECK_RESULT(uniformBuffer.map());

	updateUniformBuffers();
}

void VulkanPipeLines::updateUniformBuffers()
{
	uboVS.projection = glm::perspective(glm::radians(60.0f), (float)(width / 3.0f) / (float)height, 0.1f, 256.0f);

	glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

	uboVS.modelView = viewMatrix * glm::translate(glm::mat4(), cameraPos);
	uboVS.modelView = glm::rotate(uboVS.modelView, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	uboVS.modelView = glm::rotate(uboVS.modelView, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	uboVS.modelView = glm::rotate(uboVS.modelView, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	memcpy(uniformBuffer.mapped, &uboVS, sizeof(uboVS));
}

void VulkanPipeLines::draw()
{
    // Acquire the next image from the swap chaing
    VK_CHECK_RESULT(swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer));

	mSubmitInfo.commandBufferCount = 1;
	mSubmitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &mSubmitInfo, VK_NULL_HANDLE));

    bool submitTextOverlay = enableTextOverlay && textOverlay->visible;

    if (submitTextOverlay)
    {
        // Wait for color attachment output to finish before rendering the text overlay
        VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        mSubmitInfo.pWaitDstStageMask = &stageFlags;

        // Set semaphores
        // Wait for render complete semaphore
        mSubmitInfo.waitSemaphoreCount = 1;
        mSubmitInfo.pWaitSemaphores = &semaphores.renderComplete;
        // Signal ready with text overlay complete semaphpre
        mSubmitInfo.signalSemaphoreCount = 1;
        mSubmitInfo.pSignalSemaphores = &semaphores.textOverlayComplete;

        // Submit current text overlay command buffer
        mSubmitInfo.commandBufferCount = 1;
        mSubmitInfo.pCommandBuffers = &textOverlay->cmdBuffers[currentBuffer];
        VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &mSubmitInfo, VK_NULL_HANDLE));

        // Reset stage mask
        mSubmitInfo.pWaitDstStageMask = &submitPipelineStages;
        // Reset wait and signal semaphores for rendering next frame
        // Wait for swap chain presentation to finish
        mSubmitInfo.waitSemaphoreCount = 1;
        mSubmitInfo.pWaitSemaphores = &semaphores.presentComplete;
        // Signal ready with offscreen semaphore
        mSubmitInfo.signalSemaphoreCount = 1;
        mSubmitInfo.pSignalSemaphores = &semaphores.renderComplete;
    }

    VK_CHECK_RESULT(swapChain.queuePresent(queue, currentBuffer, submitTextOverlay ? semaphores.textOverlayComplete : semaphores.renderComplete));

    VK_CHECK_RESULT(vkQueueWaitIdle(queue));
}


VulkanPipeLines *gpVkPipeLine;
void android_main(android_app* state)
{
	app_dummy();
	gpVkPipeLine = new VulkanPipeLines(false);
	state->userData = gpVkPipeLine;
	state->onAppCmd = VulkanPipeLines::handleAppCommand;
	state->onInputEvent = VulkanPipeLines::handleAppInput;
	androidApp = state;
	gpVkPipeLine->renderLoop();
	delete(gpVkPipeLine);
}
