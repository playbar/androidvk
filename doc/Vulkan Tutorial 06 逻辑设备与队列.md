#Vulkan Tutorial 06 逻辑设备与队列 

## Introduction
在选择要使用的物理设备之后，我们需要设置一个逻辑设备用于交互。逻辑设备创建过程与instance创建过程类似，
也需要描述我们需要使用的功能。因为我们已经查询过哪些队列簇可用，在这里需要进一步为逻辑设备创建具体类型的
命令队列。如果有不同的需求，也可以基于同一个物理设备创建多个逻辑设备。

 ![Image](pic/6_1.png)
 
首先添加一个新的类成员来存储逻辑设备句柄。

VkDevice device;
接下来创建一个新的函数createLogicalDevice，并在initVulkan函数中调用，以创建逻辑设备。

<pre>
void initVulkan() {
    createInstance();
    setupDebugCallback();
    pickPhysicalDevice();
    createLogicalDevice();
}

void createLogicalDevice() {

}
</pre>

## Specifying the queues to be created
创建逻辑设备需要在结构体中明确具体的信息，首先第一个结构体VkDeviceQueueCreateInfo。
这个结构体描述队列簇中预要申请使用的队列数量。现在我们仅关心具备图形能力的队列。

<pre>
QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

VkDeviceQueueCreateInfo queueCreateInfo = {};
queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
queueCreateInfo.queueCount = 1;
</pre>

当前可用的驱动程序所提供的队列簇只允许创建少量的队列，并且很多时候没有必要创建多个队列。
这是因为可以在多个线程上创建所有命令缓冲区，然后在主线程一次性的以较低开销的调用提交队列。


Vulkan允许使用0.0到1.0之间的浮点数分配队列优先级来影响命令缓冲区执行的调用。即使只有一个队列也是必须的:

<pre>
float queuePriority = 1.0f;
queueCreateInfo.pQueuePriorities = &queuePriority;
</pre>

## Specifying used device features
下一个要明确的信息有关设备要使用的功能特性。这些是我们在上一节中用vkGetPhysicalDeviceFeatures
查询支持的功能，比如geometry shaders。现在我们不需要任何特殊的功能，所以我们可以简单的定义它并将所有
内容保留到VK_FALSE。一旦我们要开始用Vulkan做更多的事情，我们会回到这个结构体，进一步设置。

VkPhysicalDeviceFeatures deviceFeatures = {};

## Creating the logical device
使用前面的两个结构体，我们可以填充VkDeviceCreateInfo结构。

<pre>
VkDeviceCreateInfo createInfo = {};
createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
</pre>

首先添加指向队列创建信息的结构体和设备功能结构体:

<pre>
createInfo.pQueueCreateInfos = &queueCreateInfo;
createInfo.queueCreateInfoCount = 1;

createInfo.pEnabledFeatures = &deviceFeatures;
</pre>

结构体其余的部分与VkInstanceCreateInfo相似，需要指定扩展和validation layers，
总而言之这次不同之处是为具体的设备设置信息。

设置具体扩展的一个案例是VK_KHR_swapchain，它允许将来自设备的渲染图形呈现到Windows。
系统中的Vulkan设备可能缺少该功能，例如仅仅支持计算操作。我们将在交换链章节中展开这个扩展。

就像之前validation layers小节中提到的，允许为instance开启validation layers，
现在我们将为设备开启validation layers，而不需要为设备指定任何扩展。

<pre>
createInfo.enabledExtensionCount = 0;

if (enableValidationLayers) {
    createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.data();
} else {
    createInfo.enabledLayerCount = 0;
}
</pre>

就这样，我们现在可以通过调用vkCreateDevice函数来创建实例化逻辑设备。

<pre>
if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
    throw std::runtime_error("failed to create logical device!");
}
</pre>

这些参数分别是包含具体队列使用信息的物理设备，可选的分配器回调指针以及用于存储逻辑设备的句柄。
与instance创建类似，此调用可能由于启用不存在的扩展或者指定不支持的功能，导致返回错误。

在cleanup函数中逻辑设备需要调用vkDestroyDevice销毁:

<pre>
void cleanup() {
    vkDestroyDevice(device, nullptr);
    ...
}
</pre>

逻辑设备不与instance交互，所以参数中不包含instance。

## Retrieving queue handles
这些队列与逻辑设备自动的一同创建，但是我们还没有一个与它们进行交互的句柄。
在这里添加一个新的类成员来存储图形队列句柄:

VkQueue graphicsQueue;
设备队列在设备被销毁的时候隐式清理，所以我们不需要在cleanup函数中做任何操作。

 
我们可以使用vkGetDeviceQueue函数来检测每个队列簇中队列的句柄。参数是逻辑设备，队列簇，队列索引和
存储获取队列变量句柄的指针。因为我们只是从这个队列簇创建一个队列，所以需要使用索引 0。

vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
在成功获取逻辑设备和队列句柄后，我们可以通过显卡做一些实际的事情了，在接下来的几章节中，
我们会设置资源并将相应的结果提交到窗体系统。

[代码](src/06.cpp)。
