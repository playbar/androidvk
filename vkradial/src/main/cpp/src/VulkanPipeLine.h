/*
* Assorted Vulkan helper functions
*
* Copyright (C) 2016 by Sascha Willems - www.saschawillems.de
*
* This code is licensed under the MIT license (MIT) (http://opensource.org/licenses/MIT)
*/

#pragma once

#include "vulkan/vulkan.h"
#include "VulkanInitializers.hpp"

#include <math.h>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <fstream>
#include <assert.h>
#include <stdio.h>
#include <vector>
#include <iostream>
#include <stdexcept>
#include "vulkanandroid.h"
#include <android/asset_manager.h>

struct VksPipeLine{
    static VkPipelineCache mPipelineCache;
    VkPipeline mPipeLine;
    VkPipelineLayout mPipeLayout;
    VkDescriptorSet  mDescriptorSet;
    VkDescriptorSetLayout mDescritptorSetLayout;

    VksPipeLine();
    ~VksPipeLine();

};

