/*
* Vulkan Example base class
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once


#include <android/native_activity.h>
#include <android/asset_manager.h>
#include <android_native_app_glue.h>
#include <sys/system_properties.h>
#include "vulkanandroid.h"
#include <iostream>
#include <chrono>
#include <sys/stat.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
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
#include "VulkanModel.hpp"
#include "VulkanBuffer.hpp"
#include "camera.hpp"

#define VERTEX_BUFFER_BIND_ID 1

class VulkanMain
{
public:
	// Vertex layout for the models
	VksVertexLayout vertexLayout = VksVertexLayout({
															   VERTEX_COMPONENT_POSITION,
															   VERTEX_COMPONENT_NORMAL,
															   VERTEX_COMPONENT_UV,
															   VERTEX_COMPONENT_COLOR,
													   });


	HVKModel mModels;


	HVKBuffer mUniformBuffer;

	// Same uniform buffer layout as shader
	struct UBOVS {
		glm::mat4 projection;
		glm::mat4 modelView;
		glm::vec4 lightPos = glm::vec4(0.0f, 2.0f, 1.0f, 0.0f);
	} uboVS;

	VkPipelineLayout mPipelineLayout;
	VkDescriptorSet mDescriptorSet;
	VkDescriptorSetLayout mDescriptorSetLayout;

	VkPipeline mPipeLinePhong;
	VkPipeline mPipeLineWireframe;
	VkPipeline mPipeLineToon;

	////////////
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
	VulkanDevice *mVulkanDevice;
	VkQueue queue;
	VkFormat depthFormat;
	VkCommandPool mCmdPool;
	VkPipelineStageFlags submitPipelineStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo mSubmitInfo;

	VkRenderPass mRenderPass;
	std::vector<VkFramebuffer> mFrameBuffers;
	std::vector<VkCommandBuffer> mDrawCmdBuffers;
	uint32_t currentBuffer = 0;
	VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
	std::vector<VkShaderModule> mShaderModules;
	VkPipelineCache mPipelineCache;
	VulkanSwapChain mSwapChain;

    VkSemaphore mPresentComplete;
    VkSemaphore mRenderComplete;
    VkSemaphore mTextOverlayComplete;

	// Simple texture loader
	//vks::tools::VulkanTextureLoader *textureLoader = nullptr;
	// Returns the base asset path (for shaders, models, textures) depending on the os
	const std::string getAssetPath();
public: 
	bool mPrepared = false;
	uint32_t width = 1280;
	uint32_t height = 720;

	struct Settings {
		bool validation = false;
		bool fullscreen = false;
		bool vsync = false;
	} settings;

	VkClearColorValue defaultClearColor = { { 0.25f, 0.025f, 0.025f, 1.0f } };

	float zoom = 0;

	// Defines a frame rate independent timer value clamped from -1.0...1.0
	// For use in animations, rotations, etc.
	float timer = 0.0f;
	// Multiplier for speeding up (or slowing down) the global timer
	float timerSpeed = 0.25f;
	
	bool paused = false;

	bool mEnableTextOverlay = false;
	VulkanTextOverlay *mTextOverlay;

	// Use to adjust mouse rotation speed
	float rotationSpeed = 1.0f;
	// Use to adjust mouse zoom speed
	float zoomSpeed = 1.0f;

	Camera camera;

	glm::vec3 rotation = glm::vec3();
	glm::vec3 cameraPos = glm::vec3();
	glm::vec2 mousePos;

	std::string title = "VkPipeLine";
	std::string name = "VkPipeLine";

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
	VulkanMain(bool enableValidation);

	// dtor
	~VulkanMain();

	void Destroy();

	// Setup the vulkan instance, enable required extensions and connect to the physical device (GPU)
	void initVulkan();

	static int32_t handleAppInput(struct android_app* app, AInputEvent* event);
	static void handleAppCommand(android_app* app, int32_t cmd);


	virtual VkResult createInstance(bool enableValidation);

	virtual void render();
	virtual void viewChanged();
	virtual void keyPressed(uint32_t keyCode);
	virtual void windowResized();
	virtual void updateCommandBuffers();

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

	VkCommandBuffer createCommandBuffer(VkCommandBufferLevel level, bool begin);
	void flushCommandBuffer(VkCommandBuffer commandBuffer, VkQueue queue, bool free);

	void createPipelineCache();

	virtual void prepare();

	// Load a SPIR-V shader
	VkPipelineShaderStageCreateInfo loadShader(std::string fileName, VkShaderStageFlagBits stage);
	
	void renderLoop();

	void updateTextOverlay();

	// Called when the text overlay is updating
	// Can be overriden in derived class to add custom text to the overlay
	virtual void getOverlayText(VulkanTextOverlay * textOverlay);


	////////////////////
	void loadAssets();

	void setupDescriptorPool();

	void setupDescriptorSetLayout();

	void setupDescriptorSet();

	void preparePipelines();

	// Prepare and initialize uniform buffer containing shader uniforms
	void prepareUniformBuffers();

	void updateUniformBuffers();

	void draw();


};


