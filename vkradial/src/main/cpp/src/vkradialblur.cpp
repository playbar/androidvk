#include "vkradialblur.h"
#include "VulkanPipeLine.h"

#define VERTEX_BUFFER_BIND_ID 0
#define ENABLE_VALIDATION false

// Offscreen frame buffer properties
#define FB_DIM 512
#define FB_COLOR_FORMAT VK_FORMAT_R8G8B8A8_UNORM

std::vector<const char*> VKRadialBlur::args;

VkResult VKRadialBlur::createInstance(bool enableValidation)
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
		instanceCreateInfo.enabledLayerCount = giValidationLayerCount;
		instanceCreateInfo.ppEnabledLayerNames = gzsValidationLayerNames;
	}
	return vkCreateInstance(&instanceCreateInfo, nullptr, &instance);
}

std::string VKRadialBlur::getWindowTitle()
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

const std::string VKRadialBlur::getAssetPath()
{
#if defined(__ANDROID__)
	return "";
#else
	return "./../data/";
#endif
}

bool VKRadialBlur::checkCommandBuffers()
{
	for (auto& cmdBuffer : mDrawCmdBuffers)
	{
		if (cmdBuffer == VK_NULL_HANDLE)
		{
			return false;
		}
	}
	return true;
}

void VKRadialBlur::createCommandBuffers()
{
	// Create one command buffer for each swap chain image and reuse for rendering
    mDrawCmdBuffers.resize(mSwapChain.mImageCount);

	VkCommandBufferAllocateInfo cmdBufAllocateInfo =
			InitCommandBufferAllocateInfo(
					cmdPool,
					VK_COMMAND_BUFFER_LEVEL_PRIMARY,
					static_cast<uint32_t>(mDrawCmdBuffers.size()));

	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, mDrawCmdBuffers.data()));
}

void VKRadialBlur::destroyCommandBuffers()
{
	vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, cmdPool, static_cast<uint32_t>(mDrawCmdBuffers.size()), mDrawCmdBuffers.data());
}

VkCommandBuffer VKRadialBlur::createCommandBuffer(VkCommandBufferLevel level, bool begin)
{
	VkCommandBuffer cmdBuffer;

	VkCommandBufferAllocateInfo cmdBufAllocateInfo = InitCommandBufferAllocateInfo(cmdPool, level, 1);

	VK_CHECK_RESULT(vkAllocateCommandBuffers(mVulkanDevice->mLogicalDevice, &cmdBufAllocateInfo, &cmdBuffer));

	// If requested, also start the new command buffer
	if (begin)
	{
		VkCommandBufferBeginInfo cmdBufInfo = InitCommandBufferBeginInfo();
		VK_CHECK_RESULT(vkBeginCommandBuffer(cmdBuffer, &cmdBufInfo));
	}

	return cmdBuffer;
}

void VKRadialBlur::flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free)
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
		vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, cmdPool, 1, &commandBuffer);
	}
}

void VKRadialBlur::createPipelineCache()
{
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	VK_CHECK_RESULT(vkCreatePipelineCache(mVulkanDevice->mLogicalDevice, &pipelineCacheCreateInfo, nullptr, &VksPipeLine::mPipelineCache));
}

void VKRadialBlur::prepare()
{
	if (mVulkanDevice->enableDebugMarkers)
	{
		DebugMarkerSetup(mVulkanDevice->mLogicalDevice);
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
                mVulkanDevice,
			mQueue,
			mFrameBuffers,
			mSwapChain.colorFormat,
			depthFormat,
			&width,
			&height,
			shaderStages
			);
		updateTextOverlay();
	}
	///////
	loadAssets();
	prepareOffscreen();
	setupVertexDescriptions();
	prepareUniformBuffers();
	setupDescriptorSetLayout();
	preparePipelines();
	setupDescriptorPool();
	setupDescriptorSet();

	buildCommandBuffers();
	buildOffscreenCommandBuffer();
	prepared = true;

}

VkPipelineShaderStageCreateInfo VKRadialBlur::loadShader(std::string fileName, VkShaderStageFlagBits stage)
{
	VkPipelineShaderStageCreateInfo shaderStage = {};
	shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStage.stage = stage;
#if defined(__ANDROID__)
	shaderStage.module = VksLoadShader(androidApp->activity->assetManager, fileName.c_str(), mVulkanDevice->mLogicalDevice,
									   stage);
#else
	shaderStage.module = loadShader(fileName.c_str(), mVulkanDevice->mLogicalDevice, stage);
#endif
	shaderStage.pName = "main"; // todo : make param
	assert(shaderStage.module != VK_NULL_HANDLE);
	shaderModules.push_back(shaderStage.module);
	return shaderStage;
}

void VKRadialBlur::renderLoop()
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
			if (camera.type != HCamera::CameraType::firstperson)
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

	// Flush device to make sure all resources can be freed 
	vkDeviceWaitIdle(mVulkanDevice->mLogicalDevice);
}

void VKRadialBlur::updateTextOverlay()
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

    // Can be overriden in derived class
    textOverlay->addText("Press \"Button A\" to toggle blur", 5.0f, 85.0f, VulkanTextOverlay::alignLeft);
    textOverlay->addText("Press \"Button X\" to display offscreen texture", 5.0f, 105.0f, VulkanTextOverlay::alignLeft);

	textOverlay->endTextUpdate();
}


void VKRadialBlur::submitFrame()
{
	bool submitTextOverlay = enableTextOverlay && textOverlay->visible;

	if (submitTextOverlay)
	{
		// Wait for color attachment output to finish before rendering the text overlay
		VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		mSubmitInfo.pWaitDstStageMask = &stageFlags;

		// Set semaphores
		// Wait for render complete semaphore
		mSubmitInfo.waitSemaphoreCount = 1;
		mSubmitInfo.pWaitSemaphores = &mRenderComplete;
		// Signal ready with text overlay complete semaphpre
		mSubmitInfo.signalSemaphoreCount = 1;
		mSubmitInfo.pSignalSemaphores = &mTextOverlayComplete;

		// Submit current text overlay command buffer
		mSubmitInfo.commandBufferCount = 1;
		mSubmitInfo.pCommandBuffers = &textOverlay->mCmdBuffers[currentBuffer];
		VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

		// Reset stage mask
		mSubmitInfo.pWaitDstStageMask = &submitPipelineStages;
		// Reset wait and signal semaphores for rendering next frame
		// Wait for swap chain presentation to finish
		mSubmitInfo.waitSemaphoreCount = 1;
		mSubmitInfo.pWaitSemaphores = &mPresentComplete;
		// Signal ready with offscreen semaphore
		mSubmitInfo.signalSemaphoreCount = 1;
		mSubmitInfo.pSignalSemaphores = &mRenderComplete;
	}

	VK_CHECK_RESULT(mSwapChain.queuePresent(mQueue, currentBuffer,
											submitTextOverlay ? mTextOverlayComplete : mRenderComplete));

	VK_CHECK_RESULT(vkQueueWaitIdle(mQueue));
}

VKRadialBlur::VKRadialBlur(bool enableValidation)
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



	zoom = -10.0f;
	rotation = { -16.25f, -28.75f, 0.0f };
	timerSpeed *= 0.5f;
	enableTextOverlay = true;
	title = "Radial blur";
}

VKRadialBlur::~VKRadialBlur()
{

	// Color attachment
	vkDestroyImageView(mVulkanDevice->mLogicalDevice, mOffscreenPass.color.view, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalDevice, mOffscreenPass.color.image, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalDevice, mOffscreenPass.color.mem, nullptr);

	// Depth attachment
	vkDestroyImageView(mVulkanDevice->mLogicalDevice, mOffscreenPass.depth.view, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalDevice, mOffscreenPass.depth.image, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalDevice, mOffscreenPass.depth.mem, nullptr);

	vkDestroyRenderPass(mVulkanDevice->mLogicalDevice, mOffscreenPass.renderPass, nullptr);
	vkDestroySampler(mVulkanDevice->mLogicalDevice, mOffscreenPass.sampler, nullptr);
	vkDestroyFramebuffer(mVulkanDevice->mLogicalDevice, mOffscreenPass.frameBuffer, nullptr);

	vkDestroyPipeline(mVulkanDevice->mLogicalDevice, mRadialBlur.mPipeLine, nullptr);
	vkDestroyPipeline(mVulkanDevice->mLogicalDevice, mPipeLinePhong, nullptr);
	vkDestroyPipeline(mVulkanDevice->mLogicalDevice, mPipeLineColor, nullptr);
	vkDestroyPipeline(mVulkanDevice->mLogicalDevice, mPipeLineOffscreenDisplay, nullptr);

	vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, mRadialBlur.mPipeLayout, nullptr);
	vkDestroyPipelineLayout(mVulkanDevice->mLogicalDevice, mPipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, mDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(mVulkanDevice->mLogicalDevice, mRadialBlur.mDescritptorSetLayout, nullptr);

	mModels.destroy();

	uniformBufferScene.destroy();
	uniformBufferBlurParams.destroy();

	vkFreeCommandBuffers(mVulkanDevice->mLogicalDevice, cmdPool, 1, &mOffscreenPass.commandBuffer);
	vkDestroySemaphore(mVulkanDevice->mLogicalDevice, mOffscreenPass.semaphore, nullptr);

	mTextures.destroy();

	////////
	// Clean up Vulkan resources
	mSwapChain.cleanup();
	if (descriptorPool != VK_NULL_HANDLE)
	{
		vkDestroyDescriptorPool(mVulkanDevice->mLogicalDevice, descriptorPool, nullptr);
	}
	destroyCommandBuffers();
	vkDestroyRenderPass(mVulkanDevice->mLogicalDevice, renderPass, nullptr);
	for (uint32_t i = 0; i < mFrameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(mVulkanDevice->mLogicalDevice, mFrameBuffers[i], nullptr);
	}

	for (auto& shaderModule : shaderModules)
	{
		vkDestroyShaderModule(mVulkanDevice->mLogicalDevice, shaderModule, nullptr);
	}
	vkDestroyImageView(mVulkanDevice->mLogicalDevice, depthStencil.view, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalDevice, depthStencil.image, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalDevice, depthStencil.mem, nullptr);

	vkDestroyPipelineCache(mVulkanDevice->mLogicalDevice, VksPipeLine::mPipelineCache, nullptr);

	vkDestroyCommandPool(mVulkanDevice->mLogicalDevice, cmdPool, nullptr);

	vkDestroySemaphore(mVulkanDevice->mLogicalDevice, mPresentComplete, nullptr);
	vkDestroySemaphore(mVulkanDevice->mLogicalDevice, mRenderComplete, nullptr);
	vkDestroySemaphore(mVulkanDevice->mLogicalDevice, mTextOverlayComplete, nullptr);

	if (enableTextOverlay)
	{
		delete textOverlay;
	}

	delete mVulkanDevice;

	if (settings.validation)
	{
		DebugFreeDebugCallback(instance);
	}

	vkDestroyInstance(instance, nullptr);


}

void VKRadialBlur::initVulkan()
{
	VkResult err;

	// Vulkan instance
	err = createInstance(settings.validation);
	if (err)
	{
		VksExitFatal("Could not create Vulkan instance : \n" + VksErrorString(err), "Fatal error");
	}

#if defined(__ANDROID__)
	loadVulkanFunctions(instance);
#endif

	// If requested, we enable the default validation layers for debugging
	if (settings.validation)
	{
		// The report flags determine what type of messages for the layers will be displayed
		// For validating (debugging) an appplication the error and warning bits should suffice
		VkDebugReportFlagsEXT debugReportFlags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
		// Additional flags include performance info, loader and layer debug messages, etc.
		DebugSetupDebugging(instance, debugReportFlags, VK_NULL_HANDLE);
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
		VksExitFatal("Could not enumerate physical devices : \n" + VksErrorString(err), "Fatal error");
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

	// Vulkan device creation
	// This is handled by a separate class that gets a logical device representation
	// and encapsulates functions related to a device
    mVulkanDevice = new VulkanDevice(physicalDevice);
	VkResult res = mVulkanDevice->createLogicalDevice(enabledFeatures, enabledExtensions);
	if (res != VK_SUCCESS) {
		VksExitFatal("Could not create Vulkan device: \n" + VksErrorString(res), "Fatal error");
	}

	// Get a graphics queue from the device
	vkGetDeviceQueue(mVulkanDevice->mLogicalDevice, mVulkanDevice->queueFamilyIndices.graphics, 0, &mQueue);

	// Find a suitable depth format
	VkBool32 validDepthFormat = VksGetSupportedDepthFormat(physicalDevice, &depthFormat);
	assert(validDepthFormat);

	mSwapChain.connect(instance, physicalDevice, mVulkanDevice->mLogicalDevice);

	// Create synchronization objects
	VkSemaphoreCreateInfo semaphoreCreateInfo = InitSemaphoreCreateInfo();
	// Create a semaphore used to synchronize image presentation
	// Ensures that the image is displayed before we start submitting new commands to the queu
	VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &mPresentComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands have been sumbitted and executed
	VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &mRenderComplete));
	// Create a semaphore used to synchronize command submission
	// Ensures that the image is not presented until all commands for the text overlay have been sumbitted and executed
	// Will be inserted after the render complete semaphore if the text overlay is enabled
	VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &mTextOverlayComplete));

	// Set up submit info structure
	// Semaphores will stay the same during application lifetime
	// Command buffer submission info is set by each example
	mSubmitInfo = InitSubmitInfo();
	mSubmitInfo.pWaitDstStageMask = &submitPipelineStages;
	mSubmitInfo.waitSemaphoreCount = 1;
	mSubmitInfo.pWaitSemaphores = &mPresentComplete;
	mSubmitInfo.signalSemaphoreCount = 1;
	mSubmitInfo.pSignalSemaphores = &mRenderComplete;

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

int32_t VKRadialBlur::handleAppInput(struct android_app* app, AInputEvent* event)
{
	VKRadialBlur* vulkanExample = reinterpret_cast<VKRadialBlur*>(app->userData);
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

void VKRadialBlur::handleAppCommand(android_app * app, int32_t cmd)
{
	assert(app->userData != NULL);
	VKRadialBlur* vulkanExample = reinterpret_cast<VKRadialBlur*>(app->userData);
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
		vulkanExample->mSwapChain.cleanup();
		break;
	}
}


void VKRadialBlur::viewChanged()
{
	// Can be overrdiden in derived class
	updateUniformBuffersScene();
}

void VKRadialBlur::keyPressed(uint32_t keyCode)
{
	switch (keyCode)
	{
		case KEY_B:
		case GAMEPAD_BUTTON_A:
			toggleBlur();
			break;
		case KEY_T:
		case GAMEPAD_BUTTON_X:
			toggleTextureDisplay();
			break;
	}
}

void VKRadialBlur::createCommandPool()
{
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = mSwapChain.queueNodeIndex;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	VK_CHECK_RESULT(vkCreateCommandPool(mVulkanDevice->mLogicalDevice, &cmdPoolInfo, nullptr, &cmdPool));
}

void VKRadialBlur::setupDepthStencil()
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

	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalDevice, &image, nullptr, &depthStencil.image));
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalDevice, depthStencil.image, &memReqs);
	mem_alloc.allocationSize = memReqs.size;
	mem_alloc.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &mem_alloc, nullptr, &depthStencil.mem));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalDevice, depthStencil.image, depthStencil.mem, 0));

	depthStencilView.image = depthStencil.image;
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalDevice, &depthStencilView, nullptr, &depthStencil.view));
}

void VKRadialBlur::setupFrameBuffer()
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
    mFrameBuffers.resize(mSwapChain.mImageCount);
	for (uint32_t i = 0; i < mFrameBuffers.size(); i++)
	{
		attachments[0] = mSwapChain.mBuffers[i].view;
		VK_CHECK_RESULT(vkCreateFramebuffer(mVulkanDevice->mLogicalDevice, &frameBufferCreateInfo, nullptr, &mFrameBuffers[i]));
	}
}

void VKRadialBlur::setupRenderPass()
{
	std::array<VkAttachmentDescription, 2> attachments = {};
	// Color attachment
	attachments[0].format = mSwapChain.colorFormat;
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

	VK_CHECK_RESULT(vkCreateRenderPass(mVulkanDevice->mLogicalDevice, &renderPassInfo, nullptr, &renderPass));
}

void VKRadialBlur::windowResize()
{
	if (!prepared)
	{
		return;
	}
	prepared = false;

	// Ensure all operations on the device have been finished before destroying resources
	vkDeviceWaitIdle(mVulkanDevice->mLogicalDevice);

	// Recreate swap chain
	width = destWidth;
	height = destHeight;
	setupSwapChain();

	// Recreate the frame buffers

	vkDestroyImageView(mVulkanDevice->mLogicalDevice, depthStencil.view, nullptr);
	vkDestroyImage(mVulkanDevice->mLogicalDevice, depthStencil.image, nullptr);
	vkFreeMemory(mVulkanDevice->mLogicalDevice, depthStencil.mem, nullptr);
	setupDepthStencil();
	
	for (uint32_t i = 0; i < mFrameBuffers.size(); i++)
	{
		vkDestroyFramebuffer(mVulkanDevice->mLogicalDevice, mFrameBuffers[i], nullptr);
	}
	setupFrameBuffer();

	// Command buffers need to be recreated as they may store
	// references to the recreated frame buffer
	destroyCommandBuffers();
	createCommandBuffers();
	buildCommandBuffers();

	vkDeviceWaitIdle(mVulkanDevice->mLogicalDevice);

	if (enableTextOverlay)
	{
		textOverlay->reallocateCommandBuffers();
		updateTextOverlay();
	}

	camera.updateAspectRatio((float)width / (float)height);

	viewChanged();

	prepared = true;
}

void VKRadialBlur::initSwapchain()
{
#if defined(_WIN32)
	mSwapChain.initSurface(windowInstance, window);
#elif defined(__ANDROID__)	
	mSwapChain.initSurface(androidApp->window);
#elif defined(_DIRECT2DISPLAY)
	mSwapChain.initSurface(width, height);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
	mSwapChain.initSurface(display, surface);
#elif defined(__linux__)
	mSwapChain.initSurface(connection, window);
#endif
}

void VKRadialBlur::setupSwapChain()
{
	mSwapChain.create(&width, &height, settings.vsync);
}

////
void VKRadialBlur::prepareOffscreen()
{
	mOffscreenPass.width = FB_DIM;
	mOffscreenPass.height = FB_DIM;

	// Find a suitable depth format
	VkFormat fbDepthFormat;
	VkBool32 validDepthFormat = VksGetSupportedDepthFormat(physicalDevice, &fbDepthFormat);
	assert(validDepthFormat);

	// Color attachment
	VkImageCreateInfo image = InitImageCreateInfo();
	image.imageType = VK_IMAGE_TYPE_2D;
	image.format = FB_COLOR_FORMAT;
	image.extent.width = mOffscreenPass.width;
	image.extent.height = mOffscreenPass.height;
	image.extent.depth = 1;
	image.mipLevels = 1;
	image.arrayLayers = 1;
	image.samples = VK_SAMPLE_COUNT_1_BIT;
	image.tiling = VK_IMAGE_TILING_OPTIMAL;
	// We will sample directly from the color attachment
	image.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	VkMemoryAllocateInfo memAlloc = InitMemoryAllocateInfo();
	VkMemoryRequirements memReqs;

	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalDevice, &image, nullptr, &mOffscreenPass.color.image));
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalDevice, mOffscreenPass.color.image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mOffscreenPass.color.mem));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalDevice, mOffscreenPass.color.image, mOffscreenPass.color.mem, 0));

	VkImageViewCreateInfo colorImageView = InitImageViewCreateInfo();
	colorImageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	colorImageView.format = FB_COLOR_FORMAT;
	colorImageView.subresourceRange = {};
	colorImageView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorImageView.subresourceRange.baseMipLevel = 0;
	colorImageView.subresourceRange.levelCount = 1;
	colorImageView.subresourceRange.baseArrayLayer = 0;
	colorImageView.subresourceRange.layerCount = 1;
	colorImageView.image = mOffscreenPass.color.image;
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalDevice, &colorImageView, nullptr, &mOffscreenPass.color.view));

	// Create sampler to sample from the attachment in the fragment shader
	VkSamplerCreateInfo samplerInfo = InitSamplerCreateInfo();
	samplerInfo.magFilter = VK_FILTER_LINEAR;
	samplerInfo.minFilter = VK_FILTER_LINEAR;
	samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerInfo.addressModeV = samplerInfo.addressModeU;
	samplerInfo.addressModeW = samplerInfo.addressModeU;
	samplerInfo.mipLodBias = 0.0f;
	samplerInfo.maxAnisotropy = 0;
	samplerInfo.minLod = 0.0f;
	samplerInfo.maxLod = 1.0f;
	samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	VK_CHECK_RESULT(vkCreateSampler(mVulkanDevice->mLogicalDevice, &samplerInfo, nullptr, &mOffscreenPass.sampler));

	// Depth stencil attachment
	image.format = fbDepthFormat;
	image.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

	VK_CHECK_RESULT(vkCreateImage(mVulkanDevice->mLogicalDevice, &image, nullptr, &mOffscreenPass.depth.image));
	vkGetImageMemoryRequirements(mVulkanDevice->mLogicalDevice, mOffscreenPass.depth.image, &memReqs);
	memAlloc.allocationSize = memReqs.size;
	memAlloc.memoryTypeIndex = mVulkanDevice->getMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
	VK_CHECK_RESULT(vkAllocateMemory(mVulkanDevice->mLogicalDevice, &memAlloc, nullptr, &mOffscreenPass.depth.mem));
	VK_CHECK_RESULT(vkBindImageMemory(mVulkanDevice->mLogicalDevice, mOffscreenPass.depth.image, mOffscreenPass.depth.mem, 0));

	VkImageViewCreateInfo depthStencilView = InitImageViewCreateInfo();
	depthStencilView.viewType = VK_IMAGE_VIEW_TYPE_2D;
	depthStencilView.format = fbDepthFormat;
	depthStencilView.flags = 0;
	depthStencilView.subresourceRange = {};
	depthStencilView.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	depthStencilView.subresourceRange.baseMipLevel = 0;
	depthStencilView.subresourceRange.levelCount = 1;
	depthStencilView.subresourceRange.baseArrayLayer = 0;
	depthStencilView.subresourceRange.layerCount = 1;
	depthStencilView.image = mOffscreenPass.depth.image;
	VK_CHECK_RESULT(vkCreateImageView(mVulkanDevice->mLogicalDevice, &depthStencilView, nullptr, &mOffscreenPass.depth.view));

	// Create a separate render pass for the offscreen rendering as it may differ from the one used for scene rendering

	std::array<VkAttachmentDescription, 2> attchmentDescriptions = {};
	// Color attachment
	attchmentDescriptions[0].format = FB_COLOR_FORMAT;
	attchmentDescriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attchmentDescriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attchmentDescriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attchmentDescriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attchmentDescriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attchmentDescriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attchmentDescriptions[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	// Depth attachment
	attchmentDescriptions[1].format = fbDepthFormat;
	attchmentDescriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attchmentDescriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attchmentDescriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attchmentDescriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attchmentDescriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attchmentDescriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	attchmentDescriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference depthReference = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };

	VkSubpassDescription subpassDescription = {};
	subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpassDescription.colorAttachmentCount = 1;
	subpassDescription.pColorAttachments = &colorReference;
	subpassDescription.pDepthStencilAttachment = &depthReference;

	// Use subpass dependencies for layout transitions
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

	// Create the actual renderpass
	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.attachmentCount = static_cast<uint32_t>(attchmentDescriptions.size());
	renderPassInfo.pAttachments = attchmentDescriptions.data();
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDescription;
	renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
	renderPassInfo.pDependencies = dependencies.data();

	VK_CHECK_RESULT(vkCreateRenderPass(mVulkanDevice->mLogicalDevice, &renderPassInfo, nullptr, &mOffscreenPass.renderPass));

	VkImageView attachments[2];
	attachments[0] = mOffscreenPass.color.view;
	attachments[1] = mOffscreenPass.depth.view;

	VkFramebufferCreateInfo fbufCreateInfo = InitFramebufferCreateInfo();
	fbufCreateInfo.renderPass = mOffscreenPass.renderPass;
	fbufCreateInfo.attachmentCount = 2;
	fbufCreateInfo.pAttachments = attachments;
	fbufCreateInfo.width = mOffscreenPass.width;
	fbufCreateInfo.height = mOffscreenPass.height;
	fbufCreateInfo.layers = 1;

	VK_CHECK_RESULT(vkCreateFramebuffer(mVulkanDevice->mLogicalDevice, &fbufCreateInfo, nullptr, &mOffscreenPass.frameBuffer));

	// Fill a descriptor for later use in a descriptor set
	mOffscreenPass.descriptor.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	mOffscreenPass.descriptor.imageView = mOffscreenPass.color.view;
	mOffscreenPass.descriptor.sampler = mOffscreenPass.sampler;
}

// Sets up the command buffer that renders the scene to the offscreen frame buffer
void VKRadialBlur::buildOffscreenCommandBuffer()
{
	if (mOffscreenPass.commandBuffer == VK_NULL_HANDLE)
	{
		mOffscreenPass.commandBuffer = createCommandBuffer(VK_COMMAND_BUFFER_LEVEL_PRIMARY, false);
	}
	if (mOffscreenPass.semaphore == VK_NULL_HANDLE)
	{
		VkSemaphoreCreateInfo semaphoreCreateInfo = InitSemaphoreCreateInfo();
		VK_CHECK_RESULT(vkCreateSemaphore(mVulkanDevice->mLogicalDevice, &semaphoreCreateInfo, nullptr, &mOffscreenPass.semaphore));
	}

	VkCommandBufferBeginInfo cmdBufInfo = InitCommandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 0.0f } };
	clearValues[1].depthStencil = { 1.0f, 0 };

	VkRenderPassBeginInfo renderPassBeginInfo = InitRenderPassBeginInfo();
	renderPassBeginInfo.renderPass = mOffscreenPass.renderPass;
	renderPassBeginInfo.framebuffer = mOffscreenPass.frameBuffer;
	renderPassBeginInfo.renderArea.extent.width = mOffscreenPass.width;
	renderPassBeginInfo.renderArea.extent.height = mOffscreenPass.height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	VK_CHECK_RESULT(vkBeginCommandBuffer(mOffscreenPass.commandBuffer, &cmdBufInfo));

	VkViewport viewport = InitViewport((float)mOffscreenPass.width, (float)mOffscreenPass.height, 0.0f, 1.0f);
	vkCmdSetViewport(mOffscreenPass.commandBuffer, 0, 1, &viewport);

	VkRect2D scissor = InitRect2D(mOffscreenPass.width, mOffscreenPass.height, 0, 0);
	vkCmdSetScissor(mOffscreenPass.commandBuffer, 0, 1, &scissor);

	vkCmdBeginRenderPass(mOffscreenPass.commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

	vkCmdBindDescriptorSets(mOffscreenPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSet, 0, NULL);
	vkCmdBindPipeline(mOffscreenPass.commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeLineColor);

	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(mOffscreenPass.commandBuffer, VERTEX_BUFFER_BIND_ID, 1, &mModels.vertices.mBuffer, offsets);
	vkCmdBindIndexBuffer(mOffscreenPass.commandBuffer, mModels.indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(mOffscreenPass.commandBuffer, mModels.indexCount, 1, 0, 0, 0);

	vkCmdEndRenderPass(mOffscreenPass.commandBuffer);

	VK_CHECK_RESULT(vkEndCommandBuffer(mOffscreenPass.commandBuffer));

}


////
void VKRadialBlur::reBuildCommandBuffers()
{
	if (!checkCommandBuffers())
	{
		destroyCommandBuffers();
		createCommandBuffers();
	}
	buildCommandBuffers();
}


void VKRadialBlur::buildCommandBuffers()
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


	for (int32_t i = 0; i < mDrawCmdBuffers.size(); ++i)
	{
        renderPassBeginInfo.framebuffer = mFrameBuffers[i];
        VK_CHECK_RESULT(vkBeginCommandBuffer(mDrawCmdBuffers[i], &cmdBufInfo));
        vkCmdBeginRenderPass(mDrawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport = InitViewport((float)width, (float)height, 0.0f, 1.0f);
        vkCmdSetViewport(mDrawCmdBuffers[i], 0, 1, &viewport);

        VkRect2D scissor = InitRect2D(width, height, 0, 0);
        vkCmdSetScissor(mDrawCmdBuffers[i], 0, 1, &scissor);

		VkDeviceSize offsets[1] = { 0 };

		// 3D scene
		vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, 1, &mDescriptorSet, 0, NULL);
		vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipeLinePhong);

		vkCmdBindVertexBuffers(mDrawCmdBuffers[i], VERTEX_BUFFER_BIND_ID, 1, &mModels.vertices.mBuffer, offsets);
		vkCmdBindIndexBuffer(mDrawCmdBuffers[i], mModels.indices.mBuffer, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(mDrawCmdBuffers[i], mModels.indexCount, 1, 0, 0, 0);

		// Fullscreen triangle (clipped to a quad) with radial blur
		if (mBlur)
		{
			vkCmdBindDescriptorSets(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mRadialBlur.mPipeLayout, 0, 1, &mRadialBlur.mDescriptorSet, 0, NULL);
			vkCmdBindPipeline(mDrawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, (mDisplayTexture) ? mPipeLineOffscreenDisplay : mRadialBlur.mPipeLine);
			vkCmdDraw(mDrawCmdBuffers[i], 3, 1, 0, 0);
		}

		vkCmdEndRenderPass(mDrawCmdBuffers[i]);

		VK_CHECK_RESULT(vkEndCommandBuffer(mDrawCmdBuffers[i]));
	}


}

void VKRadialBlur::loadAssets()
{
	mModels.loadFromFile(getAssetPath() + "models/glowsphere.dae", vertexLayout, 0.05f, mVulkanDevice, mQueue);
	mTextures.loadFromFile(getAssetPath() + "textures/particle_gradient_rgba.ktx", VK_FORMAT_R8G8B8A8_UNORM, mVulkanDevice, mQueue);
}

void VKRadialBlur::setupVertexDescriptions()
{
	// Binding description
	vertices.bindingDescriptions.resize(1);
	vertices.bindingDescriptions[0] =
			InitVertexInputBindingDescription(
					VERTEX_BUFFER_BIND_ID,
					vertexLayout.stride(),
					VK_VERTEX_INPUT_RATE_VERTEX);

	// Attribute descriptions
	vertices.attributeDescriptions.resize(4);
	// Location 0 : Position
	vertices.attributeDescriptions[0] =
			InitVertexInputAttributeDescription(
					VERTEX_BUFFER_BIND_ID,
					0,
					VK_FORMAT_R32G32B32_SFLOAT,
					0);
	// Location 1 : Texture coordinates
	vertices.attributeDescriptions[1] =
			InitVertexInputAttributeDescription(
					VERTEX_BUFFER_BIND_ID,
					1,
					VK_FORMAT_R32G32_SFLOAT,
					sizeof(float) * 3);
	// Location 2 : Color
	vertices.attributeDescriptions[2] =
			InitVertexInputAttributeDescription(
					VERTEX_BUFFER_BIND_ID,
					2,
					VK_FORMAT_R32G32B32_SFLOAT,
					sizeof(float) * 5);
	// Location 3 : Normal
	vertices.attributeDescriptions[3] =
			InitVertexInputAttributeDescription(
					VERTEX_BUFFER_BIND_ID,
					3,
					VK_FORMAT_R32G32B32_SFLOAT,
					sizeof(float) * 8);

	vertices.inputState = InitPipelineVertexInputStateCreateInfo();
	vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
	vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
	vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
	vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
}

void VKRadialBlur::setupDescriptorPool()
{
	// Example uses three ubos and one image sampler
	std::vector<VkDescriptorPoolSize> poolSizes =
			{
					InitDescriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4),
					InitDescriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6)
			};

	VkDescriptorPoolCreateInfo descriptorPoolInfo =
			InitDescriptorPoolCreateInfo(
					poolSizes.size(),
					poolSizes.data(),
					2);

	VK_CHECK_RESULT(vkCreateDescriptorPool(mVulkanDevice->mLogicalDevice, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void VKRadialBlur::setupDescriptorSetLayout()
{
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;
	VkDescriptorSetLayoutCreateInfo descriptorLayout;
	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo;

	// Scene rendering
	setLayoutBindings =
			{
					// Binding 0: Vertex shader uniform buffer
					InitDescriptorSetLayoutBinding(
							VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
							VK_SHADER_STAGE_VERTEX_BIT,
							0),
					// Binding 1: Fragment shader image sampler
					InitDescriptorSetLayoutBinding(
							VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							VK_SHADER_STAGE_FRAGMENT_BIT,
							1),
					// Binding 2: Fragment shader uniform buffer
					InitDescriptorSetLayoutBinding(
							VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
							VK_SHADER_STAGE_FRAGMENT_BIT,
							2)
			};
	descriptorLayout = InitDescriptorSetLayoutCreateInfo(
			setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalDevice, &descriptorLayout, nullptr, &mDescriptorSetLayout));
	pPipelineLayoutCreateInfo = InitPipelineLayoutCreateInfo(&mDescriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalDevice, &pPipelineLayoutCreateInfo, nullptr, &mPipelineLayout));

	// Fullscreen radial blur
	setLayoutBindings =
			{
					// Binding 0 : Vertex shader uniform buffer
					InitDescriptorSetLayoutBinding(
							VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
							VK_SHADER_STAGE_FRAGMENT_BIT,
							0),
					// Binding 0: Fragment shader image sampler
					InitDescriptorSetLayoutBinding(
							VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							VK_SHADER_STAGE_FRAGMENT_BIT,
							1)
			};
	descriptorLayout = InitDescriptorSetLayoutCreateInfo(
			setLayoutBindings.data(), static_cast<uint32_t>(setLayoutBindings.size()));
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(mVulkanDevice->mLogicalDevice, &descriptorLayout, nullptr, &mRadialBlur.mDescritptorSetLayout));
	pPipelineLayoutCreateInfo = InitPipelineLayoutCreateInfo(
			&mRadialBlur.mDescritptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(mVulkanDevice->mLogicalDevice, &pPipelineLayoutCreateInfo, nullptr, &mRadialBlur.mPipeLayout));
}

void VKRadialBlur::setupDescriptorSet()
{
	VkDescriptorSetAllocateInfo descriptorSetAllocInfo;

	// Scene rendering
	descriptorSetAllocInfo = InitDescriptorSetAllocateInfo(descriptorPool, &mDescriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &descriptorSetAllocInfo, &mDescriptorSet));

	std::vector<VkWriteDescriptorSet> offScreenWriteDescriptorSets =
			{
					// Binding 0: Vertex shader uniform buffer
					InitWriteDescriptorSet(
							mDescriptorSet,
							VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
							0,
							&uniformBufferScene.mDescriptor),
					// Binding 1: Color gradient sampler
					InitWriteDescriptorSet(
							mDescriptorSet,
							VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
							1,
							&mTextures.descriptor),
			};
	vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, offScreenWriteDescriptorSets.size(), offScreenWriteDescriptorSets.data(), 0, NULL);

	// Fullscreen radial blur
	descriptorSetAllocInfo = InitDescriptorSetAllocateInfo(descriptorPool, &mRadialBlur.mDescritptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(mVulkanDevice->mLogicalDevice, &descriptorSetAllocInfo, &mRadialBlur.mDescriptorSet));

	std::vector<VkWriteDescriptorSet> writeDescriptorSets =
            {
                    // Binding 0: Vertex shader uniform buffer
                    InitWriteDescriptorSet(mRadialBlur.mDescriptorSet,
                                           VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 0,
                                           &uniformBufferBlurParams.mDescriptor),
                    // Binding 0: Fragment shader texture sampler
                    InitWriteDescriptorSet(mRadialBlur.mDescriptorSet,
                                           VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                                           &mOffscreenPass.descriptor),
            };

	vkUpdateDescriptorSets(mVulkanDevice->mLogicalDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
}

void VKRadialBlur::preparePipelines()
{
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
			InitPipelineInputAssemblyStateCreateInfo(
					VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
					0,
					VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationState =
			InitPipelineRasterizationStateCreateInfo(
					VK_POLYGON_MODE_FILL,
					VK_CULL_MODE_NONE,
					VK_FRONT_FACE_COUNTER_CLOCKWISE,
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
			InitPipelineMultisampleStateCreateInfo(
					VK_SAMPLE_COUNT_1_BIT,
					0);

	std::vector<VkDynamicState> dynamicStateEnables = {
			VK_DYNAMIC_STATE_VIEWPORT,
			VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState =
			InitPipelineDynamicStateCreateInfo(
					dynamicStateEnables.data(),
					dynamicStateEnables.size(),
					0);

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
			InitPipelineCreateInfo(
					mRadialBlur.mPipeLayout,
					renderPass,
					0);

	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.stageCount = shaderStages.size();
	pipelineCreateInfo.pStages = shaderStages.data();

	// Radial blur pipeline
	shaderStages[0] = loadShader(getAssetPath() + "shaders/radialblur/radialblur.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/radialblur/radialblur.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	// Empty vertex input state
	VkPipelineVertexInputStateCreateInfo emptyInputState = InitPipelineVertexInputStateCreateInfo();
	pipelineCreateInfo.pVertexInputState = &emptyInputState;
	pipelineCreateInfo.layout = mRadialBlur.mPipeLayout;
	// Additive blending
	blendAttachmentState.colorWriteMask = 0xF;
	blendAttachmentState.blendEnable = VK_TRUE;
	blendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	blendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	blendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	blendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, VksPipeLine::mPipelineCache, 1, &pipelineCreateInfo, nullptr, &mRadialBlur.mPipeLine));

	// No blending (for debug display)
	blendAttachmentState.blendEnable = VK_FALSE;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, VksPipeLine::mPipelineCache, 1, &pipelineCreateInfo, nullptr, &mPipeLineOffscreenDisplay));

	// Phong pass
	pipelineCreateInfo.layout = mPipelineLayout;
	shaderStages[0] = loadShader(getAssetPath() + "shaders/radialblur/phongpass.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/radialblur/phongpass.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	pipelineCreateInfo.pVertexInputState = &vertices.inputState;
	blendAttachmentState.blendEnable = VK_FALSE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, VksPipeLine::mPipelineCache, 1, &pipelineCreateInfo, nullptr, &mPipeLinePhong));

	// Color only pass (offscreen blur base)
	shaderStages[0] = loadShader(getAssetPath() + "shaders/radialblur/colorpass.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader(getAssetPath() + "shaders/radialblur/colorpass.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);
	pipelineCreateInfo.renderPass = mOffscreenPass.renderPass;
	VK_CHECK_RESULT(vkCreateGraphicsPipelines(mVulkanDevice->mLogicalDevice, VksPipeLine::mPipelineCache, 1, &pipelineCreateInfo, nullptr, &mPipeLineColor));
}

// Prepare and initialize uniform buffer containing shader uniforms
void VKRadialBlur::prepareUniformBuffers()
{
	// Phong and color pass vertex shader uniform buffer
	VK_CHECK_RESULT(mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBufferScene,
			sizeof(uboScene)));

	// Fullscreen radial blur parameters
	VK_CHECK_RESULT(mVulkanDevice->createBuffer(
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			&uniformBufferBlurParams,
			sizeof(uboBlurParams),
			&uboBlurParams));

	// Map persistent
	VK_CHECK_RESULT(uniformBufferScene.map());
	VK_CHECK_RESULT(uniformBufferBlurParams.map());

	updateUniformBuffersScene();
}

// Update uniform buffers for rendering the 3D scene
void VKRadialBlur::updateUniformBuffersScene()
{
	uboScene.projection = glm::perspective(glm::radians(45.0f), (float)width / (float)height, 1.0f, 256.0f);
	glm::mat4 viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, zoom));

	uboScene.model = glm::mat4();
	uboScene.model = viewMatrix * glm::translate(uboScene.model, cameraPos);
	uboScene.model = glm::rotate(uboScene.model, glm::radians(rotation.x), glm::vec3(1.0f, 0.0f, 0.0f));
	uboScene.model = glm::rotate(uboScene.model, glm::radians(rotation.y), glm::vec3(0.0f, 1.0f, 0.0f));
	uboScene.model = glm::rotate(uboScene.model, glm::radians(timer * 360.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	uboScene.model = glm::rotate(uboScene.model, glm::radians(rotation.z), glm::vec3(0.0f, 0.0f, 1.0f));

	if (!paused)
	{
		uboScene.gradientPos += frameTimer * 0.1f;
	}

	memcpy(uniformBufferScene.mMapped, &uboScene, sizeof(uboScene));
}


void VKRadialBlur::draw()
{
	// Acquire the next image from the swap chaing
	VK_CHECK_RESULT(mSwapChain.acquireNextImage(mPresentComplete, &currentBuffer));

	// Offscreen rendering

	// Wait for swap chain presentation to finish
	mSubmitInfo.pWaitSemaphores = &mPresentComplete;
	// Signal ready with offscreen semaphore
	mSubmitInfo.pSignalSemaphores = &mOffscreenPass.semaphore;

	// Submit work
	mSubmitInfo.commandBufferCount = 1;
	mSubmitInfo.pCommandBuffers = &mOffscreenPass.commandBuffer;
	VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

	// Scene rendering

	// Wait for offscreen semaphore
	mSubmitInfo.pWaitSemaphores = &mOffscreenPass.semaphore;
	// Signal ready with render complete semaphpre
	mSubmitInfo.pSignalSemaphores = &mRenderComplete;

	// Submit work
	mSubmitInfo.pCommandBuffers = &mDrawCmdBuffers[currentBuffer];
	VK_CHECK_RESULT(vkQueueSubmit(mQueue, 1, &mSubmitInfo, VK_NULL_HANDLE));

	submitFrame();
}


void VKRadialBlur::render()
{
	if (!prepared)
		return;
	draw();
	if (!paused)
	{
		updateUniformBuffersScene();
	}
}


void VKRadialBlur::toggleBlur()
{
	mBlur = !mBlur;
	updateUniformBuffersScene();
	reBuildCommandBuffers();
}

void VKRadialBlur::toggleTextureDisplay()
{
	mDisplayTexture = !mDisplayTexture;
	reBuildCommandBuffers();
}


VKRadialBlur *vulkanExample;
void android_main(android_app* state)
{
	app_dummy();
	vulkanExample = new VKRadialBlur(false);
	state->userData = vulkanExample;
	state->onAppCmd = VKRadialBlur::handleAppCommand;
	state->onInputEvent = VKRadialBlur::handleAppInput;
	androidApp = state;
	vulkanExample->renderLoop();
	delete(vulkanExample);
}