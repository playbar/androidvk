# Vulkan Tutorial 09 图像与视图

使用任何的VkImage，包括在交换链或者渲染管线中的，我们都需要创建VkImageView对象。
从字面上理解它就是一个针对图像的视图或容器，通过它具体的渲染管线才能够读写渲染数据，换句话说VkImage不能与渲染管线进行交互。
除此之外，图像视图可以进一步定义具体Image的格式，比如定义为2D贴图，那么本质上就不需要任何级别的mipmapping。

 

在本章节我们会新增一个createImageViews函数，为每一个交换链中的图像创建基本的视图，
这些视图在后面的内容中会被作为颜色目标与渲染管线配合使用。

首先添加一个类成员用于保存图像视图的句柄集:

std::vector<VkImageView> swapChainImageViews;
创建createImagesViews函数，并在创建交换链完成之后调用:

<pre>
void initVulkan() {
    createInstance();
    setupDebugCallback();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
}

void createImageViews() {

}
</pre>

我们需要做的第一件事情需要定义保存图像视图集合的大小:

<pre>
void createImageViews() {
    swapChainImageViews.resize(swapChainImages.size());

}
</pre>

下一步，循环迭代所有的交换链图像。
<pre>
for (size_t i = 0; i < swapChainImages.size(); i++) {

}
</pre>

创建图像视图的参数被定义在VkImageViewCreateInfo结构体中。前几个参数的填充非常简单、直接。

<pre>
VkImageViewCreateInfo createInfo = {};
createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
createInfo.image = swapChainImages[i];
</pre>

其中viewType和format字段用于描述图像数据该被如何解释。
viewType参数允许将图像定义为1D textures, 2D textures, 3D textures 和cube maps。

<pre>
createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
createInfo.format = swapChainImageFormat;
</pre>

components字段允许调整颜色通道的最终的映射逻辑。比如，我们可以将所有颜色通道映射为红色通道，以实现单色纹理。
我们也可以将通道映射具体的常量数值0和1。在章节中我们使用默认的映射策略。

<pre>
createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
</pre>

subresourceRangle 字段用于描述图像的使用目标是什么，以及可以被访问的有效区域。我们的图像将会作为color targets，
没有任何mipmapping levels 或是多层 multiple layers。

<pre>
createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
createInfo.subresourceRange.baseMipLevel = 0;
createInfo.subresourceRange.levelCount = 1;
createInfo.subresourceRange.baseArrayLayer = 0;
createInfo.subresourceRange.layerCount = 1;
</pre>

如果在编写沉浸式的3D应用程序，比如VR，就需要创建支持多层的交换链。并且通过不同的层为每一个图像创建多个视图，以满足不同层的图像在左右眼渲染时对视图的需要。

 
创建图像视图调用vkCreateImageView函数:

<pre>
if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
    throw std::runtime_error("failed to create image views!");
}
</pre>

与图像不同的是，图像视图需要明确的创建过程，所以在程序退出的时候，我们需要添加一个循环去销毁他们。

<pre>
void cleanup() {
    for (size_t i = 0; i < swapChainImageViews.size(); i++) {
        vkDestroyImageView(device, swapChainImageViews[i], nullptr);
    }

    ...
}
</pre>

拥有了图像视图后，使用图像作为贴图已经足够了，但是它还没有准备好作为渲染的 target 。
它需要更多的间接步骤去准备，其中一个就是 framebuffer，被称作帧缓冲区。但首先我们要设置图形管线。

[代码](src/09.cpp)
