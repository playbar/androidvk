#Vulkan Tutorial 14 Integration pipeline


我们现在整合前几章节的结构体和对象创建图形管线！以下是我们现在用到的对象类型，作为一个快速回顾:

* Shader stages: 着色器模块定义了图形管线可编程阶段的功能
* Fixed-function state: 结构体定义固定管线功能，比如输入装配、光栅化、viewport和color blending
* Pipeline layout: 管线布局定义uniform 和 push values，被着色器每一次绘制的时候引用
* Render pass: 渲染通道通过管线阶段引用附件，并定义它的使用方式

所有这些决定了图形管线的最终功能，所以我们在createGraphicsPipeline函数的最后填充VkGraphicsPipelineCreateInfo结构体。

<pre>
VkGraphicsPipelineCreateInfo pipelineInfo = {};
pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
pipelineInfo.stageCount = 2;
pipelineInfo.pStages = shaderStages;
</pre>

现在开始引用之前的VkPipelineShaderStageCreateInfo结构体数组。

<pre>
pipelineInfo.pVertexInputState = &vertexInputInfo;
pipelineInfo.pInputAssemblyState = &inputAssembly;
pipelineInfo.pViewportState = &viewportState;
pipelineInfo.pRasterizationState = &rasterizer;
pipelineInfo.pMultisampleState = &multisampling;
pipelineInfo.pDepthStencilState = nullptr; // Optional
pipelineInfo.pColorBlendState = &colorBlending;
pipelineInfo.pDynamicState = nullptr; // Optional
</pre>


并引用之前描述固定管线功能的结构体。

pipelineInfo.layout = pipelineLayout;  
完成之后，pipeline layout管线布局，它是一个Vulkan句柄而不是结构体指针。

pipelineInfo.renderPass = renderPass;  
pipelineInfo.subpass = 0;  
最后我们需要引用render pass和图形管线将要使用的子通道sub pass的索引。

pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional  
pipelineInfo.basePipelineIndex = -1; // Optional  
实际上还有两个参数:basePipelineHandle 和 basePipelineIndex。Vulkan允许您通过已经存在的管线创建新的图形管线。这种衍生出新管线的想法在于，当要创建的管线与现有管道功能相同时，获得较低的开销，同时也可以更快的完成管线切换，当它们来自同一个父管线。可以通过basePipelineHandle指定现有管线的句柄，也可以引用由basePipelineIndex所以创建的另一个管线。目前只有一个管线，所以我们只需要指定一个空句柄和一个无效的索引。只有在VkGraphicsPipelineCreateInfo的flags字段中也指定了VK_PIPELINE_CREATE_DERIVATIVE_BIT标志时，才需要使用这些值。


现在准备最后一步，创建一个类成员保存VkPipeline对象:

VkPipeline graphicsPipeline;  
最后创建图形管线:

<pre>
if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
    throw std::runtime_error("failed to create graphics pipeline!");
}
</pre>

vkCreateGraphicsPipelines函数在Vulkan中比起一般的创建对象函数需要更多的参数。
它可以用来传递多个VkGraphicsPipelineCreateInfo对象并创建多个VkPipeline对象。

我们传递VK_NULL_HANDLE参数作为第二个参数，作为可选VkPipelineCache对象的引用。
管线缓存可以用于存储和复用与通过多次调用vkCreateGraphicsPipelines函数相关的数据，甚至在程序执行的时候缓存到一个文件中。
这样可以加速后续的管线创建逻辑。具体的内容我们会在管线缓存章节介绍。

 

图形管线对于常见的绘图操作是必须的，所以它也应该在程序结束时销毁:

<pre>
void cleanup() {
    vkDestroyPipeline(device, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    ...
}
</pre>

现在运行程序，确认所有工作正常，并创建图形管线成功！我们已经无比接近在屏幕上绘制出东西来了。
在接下来的几个章节中，我们将从交换链图像中设置实际的帧缓冲区，并准备绘制命令。


[代码](src/14.cpp)。
