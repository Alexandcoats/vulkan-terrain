#include "Mesh.h"

Mesh::Mesh(bool enableValidation) : VulkanBase(enableValidation) {
	title = "Vulkan Terrain";

	cam = new Camera((float)width, (float)height);

	moveSpeed = 50.0f;
	sprintSpeed = 100.0f;

	GetClientRect(winHandle, &rect);
	ClientToScreen(winHandle, reinterpret_cast<POINT*>(&rect.left));
	centerPos = glm::vec2(rect.left + (float)width / 2.0f, rect.top + (float)height / 2.0f);
}

Mesh::~Mesh() {
	// Clean up resources

}

void Mesh::loadTextures() {
	textureLoader->loadTexture(
		"./../data/textures/dirt.png",
		VK_FORMAT_R8G8B8A8_UNORM,
		&textures.dirt,
		false);
	textureLoader->loadTexture(
		"./../data/textures/grass.png",
		VK_FORMAT_R8G8B8A8_UNORM,
		&textures.grass,
		false);
}

void Mesh::buildCommandBuffers() {
	if (!checkCommandBuffers()) {
		destroyCommandBuffers();
		createCommandBuffers();
	}

	VkCommandBufferBeginInfo cmdBufInfo = vkTools::initializers::commandBufferBeginInfo();

	VkClearValue clearValues[2];
	clearValues[0].color = defaultClearColor;
	clearValues[1].depthStencil = {1, 0};

	VkRenderPassBeginInfo renderPassBeginInfo = vkTools::initializers::renderPassBeginInfo();
	renderPassBeginInfo.renderPass = renderPass;
	renderPassBeginInfo.renderArea.offset.x = 0;
	renderPassBeginInfo.renderArea.offset.y = 0;
	renderPassBeginInfo.renderArea.extent.width = width;
	renderPassBeginInfo.renderArea.extent.height = height;
	renderPassBeginInfo.clearValueCount = 2;
	renderPassBeginInfo.pClearValues = clearValues;

	for (int32_t i = 0; i < drawCmdBuffers.size(); ++i) {
		renderPassBeginInfo.framebuffer = frameBuffers[i];

		vkTools::checkResult(vkBeginCommandBuffer(drawCmdBuffers[i], &cmdBufInfo));

		vkCmdBeginRenderPass(drawCmdBuffers[i], &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

		VkViewport viewport = vkTools::initializers::viewport(
			(float)width,
			(float)height,
			0.0f,
			1.0f);
		vkCmdSetViewport(drawCmdBuffers[i], 0, 1, &viewport);

		VkRect2D scissor = vkTools::initializers::rect2D(
			width,
			height,
			0,
			0);
		vkCmdSetScissor(drawCmdBuffers[i], 0, 1, &scissor);

		vkCmdBindDescriptorSets(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSetPostCompute, 0, NULL);
		vkCmdBindPipeline(drawCmdBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.render);

		VkDeviceSize offsets[1] = { 0 };
		vkCmdBindVertexBuffers(drawCmdBuffers[i], 0, 1, &meshes.terrain.vertices.buf, offsets);
		vkCmdBindIndexBuffer(drawCmdBuffers[i], meshes.terrain.indices.buf, 0, VK_INDEX_TYPE_UINT32);
		vkCmdDrawIndexed(drawCmdBuffers[i], meshes.terrain.indexCount, 1, 0, 0, 0);
	}
}

void Mesh::draw() {
	vkTools::checkResult(swapChain.acquireNextImage(semaphores.presentComplete, &currentBuffer));

	submitPostPresentBarrier(swapChain.buffers[currentBuffer].image);

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &drawCmdBuffers[currentBuffer];

	vkTools::checkResult(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));

	submitPrePresentBarrier(swapChain.buffers[currentBuffer].image);

	vkTools::checkResult(swapChain.queuePresent(queue, currentBuffer, semaphores.renderComplete));

	vkTools::checkResult(vkQueueWaitIdle(queue));
}

void Mesh::setupVertexDescriptions() {
	vertices.bindingDescriptions.resize(1);
	vertices.bindingDescriptions[0] =
		vkTools::initializers::vertexInputBindingDescription(
			0,
			(3+3)*sizeof(float),
			VK_VERTEX_INPUT_RATE_VERTEX);

	vertices.attributeDescriptions.resize(2);
	vertices.attributeDescriptions[0] =
		vkTools::initializers::vertexInputAttributeDescription(
			0,
			0,
			VK_FORMAT_R32G32B32_SFLOAT,
			0);
	vertices.attributeDescriptions[1] =
		vkTools::initializers::vertexInputAttributeDescription(
			0,
			1,
			VK_FORMAT_R32G32B32_SFLOAT,
			3 * sizeof(float));

	vertices.inputState = vkTools::initializers::pipelineVertexInputStateCreateInfo();
	vertices.inputState.vertexBindingDescriptionCount = vertices.bindingDescriptions.size();
	vertices.inputState.pVertexBindingDescriptions = vertices.bindingDescriptions.data();
	vertices.inputState.vertexAttributeDescriptionCount = vertices.attributeDescriptions.size();
	vertices.inputState.pVertexAttributeDescriptions = vertices.attributeDescriptions.data();
}

void Mesh::setupDescriptorPool() {
	std::vector<VkDescriptorPoolSize> poolSizes = {
		vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		vkTools::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2)
	};

	VkDescriptorPoolCreateInfo descriptorPoolInfo =
		vkTools::initializers::descriptorPoolCreateInfo(
			poolSizes.size(),
			poolSizes.data(),
			1);

	vkTools::checkResult(vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool));
}

void Mesh::setupDescriptorSetLayout() {
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		vkTools::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			VK_SHADER_STAGE_VERTEX_BIT,
			0),
		vkTools::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			1),
		vkTools::initializers::descriptorSetLayoutBinding(
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			VK_SHADER_STAGE_FRAGMENT_BIT,
			2)
	};

	VkDescriptorSetLayoutCreateInfo descriptorLayout =
		vkTools::initializers::descriptorSetLayoutCreateInfo(
			setLayoutBindings.data(),
			setLayoutBindings.size());

	vkTools::checkResult(vkCreateDescriptorSetLayout(device, &descriptorLayout, nullptr, &descriptorSetLayout));

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo =
		vkTools::initializers::pipelineLayoutCreateInfo(
			&descriptorSetLayout,
			1);

	vkTools::checkResult(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
}

void Mesh::setupDescriptorSet() {
	VkDescriptorSetAllocateInfo allocInfo =
		vkTools::initializers::descriptorSetAllocateInfo(
			descriptorPool,
			&descriptorSetLayout,
			1);

	vkTools::checkResult(vkAllocateDescriptorSets(device, &allocInfo, &descriptorSetPostCompute));

	std::vector<VkDescriptorImageInfo> texDescriptors = {
		vkTools::initializers::descriptorImageInfo(
			textures.dirt.sampler,
			textures.dirt.view,
			VK_IMAGE_LAYOUT_GENERAL),
		vkTools::initializers::descriptorImageInfo(
			textures.grass.sampler,
			textures.grass.view,
			VK_IMAGE_LAYOUT_GENERAL)
	};

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		vkTools::initializers::writeDescriptorSet(
			descriptorSetPostCompute,
			VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			0,
			&uniformData.mvp.descriptor),
		vkTools::initializers::writeDescriptorSet(
			descriptorSetPostCompute,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			&texDescriptors[0]),
		vkTools::initializers::writeDescriptorSet(
			descriptorSetPostCompute,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			1,
			&texDescriptors[1])
	};
	vkUpdateDescriptorSets(device, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, NULL);
}

void Mesh::preparePipelines() {
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState =
		vkTools::initializers::pipelineInputAssemblyStateCreateInfo(
			VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			0,
			VK_FALSE);

	VkPipelineRasterizationStateCreateInfo rasterizationState =
		vkTools::initializers::pipelineRasterizationStateCreateInfo(
			VK_POLYGON_MODE_FILL,
			VK_CULL_MODE_NONE,
			VK_FRONT_FACE_COUNTER_CLOCKWISE,
			0);

	VkPipelineColorBlendAttachmentState blendAttachmentState =
		vkTools::initializers::pipelineColorBlendAttachmentState(
			0xf,
			VK_FALSE);

	VkPipelineColorBlendStateCreateInfo colorBlendState =
		vkTools::initializers::pipelineColorBlendStateCreateInfo(
			1,
			&blendAttachmentState);

	VkPipelineDepthStencilStateCreateInfo depthStencilState =
		vkTools::initializers::pipelineDepthStencilStateCreateInfo(
			VK_TRUE,
			VK_TRUE,
			VK_COMPARE_OP_LESS_OR_EQUAL);

	VkPipelineViewportStateCreateInfo viewportState =
		vkTools::initializers::pipelineViewportStateCreateInfo(1, 1, 0);

	VkPipelineMultisampleStateCreateInfo multisampleState =
		vkTools::initializers::pipelineMultisampleStateCreateInfo(
			VK_SAMPLE_COUNT_1_BIT,
			0);

	std::vector<VkDynamicState> dynamicStateEnables = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR
	};
	VkPipelineDynamicStateCreateInfo dynamicState =
		vkTools::initializers::pipelineDynamicStateCreateInfo(
			dynamicStateEnables.data(),
			dynamicStateEnables.size(),
			0);

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages;

	shaderStages[0] = loadShader("./../data/shaders/render.vert.spv", VK_SHADER_STAGE_VERTEX_BIT);
	shaderStages[1] = loadShader("./../data/shaders/render.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT);

	VkGraphicsPipelineCreateInfo pipelineCreateInfo =
		vkTools::initializers::pipelineCreateInfo(
			pipelineLayout,
			renderPass,
			0);

	pipelineCreateInfo.pVertexInputState = &vertices.inputState;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.pDynamicState = &dynamicState;
	pipelineCreateInfo.stageCount = shaderStages.size();
	pipelineCreateInfo.pStages = shaderStages.data();
	pipelineCreateInfo.renderPass = renderPass;

	vkTools::checkResult(vkCreateGraphicsPipelines(device, pipelineCache, 1, &pipelineCreateInfo, nullptr, &pipelines.render));
}

void Mesh::prepareUniformBuffers() {
	createBuffer(
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
		sizeof(uboMVP),
		&uboMVP,
		&uniformData.mvp.buffer,
		&uniformData.mvp.memory,
		&uniformData.mvp.descriptor);

	updateUniformBuffers();
}

void Mesh::updateUniformBuffers() {
	uboMVP.projection = cam->projection;
	uboMVP.view = cam->view;
	uboMVP.model = glm::mat4();

	uint8_t *pData;
	vkTools::checkResult(vkMapMemory(device, uniformData.mvp.memory, 0, sizeof(uboMVP), 0, (void **)&pData));
	memcpy(pData, &uboMVP, sizeof(uboMVP));
	vkUnmapMemory(device, uniformData.mvp.memory);
}

void Mesh::prepare() {
	VulkanBase::prepare();
	loadTextures();
	setupVertexDescriptions();
	prepareUniformBuffers();
	setupDescriptorSetLayout();
	preparePipelines();
	setupDescriptorPool();
	setupDescriptorSet();
	buildCommandBuffers();
	prepared = true;
}

void Mesh::render() {
	if (!prepared)
		return;
	updateCamera();
	vkDeviceWaitIdle(device);
	draw();
	vkDeviceWaitIdle(device);
}

void Mesh::viewChanged() {
	updateUniformBuffers();
}

// Updates the camera once per frame
void Mesh::updateCamera() {
	float speed;
	if (keyboardState[VK_SHIFT])
		speed = sprintSpeed;
	else
		speed = moveSpeed;
	if (keyboardState[KEYBOARD_W]) {
		cam->translate(Direction::Forward, speed*frameTimer);
	}
	if (keyboardState[KEYBOARD_S]) {
		cam->translate(Direction::Backward, speed*frameTimer);
	}
	if (keyboardState[KEYBOARD_A]) {
		cam->translate(Direction::Left, speed*frameTimer);
	}
	if (keyboardState[KEYBOARD_D]) {
		cam->translate(Direction::Right, speed*frameTimer);
	}
	cam->rotate(Axis::V, speed*frameTimer*mouseDelta[0]);
	if(true)
		cam->rotate(Axis::U, speed*frameTimer*mouseDelta[1]);
	SetCursorPos(centerPos.x, centerPos.y);
	viewChanged();
}

void Mesh::handleMessages(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	VulkanBase::handleMessages(hWnd, uMsg, wParam, lParam);
	switch (uMsg) {
	case WM_KEYDOWN:
	{
		keyboardState[wParam] = true;
		switch (wParam) {
		case VK_ESCAPE:
			exit(0);
			break;
		}
		break;
	}
	case WM_KEYUP:
	{
		keyboardState[wParam] = false;
		break;
	}
	case WM_MOUSEMOVE:
	{
		int32_t posx = LOWORD(lParam);
		int32_t posy = HIWORD(lParam);
		mouseDelta[0] = posx + rect.left - centerPos.x;
		mouseDelta[1] = posy + rect.top - centerPos.y;
		break;
	}
	}
}