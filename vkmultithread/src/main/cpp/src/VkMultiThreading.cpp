/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VkMultiThreading.h"

VkMultiThreading::VkMultiThreading(bool enableValidation)
{
	settings.validation = false;
	settings.vsync = false;
	settings.fullscreen = true;

	// Vulkan library is loaded dynamically on Android
	bool libLoaded = loadVulkanLibrary();
	assert(libLoaded);

	zoom = -32.5f;
	zoomSpeed = 2.5f;
	rotationSpeed = 0.5f;
	rotation = { 0.0f, 37.5f, 0.0f };
	mEnableTextOverlay = true;
	title = "Multi threaded rendering";
	// Get number of max. concurrrent threads
	numThreads = std::thread::hardware_concurrency();
	assert(numThreads > 0);
	LOGD("numThreads = %d", numThreads);

	srand(time(NULL));

	mThreadPool.setThreadCount(numThreads);

	numObjectsPerThread = 512 / numThreads;


}

VkMultiThreading::~VkMultiThreading()
{
	// Clean up used Vulkan resources
	// Note : Inherited destructor cleans up resources stored in base class
	vkDestroyPipeline(device, pipelines.phong, nullptr);
	vkDestroyPipeline(device, pipelines.starsphere, nullptr);

	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

	vkFreeCommandBuffers(device, mCmdPool, 1, &mPrimaryCommandBuffer);
	vkFreeCommandBuffers(device, mCmdPool, 1, &mSecondaryCommandBuffer);

	models.ufo.destroy();
	models.skysphere.destroy();

	for (auto& thread : threadData)
	{
		vkFreeCommandBuffers(device, thread.commandPool, thread.commandBuffer.size(), thread.commandBuffer.data());
		vkDestroyCommandPool(device, thread.commandPool, nullptr);
	}

	vkDestroyFence(device, renderFence, nullptr);

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

	vkDestroyCommandPool(device, mCmdPool, nullptr);

	vkDestroySemaphore(device, semaphores.presentComplete, nullptr);
	vkDestroySemaphore(device, semaphores.renderComplete, nullptr);
	vkDestroySemaphore(device, semaphores.textOverlayComplete, nullptr);

	if (mEnableTextOverlay)
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

VkResult VkMultiThreading::createInstance(bool enableValidation)
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

std::string VkMultiThreading::getWindowTitle()
{
	std::string device(deviceProperties.deviceName);
	std::string windowTitle;
	windowTitle = title + " - " + device;
	if (!mEnableTextOverlay)
	{
		windowTitle += " - " ;
		windowTitle += frameCounter;
		windowTitle += " fps";
	}
	return windowTitle;
}

const std::string VkMultiThreading::getAssetPath()
{
#if defined(__ANDROID__)
	return "";
#else
	return "./../data/";
#endif
}

bool VkMultiThreading::checkCommandBuffers()
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

void VkMultiThreading::createCommandBuffers()
{
	// Create one command buffer for each swap chain image and reuse for rendering
	drawCmdBuffers.resize(swapChain.imageCount);

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vks::initializers::commandBufferAllocateInfo(
			mCmdPool,
			VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			static_cast<uint32_t>(drawCmdBuffers.size()));

	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, drawCmdBuffers.data()));
}

void VkMultiThreading::destroyCommandBuffers()
{
	vkFreeCommandBuffers(device, mCmdPool, static_cast<uint32_t>(drawCmdBuffers.size()), drawCmdBuffers.data());
}

VkCommandBuffer VkMultiThreading::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
		vks::initializers::commandBufferAllocateInfo(
			mCmdPool,
			level,
			1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &cmdBuffer));

	// If requested, also start the new command buffer
	if (begin)
	{
		VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	}

	return cmdBuffer;
}

void VkMultiThreading::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
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
		vkFreeCommandBuffers(device, mCmdPool, 1, &commandBuffer);
	}
}

void VkMultiThreading::createPipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(device, &pipelineCacheCreateInfo, nullptr, &pipelineCache));
}

void VkMultiThreading::prepare()
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

	if (mEnableTextOverlay)
	{
		// Load the text rendering shaders
		std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
		shaderStages.push_back(loadShader(getAssetPath() + "shaders/base/textoverlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT));
		shaderStages.push_back(loadShader(getAssetPath() + "shaders/base/textoverlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT));
		textOverlay = new VulkanTextOverlay(
			vulkanDevice,
			mVkQueue,
			frameBuffers,
			swapChain.colorFormat,
			depthFormat,
			&width,
			&height,
			shaderStages
			);
		updateTextOverlay();
	}

	// Create a fence for synchronization
	VkFenceCreateInfo fenceCreateInfo = vks::initializers::fenceCreateInfo(VK_FLAGS_NONE);
	vkCreateFence(device, &fenceCreateInfo, NULL, &renderFence);
	loadMeshes();
	setupVertexDescriptions();
	setupPipelineLayout();
	preparePipelines();
	prepareMultiThreadedRenderer();
	updateMatrices();
	prepared = true;

}

VkPipelineShaderStageCreateInfo VkMultiThreading::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
	shaderStage.module = vks::tools::loadShader(androidApp->activity->assetManager, fileName.c_str(), device, stage);
	shaderStage.pName = "main"; // todo : make param
	assert(shaderStage.module != VK_NULL_HANDLE);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}

void VkMultiThreading::renderLoop()
{
	destWidth = width;
	destHeight = height;

	while (1)
	{
		int ident;
		int events;
		struct android_poll_source* source;
		bool destroy = false;

		focused = true;
		LOGE("Fun:%s, Line:%d", __FUNCTION__, __LINE__ );

		while ((ident = ALooper_pollAll(focused ? 0 : -1, NULL, &events, (void**)&source)) >= 0)
		{
			if (source != NULL)
			{
				source->process(androidApp, source);
			}
			LOGE("Fun:%s, Line:%d", __FUNCTION__, __LINE__ );
			if (androidApp->destroyRequested != 0)
			{
				LOGD("Android app destroy requested");
				destroy = true;
				break;
			}
			LOGE("Fun:%s, Line:%d", __FUNCTION__, __LINE__ );
		}
		LOGE("Fun:%s, Line:%d", __FUNCTION__, __LINE__ );
		// App destruction requested
		// Exit loop, example will be destroyed in application main
		if (destroy)
		{
			break;
		}

		LOGE("Fun:%s, Line:%d", __FUNCTION__, __LINE__ );
		// Render frame
		if (prepared)
		{
			auto tStart = std::chrono::high_resolution_clock::now();
			LOGE("Fun:%s, Line:%d", __FUNCTION__, __LINE__ );
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


    LOGE("Fun:%s, Line:%d", __FUNCTION__, __LINE__ );

	// Flush device to make sure all resources can be freed 
	vkDeviceWaitIdle(device);
}

void VkMultiThreading::updateTextOverlay()
{
	if (!mEnableTextOverlay)
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

void VkMultiThreading::getOverlayText(VulkanTextOverlay *textOverlay)
{
	// Can be overriden in derived class
    textOverlay->addText("Using " + std::to_string(numThreads) + " threads", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
}


void VkMultiThreading::submitFrame()
{
	bool submitTextOverlay = mEnableTextOverlay && textOverlay->visible;

	if (submitTextOverlay)
	{
		// Wait for color attachment output to finish before rendering the text overlay
		VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		submitInfo.pWaitDstStageMask = &stageFlags;

		// Set semaphores
		// Wait for render complete semaphore
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &semaphores.renderComplete;
		// Signal ready with text overlay complete semaphpre
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &semaphores.textOverlayComplete;

		// Submit current text overlay command buffer
		submitInfo.commandBufferCount = 1;
		submitInfo.pCommandBuffers = &textOverlay->cmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(mVkQueue, 1, &submitInfo, VK_NULL_HANDLE));

		// Reset stage mask
		submitInfo.pWaitDstStageMask = &submitPipelineStages;
		// Reset wait and signal semaphores for rendering next frame
		// Wait for swap chain presentation to finish
		submitInfo.waitSemaphoreCount = 1;
		submitInfo.pWaitSemaphores = &semaphores.presentComplete;
		// Signal ready with offscreen semaphore
		submitInfo.signalSemaphoreCount = 1;
		submitInfo.pSignalSemaphores = &semaphores.renderComplete;
	}

	VK_CHECK_RESULT(swapChain.queuePresent(mVkQueue, currentBuffer, submitTextOverlay ? semaphores.textOverlayComplete : semaphores.renderComplete));

//	VK_CHECK_RESULT(vkQueueWaitIdle(queue));
}

void VkMultiThreading::initVulkan()
{
	VkResult err;

	// Vulkan instance
	err = createInstance(settings.validation);
	if (err)
	{
		vks::tools::exitFatal("Could not create Vulkan instance : \n" + vks::tools::errorString(err), "Fatal error");
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
		vks::tools::exitFatal("Could not enumerate physical devices : \n" + vks::tools::errorString(err), "Fatal error");
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
		vks::tools::exitFatal("Could not create Vulkan device: \n" + vks::tools::errorString(res), "Fatal error");
	}
	device = vulkanDevice->logicalDevice;

	// Get a graphics queue from the device
	vkGetDeviceQueue(device, vulkanDevice->queueFamilyIndices.graphics, 0, &mVkQueue);

	// Find a suitable depth format
	VkBool32 validDepthFormat = vks::tools::getSupportedDepthFormat(physicalDevice, &depthFormat);
	assert(validDepthFormat);

	swapChain.connect(instance, physicalDevice, device);

	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = vks::initializers::semaphoreCreateInfo();
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
	submitInfo = vks::initializers::submitInfo();
	submitInfo.pWaitDstStageMask = &submitPipelineStages;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &semaphores.presentComplete;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &semaphores.renderComplete;

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

int32_t VkMultiThreading::handleAppInput(struct android_app* app, AInputEvent* event)
{
    VkMultiThreading* vulkanExample = reinterpret_cast<VkMultiThreading*>(app->userData);
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

void VkMultiThreading::handleAppCommand(android_app * app, int32_t cmd)
{
	assert(app->userData != NULL);
	VkMultiThreading* vulkanExample = reinterpret_cast<VkMultiThreading*>(app->userData);
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


void VkMultiThreading::viewChanged()
{
    updateMatrices();
}

void VkMultiThreading::keyPressed(uint32_t keyCode)
{
	// Can be overriden in derived class
}

void VkMultiThreading::buildCommandBuffers()
{
	// Can be overriden in derived class
}

void VkMultiThreading::createCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &mCmdPool));
}

void VkMultiThreading::setupDepthStencil()
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

void VkMultiThreading::setupFrameBuffer()
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

void VkMultiThreading::setupRenderPass()
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

void VkMultiThreading::getEnabledFeatures()
{
	// Can be overriden in derived class
}

void VkMultiThreading::windowResize()
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

	if (mEnableTextOverlay)
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

void VkMultiThreading::windowResized()
{
	// Can be overriden in derived class
}

void VkMultiThreading::initSwapchain()
{
	swapChain.initSurface(androidApp->window);
}

void VkMultiThreading::setupSwapChain()
{
	swapChain.create(&width, &height, settings.vsync);
}

float VkMultiThreading::rnd(float range)
{
    return range * (rand() / double(RAND_MAX));
}

// Create all threads and initialize shader push constants
void VkMultiThreading::prepareMultiThreadedRenderer()
{
    // Since this demo updates the command buffers on each frame
    // we don't use the per-framebuffer command buffers from the
    // base class, and create a single primary command buffer instead

    VkCommandBufferAllocateInfo cmdBufAllocateInfo =
            vks::initializers::commandBufferAllocateInfo(
                    mCmdPool,
                    VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    1);
    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &mPrimaryCommandBuffer));

    // Create a secondary command buffer for rendering the star sphere
    cmdBufAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &mSecondaryCommandBuffer));

    threadData.resize(numThreads);

    float maxX = std::floor(std::sqrt(numThreads * numObjectsPerThread));
    uint32_t posX = 0;
    uint32_t posZ = 0;

    std::mt19937 rndGenerator((unsigned)time(NULL));
    std::uniform_real_distribution<float> uniformDist(0.0f, 1.0f);

    for (uint32_t i = 0; i < numThreads; i++)
    {
        ThreadData *thread = &threadData[i];

        // Create one command pool for each thread
        VkCommandPoolCreateInfo cmdPoolInfo = vks::initializers::commandPoolCreateInfo();
        cmdPoolInfo.queueFamilyIndex = swapChain.queueNodeIndex;
        cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        VK_CHECK_RESULT(vkCreateCommandPool(device, &cmdPoolInfo, nullptr, &thread->commandPool));

        // One secondary command buffer per object that is updated by this thread
        thread->commandBuffer.resize(numObjectsPerThread);
        // Generate secondary command buffers for each thread
        VkCommandBufferAllocateInfo secondaryCmdBufAllocateInfo =
                vks::initializers::commandBufferAllocateInfo(
                        thread->commandPool,
                        VK_COMMAND_BUFFER_LEVEL_SECONDARY,
                        thread->commandBuffer.size());
        VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &secondaryCmdBufAllocateInfo, thread->commandBuffer.data()));

        thread->pushConstBlock.resize(numObjectsPerThread);
        thread->objectData.resize(numObjectsPerThread);

        for (uint32_t j = 0; j < numObjectsPerThread; j++)
        {
            float theta = 2.0f * float(M_PI) * uniformDist(rndGenerator);
            float phi = acos(1.0f - 2.0f * uniformDist(rndGenerator));
            thread->objectData[j].pos = glm::vec3(sin(phi) * cos(theta), 0.0f, cos(phi)) * 35.0f;

            thread->objectData[j].rotation = glm::vec3(0.0f, rnd(360.0f), 0.0f);
            thread->objectData[j].deltaT = rnd(1.0f);
            thread->objectData[j].rotationDir = (rnd(100.0f) < 50.0f) ? 1.0f : -1.0f;
            thread->objectData[j].rotationSpeed = (2.0f + rnd(4.0f)) * thread->objectData[j].rotationDir;
            thread->objectData[j].scale = 0.75f + rnd(0.5f);

            thread->pushConstBlock[j].color = glm::vec3(rnd(1.0f), rnd(1.0f), rnd(1.0f));
        }
    }

}

// Builds the secondary command buffer for each thread
void VkMultiThreading::threadRenderCode(uint32_t threadIndex, uint32_t cmdBufferIndex, VkCommandBufferInheritanceInfo inheritanceInfo)
{
    ThreadData *thread = &threadData[threadIndex];
    ObjectData *objectData = &thread->objectData[cmdBufferIndex];

    // Check visibility against view frustum
    objectData->visible = frustum.checkSphere(objectData->pos, objectSphereDim * 0.5f);

    if (!objectData->visible)
    {
        return;
    }

    VkCommandBufferBeginInfo commandBufferBeginInfo = vks::initializers::commandBufferBeginInfo();
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    commandBufferBeginInfo.pInheritanceInfo = &inheritanceInfo;

    VkCommandBuffer cmdBuffer = thread->commandBuffer[cmdBufferIndex];

    VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &commandBufferBeginInfo));

    VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.phong);

    // Update
    objectData->rotation.y += 2.5f * objectData->rotationSpeed * frameTimer;
    if (objectData->rotation.y > 360.0f)
    {
        objectData->rotation.y -= 360.0f;
    }
    objectData->deltaT += 0.15f * frameTimer;
    if (objectData->deltaT > 1.0f)
        objectData->deltaT -= 1.0f;
    objectData->pos.y = sin(glm::radians(objectData->deltaT * 360.0f)) * 2.5f;

    objectData->model = glm::translate(glm::mat4(), objectData->pos);
    objectData->model = glm::rotate(objectData->model, -sinf(glm::radians(objectData->deltaT * 360.0f)) * 0.25f, glm::vec3(objectData->rotationDir, 0.0f, 0.0f));
    objectData->model = glm::rotate(objectData->model, glm::radians(objectData->rotation.y), glm::vec3(0.0f, objectData->rotationDir, 0.0f));
    objectData->model = glm::rotate(objectData->model, glm::radians(objectData->deltaT * 360.0f), glm::vec3(0.0f, objectData->rotationDir, 0.0f));
    objectData->model = glm::scale(objectData->model, glm::vec3(objectData->scale));

    thread->pushConstBlock[cmdBufferIndex].mvp = matrices.projection * matrices.view * objectData->model;

    // Update shader push constant block
    // Contains model view matrix
    vkCmdPushConstants(
            cmdBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(ThreadPushConstantBlock),
            &thread->pushConstBlock[cmdBufferIndex]);

    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, &models.ufo.vertices.buffer, offsets);
    vkCmdBindIndexBuffer(cmdBuffer, models.ufo.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmdBuffer, models.ufo.indexCount, 1, 0, 0, 0);

    VK_CHECK_RESULT(vkEndCommandBuffer(cmdBuffer));
}

void VkMultiThreading::updateSecondaryCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo)
{
    // Secondary command buffer for the sky sphere
    VkCommandBufferBeginInfo commandBufferBeginInfo = vks::initializers::commandBufferBeginInfo();
    commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
    commandBufferBeginInfo.pInheritanceInfo = &inheritanceInfo;

    VK_CHECK_RESULT(vkBeginCommandBuffer(mSecondaryCommandBuffer, &commandBufferBeginInfo));

    VkViewport viewport = vks::initializers::viewport((float)width, (float)height, 0.0f, 1.0f);
    vkCmdSetViewport(mSecondaryCommandBuffer, 0, 1, &viewport);

    VkRect2D scissor = vks::initializers::rect2D(width, height, 0, 0);
    vkCmdSetScissor(mSecondaryCommandBuffer, 0, 1, &scissor);

    vkCmdBindPipeline(mSecondaryCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.starsphere);

    glm::mat4 view = glm::mat4();
    view = glm::rotate(view, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    view = glm::rotate(view, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    view = glm::rotate(view, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    glm::mat4 mvp = matrices.projection * view;

    vkCmdPushConstants(
            mSecondaryCommandBuffer,
            pipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(mvp),
            &mvp);

    VkDeviceSize offsets[1] = { 0 };
    vkCmdBindVertexBuffers(mSecondaryCommandBuffer, 0, 1, &models.skysphere.vertices.buffer, offsets);
    vkCmdBindIndexBuffer(mSecondaryCommandBuffer, models.skysphere.indices.buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(mSecondaryCommandBuffer, models.skysphere.indexCount, 1, 0, 0, 0);

    VK_CHECK_RESULT(vkEndCommandBuffer(mSecondaryCommandBuffer));
}

// Updates the secondary command buffers using a thread pool
// and puts them into the primary command buffer that's
// lat submitted to the queue for rendering
void VkMultiThreading::updateCommandBuffers(VkFramebuffer frameBuffer)
{
    VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();

    VkClearValue clearValues[2];
    clearValues[0].color = defaultClearColor;
    clearValues[0].color = { {0.0f, 0.0f, 0.2f, 0.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };

    VkRenderPassBeginInfo renderPassBeginInfo = vks::initializers::renderPassBeginInfo();
    renderPassBeginInfo.renderPass = renderPass;
    renderPassBeginInfo.renderArea.offset.x = 0;
    renderPassBeginInfo.renderArea.offset.y = 0;
    renderPassBeginInfo.renderArea.extent.width = width;
    renderPassBeginInfo.renderArea.extent.height = height;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;
    renderPassBeginInfo.framebuffer = frameBuffer;

    // Set target frame buffer

    VK_CHECK_RESULT(vkBeginCommandBuffer(mPrimaryCommandBuffer, &cmdBufInfo));

    // The primary command buffer does not contain any rendering commands
    // These are stored (and retrieved) from the secondary command buffers
    vkCmdBeginRenderPass(mPrimaryCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    // Inheritance info for the secondary command buffers
    VkCommandBufferInheritanceInfo inheritanceInfo = vks::initializers::commandBufferInheritanceInfo();
    inheritanceInfo.renderPass = renderPass;
    // Secondary command buffer also use the currently active framebuffer
    inheritanceInfo.framebuffer = frameBuffer;

    // Contains the list of secondary command buffers to be executed
    std::vector<VkCommandBuffer> commandBuffers;

    // Secondary command buffer with star background sphere
    updateSecondaryCommandBuffer(inheritanceInfo);
    commandBuffers.push_back(mSecondaryCommandBuffer);

    // Add a job to the thread's queue for each object to be rendered
    for (uint32_t t = 0; t < numThreads; t++)
    {
        for (uint32_t i = 0; i < numObjectsPerThread; i++)
        {
            mThreadPool.threads[t]->addJob([=] { threadRenderCode(t, i, inheritanceInfo); });
        }
    }

    mThreadPool.wait();

    // Only submit if object is within the current view frustum
    for (uint32_t t = 0; t < numThreads; t++)
    {
        for (uint32_t i = 0; i < numObjectsPerThread; i++)
        {
            if (threadData[t].objectData[i].visible)
            {
                commandBuffers.push_back(threadData[t].commandBuffer[i]);
            }
        }
    }

    // Execute render commands from the secondary command buffer
    vkCmdExecuteCommands(mPrimaryCommandBuffer, commandBuffers.size(), commandBuffers.data());

    vkCmdEndRenderPass(mPrimaryCommandBuffer);

    VK_CHECK_RESULT(vkEndCommandBuffer(mPrimaryCommandBuffer));
}

void VkMultiThreading::loadMeshes()
{
    models.ufo.loadFromFile(getAssetPath() + "models/retroufo_red.dae", vertexLayout, 0.12f, vulkanDevice, mVkQueue);
    models.skysphere.loadFromFile(getAssetPath() + "models/sphere.obj", vertexLayout, 1.0f, vulkanDevice, mVkQueue);
    objectSphereDim = std::max(std::max(models.ufo.dim.size.x, models.ufo.dim.size.y), models.ufo.dim.size.z);
}

void VkMultiThreading::setupVertexDescriptions()
{
    // Binding description
    vertices.bindingDescriptions.resize(1);
    vertices.bindingDescriptions[0] =
            vks::initializers::vertexInputBindingDescription(
                    VERTEX_BUFFER_BIND_ID,
                    vertexLayout.stride(),
                    VK_VERTEX_INPUT_RATE_VERTEX);

    // Attribute descriptions
    // Describes memory layout and shader positions
    vertices.attributeDescriptions.resize(3);
    // Location 0 : Position
    vertices.attributeDescriptions[0] =
            vks::initializers::vertexInputAttributeDescription(
                    VERTEX_BUFFER_BIND_ID,
                    0,
                    VK_FORMAT_R32G32B32_SFLOAT,
                    0);
    // Location 1 : Normal
    vertices.attributeDescriptions[1] =
            vks::initializers::vertexInputAttributeDescription(
                    VERTEX_BUFFER_BIND_ID,
                    1,
                    VK_FORMAT_R32G32B32_SFLOAT,
                    sizeof(float) * 3);
    // Location 3 : Color
    vertices.attributeDescriptions[2] =
            vks::initializers::vertexInputAttributeDescription(
                    VERTEX_BUFFER_BIND_ID,
                    2,
                    VK_FORMAT_R32G32B32_SFLOAT,
                    sizeof(float) * 6);

    vertices.inputState = vks::initializers::pipelineVertexInputStateCreateInfo();
    vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
    vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
    vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
    vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
}

void VkMultiThreading::setupPipelineLayout()
{
    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo =
            vks::initializers::pipelineLayoutCreateInfo(nullptr, 0);

    // Push constants for model matrices
    VkPushConstantRange pushConstantRange =
            vks::initializers::pushConstantRange(
                    VK_SHADER_STAGE_VERTEX_BIT,
                    sizeof(ThreadPushConstantBlock),
                    0);

    // Push constant ranges are part of the pipeline layout
    pPipelineLayoutCreateInfo.pushConstantRangeCount = 1;
    pPipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

    VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pPipelineLayoutCreateInfo, nullptr, &pipelineLayout));
}

void VkMultiThreading::preparePipelines()
{
    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
            vks::initializers::pipelineInputAssemblyStateCreateInfo(
                    VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                    0,
                    VK_FALSE);

    VkPipelineRasterizationStateCreateInfo rasterizationState =
            vks::initializers::pipelineRasterizationStateCreateInfo(
                    VK_POLYGON_MODE_FILL,
                    VK_CULL_MODE_BACK_BIT,
                    VK_FRONT_FACE_CLOCKWISE,
                    0);

    VkPipelineColorBlendAttachmentState blendAttachmentState =
            vks::initializers::pipelineColorBlendAttachmentState(
                    0xf,
                    VK_FALSE);

    VkPipelineColorBlendStateCreateInfo colorBlendState =
            vks::initializers::pipelineColorBlendStateCreateInfo(
                    1,
                    &blendAttachmentState);

    VkPipelineDepthStencilStateCreateInfo depthStencilState =
            vks::initializers::pipelineDepthStencilStateCreateInfo(
                    VK_TRUE,
                    VK_TRUE,
                    VK_COMPARE_OP_LESS_OR_EQUAL);

    VkPipelineViewportStateCreateInfo viewportState =
            vks::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

    VkPipelineMultisampleStateCreateInfo multisampleState =
            vks::initializers::pipelineMultisampleStateCreateInfo(
                    VK_SAMPLE_COUNT_1_BIT,
                    0);

    std::vector<VkDynamicState> dynamicStateEnables = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState =
            vks::initializers::pipelineDynamicStateCreateInfo(
                    dynamicStateEnables.data(),
                    dynamicStateEnables.size(),
                    0);

    // Solid rendering pipeline
    // Load shaders
    std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

    shaderStages[0] = loadShader(getAssetPath() + "shaders/multithreading/phong.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader(getAssetPath() + "shaders/multithreading/phong.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

    VkGraphicsPipelineCreateInfo pipelineCreateInfo =
            vks::initializers::pipelineCreateInfo(
                    pipelineLayout,
                    renderPass,
                    0);

    pipelineCreateInfo.pVertexInputState = &vertices.inputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &rasterizationState;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &multisampleState;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &depthStencilState;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.stageCount = shaderStages.size();
    pipelineCreateInfo.pStages = shaderStages.data();

    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.phong));

    // Star sphere rendering pipeline
    rasterizationState.cullMode = VK_CULL_MODE_FRONT_BIT;
    depthStencilState.depthWriteEnable = VK_FALSE;
    shaderStages[0] = loadShader(getAssetPath() + "shaders/multithreading/starsphere.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
    shaderStages[1] = loadShader(getAssetPath() + "shaders/multithreading/starsphere.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
    VK_CHECK_RESULT(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.starsphere));
}

void VkMultiThreading::updateMatrices()
{
    matrices.projection = glm::perspective(glm::radians(60.0f), (float)width / (float)height, 0.1f, 256.0f);
    matrices.view = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));
    matrices.view = glm::rotate(matrices.view, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
    matrices.view = glm::rotate(matrices.view, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
    matrices.view = glm::rotate(matrices.view, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

    frustum.update(matrices.projection * matrices.view);
}

void VkMultiThreading::draw()
{
	LOGE("multithread, Fun:%s, Line:%d  begin -------", __FUNCTION__, __LINE__);
    // Acquire the next image from the swap chaing
    VK_CHECK_RESULT(swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer));

    updateCommandBuffers(frameBuffers[currentBuffer]);

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mPrimaryCommandBuffer;

    VK_CHECK_RESULT(vkQueueSubmit(mVkQueue, 1, &submitInfo, renderFence));

    LOGE("multithread, Fun:%s, Line:%d   ----", __FUNCTION__, __LINE__);
    // Wait for fence to signal that all command buffers are ready
    VkResult fenceRes;
    do
    {
        fenceRes = vkWaitForFences(device, 1, &renderFence, VK_TRUE, 100000000);
    } while (fenceRes == VK_TIMEOUT);
    VK_CHECK_RESULT(fenceRes);
    LOGE("multithread, Fun:%s, Line:%d", __FUNCTION__, __LINE__);
    vkResetFences(device, 1, &renderFence);

	/////

    submitFrame();
	LOGE("multithread, Fun:%s, Line:%d  end ====", __FUNCTION__, __LINE__);
}

void VkMultiThreading::render()
{
    if (!prepared)
        return;
    draw();
}



void android_main(android_app* state)
{
	app_dummy();
	VkMultiThreading *vulkanExample = new VkMultiThreading(false);
	state->userData = vulkanExample;
	state->onAppCmd = VkMultiThreading::handleAppCommand;
	state->onInputEvent = VkMultiThreading::handleAppInput;
	androidApp = state;
	vulkanExample->renderLoop();
	delete(vulkanExample);
}
