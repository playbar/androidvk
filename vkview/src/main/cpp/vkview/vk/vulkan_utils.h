#ifndef __BAR_VULKAN_UTILS_H__
#define __BAR_VULKAN_UTILS_H__

#include <android/log.h>
#include <android/asset_manager.h>

#include <vector>
#include <array>
#include <string>

#include <glm/glm.hpp>
#include <vulkan_wrapper.h>

#include "vulkan_data.h"
#include "vulkan_device.h"
#include "vulkan_buffer.h"
#include "vulkan_texture.h"

#define STATE_RUNNING 1
#define STATE_PAUSED 2
#define STATE_EXIT 3

class VulkanUtils {
public:

    VulkanUtils(AAssetManager *assetManager, const char *vertexShader, const char *fragmentShader);

    VulkanUtils();
    ~VulkanUtils();
    void SetData(AAssetManager *assetManager, const char *vertexShader, const char *fragmentShader);

public:
    void OnSurfaceCreated();
    void OnSurfaceChanged();
    void OnDrawFrame();

public:
    void start();
    void pause();

    inline void resume() { state = STATE_RUNNING; }

    inline void stop() { state = STATE_EXIT; }

    inline void initWindow(ANativeWindow *window) { this->window = window; }

    void createSurfaceDevice();

    void cleanUp();

public:
    void createSwapchain();
    void recreateSwapchain();
    void cleanupSwapchain();

    void createImageViews();
    void createRenderPass();
    void createCommandBuffers();
    void createDescriptorSetLayout();

    void createGraphicsPipelineTest();

    void createGraphicsPipeline();
    void createFramebuffers();

public:

public:


    void createDescriptorPool();
    VkDescriptorSet createDescriptorSet();
    void updateUniformBuffer();
    void drawCommandBuffers();

    void updateUniformBuffer1();
    void drawCommandBuffers1();

    void bindDescriptorSet();
    void bindDescriptorSetTexture(HVkTexture &texImg);
    void bindDescriptorSetTexture1(HVkTexture &texImg);

    void createSemaphores();
    void AcquireNextImage();
    void drawFrame();
    void QueuePresent();
    uint32_t mImageIndex = 0;

    std::vector<char> readAsset(std::string name);

    VkShaderModule createShaderModule(const std::vector<char> &code);
    VkShaderModule createShaderModule(const std::vector<uint32_t> &code);

    AAssetManager *assetManager;
    std::string vertexShader;
    std::string fragmentShader;
    int state;

    ANativeWindow *window;
    VulkanDevice mVKDevice;

    VkSwapchainKHR swapchain;
    VkRenderPass renderPass;
    VkDescriptorPool mDescriptorPool;
    VkDescriptorSetLayout mDescriptorSetLayout;
    VkDescriptorSet mDescriptorSet;
    VkDescriptorSet mDescriptorSet1;
    VkPipelineLayout mPipelineLayout;
    VkPipeline mGraphicsPipeline;


    VkFormat swapchainImageFormat;
    VkExtent2D swapchainExtent;
    std::vector<VkImage> mSwapchainImages;
    std::vector<VkImageView> mSwapchainImageViews;
    std::vector<VkFramebuffer> mFramebuffers;
    std::vector<VkCommandBuffer> mCommandBuffers;
    std::vector<VkDescriptorPool >mDescriptorPools;

    VkPipelineInputAssemblyStateCreateInfo mInputAssembly;

//    HVkBuffer mVertexBuffer;
//    HVkBuffer mIndexBuffer;
//    HVkBuffer mUniformBuffer;

    std::vector<HVkBuffer*> mVertexBuffers;
    std::vector<HVkBuffer*> mIndexBuffers;
    std::vector<HVkBuffer*> mUniformBuffers;
//    void createVertexBuffer();
//    void createIndexBuffer();
//    void createUniformBuffer();
    void createCacheBuffers();

    HVkTexture mTexImage;
    HVkTexture mTexImage1;

    VkSemaphore mImageAvailableSemaphore;
    VkSemaphore mRenderFinishedSemaphore;
};

#endif //__BAR_VULKAN_UTILS_H__
