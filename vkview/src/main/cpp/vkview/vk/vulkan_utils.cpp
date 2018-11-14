#include <set>
#include <algorithm>
#include <stdexcept>
#include <chrono>
#include "mylog.h"

#define GLM_FORCE_RADIANS
#include <glm/gtc/matrix_transform.hpp>

//#define STB_IMAGE_IMPLEMENTATION
//#include <stb_image.h>
#include <unistd.h>
#include <android/native_window.h>
#include <cstring>

#include "string"
#include "vulkan_utils.h"
#include "shaderc/shaderc.hpp"


const char* IMAGE_PATH = "sample_tex.png";
const char *IMAGE_APPLE = "apple.png";

const int WIDTH = 800;
const int HEIGHT = 448;

//const std::vector<Vertex> vertices = {
//        {{-0.5f, -0.5f}, {1.0f, 0.0f}},
//        {{0.5f, -0.5f}, {0.0f, 0.0f}},
//        {{0.5f, 0.5f}, {0.0f, 1.0f}},
//        {{-0.5f, 0.5f}, {1.0f, 1.0f}}
//};

std::vector<Vertex> vertices = {
        {{-540.0f, 865.0f}, {1.0f, 0.0f}},
        {{-540.0f, 0.0},    {0.0f, 0.0f}},
        {{0.0f,    0.0f},   {0.0f, 1.0f}},
        {{0.0f,    865.0f}, {1.0f, 1.0f}}
};


std::vector<Vertex> vertices1 = {
        {{0.0f,   0.0f},   {1.0f, 0.0f}},
        {{0.0f,   -865.0}, {0.0f, 0.0f}},
        {{540.0f, -865.0f},{0.0f, 1.0f}},
        {{540.0f, 0.0f},   {1.0f, 1.0f}}

};


std::vector<uint16_t> indices = {
        0, 1, 2, 2, 3, 0
};


struct shader_type_mapping {
    VkShaderStageFlagBits vkshader_type;
    shaderc_shader_kind   shaderc_type;
};
static const shader_type_mapping shader_map_table[] = {
        {
                VK_SHADER_STAGE_VERTEX_BIT,
                shaderc_glsl_vertex_shader
        },
        {
                VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT,
                shaderc_glsl_tess_control_shader
        },
        {
                VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT,
                shaderc_glsl_tess_evaluation_shader
        },
        {
                VK_SHADER_STAGE_GEOMETRY_BIT,
                shaderc_glsl_geometry_shader},
        {
                VK_SHADER_STAGE_FRAGMENT_BIT,
                shaderc_glsl_fragment_shader
        },
        {
                VK_SHADER_STAGE_COMPUTE_BIT,
                shaderc_glsl_compute_shader
        },
};

enum DataFlow{
    DataFlow_in = 1,
    DataFlow_uniform = 2,
    DataFlow_out = 3
};

struct ShaderInfo{
    int location;
    uint32_t length;
    VkFormat format;
    DataFlow data_flow;
    char loc_name[32];
};


shaderc_shader_kind MapShadercType(VkShaderStageFlagBits vkShader) {
    for (auto shader : shader_map_table) {
        if (shader.vkshader_type == vkShader) {
            return shader.shaderc_type;
        }
    }
    assert(false);
    return shaderc_glsl_infer_from_source;
}


VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats)
{
    if (availableFormats.size() == 1 && availableFormats[0].format == VK_FORMAT_UNDEFINED) {
        return {VK_FORMAT_B8G8R8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    }

    for (const auto &format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8_UNORM
            && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes)
{
    VkPresentModeKHR bestMode = VK_PRESENT_MODE_FIFO_KHR;

    for (const auto &mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        } else if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            bestMode = mode;
        }
    }

    return bestMode;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent = {WIDTH, HEIGHT};
    actualExtent.width = std::max(
            capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, actualExtent.width)
    );
    actualExtent.height = std::max(
            capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, actualExtent.height)
    );

    return actualExtent;
}


bool GLSLtoSPV(const VkShaderStageFlagBits shader_type, const char *pshader,
               std::vector<unsigned int> &spirv)
{
    // On Android, use shaderc instead.
    shaderc::Compiler compiler;
    shaderc::SpvCompilationResult module =
            compiler.CompileGlslToSpv(pshader, strlen(pshader),
                                      MapShadercType(shader_type),
                                      "shader");
    if (module.GetCompilationStatus() !=
        shaderc_compilation_status_success) {
        LOGE("Error: Id=%d, Msg=%s",
             module.GetCompilationStatus(),
             module.GetErrorMessage().c_str());
        return false;
    }
    spirv.assign(module.cbegin(), module.cend());

    return true;
}




VulkanUtils::VulkanUtils(AAssetManager *assetManager, const char *vertexShader, const char *fragmentShader) :
        mVertexBuffer(&mVKDevice),
        mIndexBuffer(&mVKDevice),
//        mUniformBuffer(&mVKDevice),
        mUniformProj(&mVKDevice),
        mTexImage(&mVKDevice),
        mTexImage1(&mVKDevice),
    assetManager(assetManager),
    vertexShader(std::string(vertexShader)),
    fragmentShader(std::string(fragmentShader)),
    state(STATE_RUNNING)
{
}

VulkanUtils::VulkanUtils():
        mVertexBuffer(&mVKDevice),
        mIndexBuffer(&mVKDevice),
//        mUniformBuffer(&mVKDevice),
        mUniformProj(&mVKDevice),
        mTexImage1(&mVKDevice),
        mTexImage(&mVKDevice)
{
    state = STATE_RUNNING;
}

VulkanUtils::~VulkanUtils()
{
    LOGE("VulkanUtils::~VulkanUtils");
}

void VulkanUtils::SetData(AAssetManager *assetMgr, const char *verShader, const char *fraShader)
{
    assetManager = assetMgr;
    vertexShader = std::string(verShader);
    fragmentShader = std::string(fraShader);
}

void VulkanUtils::pause() {
    state = STATE_PAUSED;
    vkDeviceWaitIdle(mVKDevice.logicalDevice);
}

void VulkanUtils::createSurfaceDevice()
{
    mVKDevice.createInstance();
    mVKDevice.setUpDebugCallback();
    mVKDevice.createSurface(window);
    mVKDevice.pickPhysicalDevice();
    mVKDevice.createLogicalDevice();
    mVKDevice.createCommandPool();
}


void VulkanUtils::OnSurfaceCreated()
{
    createSwapchain();
    createImageViews();
    createRenderPass();
    createFramebuffers();
    createDescriptorSetLayout();
    createGraphicsPipeline();


    mTexImage.createTextureImage(assetManager, IMAGE_PATH);
    mTexImage.createTextureImageView();
    mTexImage.createTextureSampler();

    mTexImage1.createTextureImage(assetManager, IMAGE_APPLE );
    mTexImage1.createTextureImageView();
    mTexImage1.createTextureSampler();

    createVertexBuffer();
    createIndexBuffer();
    createUniformBuffer();
    createDescriptorPool();
    createDescriptorSet();
    bindDescriptorSet();

    createCommandBuffers();
//    drawCommandBuffers();



//    drawCommandBuffers();

    createSemaphores();
}

void VulkanUtils::OnSurfaceChanged()
{
    state = STATE_PAUSED;
//    recreateSwapchain();
    state = STATE_RUNNING;
}

void VulkanUtils::OnDrawFrame()
{
    static int count = 0;
    if( count < 500 ) {
        bindDescriptorSetTexture(mTexImage);
        bindDescriptorSetTexture1(mTexImage1);
    }else{
        bindDescriptorSetTexture(mTexImage1);
        bindDescriptorSetTexture1(mTexImage);
    }
    ++count;
    if( count > 1000)
        count = 0;

    AcquireNextImage();


//    updateBufferData();

    updateUniformBuffer();
    drawCommandBuffers();

    updateUniformBuffer1();
    drawCommandBuffers1();


//    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
//    vkCmdUpdateBuffer(mCommandBuffers[mImageIndex], mVertexBuffer.mBuffer, 0, bufferSize, vertices.data() );
//    bindDescriptorSetTexture1(mTexImage);

//

    drawFrame();
    QueuePresent();
    vkQueueWaitIdle(mVKDevice.presentQueue);

    mVertexBuffer.reset();
    mUniformProj.reset();
//    mVertexBuffer.destroy();

}

void VulkanUtils::start()
{
    if (!InitVulkan()) {
        throw std::runtime_error("InitVulkan fail!");
    }
    return;
}

void VulkanUtils::cleanUp() {
    cleanupSwapchain();

    vkDestroyDescriptorPool(mVKDevice.logicalDevice, mDescriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(mVKDevice.logicalDevice, mDescriptorSetLayout, nullptr);

//    mUniformBuffer.destroy();
    mIndexBuffer.destroy();
    mVertexBuffer.destroy();

    mTexImage.destroyImage();
    mTexImage.destroySampler();

    mTexImage1.destroyImage();
    mTexImage1.destroySampler();

    vkDestroySemaphore(mVKDevice.logicalDevice, mRenderFinishedSemaphore, nullptr);
    vkDestroySemaphore(mVKDevice.logicalDevice, mImageAvailableSemaphore, nullptr);
    mVKDevice.destroy();

    ANativeWindow_release(window);
}


void VulkanUtils::createSwapchain()
{
    SwapchainSupportDetails swapchainSupport = mVKDevice.querySwapchainSupport(mVKDevice.physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0
        && imageCount > swapchainSupport.capabilities.maxImageCount)
    {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = mVKDevice.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = mVKDevice.findQueueFamilies(mVKDevice.physicalDevice);
    if (indices.graphicsFamily != indices.presentFamily) {
        uint32_t queueFamilyIndices[] = {
                (uint32_t) indices.graphicsFamily, (uint32_t) indices.presentFamily
        };
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(mVKDevice.logicalDevice, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
    }

    vkGetSwapchainImagesKHR(mVKDevice.logicalDevice, swapchain, &imageCount, nullptr);
    swapchainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(mVKDevice.logicalDevice, swapchain, &imageCount, swapchainImages.data());

    swapchainImageFormat = surfaceFormat.format;
    swapchainExtent = extent;
}

void VulkanUtils::createImageViews()
{
    swapchainImageViews.resize(swapchainImages.size());
    for (size_t i = 0; i < swapchainImages.size(); i++)
    {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages[i];

        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainImageFormat;

        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(mVKDevice.logicalDevice, &createInfo, nullptr,
                              &swapchainImageViews[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create image views!");
        }
    }
}

void VulkanUtils::createRenderPass()
{
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(mVKDevice.logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
    {
        throw std::runtime_error("failed to create render pass!");
    }
}

void VulkanUtils::createDescriptorSetLayout() {


    VkDescriptorSetLayoutBinding uboLayoutProjBinding = {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr,
    };

    VkDescriptorSetLayoutBinding samplerLayoutBinding = {
            .binding = 10,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr,
    };

    std::array<VkDescriptorSetLayoutBinding, 2> bindings = {uboLayoutProjBinding,
                                                            samplerLayoutBinding};

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = static_cast<uint32_t>(bindings.size()),
            .pBindings = bindings.data(),
    };

    if (vkCreateDescriptorSetLayout(mVKDevice.logicalDevice, &layoutInfo, nullptr, &mDescriptorSetLayout)
        != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor set layout!");
    }

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
            .setLayoutCount = 1,
            .pSetLayouts = &mDescriptorSetLayout,
    };
    if (vkCreatePipelineLayout(mVKDevice.logicalDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout)
        != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

}

//void VulkanUtils::createGraphicsPipelineTest()
//{
//    const char *srcVSInfo = "9000000000000000100000006d00000001000000706f736974696f6e000000000000000000000000000000260000001e0000000001000000080000006700000001000000746578436f6f7264000000000000000000000000000000260000001e00000000000000004000000000000000020000004d56500000000000000000000000000000000000000000200000001d00000000";
//    const char *srcVS = "030223070000010002000d00340000000000000011000200010000000b00060001000000474c534c2e7374642e343530000000000e00030000000000010000000f000b0000000000040000006d61696e000000000a000000200000002c0000002e00000031000000320000000300030002000000c201000004000900474c5f4152425f73657061726174655f7368616465725f6f626a65637473000004000a00474c5f474f4f474c455f6370705f7374796c655f6c696e655f646972656374697665000004000800474c5f474f4f474c455f696e636c7564655f6469726563746976650005000400040000006d61696e000000000500060008000000676c5f50657256657274657800000000060006000800000000000000676c5f506f736974696f6e00050003000a00000000000000050004000e0000005550726f6a000000060005000e0000000000000070726f6a0000000005000400100000007570726f6a0000000500070014000000556e69666f726d4275666665724f626a656374000600050014000000000000006d6f64656c0000000600050014000000010000007669657700000000050003001600000075626f000500050020000000696e506f736974696f6e0000050005002c00000066726167436f6c6f72000000050004002e000000696e436f6c6f7200050006003100000066726167546578436f6f7264000000000500050032000000696e546578436f6f726400004800050008000000000000000b00000000000000470003000800000002000000480004000e0000000000000005000000480005000e000000000000002300000000000000480005000e000000000000000700000010000000470003000e00000002000000470004001000000022000000000000004700040010000000210000000100000048000400140000000000000005000000480005001400000000000000230000000000000048000500140000000000000007000000100000004800040014000000010000000500000048000500140000000100000023000000400000004800050014000000010000000700000010000000470003001400000002000000470004001600000022000000000000004700040016000000210000000000000047000400200000001e00000000000000470004002c0000001e00000000000000470004002e0000001e0000000100000047000400310000001e0000000100000047000400320000001e000000020000001300020002000000210003000300000002000000160003000600000020000000170004000700000006000000040000001e0003000800000007000000200004000900000003000000080000003b000400090000000a00000003000000150004000b00000020000000010000002b0004000b0000000c00000000000000180004000d00000007000000040000001e0003000e0000000d000000200004000f000000020000000e0000003b0004000f00000010000000020000002000040011000000020000000d0000001e000400140000000d0000000d000000200004001500000002000000140000003b0004001500000016000000020000002b0004000b0000001700000001000000170004001e0000000600000002000000200004001f000000010000001e0000003b0004001f00000020000000010000002b0004000600000022000000000000002b00040006000000230000000000803f20000400280000000300000007000000170004002a0000000600000003000000200004002b000000030000002a0000003b0004002b0000002c00000003000000200004002d000000010000002a0000003b0004002d0000002e000000010000002000040030000000030000001e0000003b0004003000000031000000030000003b0004001f00000032000000010000003600050002000000040000000000000003000000f800020005000000410005001100000012000000100000000c0000003d0004000d000000130000001200000041000500110000001800000016000000170000003d0004000d0000001900000018000000920005000d0000001a000000130000001900000041000500110000001b000000160000000c0000003d0004000d0000001c0000001b000000920005000d0000001d0000001a0000001c0000003d0004001e000000210000002000000051000500060000002400000021000000000000005100050006000000250000002100000001000000500007000700000026000000240000002500000022000000230000009100050007000000270000001d000000260000004100050028000000290000000a0000000c0000003e00030029000000270000003d0004002a0000002f0000002e0000003e0003002c0000002f0000003d0004001e00000033000000320000003e0003003100000033000000fd00010038000100";
//    const char *srcFSInfo = "6000000000000000080000006700000001000000746578436f6f72645f6600000000000000000000000000280000001e0000000000000000000000000000000002000000746578556e6974000000000000000000000000000000002e0000002700000000";
//    const char *srcFS = "030223070000010002000d00170000000000000011000200010000000b00060001000000474c534c2e7374642e343530000000000e00030000000000010000000f00080004000000040000006d61696e000000000900000011000000160000001000030004000000070000000300030002000000c201000004000900474c5f4152425f73657061726174655f7368616465725f6f626a65637473000004000a00474c5f474f4f474c455f6370705f7374796c655f6c696e655f646972656374697665000004000800474c5f474f4f474c455f696e636c7564655f6469726563746976650005000400040000006d61696e0000000005000500090000006f7574436f6c6f7200000000050005000d00000074657853616d706c65720000050006001100000066726167546578436f6f726400000000050005001600000066726167436f6c6f7200000047000400090000001e00000000000000470004000d0000002200000000000000470004000d000000210000000200000047000400110000001e0000000100000047000400160000001e00000000000000130002000200000021000300030000000200000016000300060000002000000017000400070000000600000004000000200004000800000003000000070000003b000400080000000900000003000000190009000a000000060000000100000000000000000000000000000001000000000000001b0003000b0000000a000000200004000c000000000000000b0000003b0004000c0000000d00000000000000170004000f00000006000000020000002000040010000000010000000f0000003b00040010000000110000000100000017000400140000000600000003000000200004001500000001000000140000003b0004001500000016000000010000003600050002000000040000000000000003000000f8000200050000003d0004000b0000000e0000000d0000003d0004000f00000012000000110000005700050007000000130000000e000000120000003e0003000900000013000000fd00010038000100";
//
//    //srcVSInfo
//    int ilenFSInfo = strlen(srcVSInfo) / 2;
//    unsigned char *pVSInfo = new unsigned char[ilenFSInfo + 1 ];
//    memset( pVSInfo, 0, ilenFSInfo + 1 );
//    StrToHex(pVSInfo, (unsigned char *)srcVSInfo, ilenFSInfo);
//    unsigned char *pTemp = pVSInfo;
//    uint32_t len = 0;
//    memcpy(&len, pTemp, sizeof(uint32_t));
//    std::vector<ShaderInfo> shaderInfo;
//    pTemp += sizeof(uint32_t);
//    int count = (ilenFSInfo - sizeof(uint32_t)) / sizeof(ShaderInfo);
//    shaderInfo.resize(count);
//    memcpy(shaderInfo.data(), pTemp, ilenFSInfo - sizeof(uint32_t));
//    delete[]pVSInfo;
//
//    //srcVS
//    int ilenVS = strlen(srcVS) / 2;
//    std::vector<char> vertexShaderCode(ilenVS);
//    StrToHex((unsigned char *)vertexShaderCode.data(), (unsigned char *)srcVS, ilenVS );
//
//    //srcFS
//    int ilenFS = strlen(srcFS) / 2;
//    std::vector<char> fragmentShaderCode(ilenFS);
//    StrToHex((unsigned char *)fragmentShaderCode.data(), (unsigned char *)srcFS, ilenFS );
//
//
////    auto vertexShaderCode = readAsset(vertexShader);
////    auto fragmentShaderCode = readAsset(fragmentShader);
//    VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
//    VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);
//
////    auto bindingDesc = getBindingDescription(shaderInfo);
////    auto attrDesc =    getAttributeDescriptions(shaderInfo);
//
//    auto bindingDesc = Vertex::getBindingDescription();
//    auto attrDesc = Vertex::getAttributeDescriptions();
//
//    VkPipelineShaderStageCreateInfo vertexShaderStageInfo = {
//            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
//            .stage = VK_SHADER_STAGE_VERTEX_BIT,
//            .module = vertexShaderModule,
//            .pName = "main",
//    };
//    VkPipelineShaderStageCreateInfo fragmentShaderStageInfo = {
//            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
//            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
//            .module = fragmentShaderModule,
//            .pName = "main",
//    };
//    VkPipelineShaderStageCreateInfo shaderStages[] = {
//            vertexShaderStageInfo, fragmentShaderStageInfo
//    };
//
//
//    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
//            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
//            .vertexBindingDescriptionCount = 1,
//            .pVertexBindingDescriptions = &bindingDesc,
//            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDesc.size()),
//            .pVertexAttributeDescriptions = attrDesc.data(),
//    };
//
//    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
//            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
//            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
//            .primitiveRestartEnable = VK_FALSE,
//    };
//
//    VkViewport viewport = {
//            .x = 0.0f,
//            .y = 0.0f,
//            .width = (float) swapchainExtent.width,
//            .height = (float) swapchainExtent.height,
//            .minDepth = 0.0f,
//            .maxDepth = 1.0f,
//    };
//
//    VkRect2D scissor = {
//            .offset = {0, 0},
//            .extent = swapchainExtent,
//    };
//
//    VkPipelineViewportStateCreateInfo viewportState = {
//            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
//            .viewportCount = 1,
//            .pViewports = &viewport,
//            .scissorCount = 1,
//            .pScissors = &scissor,
//    };
//
//    VkPipelineRasterizationStateCreateInfo rasterizer = {
//            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
//            .depthClampEnable = VK_FALSE,
//            .rasterizerDiscardEnable = VK_FALSE,
//            .polygonMode = VK_POLYGON_MODE_FILL,
//            .lineWidth = 1.0f,
//            .cullMode = VK_CULL_MODE_BACK_BIT,
//            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
//            .depthBiasEnable = VK_FALSE,
//    };
//
//    VkPipelineMultisampleStateCreateInfo multisampling = {
//            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
//            .sampleShadingEnable = VK_FALSE,
//            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
//    };
//
//    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
//            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
//                              | VK_COLOR_COMPONENT_G_BIT
//                              | VK_COLOR_COMPONENT_B_BIT
//                              | VK_COLOR_COMPONENT_A_BIT,
//            .blendEnable = VK_FALSE,
//    };
//    VkPipelineColorBlendStateCreateInfo colorBlending = {
//            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
//            .logicOpEnable = VK_FALSE,
//            .logicOp = VK_LOGIC_OP_COPY,
//            .attachmentCount = 1,
//            .pAttachments = &colorBlendAttachment,
//            .blendConstants[0] = 0.0f,
//            .blendConstants[1] = 0.0f,
//            .blendConstants[2] = 0.0f,
//            .blendConstants[3] = 0.0f,
//    };
//
//    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
//            .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
//            .setLayoutCount = 1,
//            .pSetLayouts = &mDescriptorSetLayout,
//    };
//    if (vkCreatePipelineLayout(mVKDevice.logicalDevice, &pipelineLayoutInfo, nullptr, &mPipelineLayout)
//        != VK_SUCCESS) {
//        throw std::runtime_error("failed to create pipeline layout!");
//    }
//
//    VkGraphicsPipelineCreateInfo pipelineInfo = {
//            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
//            .stageCount = 2,
//            .pStages = shaderStages,
//            .pVertexInputState = &vertexInputInfo,
//            .pInputAssemblyState = &inputAssembly,
//            .pViewportState = &viewportState,
//            .pRasterizationState = &rasterizer,
//            .pMultisampleState = &multisampling,
//            .pColorBlendState = &colorBlending,
//            .layout = mPipelineLayout,
//            .renderPass = renderPass,
//            .subpass = 0,
//            .basePipelineHandle = VK_NULL_HANDLE,
//    };
//
//    if (vkCreateGraphicsPipelines(mVKDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
//                                  &mGraphicsPipeline) != VK_SUCCESS)
//    {
//        throw std::runtime_error("failed to create graphics pipeline!");
//    }
//
//    vkDestroyShaderModule(mVKDevice.logicalDevice, vertexShaderModule, nullptr);
//    vkDestroyShaderModule(mVKDevice.logicalDevice, fragmentShaderModule, nullptr);
//}

//#define USE_SPV

void VulkanUtils::createGraphicsPipeline() {

#ifdef USE_SPV
    vertexShader = "shaders/triangle.vert.spv";
    fragmentShader = "shaders/triangle.frag.spv";
    auto vertexShaderCode = readAsset(vertexShader);
    auto fragmentShaderCode = readAsset(fragmentShader);
    VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
    VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);
#else
    auto vertexShaderCode = readAsset("shaders/triangle.vert");
    auto fragmentShaderCode = readAsset("shaders/triangle.frag");
    std::vector<unsigned int> vtx_spv;
    std::vector<unsigned int> frag_spv;
    GLSLtoSPV(VK_SHADER_STAGE_VERTEX_BIT, vertexShaderCode.data(), vtx_spv);
    GLSLtoSPV(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShaderCode.data(), frag_spv);
    VkShaderModule vertexShaderModule = createShaderModule(vtx_spv);
    VkShaderModule fragmentShaderModule = createShaderModule(frag_spv);
#endif

    VkPipelineShaderStageCreateInfo vertexShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertexShaderModule,
            .pName = "main",
    };
    VkPipelineShaderStageCreateInfo fragmentShaderStageInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragmentShaderModule,
            .pName = "main",
    };
    VkPipelineShaderStageCreateInfo shaderStages[] = {
            vertexShaderStageInfo, fragmentShaderStageInfo
    };

    auto bindingDesc = Vertex::getBindingDescription();
    auto attrDesc =    Vertex::getAttributeDescriptions();
    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
            .vertexBindingDescriptionCount = 1,
            .pVertexBindingDescriptions = &bindingDesc,
            .vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDesc.size()),
            .pVertexAttributeDescriptions = attrDesc.data(),
    };

    mInputAssembly = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
            .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN,
            .primitiveRestartEnable = VK_TRUE,
    };

    VkViewport viewport = {
            .x = 0.0f,
            .y = 0.0f,
            .width = (float) swapchainExtent.width,
            .height = (float) swapchainExtent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
            .offset = {0, 0},
            .extent = swapchainExtent,
    };

    VkPipelineViewportStateCreateInfo viewportState = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
            .viewportCount = 1,
            .pViewports = &viewport,
            .scissorCount = 1,
            .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
            .depthClampEnable = VK_FALSE,
            .rasterizerDiscardEnable = VK_FALSE,
            .polygonMode = VK_POLYGON_MODE_FILL,
            .lineWidth = 1.0f,
            .cullMode = VK_CULL_MODE_BACK_BIT,
            .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
            .depthBiasEnable = VK_FALSE,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .sampleShadingEnable = VK_FALSE,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT
                              | VK_COLOR_COMPONENT_G_BIT
                              | VK_COLOR_COMPONENT_B_BIT
                              | VK_COLOR_COMPONENT_A_BIT,
            .blendEnable = VK_FALSE,
    };
    VkPipelineColorBlendStateCreateInfo colorBlending = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
            .logicOpEnable = VK_FALSE,
            .logicOp = VK_LOGIC_OP_COPY,
            .attachmentCount = 1,
            .pAttachments = &colorBlendAttachment,
            .blendConstants[0] = 0.0f,
            .blendConstants[1] = 0.0f,
            .blendConstants[2] = 0.0f,
            .blendConstants[3] = 0.0f,
    };


    VkGraphicsPipelineCreateInfo pipelineInfo = {
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            .stageCount = 2,
            .pStages = shaderStages,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &mInputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pColorBlendState = &colorBlending,
            .layout = mPipelineLayout,
            .renderPass = renderPass,
            .subpass = 0,
            .basePipelineHandle = VK_NULL_HANDLE,
    };

    VkResult vkre = vkCreateGraphicsPipelines(mVKDevice.logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                                              &mGraphicsPipeline);
    if ( vkre != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    vkDestroyShaderModule(mVKDevice.logicalDevice, vertexShaderModule, nullptr);
    vkDestroyShaderModule(mVKDevice.logicalDevice, fragmentShaderModule, nullptr);
}

void VulkanUtils::createFramebuffers() {
    mFramebuffers.resize(swapchainImageViews.size());

    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        VkImageView attachments[] = {
                swapchainImageViews[i]
        };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainExtent.width;
        framebufferInfo.height = swapchainExtent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(mVKDevice.logicalDevice, &framebufferInfo, nullptr, &mFramebuffers[i])
            != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void VulkanUtils::createVertexBuffer() {
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

//    HVkBuffer stagBuffer(&mVKDevice);
//    stagBuffer.createBuffer(bufferSize,
//                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
//                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
//
//    stagBuffer.updateData((void*)vertices.data());

    mVertexBuffer.createBuffer(MAX_BUFFER_SIZE,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

//    mVertexBuffer.copyBuffer(stagBuffer);
    mVertexBuffer.updateData((void*)vertices.data(), bufferSize);

    return;
}

void VulkanUtils::createIndexBuffer() {
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    HVkBuffer stagBuffer(&mVKDevice);

    stagBuffer.createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    stagBuffer.updateData((void*)indices.data(), bufferSize);


    mIndexBuffer.createBuffer(bufferSize,
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    mIndexBuffer.copyBuffer(stagBuffer);


}

void VulkanUtils::createUniformBuffer() {
//    VkDeviceSize bufferSize = sizeof(UniformBufferObject);
//    mUniformBuffer.createBuffer(bufferSize,
//                 VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceSize bufferSizeProj = sizeof(UniformBufferProj);
    mUniformProj.createBuffer(MAX_BUFFER_SIZE,
                                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

}

void VulkanUtils::createDescriptorPool() {
    std::array<VkDescriptorPoolSize, 2> poolSizes = {};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = 10;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = 10;

    VkDescriptorPoolCreateInfo poolInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes = poolSizes.data(),
            .maxSets = 1,
    };
    if (vkCreateDescriptorPool(mVKDevice.logicalDevice, &poolInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }
}

void VulkanUtils::createDescriptorSet() {
    VkDescriptorSetLayout layouts[] = {mDescriptorSetLayout};
    VkDescriptorSetAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool = mDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = layouts,
    };
    if (vkAllocateDescriptorSets(mVKDevice.logicalDevice, &allocInfo, &mDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor set!");
    }


    if (vkAllocateDescriptorSets(mVKDevice.logicalDevice, &allocInfo, &mDescriptorSet1) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor set!");
    }


}

void VulkanUtils::bindDescriptorSet()
{
//    VkDescriptorBufferInfo bufferInfo = {
//            .buffer = mUniformBuffer.mBuffer,
//            .offset = 0,
//            .range = sizeof(UniformBufferObject),
//    };

    VkDescriptorBufferInfo bufferInfoProj = {
            .buffer = mUniformProj.mBuffer,
            .offset = 0,
            .range = sizeof(UniformBufferProj),
    };


    std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = mDescriptorSet;
    descriptorWrites[0].dstBinding = 1;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pBufferInfo = &bufferInfoProj;


    vkUpdateDescriptorSets(mVKDevice.logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);


    //////////////

    VkDescriptorBufferInfo bufferInfoProj1 = {
            .buffer = mUniformProj.mBuffer,
            .offset = sizeof(UniformBufferProj),
            .range = sizeof(UniformBufferProj),
    };
    std::array<VkWriteDescriptorSet, 1> descriptorWrites1 = {};

    descriptorWrites1[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites1[0].dstSet = mDescriptorSet1;
    descriptorWrites1[0].dstBinding = 1;
    descriptorWrites1[0].dstArrayElement = 0;
    descriptorWrites1[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrites1[0].descriptorCount = 1;
    descriptorWrites1[0].pBufferInfo = &bufferInfoProj1;
    vkUpdateDescriptorSets(mVKDevice.logicalDevice, static_cast<uint32_t>(descriptorWrites1.size()), descriptorWrites1.data(), 0, nullptr);

    return;
}

void VulkanUtils::bindDescriptorSetTexture(HVkTexture &texImg)
{
    std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};
    VkDescriptorImageInfo imageInfo = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = texImg.mTextureImageView,
            .sampler = texImg.mTextureSampler,
    };

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = mDescriptorSet;
    descriptorWrites[0].dstBinding = 10;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(mVKDevice.logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);

}

void VulkanUtils::bindDescriptorSetTexture1(HVkTexture &texImg)
{
    std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};
    VkDescriptorImageInfo imageInfo = {
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            .imageView = texImg.mTextureImageView,
            .sampler = texImg.mTextureSampler,
    };

    descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrites[0].dstSet = mDescriptorSet1;
    descriptorWrites[0].dstBinding = 10;
    descriptorWrites[0].dstArrayElement = 0;
    descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrites[0].descriptorCount = 1;
    descriptorWrites[0].pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(mVKDevice.logicalDevice, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanUtils::createCommandBuffers() {
    mCommandBuffers.resize(mFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = mVKDevice.mCommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t) mCommandBuffers.size();

    if (vkAllocateCommandBuffers(mVKDevice.logicalDevice, &allocInfo, mCommandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }


}

void VulkanUtils::drawCommandBuffers()
{

    size_t i = mImageIndex;

//    HVkBuffer vertexBuffer(&mVKDevice);
    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();

    VkDeviceSize offset = mVertexBuffer.mOffset;
    mVertexBuffer.updateData(vertices.data(), bufferSize );
    mVertexBuffer.flush(bufferSize);

    vkCmdBindPipeline(mCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

    VkBuffer vertexBuffers[] = {mVertexBuffer.mBuffer};
    VkDeviceSize offsets[] = {offset};
    vkCmdBindVertexBuffers(mCommandBuffers[i], VERTEXT_BUFFER_ID, 1, vertexBuffers, offsets);

//        VkBuffer vertexBuffers1[] = {mVertexBuffer1.mBuffer};     //error
//        vkCmdBindVertexBuffers(mCommandBuffers[i], 1, 1, vertexBuffers1, offsets); // error

    vkCmdBindDescriptorSets(mCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout,
                            0, 1, &mDescriptorSet, 0, nullptr);

//        vkCmdBindIndexBuffer(mCommandBuffers[i], mIndexBuffer.mBuffer, 0, VK_INDEX_TYPE_UINT16);
//        vkCmdDrawIndexed(mCommandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    vkCmdDraw(mCommandBuffers[i], static_cast<uint32_t>(vertices.size()), 1, 0, 0);

//    vertexBuffer.destroy();

}


void VulkanUtils::drawCommandBuffers1()
{
    static int step = 0;

    ++step;
    vertices1[0].pos.x = step;
    vertices1[1].pos.x = step;
    vertices1[2].pos.x = step + 540;
    vertices1[3].pos.x = step + 540;
    if( step > 200 )
    {
        step = 0;
    }

    VkDeviceSize bufferSize = sizeof(vertices1[0]) * vertices1.size();
    VkDeviceSize offset = mVertexBuffer.mOffset;
    mVertexBuffer.updateData(vertices1.data(), bufferSize );
    mVertexBuffer.flush(bufferSize);

    size_t i = mImageIndex;
    vkCmdBindPipeline(mCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

    VkBuffer vertexBuffers[] = {mVertexBuffer.mBuffer};
    VkDeviceSize offsets[] = {offset};
    vkCmdBindVertexBuffers(mCommandBuffers[i], VERTEXT_BUFFER_ID, 1, vertexBuffers, offsets);

    vkCmdBindDescriptorSets(mCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout,
                            0, 1, &mDescriptorSet1, 0, nullptr);

//        vkCmdBindIndexBuffer(mCommandBuffers[i], mIndexBuffer.mBuffer, 0, VK_INDEX_TYPE_UINT16);
//        vkCmdDrawIndexed(mCommandBuffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);
    vkCmdDraw(mCommandBuffers[i], static_cast<uint32_t>(vertices1.size()), 1, 0, 0);

}

void VulkanUtils::createSemaphores() {
    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    if (vkCreateSemaphore(mVKDevice.logicalDevice, &semaphoreInfo, nullptr, &mImageAvailableSemaphore) != VK_SUCCESS
        || vkCreateSemaphore(mVKDevice.logicalDevice, &semaphoreInfo, nullptr, &mRenderFinishedSemaphore)
           != VK_SUCCESS) {
        throw std::runtime_error("failed to create semaphores!");
    }
}

void VulkanUtils::updateUniformBuffer() {
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1e3f;

    UniformBufferObject ubo = {
//            .model = glm::rotate(glm::mat4(), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .model = glm::scale( glm::mat4(), glm::vec3(0.002, 0.002f, 1.0f)),
            .view = glm::lookAt(glm::vec3(0.0f, 0.0f, -2.0f),
                                glm::vec3(0.0f, 0.0f, 1.0f),
                                glm::vec3(1.0f, 0.0f, 0.0f)),

//            .proj = glm::perspective(glm::radians(45.0f),
//                                     swapchainExtent.width / (float) swapchainExtent.height, 0.1f, 10.0f),

    };
    ubo.model = glm::rotate(ubo.model, time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    UniformBufferProj uboproj = {
            .proj = glm::perspective(glm::radians(90.0f),
                                     swapchainExtent.width / (float) swapchainExtent.height, 0.001f, 1000.0f),
    };

    uboproj.proj *= ubo.view;
    uboproj.proj *= ubo.model;

    mUniformProj.updateData(&uboproj, sizeof(uboproj));
    mUniformProj.flush(sizeof(uboproj));

    return;
}

void VulkanUtils::updateUniformBuffer1()
{
    static auto startTime = std::chrono::high_resolution_clock::now();

    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - startTime).count() / 1e3f;

    UniformBufferObject ubo = {
//            .model = glm::rotate(glm::mat4(), glm::radians(0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
            .model = glm::scale( glm::mat4(), glm::vec3(0.002, 0.002f, 1.0f)),
            .view = glm::lookAt(glm::vec3(0.0f, 0.0f, -2.0f),
                                glm::vec3(0.0f, 0.0f, 1.0f),
                                glm::vec3(1.0f, 0.0f, 0.0f)),

//            .proj = glm::perspective(glm::radians(45.0f),
//                                     swapchainExtent.width / (float) swapchainExtent.height, 0.1f, 10.0f),

    };
//    ubo.model = glm::rotate(ubo.model, time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    UniformBufferProj uboproj = {
            .proj = glm::perspective(glm::radians(90.0f),
                                     swapchainExtent.width / (float) swapchainExtent.height, 0.001f, 1000.0f),
    };

    uboproj.proj *= ubo.view;
    uboproj.proj *= ubo.model;

    mUniformProj.updateData(&uboproj, sizeof(uboproj));
    mUniformProj.flush(sizeof(uboproj));

    return;
}

void VulkanUtils::AcquireNextImage()
{
    VkResult result = vkAcquireNextImageKHR(mVKDevice.logicalDevice, swapchain, std::numeric_limits<uint64_t>::max(),
                                            mImageAvailableSemaphore, VK_NULL_HANDLE, &mImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapchain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    mInputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    beginInfo.pInheritanceInfo = nullptr; // Optional

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = swapchainExtent;

    VkClearValue clearColor = {1.0f, 0.0f, 0.0f, 1.0f};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    size_t i = mImageIndex;


    vkBeginCommandBuffer(mCommandBuffers[i], &beginInfo);

    renderPassInfo.framebuffer = mFramebuffers[i];

    vkCmdBeginRenderPass(mCommandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

}


//void VulkanUtils::updateBufferData()
//{
//    static int step = 0;
//
//    ++step;
//    vertices1[0].pos.x = step;
//    vertices1[1].pos.x = step;
//    vertices1[2].pos.x = step + 540;
//    vertices1[3].pos.x = step + 540;
//    if( step > 200 )
//    {
//        step = 0;
//    }
//
//    VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
//    vkCmdUpdateBuffer(mCommandBuffers[mImageIndex], mVertexBuffer1.mBuffer, 0, bufferSize, vertices1.data() );
//
////    mVertexBuffer1.updateData(vertices1.data());
//
//}

void VulkanUtils::drawFrame() {

    vkCmdEndRenderPass(mCommandBuffers[mImageIndex]);

    if (vkEndCommandBuffer(mCommandBuffers[mImageIndex]) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    VkResult result = VK_SUCCESS;

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {mImageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &mCommandBuffers[mImageIndex];

    VkSemaphore signalSemaphores[] = {mRenderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(mVKDevice.graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

}

void VulkanUtils::QueuePresent()
{
    VkSemaphore signalSemaphores[] = {mRenderFinishedSemaphore};
    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &mImageIndex;

    VkResult result = vkQueuePresentKHR(mVKDevice.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        recreateSwapchain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

//    vkQueueWaitIdle(mVKDevice.presentQueue);
}


void VulkanUtils::recreateSwapchain() {
    LOGI("recreateSwapchain");

    vkDeviceWaitIdle(mVKDevice.logicalDevice);

    cleanupSwapchain();

    createSwapchain();
    createImageViews();
    createRenderPass();
    createDescriptorSetLayout();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandBuffers();
//    drawCommandBuffers();
}

void VulkanUtils::cleanupSwapchain() {
    for (size_t i = 0; i < mFramebuffers.size(); i++) {
        vkDestroyFramebuffer(mVKDevice.logicalDevice, mFramebuffers[i], nullptr);
    }

    vkFreeCommandBuffers(mVKDevice.logicalDevice, mVKDevice.mCommandPool, static_cast<uint32_t>(mCommandBuffers.size()),
                         mCommandBuffers.data());

    vkDestroyPipeline(mVKDevice.logicalDevice, mGraphicsPipeline, nullptr);
    vkDestroyPipelineLayout(mVKDevice.logicalDevice, mPipelineLayout, nullptr);
    vkDestroyRenderPass(mVKDevice.logicalDevice, renderPass, nullptr);

    for (size_t i = 0; i < swapchainImageViews.size(); i++) {
        vkDestroyImageView(mVKDevice.logicalDevice, swapchainImageViews[i], nullptr);
    }

    vkDestroySwapchainKHR(mVKDevice.logicalDevice, swapchain, nullptr);
}

std::vector<char> VulkanUtils::readAsset(std::string name) {
    AAsset *file = AAssetManager_open(assetManager, name.c_str(), AASSET_MODE_BUFFER);
    size_t len = AAsset_getLength(file);
    std::vector<char> buffer(len);

    AAsset_read(file, buffer.data(), len);

    AAsset_close(file);

    LOGI("read asset %s, length %d", name.c_str(), (int)len);

    return buffer;
}

VkShaderModule VulkanUtils::createShaderModule(const std::vector<char> &code) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(mVKDevice.logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}


VkShaderModule VulkanUtils::createShaderModule(const std::vector<uint32_t> &code) {
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(mVKDevice.logicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("failed to create shader module!");
    }

    return shaderModule;
}