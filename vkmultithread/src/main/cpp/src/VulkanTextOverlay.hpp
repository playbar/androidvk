#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <sstream>
#include <iomanip>

#include <vulkan/vulkan.h>
#include "VulkanTools.h"
#include "VulkanDebug.h"
#include "VulkanBuffer.hpp"
#include "VulkanDevice.hpp"

#include "../stb/stb_font_consolas_24_latin1.inl"

// Defines for the STB font used
// STB font files can be found at http://nothings.org/stb/font/
#define STB_FONT_NAME stb_font_consolas_24_latin1
#define STB_FONT_WIDTH STB_FONT_consolas_24_latin1_BITMAP_WIDTH
#define STB_FONT_HEIGHT STB_FONT_consolas_24_latin1_BITMAP_HEIGHT 
#define STB_FIRST_CHAR STB_FONT_consolas_24_latin1_FIRST_CHAR
#define STB_NUM_CHARS STB_FONT_consolas_24_latin1_NUM_CHARS

// Max. number of chars the text overlay buffer can hold
#define MAX_CHAR_COUNT 1024

/**
* @brief Mostly self-contained text overlay class
* @note Will only work with compatible render passes
*/ 
class VulkanTextOverlay
{
private:
	vks::VulkanDevice *vulkanDevice;

	VkQueue queue;
	VkFormat colorFormat;
	VkFormat depthFormat;

	uint32_t *frameBufferWidth;
	uint32_t *frameBufferHeight;

	VkSampler sampler;
	VkImage image;
	VkImageView view;
	vks::Buffer vertexBuffer;
	VkDeviceMemory imageMemory;
	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	VkDescriptorSet descriptorSet;
	VkPipelineLayout pipelineLayout;
	VkPipelineCache pipelineCache;
	VkPipeline pipeline;
	VkRenderPass renderPass;
	VkCommandPool commandPool;
	std::vector<VkFramebuffer*> frameBuffers;
	std::vector<VkPipelineShaderStageCreateInfo> shaderStages;
	VkFence fence;

	// Used during text updates
	glm::vec4 *mappedLocal = nullptr;

	stb_fontchar stbFontData[STB_NUM_CHARS];
	uint32_t numLetters;

public:

	enum TextAlign { alignLeft, alignCenter, alignRight };

	bool visible = true;
	bool invalidated = false;

	float scale = 2.0f;

	std::vector<VkCommandBuffer> cmdBuffers;

	VulkanTextOverlay(
		vks::VulkanDevice *vulkanDevice,
		VkQueue queue,
		std::vector<VkFramebuffer> &framebuffers,
		VkFormat colorformat,
		VkFormat depthformat,
		uint32_t *framebufferwidth,
		uint32_t *framebufferheight,
		std::vector<VkPipelineShaderStageCreateInfo> shaderstages);

	~VulkanTextOverlay();

	void prepareResources();

	void preparePipeline();

	void prepareRenderPass();

	void beginTextUpdate();

	void addText(std::string text, float x, float y, TextAlign align);

	void endTextUpdate();

	void updateCommandBuffers();

	void submit(VkQueue queue, uint32_t bufferindex, VkSubmitInfo submitInfo);

	void reallocateCommandBuffers();

};