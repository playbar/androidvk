/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#ifdef _WIN32
#pragma comment(linker, "/subsystem:windows")
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#elif defined(__ANDROID__)
#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <sys/system_properties.h>
#include "vulkanandroid.h"
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
#include <wayland-client.h>
#elif defined(__linux__)
#include <xcb/xcb.h>
#endif

#include <iostream>
#include <chrono>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <thread>
#include <random>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <array>

#include "vulkan/vulkan.h"

#include "keycodes.hpp"
#include "VulkanTools.h"
#include "VulkanDebug.h"

#include "VulkanInitializers.hpp"
#include "VulkanDevice.hpp"
#include "VulkanSwapChain.hpp"
#include "VulkanTextOverlay.hpp"
#include "camera.hpp"

#include <vulkan/vulkan.h>
#include "threadpool.hpp"
#include "frustum.hpp"

#include "VulkanModel.hpp"

#define VERTEX_BUFFER_BIND_ID 0

class VkMultiThreading
{
private:	
	float fpsTimer = 0.0f;
	std::string getWindowTitle();
	bool viewUpdated = false;
	uint32_t destWidth;
	uint32_t destHeight;
	bool resizing = false;
	void windowResize();
protected:
	float frameTimer = 1.0f;
	uint32_t frameCounter = 0;
	uint32_t lastFPS = 0;
	VkInstance instance;
	VkPhysicalDevice physicalDevice;
	VkPhysicalDeviceProperties deviceProperties;
	VkPhysicalDeviceFeatures deviceFeatures;
	VkPhysicalDeviceMemoryProperties deviceMemoryProperties;

	VkPhysicalDeviceFeatures enabledFeatures{};
	std::vector<const char*> enabledExtensions;
	VkDevice device;
	vks::VulkanDevice *vulkanDevice;
	VkQueue queue;
	VkFormat depthFormat;
	VkCommandPool cmdPool;
	VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo;
	std::vector<VkCommandBuffer> drawCmdBuffers;
	VkRenderPass renderPass;
	std::vector<VkFramebuffer>frameBuffers;
	uint32_t currentBuffer = 0;
	VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
	std::vector<VkShaderModule> shaderModules;
	VkPipelineCache pipelineCache;
	VulkanSwapChain swapChain;
	struct {
		VkSemaphore presentComplete;
		VkSemaphore renderComplete;
		VkSemaphore textOverlayComplete;
	} semaphores;
	// Simple texture loader
	//vks::tools::VulkanTextureLoader *textureLoader = nullptr;
	// Returns the base asset path (for shaders, models, textures) depending on the os
	const std::string getAssetPath();
public: 
	bool prepared = false;
	uint32_t width = 1280;
	uint32_t height = 720;

	/** @brief Example settings that can be changed e.g. by command line arguments */
	struct Settings {
		/** @brief Activates validation layers (and message output) when set to true */
		bool validation = false;
		/** @brief Set to true if fullscreen mode has been requested via command line */
		bool fullscreen = false;
		/** @brief Set to true if v-sync will be forced for the swapchain */
		bool vsync = false;
	} settings;

	VkClearColorValue defaultClearColor = { { 0.025f, 0.025f, 0.025f, 1.0f } };

	float zoom = 0;

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float timer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float timerSpeed = 0.25f;
	
	bool paused = false;

	bool enableTextOverlay = false;
	VulkanTextOverlay *textOverlay;

	// Use to adjust mouse rotation speed
	float rotationSpeed = 1.0f;
	// Use to adjust mouse zoom speed
	float zoomSpeed = 1.0f;

	Camera camera;

	glm::vec3 rotation = glm::vec3();
	glm::vec3 cameraPos = glm::vec3();
	glm::vec2 mousePos;

	std::string title = "VkMultiThread";
	std::string name = "VkMultiThread";

	struct 
	{
		VkImage image;
		VkDeviceMemory mem;
		VkImageView view;
	} depthStencil;

	// Gamepad state (only one pad supported)
	struct
	{
		glm::vec2 axisLeft = glm::vec2(0.0f);
		glm::vec2 axisRight = glm::vec2(0.0f);
	} gamePadState;


	// true if application has focused, false if moved to background
	bool focused = false;
	struct TouchPos {
		int32_t x;
		int32_t y;
	} touchPos;
	bool touchDown = false;
	double touchTimer = 0.0;
	/** @brief Product model and manufacturer of the Android device (via android.Product*) */
	std::string androidProduct;

	// Default ctor
	VkMultiThreading(bool enableValidation);

	// dtor
	~VkMultiThreading();

	// Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
	void initVulkan();

	static int32_t handleAppInput(struct android_app* app, AInputEvent* event);
	static void handleAppCommand(android_app* app, int32_t cmd);


	virtual VkResult createInstance(bool enableValidation);
	virtual void keyPressed(uint32_t keyCode);
	virtual void windowResized();
	virtual void buildCommandBuffers();

	void createCommandPool();
	virtual void setupDepthStencil();
	virtual void setupFrameBuffer();
	virtual void setupRenderPass();
	virtual void getEnabledFeatures();

	void initSwapchain();
	void setupSwapChain();

	bool checkCommandBuffers();
	void createCommandBuffers();
	void destroyCommandBuffers();

	// Command buffer creation
	// Creates and returns a new command buffer
	VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);
	// End the command buffer, submit it to the queue and free (if requested)
	// Note : Waits for the queue to become idle
	void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);

	// Create a cache pool for rendering pipelines
	void createPipelineCache();

	// Prepare commonly used Vulkan functions
	virtual void prepare();

	// Load a SPIR-V shader
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);
	
	// Start the main render loop
	void renderLoop();

	void updateTextOverlay();


	// Submit the frames' workload 
	// - Submits the text overlay (if enabled)
	void submitFrame();

	///////////////
	// Vertex layout for the models
	vks::VertexLayout vertexLayout = vks::VertexLayout({
															   vks::VERTEX_COMPONENT_POSITION,
															   vks::VERTEX_COMPONENT_NORMAL,
															   vks::VERTEX_COMPONENT_COLOR,
													   });

	struct {
		vks::Model ufo;
		vks::Model skysphere;
	} models;

	struct {
		VkPipelineVertexInputStateCreateInfo inputState;
		std::vector<VkVertexInputBindingDescription> bindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
	} vertices;

	// Shared matrices used for thread push constant blocks
	struct {
		glm::mat4 projection;
		glm::mat4 view;
	} matrices;

	struct {
		VkPipeline phong;
		VkPipeline starsphere;
	} pipelines;

	VkPipelineLayout pipelineLayout;

	VkCommandBuffer primaryCommandBuffer;
	VkCommandBuffer secondaryCommandBuffer;

	// Number of animated objects to be renderer
	// by using threads and secondary command buffers
	uint32_t numObjectsPerThread;

	// Multi threaded stuff
	// Max. number of concurrent threads
	uint32_t numThreads;

	// Use push constants to update shader
	// parameters on a per-thread base
	struct ThreadPushConstantBlock {
		glm::mat4 mvp;
		glm::vec3 color;
	};

	struct ObjectData {
		glm::mat4 model;
		glm::vec3 pos;
		glm::vec3 rotation;
		float rotationDir;
		float rotationSpeed;
		float scale;
		float deltaT;
		float stateT = 0;
		bool visible = true;
	};

	struct ThreadData {
		VkCommandPool commandPool;
		// One command buffer per render object
		std::vector<VkCommandBuffer> commandBuffer;
		// One push constant block per render object
		std::vector<ThreadPushConstantBlock> pushConstBlock;
		// Per object information (position, rotation, etc.)
		std::vector<ObjectData> objectData;
	};
	std::vector<ThreadData> threadData;

	vks::ThreadPool threadPool;

	// Fence to wait for all command buffers to finish before
	// presenting to the swap chain
	VkFence renderFence = {};

	// Max. dimension of the ufo mesh for use as the sphere
	// radius for frustum culling
	float objectSphereDim;

	// View frustum for culling invisible objects
	vks::Frustum frustum;


	float rnd(float range);

	// Create all threads and initialize shader push constants
	void prepareMultiThreadedRenderer();

	// Builds the secondary command buffer for each thread
	void threadRenderCode(uint32_t threadIndex, uint32_t cmdBufferIndex, VkCommandBufferInheritanceInfo inheritanceInfo);

	void updateSecondaryCommandBuffer(VkCommandBufferInheritanceInfo inheritanceInfo);

	void updateCommandBuffers(VkFramebuffer frameBuffer);

	void loadMeshes();

	void setupVertexDescriptions();

	void setupPipelineLayout();

	void preparePipelines();

	void updateMatrices();

	void draw();

	virtual void render();

	virtual void viewChanged();

	virtual void getOverlayText(VulkanTextOverlay *textOverlay);

};


