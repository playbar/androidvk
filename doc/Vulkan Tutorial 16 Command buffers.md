# Vulkan Tutorial 16 Command buffers

诸如绘制和内存操作相关命令，在Vulkan中不是通过函数直接调用的。我们需要在命令缓冲区对象中记录我们期望的任何操作。
这样做的优点是可以提前在多线程中完成所有绘制命令相关的装配工作，并在主线程循环结构中通知Vulkan执行具体的命令。

## Command pools
我们在使用任何command buffers之前需要创建命令对象池command pool。Command pools管理用于存储缓冲区的内存，
并从中分配命令缓冲区。添加新的类成员保存VkCommandPool:

VkCommandPool commandPool;
创建新的函数createCommandPool并在initVulkan函数创建完framebuffers后调用。

<pre>
void initVulkan() {
    createInstance();
    setupDebugCallback();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
}

...

void createCommandPool() {

}
</pre>

命令对象池创建仅仅需要两个参数:

<pre>
QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

VkCommandPoolCreateInfo poolInfo = {};
poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
poolInfo.flags = 0; // Optional
</pre>

命令缓冲区通过将其提交到其中一个设备队列上来执行，如我们检索的graphics和presentation队列。
每个命令对象池只能分配在单一类型的队列上提交的命令缓冲区，换句话说要分配的命令需要与队列类型一致。
我们要记录绘制的命令，这就说明为什么要选择图形队列簇的原因。

有两个标志位用于command pools:

* VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: 提示命令缓冲区非常频繁的重新记录新命令(可能会改变内存分配行为)
* VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: 允许命令缓冲区单独重新记录，没有这个标志，所有的命令缓冲区都必须一起重置

我们仅仅在程序开始的时候记录命令缓冲区，并在主循环体main loop中多次执行，因此我们不会使用这些标志。

<pre>
if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
    throw std::runtime_error("failed to create command pool!");
}
</pre>

通过vkCreateCommandPool函数完成command pool创建工作。它不需要任何特殊的参数设置。
命令将被整个程序的生命周期使用以完成屏幕的绘制工作，所以对象池应该被在最后销毁:

<pre>
void cleanup() {
    vkDestroyCommandPool(device, commandPool, nullptr);

    ...
}
</pre>

## Command buffer allocation
现在我们开始分配命令缓冲区并通过它们记录绘制指令。因为其中一个绘图命令需要正确绑定VkFrameBuffer，
我们实际上需要为每一个交换链中的图像记录一个命令缓冲区。最后创建一个VkCommandBuffer对象列表作为成员变量。
命令缓冲区会在common pool销毁的时候自动释放系统资源，所以我们不需要明确编写cleanup逻辑。

std::vector<VkCommandBuffer> commandBuffers;  
现在开始使用一个createCommandBuffers函数来分配和记录每一个交换链图像将要应用的命令。

<pre>
void initVulkan() {
    createInstance();
    setupDebugCallback();
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
    createRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
}

...

void createCommandBuffers() {
    commandBuffers.resize(swapChainFramebuffers.size());
}
</pre>


命令缓冲区通过vkAllocateCommandBuffers函数分配，它需要VkCommandBufferAllocateInfo结构体作为参数，
用以指定command pool和缓冲区将会分配的大小:

<pre>
VkCommandBufferAllocateInfo allocInfo = {};
allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
allocInfo.commandPool = commandPool;
allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
allocInfo.commandBufferCount = (uint32_t) commandBuffers.size();

if (vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
}
</pre>

level参数指定分配的命令缓冲区的主从关系。 

* VK_COMMAND_BUFFER_LEVEL_PRIMARY: 可以提交到队列执行，但不能从其他的命令缓冲区调用。
* VK_COMMAND_BUFFER_LEVEL_SECONDARY: 无法直接提交，但是可以从主命令缓冲区调用。

我们不会在这里使用辅助缓冲区功能，但是可以想像，对于复用主缓冲区的常用操作很有帮助。

## Starting command buffer recording
通过vkBeginCommandBuffer来开启命令缓冲区的记录功能，该函数需要传递VkCommandBufferBeginInfo结构体作为参数，
用以指定命令缓冲区在使用过程中的一些具体信息。

<pre>
for (size_t i = 0; i < commandBuffers.size(); i++) {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
    beginInfo.pInheritanceInfo = nullptr; // Optional

    vkBeginCommandBuffer(commandBuffers[i], &beginInfo);
}
</pre>

flags标志位参数用于指定如何使用命令缓冲区。可选的参数类型如下:

* VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT: 命令缓冲区将在执行一次后立即重新记录。
* VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT: 这是一个辅助缓冲区，它限制在在一个渲染通道中。
* VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT: 命令缓冲区也可以重新提交，同时它也在等待执行。

我们使用了最后一个标志，因为我们可能已经在下一帧的时候安排了绘制命令，而最后一帧尚未完成。pInheritanceInfo参数与辅助缓冲区相关。
它指定从主命令缓冲区继承的状态。

如果命令缓冲区已经被记录一次，那么调用vkBeginCommandBuffer会隐式地重置它。否则将命令附加到缓冲区是不可能的。

## Starting a render pass

绘制开始于调用vkCmdBeginRenderPass开启渲染通道。render pass使用VkRenderPassBeginInfo结构体填充配置信息作为调用时使用的参数。

<pre>
VkRenderPassBeginInfo renderPassInfo = {};
renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
renderPassInfo.renderPass = renderPass;
renderPassInfo.framebuffer = swapChainFramebuffers[i];
</pre>

结构体第一个参数传递为绑定到对应附件的渲染通道本身。我们为每一个交换链的图像创建帧缓冲区，并指定为颜色附件。

renderPassInfo.renderArea.offset = {0, 0};  
renderPassInfo.renderArea.extent = swapChainExtent;  
后两个参数定义了渲染区域的大小。渲染区域定义着色器加载和存储将要发生的位置。区域外的像素将具有未定的值。为了最佳的性能它的尺寸应该与附件匹配。

<pre>
VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
renderPassInfo.clearValueCount = 1;
renderPassInfo.pClearValues = &clearColor;
</pre>

最后两个参数定义了用于 VK_ATTACHMENT_LOAD_OP_CLEAR 的清除值，我们将其用作颜色附件的加载操作。
为了简化操作，我们定义了clear color为100%黑色。

vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
渲染通道现在可以启用。所有可以被记录的命令，被识别的前提是使用vkCmd前缀。它们全部返回void，所以在结束记录之前不会有任何错误处理。

对于每个命令，第一个参数总是记录该命令的命令缓冲区。第二个参数指定我们传递的渲染通道的具体信息。
最后的参数控制如何提供render pass将要应用的绘制命令。它使用以下数值任意一个:

* VK_SUBPASS_CONTENTS_INLINE: 渲染过程命令被嵌入在主命令缓冲区中，没有辅助缓冲区执行。
* VK_SUBPASS_CONTENTS_SECONDARY_COOMAND_BUFFERS: 渲染通道命令将会从辅助命令缓冲区执行。
我们不会使用辅助命令缓冲区，所以我们选择第一个。

## Basic drawing commands
 现在我们绑定图形管线:

vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);  
第二个参数指定具体管线类型，graphics or compute pipeline。我们告诉Vulkan在图形管线中每一个操作
如何执行及哪个附件将会在片段着色器中使用，所以剩下的就是告诉它绘制三角形。

vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);  
实际的vkCmdDraw函数有点与字面意思不一致，它是如此简单，仅因为我们提前指定所有渲染相关的信息。它有如下的参数需要指定，除了命令缓冲区:

* vertexCount: 即使我们没有顶点缓冲区，但是我们仍然有3个定点需要绘制。
* instanceCount: 用于instanced 渲染，如果没有使用请填1。
* firstVertex: 作为顶点缓冲区的偏移量，定义gl_VertexIndex的最小值。
* firstInstance: 作为instanced 渲染的偏移量，定义了gl_InstanceIndex的最小值。

## Finishing up

render pass执行完绘制，可以结束渲染作业: 

vkCmdEndRenderPass(commandBuffers[i]);  
并停止记录命令缓冲区的工作:

<pre>
if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
    throw std::runtime_error("failed to record command buffer!");
}
</pre>

在下一章节我们会尝试在main loop中编写代码，用于从交换链中获取图像，执行命令缓冲区的命令，再将渲染后的图像返还给交换链。


[代码](src/16.cpp)。