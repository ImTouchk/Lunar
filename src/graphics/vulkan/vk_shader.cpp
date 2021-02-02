#include "utils/debug.hpp"
#include "utils/types.hpp"
#include "graphics/vk_shader.hpp"
#include "graphics/vk_renderer.hpp"
#include <vulkan/vulkan.hpp>
#include <fmt/core.h>
#include <fstream>
#include <vector>
#include <mutex>

using namespace std;

static Aery::mut_u32 Index = 0;
static mutex ListMutex = {};
static mutex PoolMutex = {};

static bool LoadFile(const char* Input, vector<char>& Output, Aery::u32 ID) {
    ifstream Stream = ifstream(Input, ios::ate | ios::binary);
    if (!Stream.is_open()) {
        Aery::log(fmt::format("<VkShader::LoadFile> ID {} failed to load file {}.", ID, Input));
        return false;
    }

    Aery::u32 FileSize = Aery::u32 ( Stream.tellg() );
    if (!Output.empty()) { Output.clear(); }
    Output.resize(FileSize);
    Stream.seekg(0);
    Stream.read(Output.data(), FileSize);
    Stream.close();
    return true;
}

static bool CreateModule(vector<char>& Input, vk::ShaderModule& Output, vk::Device& Device) {
    const vk::ShaderModuleCreateInfo ModuleInfo = {
        .codeSize = Input.size(),
        .pCode = reinterpret_cast<Aery::u32*>(Input.data())
    };
    const vk::Result Result = Device.createShaderModule(&ModuleInfo, nullptr, &Output);
    if (Result != vk::Result::eSuccess) {
        Aery::error("<VkShader::CreateModule> Failed to create a shader module.");
        return false;
    }
    return true;
}

namespace Aery {
    bool VkRenderer::createDefaultShader(VkShader* Output) {
        static bool Existent = false;
        static VkShader DefaultShader;

        if (Existent) {
            if (Output != nullptr) { *Output = DefaultShader; }
            return true;
        }

        VkShaderCreateInfo Default = {
            .vertex = "assets/default_vert.spv",
            .fragment = "assets/default_frag.spv",
        };

        if (!createShader(Default, &DefaultShader)) { return false; }
        if (Output != nullptr) { *Output = DefaultShader; }
        return true;
    }

    bool VkRenderer::createShader(VkShaderCreateInfo& Input, VkShader* Output) {
        auto CreatePipeline = [&](VkShader& Shader) {
            vector<char> VertexCode = {},
                FragmentCode = {};

            if (!LoadFile(Input.vertex, VertexCode, m_ID)) { return false; }
            if (!LoadFile(Input.fragment, FragmentCode, m_ID)) { return false; }

            vk::ShaderModule FragmentModule = {},
                VertexModule = {};

            if (!CreateModule(VertexCode, VertexModule, m_Device)) { return false; }
            if (!CreateModule(FragmentCode, FragmentModule, m_Device)) { return false; }


            vk::PipelineShaderStageCreateInfo VertexCreateInfo = {
                .stage = vk::ShaderStageFlagBits::eVertex,
                .module = VertexModule,
                .pName = "main",
            };

            vk::PipelineShaderStageCreateInfo FragmentCreateInfo = {
                .stage = vk::ShaderStageFlagBits::eFragment,
                .module = FragmentModule,
                .pName = "main",
            };

            vk::PipelineShaderStageCreateInfo ShaderStages[2] = {
                VertexCreateInfo,
                FragmentCreateInfo
            };

            vk::PipelineVertexInputStateCreateInfo VertexInputInfo = {
                .vertexBindingDescriptionCount = 0,
                .pVertexBindingDescriptions = nullptr,
                .vertexAttributeDescriptionCount = 0,
                .pVertexAttributeDescriptions = nullptr
            };

            vk::PipelineInputAssemblyStateCreateInfo AssemblyInfo = {
                .topology = vk::PrimitiveTopology::eTriangleList,
                .primitiveRestartEnable = VK_FALSE
            };

            vk::Viewport Viewport = {
                .x = 0, .y = 0,
                .width = (float)m_Swapchain.extent.width,
                .height = (float)m_Swapchain.extent.height,
                .minDepth = 0.0f, .maxDepth = 1.0f
            };

            vk::Rect2D Scissor = {
                .offset = { 0, 0 },
                .extent = m_Swapchain.extent
            };

            vk::PipelineViewportStateCreateInfo ViewportStateInfo = {
                .viewportCount = 1,
                .pViewports = &Viewport,
                .scissorCount = 1,
                .pScissors = &Scissor
            };

            vk::PipelineRasterizationStateCreateInfo RasterStateInfo = {
                .depthClampEnable = VK_FALSE,
                .rasterizerDiscardEnable = VK_FALSE,
                .polygonMode = vk::PolygonMode::eFill,
                .cullMode = vk::CullModeFlagBits::eBack,
                .frontFace = vk::FrontFace::eClockwise,
                .depthBiasEnable = VK_FALSE,
                .depthBiasConstantFactor = 0.0f,
                .depthBiasClamp = 0.0f,
                .depthBiasSlopeFactor = 0.0f,
                .lineWidth = 1.0f,
            };

            vk::PipelineMultisampleStateCreateInfo MultisampleStateInfo = {
                .rasterizationSamples = vk::SampleCountFlagBits::e1,
                .sampleShadingEnable = VK_FALSE,
                .minSampleShading = 1.0f,
                .pSampleMask = nullptr,
                .alphaToCoverageEnable = VK_FALSE,
                .alphaToOneEnable = VK_FALSE,
            };

            using CCFlags = vk::ColorComponentFlagBits;
            vk::PipelineColorBlendAttachmentState BlendAttachmentState = {
                .blendEnable = VK_TRUE,
                .srcColorBlendFactor = vk::BlendFactor::eSrcAlpha,
                .dstColorBlendFactor = vk::BlendFactor::eOneMinusSrcAlpha,
                .colorBlendOp = vk::BlendOp::eAdd,
                .srcAlphaBlendFactor = vk::BlendFactor::eOne,
                .dstAlphaBlendFactor = vk::BlendFactor::eZero,
                .alphaBlendOp = vk::BlendOp::eAdd,
                .colorWriteMask = CCFlags::eR | CCFlags::eG | CCFlags::eB | CCFlags::eA,
            };

            vk::PipelineColorBlendStateCreateInfo BlendState = {
                .logicOpEnable = VK_FALSE,
                .logicOp = vk::LogicOp::eCopy,
                .attachmentCount = 1,
                .pAttachments = &BlendAttachmentState,
            };
            BlendState.setBlendConstants({ 0.0f, 0.0f, 0.0f, 0.0f });

            vk::PipelineLayoutCreateInfo LayoutInfo = {
                .setLayoutCount = 0,
                .pSetLayouts = nullptr,
                .pushConstantRangeCount = 0,
                .pPushConstantRanges = nullptr
            };

            vk::Result Result = m_Device.createPipelineLayout(&LayoutInfo, nullptr, &Shader.layout);
            if (Result != vk::Result::eSuccess) {
                Aery::error(fmt::format("<VkRenderer::createShader> ID {} failed to create a pipeline layout.", m_ID));
                return false;
            }

            vk::GraphicsPipelineCreateInfo PipelineInfo = {
                .stageCount = 2,
                .pStages = ShaderStages,
                .pVertexInputState = &VertexInputInfo,
                .pInputAssemblyState = &AssemblyInfo,
                .pViewportState = &ViewportStateInfo,
                .pRasterizationState = &RasterStateInfo,
                .pMultisampleState = &MultisampleStateInfo,
                .pDepthStencilState = nullptr,
                .pColorBlendState = &BlendState,
                .pDynamicState = nullptr,
                .layout = Shader.layout,
                .renderPass = m_RenderPass,
                .subpass = 0,
                .basePipelineHandle = nullptr,
                .basePipelineIndex = -1,
            };
        
            vk::ResultValue<vk::Pipeline> PipelineRes = m_Device.createGraphicsPipeline({}, PipelineInfo);
            if (PipelineRes.result != vk::Result::eSuccess) {
                Aery::warn(fmt::format("<VkRenderer::createShader> ID {} failed to create a pipeline.", m_ID));
                return false;
            }

            Shader.pipeline = PipelineRes.value;
            return true;
        };

        auto CreateCommandBuffers = [&](VkShader& Shader) {
            Shader.cmdBuffers.resize(m_Swapchain.buffers.size());
            vk::CommandBufferAllocateInfo BufferAllocateInfo = {
                .commandPool = m_CommandPool,
                .level = vk::CommandBufferLevel::eSecondary,
                .commandBufferCount = (u32)m_CommandBuffers.size()
            };

            PoolMutex.lock();
            vk::Result Result = m_Device.allocateCommandBuffers(&BufferAllocateInfo, Shader.cmdBuffers.data());
            PoolMutex.unlock();

            if (Result != vk::Result::eSuccess) {
                Aery::error(fmt::format("<VkRenderer::createShader> ID {} failed to allocate command buffers.", m_ID));
                return false;
            }

            for (mut_u32 i = 0; i < m_CommandBuffers.size(); i++) {
                vk::CommandBufferBeginInfo BufferBeginInfo = {};
                m_CommandBuffers[i].begin(BufferBeginInfo);

                vk::ClearColorValue ClearColor = {};
                ClearColor.setFloat32({ 0.0f, 0.0f, 0.0f, 1.0f });
                vk::ClearValue ClearValue = {};
                ClearValue.setColor(ClearColor);

                vk::RenderPassBeginInfo PassBeginInfo = {
                    .renderPass = m_RenderPass,
                    .framebuffer = m_Swapchain.buffers[i],
                    .renderArea = {
                        .offset = { 0, 0 },
                        .extent = m_Swapchain.extent
                    },
                    .clearValueCount = 1,
                    .pClearValues = &ClearValue,
                };

                m_CommandBuffers[i].beginRenderPass(PassBeginInfo, vk::SubpassContents::eInline);
                for (mut_u32 sh = 0; sh < m_Shaders.size(); sh++) {
                    m_CommandBuffers[i].bindPipeline(vk::PipelineBindPoint::eGraphics, m_Shaders[sh].pipeline);
                    m_CommandBuffers[i].draw(3, 1, 0, 0);
                }
                m_CommandBuffers[i].endRenderPass();
                m_CommandBuffers[i].end();
            }
            return true;
        };

        VkShader NewShader = {};

        if (!CreatePipeline(NewShader)) { return false; }
        if (!CreateCommandBuffers(NewShader)) { return false; }

        ListMutex.lock();
        NewShader.id = Index; Index++;
        m_Shaders.push_back(NewShader);
        ListMutex.unlock();
        if (Output != nullptr) { 
            *Output = m_Shaders[m_Shaders.size() - 1]; 
        }

        Aery::log(fmt::format("<VkRenderer::createShader> ID {} created a pipeline {}.", m_ID, NewShader.id));
        return true;
    }

    void VkRenderer::destroyShader(VkShader& Shader) {
        m_Device.destroyPipeline(Shader.pipeline);
        m_Device.destroyPipelineLayout(Shader.layout);
        Aery::log(fmt::format("<VkRenderer::createShader> ID {} destroyed pipeline {}.", m_ID, Shader.id));
    }

    VkShader& VkRenderer::getShaderByID(u32 ID) {
        return m_Shaders[ID];
    }
}