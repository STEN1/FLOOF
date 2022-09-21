#include "Components.h"

#include <cstring>
#include "stb_image.h"
#include "ObjLoader.h"
#include "LoggerMacros.h"
#include "Utils.h"

namespace FLOOF {
	TextureComponent::TextureComponent(const std::string& path) {
		auto it = s_TextureDataCache.find(path);
		if (it != s_TextureDataCache.end()) {
			Data = it->second;
			return;
		}

		auto renderer = VulkanRenderer::Get();
		// Load texture
		int xWidth, yHeight, channels;
		stbi_set_flip_vertically_on_load(true);
		auto* data = stbi_load(path.c_str(), &xWidth, &yHeight, &channels, 0);
		uint32_t size = xWidth * yHeight * channels;
		ASSERT(channels == 4);

		// staging buffer
		VkBufferCreateInfo stagingCreateInfo = { VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO };
		stagingCreateInfo.size = size;
		stagingCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;

		VmaAllocationCreateInfo stagingBufAllocCreateInfo = {};
		stagingBufAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;
		stagingBufAllocCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT |
			VMA_ALLOCATION_CREATE_MAPPED_BIT;

		VkBuffer stagingBuffer{};
		VmaAllocation stagingBufferAlloc{};
		VmaAllocationInfo stagingBufferAllocInfo{};
		vmaCreateBuffer(renderer->m_Allocator, &stagingCreateInfo, &stagingBufAllocCreateInfo, &stagingBuffer,
			&stagingBufferAlloc, &stagingBufferAllocInfo);

		ASSERT(stagingBufferAllocInfo.pMappedData != nullptr);
		memcpy(stagingBufferAllocInfo.pMappedData, data, size);
		stbi_image_free(data);

		// Image
		VkImageCreateInfo imageInfo = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.extent.width = xWidth;
		imageInfo.extent.height = yHeight;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.flags = 0;

		VmaAllocationCreateInfo imageAllocCreateInfo = {};
		imageAllocCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

		vmaCreateImage(renderer->m_Allocator, &imageInfo, &imageAllocCreateInfo, &Data.CombinedTextureSampler.Image,
			&Data.CombinedTextureSampler.Allocation, &Data.CombinedTextureSampler.AllocationInfo);

		// copy image from staging buffer to image buffer(gpu only memory)
		renderer->CopyBufferToImage(stagingBuffer, Data.CombinedTextureSampler.Image, xWidth, yHeight);

		// free staging buffer
		vmaDestroyBuffer(renderer->m_Allocator, stagingBuffer, stagingBufferAlloc);

		// create image view
		VkImageViewCreateInfo textureImageViewInfo = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
		textureImageViewInfo.image = Data.CombinedTextureSampler.Image;
		textureImageViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		textureImageViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
		textureImageViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		textureImageViewInfo.subresourceRange.baseMipLevel = 0;
		textureImageViewInfo.subresourceRange.levelCount = 1;
		textureImageViewInfo.subresourceRange.baseArrayLayer = 0;
		textureImageViewInfo.subresourceRange.layerCount = 1;
		vkCreateImageView(renderer->m_LogicalDevice, &textureImageViewInfo, nullptr, &Data.CombinedTextureSampler.ImageView);

		// sampler
		VkSamplerCreateInfo samplerInfo = { VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO };
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		samplerInfo.anisotropyEnable = VK_TRUE;
		samplerInfo.maxAnisotropy = 16;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.f;
		samplerInfo.minLod = 0.f;
		samplerInfo.maxLod = FLT_MAX;
		vkCreateSampler(renderer->m_LogicalDevice, &samplerInfo, nullptr, &Data.CombinedTextureSampler.Sampler);

		// Get descriptor set and point it to data.
		Data.DesctriptorSet = renderer->AllocateTextureDescriptorSet();

		VkDescriptorImageInfo descriptorImageInfo{};
		descriptorImageInfo.sampler = Data.CombinedTextureSampler.Sampler;
		descriptorImageInfo.imageView = Data.CombinedTextureSampler.ImageView;
		descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet writeDescriptorSet{};
		writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		writeDescriptorSet.dstSet = Data.DesctriptorSet;
		writeDescriptorSet.dstBinding = 0;
		writeDescriptorSet.descriptorCount = 1;
		writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		writeDescriptorSet.pImageInfo = &descriptorImageInfo;

		vkUpdateDescriptorSets(renderer->m_LogicalDevice, 1, &writeDescriptorSet, 0, nullptr);

		s_TextureDataCache[path] = Data;
	}
	TextureComponent::~TextureComponent() {
	}
	void TextureComponent::Bind(VkCommandBuffer commandBuffer) {
		auto renderer = VulkanRenderer::Get();

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, renderer->GetPipelineLayout(RenderPipelineKeys::Basic),
			0, 1, &Data.DesctriptorSet, 0, 0);
	}
	void TextureComponent::ClearTextureDataCache() {
		auto renderer = VulkanRenderer::Get();

		for (auto& [key ,data] : s_TextureDataCache) {
			renderer->FreeTextureDescriptorSet(data.DesctriptorSet);
			vkDestroyImageView(renderer->m_LogicalDevice, data.CombinedTextureSampler.ImageView, nullptr);
			vmaDestroyImage(renderer->m_Allocator, data.CombinedTextureSampler.Image, data.CombinedTextureSampler.Allocation);
			vkDestroySampler(renderer->m_LogicalDevice, data.CombinedTextureSampler.Sampler, nullptr);
		}
	}
	MeshComponent::MeshComponent(const std::string& path) {
		auto* renderer = VulkanRenderer::Get();

		auto it = s_MeshDataCache.find(path);
		if (it == s_MeshDataCache.end()) {
			auto [vertexData, indexData] = ObjLoader(path).GetIndexedData();
			Data.VertexBuffer = renderer->CreateVertexBuffer(vertexData);
			Data.IndexBuffer = renderer->CreateIndexBuffer(indexData);
			Data.VertexCount = vertexData.size();
			Data.IndexCount = indexData.size();
			s_MeshDataCache[path] = Data;
		} else {
			Data = it->second;
		}
		m_IsCachedMesh = true;
	}
	MeshComponent::MeshComponent(const std::vector<MeshVertex>& vertexData, const std::vector<uint32_t>& indexData) {
		auto* renderer = VulkanRenderer::Get();

		Data.VertexBuffer = renderer->CreateVertexBuffer(vertexData);
		Data.IndexBuffer = renderer->CreateIndexBuffer(indexData);
		Data.VertexCount = vertexData.size();
		Data.IndexCount = indexData.size();
	}
	MeshComponent::MeshComponent(const std::vector<MeshVertex>& vertexData) {
		auto* renderer = VulkanRenderer::Get();

		Data.VertexBuffer = renderer->CreateVertexBuffer(vertexData);
		Data.VertexCount = vertexData.size();
	}
	MeshComponent::~MeshComponent() {
		if (m_IsCachedMesh == false) {
			auto* renderer = VulkanRenderer::Get();
			vmaDestroyBuffer(renderer->m_Allocator, Data.IndexBuffer.Buffer, Data.IndexBuffer.Allocation);
			vmaDestroyBuffer(renderer->m_Allocator, Data.VertexBuffer.Buffer, Data.VertexBuffer.Allocation);
		}
	}
	void MeshComponent::Draw(VkCommandBuffer commandBuffer) {
		VkDeviceSize offset{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &Data.VertexBuffer.Buffer, &offset);
		if (Data.IndexBuffer.Buffer != VK_NULL_HANDLE) {
			vkCmdBindIndexBuffer(commandBuffer, Data.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);
			vkCmdDrawIndexed(commandBuffer, Data.IndexCount,
				1, 0, 0, 0);
		} else {
			vkCmdDraw(commandBuffer, Data.VertexCount, 1, 0, 0);
		}
	}
	void MeshComponent::ClearMeshDataCache() {
		auto* renderer = VulkanRenderer::Get();
		for (auto& [key, data] : s_MeshDataCache) {
			vmaDestroyBuffer(renderer->m_Allocator, data.IndexBuffer.Buffer, data.IndexBuffer.Allocation);
			vmaDestroyBuffer(renderer->m_Allocator, data.VertexBuffer.Buffer, data.VertexBuffer.Allocation);
		}
	}
	LineMeshComponent::LineMeshComponent(const std::vector<ColorVertex>& vertexData) {
		auto renderer = VulkanRenderer::Get();

		VertexBuffer = renderer->CreateVertexBuffer(vertexData);
		VertexCount = vertexData.size();
		MaxVertexCount = VertexCount;
	}
	LineMeshComponent::~LineMeshComponent() {
		auto renderer = VulkanRenderer::Get();

		vmaDestroyBuffer(renderer->m_Allocator, VertexBuffer.Buffer, VertexBuffer.Allocation);
	}
	void LineMeshComponent::Draw(VkCommandBuffer commandBuffer) {
		auto renderer = VulkanRenderer::Get();

		VkDeviceSize offset{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &VertexBuffer.Buffer, &offset);
		vkCmdDraw(commandBuffer, VertexCount, 1, 0, 0);
	}
	void LineMeshComponent::UpdateBuffer(VkCommandBuffer commandBuffer, const std::vector<ColorVertex>& vertexData) {
		if (vertexData.size() == 0) {
			VertexCount = 0;
			return;
		}
		if (vertexData.size() > MaxVertexCount) {
			std::cout << "Cant update buffer with data larger than vkBuffer.\n";
			return;
		}
		VertexCount = vertexData.size();
		vkCmdUpdateBuffer(commandBuffer, VertexBuffer.Buffer, {}, VertexCount * sizeof(ColorVertex), vertexData.data());
	}
	CameraComponent::CameraComponent(glm::vec3 position) : Position{ position } {
		Up = glm::vec3(0.f, -1.f, 0.f);
		Forward = glm::vec3(0.f, 0.f, -1.f);
		Right = glm::normalize(glm::cross(Forward, Up));
	}
	glm::mat4 CameraComponent::GetVP(float fov, float aspect, float near, float far) {
		FOV = fov;
		Aspect = aspect;
		Near = near;
		Far = far;
		glm::mat4 view = glm::lookAt(Position, Position + Forward, Up);
		glm::mat4 projection = glm::perspective(fov, aspect, near, far);
		return projection * view;
	}
	void CameraComponent::MoveForward(float amount) {
		Position += Forward * amount;
	}
	void CameraComponent::MoveRight(float amount) {
		glm::vec3 right = glm::normalize(glm::cross(Forward, Up));
		Position += right * amount;
	}
	void CameraComponent::Pitch(float amount) {
		if (amount == 0.f) return;
		glm::vec3 right = glm::normalize(glm::cross(Forward, Up));
		glm::mat4 rotation = glm::rotate(amount, right);
		Forward = glm::normalize(glm::vec3(rotation * glm::vec4(Forward, 1.f)));
	}
	void CameraComponent::Yaw(float amount) {
		if (amount == 0.f) return;
		glm::mat4 rotation = glm::rotate(-amount, Up);
		Forward = glm::normalize(glm::vec3(rotation * glm::vec4(Forward, 1.f)));
	}
	TerrainComponent::TerrainComponent(std::vector<Triangle>&& triangles) {
		Triangles = triangles;
	}
	TerrainComponent::TerrainComponent(const std::vector<Triangle>& triangles) {
		Triangles = triangles;
	}
	void TerrainComponent::PrintTriangleData() {
		uint32_t triangleId = 0;
		for (auto& triangle : Triangles) {
			std::cout << "Triangle: " << triangleId++ << std::endl;
			std::cout << "A: " << triangle.A << std::endl;
			std::cout << "B: " << triangle.B << std::endl;
			std::cout << "C: " << triangle.C << std::endl;
			std::cout << "Normal: " << triangle.N << std::endl;
		}
	}
	PointCloudComponent::PointCloudComponent(const std::vector<ColorVertex>& vertexData) {
		auto renderer = VulkanRenderer::Get();

		VertexBuffer = renderer->CreateVertexBuffer(vertexData);
		VertexCount = vertexData.size();
	}
	PointCloudComponent::~PointCloudComponent() {
		auto renderer = VulkanRenderer::Get();

		vmaDestroyBuffer(renderer->m_Allocator, VertexBuffer.Buffer, VertexBuffer.Allocation);
	}
	void PointCloudComponent::Draw(VkCommandBuffer commandBuffer) {
		auto renderer = VulkanRenderer::Get();

		VkDeviceSize offset{ 0 };
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &VertexBuffer.Buffer, &offset);
		vkCmdDraw(commandBuffer, VertexCount, 1, 0, 0);
	}
}