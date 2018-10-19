/*
* Assorted commonly used Vulkan helper functions
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#include "VulkanPipeLine.h"


VkPipelineCache VksPipeLine::mPipelineCache = NULL;

VksPipeLine::VksPipeLine(){
    mPipeLine = NULL;
    mPipeLayout = NULL;
    mDescriptorSet = NULL;
    mDescritptorSetLayout = NULL;


}
VksPipeLine::~VksPipeLine()
{

}