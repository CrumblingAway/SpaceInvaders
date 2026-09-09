#include "space_invaders_game.h"

#include "tiny_obj_loader.h"

#include <iostream>

#include <ktx.h>
#include <ktxvulkan.h>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define PRINT_SDL_CRITICAL_ERROR(text)\
    std::cerr << "SDL: " << text << ". Error " << SDL_GetError() << ".\n";
#define PRINT_VULKAN_CRITICAL_ERROR(text, error_enum)\
    std::cerr << "Vulkan: " << text << ". Error " << error_enum << ".\n";
#define PRINT_TINYOBJ_CRITICAL_ERROR(text, string_warn, string_error)\
    std::cerr << "tinyobj: " << text << ". Warning: " << string_warn << ". Error: " << string_error << ".\n";

#define RETURN_FALSE_ON_FAIL_VULKAN(text, error_num) if (error_num != VK_SUCCESS) { PRINT_VULKAN_CRITICAL_ERROR(text, error_num); return false; }

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 normal;
    glm::vec2 uv;
};

struct ShaderDataBuffer
{
    VmaAllocation allocation{ VK_NULL_HANDLE };
    VmaAllocationInfo allocationInfo{};
    VkBuffer buffer{ VK_NULL_HANDLE };
    VkDeviceAddress deviceAddress{};
};

struct ShaderData
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 model[3];
    glm::vec4 lightPos{ 0.0f, -10.0f, 10.0f, 0.0f };
    uint32_t selected{ 1 };
};

void SpaceInvadersGame::Run()
{
    Init();
    MainLoop();
    Cleanup();
}

const char *SpaceInvadersGame::GetName() const
{
    return m_pApplicationName;
}

void SpaceInvadersGame::Init()
{
    if (!InitSDL())
    {

    }
    if (!InitVulkan())
    {

    }
}

bool SpaceInvadersGame::InitSDL()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        PRINT_SDL_CRITICAL_ERROR("Failed to initialize");
        return false;
    }

    m_sdlWindow = SDL_CreateWindow(
        m_pApplicationName,
        m_windowWidth,
        m_windowHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    );
    if (m_sdlWindow == nullptr)
    {
        PRINT_SDL_CRITICAL_ERROR("Failed to create window");
        return false;
    }

    return true;
}

void SpaceInvadersGame::CleanupSDL()
{
    SDL_DestroyWindow(m_sdlWindow);
}

bool SpaceInvadersGame::InitVulkan()
{
    VkResult vkResult;

    { /* Instance. */
    VkApplicationInfo appInfo
    {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = m_pApplicationName,
        .apiVersion = VK_API_VERSION_1_4
    };

    uint32_t instanceExtensionsCount{ 0 };
    char const* const* instanceExtensions{ SDL_Vulkan_GetInstanceExtensions(&instanceExtensionsCount) };

    VkInstanceCreateInfo instanceCI
    {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = instanceExtensionsCount,
        .ppEnabledExtensionNames = instanceExtensions
    };

    vkResult = vkCreateInstance(&instanceCI, nullptr, &m_vkInstance); 
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to init instance", vkResult);
    }

    std::vector<VkPhysicalDevice> devices;
    uint32_t deviceIndex{ 0 };
    { /* Physical device. */
    #pragma region Select physical device.
    uint32_t deviceCount{ 0 };
    vkResult = vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, nullptr);
    devices.resize(deviceCount);
    vkResult = vkEnumeratePhysicalDevices(m_vkInstance, &deviceCount, devices.data());
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to enumerate physical devices", vkResult);
    VkPhysicalDeviceProperties2 deviceProperties
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2
    };
    vkGetPhysicalDeviceProperties2(devices[deviceIndex], &deviceProperties);
    std::cout << "Selected device: " << deviceProperties.properties.deviceName <<  "\n";
    #pragma endregion Select physical device.
    }

    uint32_t queueFamily{ 0 };
    { /* Queue family. */
    #pragma region Get queue family info.
    uint32_t queueFamilyCount{ 0 };
    vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIndex], &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(devices[deviceIndex], &queueFamilyCount, queueFamilies.data());
    for (size_t i = 0; i < queueFamilies.size(); ++i)
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            queueFamily = i;
            break;
        }
    }
    if (!SDL_Vulkan_GetPresentationSupport(m_vkInstance, devices[deviceIndex], queueFamily))
    {
        PRINT_SDL_CRITICAL_ERROR("Presentation not supported with provided Vulkan physical device and"
        " queue family");
        return false;
    }
    }

    { /* Device. */
    const float queuePriorities{ 1.0f };
    VkDeviceQueueCreateInfo queueCI
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriorities
    };

    const std::vector<const char*> deviceExtensions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkPhysicalDeviceVulkan12Features enabledVk12Features
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .shaderSampledImageArrayNonUniformIndexing = true,
        .descriptorBindingVariableDescriptorCount = true,
        .runtimeDescriptorArray = true,
        .bufferDeviceAddress = true
    };
    VkPhysicalDeviceVulkan13Features enabledVk13Features
    {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &enabledVk12Features,
        .synchronization2 = true,
        .dynamicRendering = true
    };
    VkPhysicalDeviceFeatures enabledVk10Features
    {
        .samplerAnisotropy = VK_TRUE
    };

    VkDeviceCreateInfo deviceCI
    {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabledVk13Features,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCI,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &enabledVk10Features
    };
    vkResult = vkCreateDevice(devices[deviceIndex], &deviceCI, nullptr, &m_vkDevice);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create device handle", vkResult);
    vkGetDeviceQueue(m_vkDevice, queueFamily, 0, &m_vkQueue);
    }

    { /* VMA. */
    VmaVulkanFunctions vkFunctions
    {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
        .vkCreateImage = vkCreateImage
    };
    VmaAllocatorCreateInfo allocatorCI
    {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = devices[deviceIndex],
        .device = m_vkDevice,
        .pVulkanFunctions = &vkFunctions,
        .instance = m_vkInstance
    };
    vkResult = vmaCreateAllocator(&allocatorCI, &m_vmaAllocator);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create VMA allocator", vkResult);
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    { /* Vulkan surface. */
    if (!SDL_Vulkan_CreateSurface(m_sdlWindow, m_vkInstance, nullptr, &m_vkSurface)
        || !SDL_GetWindowSize(m_sdlWindow, &m_glmWindowSize.x, &m_glmWindowSize.y))
    {
        PRINT_SDL_CRITICAL_ERROR("Failed to create Vulkan surface");
        return false;
    }
    vkResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        devices[deviceIndex],
        m_vkSurface,
        &surfaceCapabilities
    );
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to get physical device surface capabilities", vkResult);
    }

    { /* Swapchain. */
    VkExtent2D swapchainExtent{ surfaceCapabilities.currentExtent };
    if (surfaceCapabilities.currentExtent.width == 0xFFFFFFFF)
    {
        swapchainExtent = {
            .width = static_cast<uint32_t>(m_glmWindowSize.x),
            .height = static_cast<uint32_t>(m_glmWindowSize.y),
        };
    }

    const VkFormat imageFormat{ VK_FORMAT_B8G8R8A8_SRGB };
    VkSwapchainCreateInfoKHR swapchainCI
    {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_vkSurface,
        .minImageCount = surfaceCapabilities.minImageCount,
        .imageFormat = imageFormat,
        .imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR,
        .imageExtent{ .width = swapchainExtent.width, .height = swapchainExtent.height },
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR
    };
    vkResult = vkCreateSwapchainKHR(m_vkDevice, &swapchainCI, nullptr, &m_vkSwapchain);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create swapchain", vkResult);

    uint32_t imageCount{ 0 };
    vkResult = vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &imageCount, nullptr);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to get swapchain images", vkResult);
    m_vkSwapchainImages.resize(imageCount);
    vkResult = vkGetSwapchainImagesKHR(m_vkDevice, m_vkSwapchain, &imageCount, m_vkSwapchainImages.data());
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to get swapchain images", vkResult);
    m_vkSwapchainImageViews.resize(imageCount);
    }

    { /* Depth attachment. */
    std::vector<VkFormat> depthFormatList{
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    VkFormat depthFormat{ VK_FORMAT_UNDEFINED };
    for (VkFormat& format : depthFormatList)
    {
        VkFormatProperties2 formatProperties{ .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2 };
        vkGetPhysicalDeviceFormatProperties2(devices[deviceIndex], format, &formatProperties);
        if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            depthFormat = format;
            break;
        }
    }

    VkImageCreateInfo depthImageCI
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthFormat,
        .extent{
            .width = static_cast<uint32_t>(m_glmWindowSize.x),
            .height = static_cast<uint32_t>(m_glmWindowSize.y),
            .depth = 1
        },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VmaAllocationCreateInfo allocationCI
    {
        .flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    vkResult = vmaCreateImage(
        m_vmaAllocator,
        &depthImageCI,
        &allocationCI,
        &m_vkDepthImage,
        &m_vmaDepthImageAllocation,
        nullptr
    );
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create depth image", vkResult);

    VkImageViewCreateInfo depthViewCI
    {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_vkDepthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depthFormat,
        .subresourceRange{
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1
        }
    };
    vkResult = vkCreateImageView(m_vkDevice, &depthViewCI, nullptr, &m_vkDepthImageView);
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create depth image view", vkResult);
    }

    { /* Load meshes. */
    std::string tinyobj_warn;
    std::string tinyobj_error;
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &tinyobj_warn, &tinyobj_error, "../assets/suzanne.obj"))
    {
        PRINT_TINYOBJ_CRITICAL_ERROR("Failed to load meshes", tinyobj_warn, tinyobj_error);
        return false;
    }

    const VkDeviceSize indexCount{ shapes[0].mesh.indices.size() };
    std::vector<Vertex> vertices{};
    std::vector<uint16_t> indices{};
    for (auto& index : shapes[0].mesh.indices)
    {
        Vertex v{
            .pos = { attrib.vertices[index.vertex_index * 3], -attrib.vertices[index.vertex_index * 3 + 1], attrib.vertices[index.vertex_index * 3 + 2] },
            .normal = { attrib.normals[index.normal_index * 3], -attrib.normals[index.normal_index * 3 + 1], attrib.normals[index.normal_index * 3 + 2] },
            .uv = { attrib.texcoords[index.texcoord_index * 2], 1.0 - attrib.texcoords[index.texcoord_index * 2 + 1] }
        };
        vertices.push_back(v);
        indices.push_back(indices.size());
    }

    VkDeviceSize vBufSize{ sizeof(Vertex) * vertices.size() };
    VkDeviceSize iBufSize{ sizeof(uint16_t) * indices.size() };
    VkBufferCreateInfo bufferCI
    {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = vBufSize + iBufSize,
        .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT
    };

    VmaAllocationCreateInfo vBufferAllocationCI
    {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                 | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
                 | VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };
    VmaAllocationInfo vBufferAllocationInfo{};
    vkResult = vmaCreateBuffer(
        m_vmaAllocator,
        &bufferCI,
        &vBufferAllocationCI,
        &m_vkBuffer,
        &m_vmaBufferAllocation,
        &vBufferAllocationInfo
    );
    RETURN_FALSE_ON_FAIL_VULKAN("Failed to create VMA buffer", vkResult);
    memcpy(vBufferAllocationInfo.pMappedData, vertices.data(), vBufSize);
    memcpy(((char*)vBufferAllocationInfo.pMappedData) + vBufSize, indices.data(), iBufSize);
    }

    { /* Parallelism. */
        std::array<ShaderDataBuffer, m_maxFramesInFlight> shaderDataBuffers;
        std::array<VkCommandBuffer, m_maxFramesInFlight> commandBuffers;

        for (uint32_t i = 0; i < m_maxFramesInFlight; ++i)
        {
            VkBufferCreateInfo uBufferCI
            {
                .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                .size = sizeof(ShaderData),
                .usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
            };
            VmaAllocationCreateInfo uBufferAllocCI
            {
                .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                         | VMA_ALLOCATION_CREATE_HOST_ACCESS_ALLOW_TRANSFER_INSTEAD_BIT
                         | VMA_ALLOCATION_CREATE_MAPPED_BIT,
                .usage = VMA_MEMORY_USAGE_AUTO
            };
            vkResult = vmaCreateBuffer(
                m_vmaAllocator,
                &uBufferCI,
                &uBufferAllocCI,
                &shaderDataBuffers[i].buffer,
                &shaderDataBuffers[i].allocation,
                &shaderDataBuffers[i].allocationInfo
            );
            RETURN_FALSE_ON_FAIL_VULKAN("Failed to create VMA buffer", vkResult);

            VkBufferDeviceAddressInfo uBufferBdaInfo
            {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                .buffer = shaderDataBuffers[i].buffer
            };
            shaderDataBuffers[i].deviceAddress = vkGetBufferDeviceAddress(m_vkDevice, &uBufferBdaInfo);
        }

        VkSemaphoreCreateInfo semaphoreCI
        {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
        };
        VkFenceCreateInfo fenceCI
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT
        };
        for (uint32_t i = 0; i < m_maxFramesInFlight; ++i)
        {
            vkResult = vkCreateFence(m_vkDevice, &fenceCI, nullptr, &m_vkFences[i]);
            RETURN_FALSE_ON_FAIL_VULKAN("Failed to create fences", vkResult);
            vkResult = vkCreateSemaphore(m_vkDevice, &semaphoreCI, nullptr, &m_vkImageAcquiredSemaphores[i]);
            RETURN_FALSE_ON_FAIL_VULKAN("Failed to create semaphores", vkResult);
            m_vkRenderCompleteSemaphores.resize(m_vkSwapchainImages.size());
            for (auto& semaphore : m_vkRenderCompleteSemaphores)
            {
                vkResult = vkCreateSemaphore(m_vkDevice, &semaphoreCI, nullptr, &semaphore);
                RETURN_FALSE_ON_FAIL_VULKAN("Failed to create semaphore", vkResult);
            }
        }
    }

    { /* Command buffers. */
        VkCommandPoolCreateInfo commandPoolCI
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
            .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
            .queueFamilyIndex = queueFamily
        };
        vkResult = vkCreateCommandPool(m_vkDevice, &commandPoolCI, nullptr, &m_vkCommandPool);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create command buffer pool", vkResult);

        VkCommandBufferAllocateInfo commandBufferAllocateCI
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_vkCommandPool,
            .commandBufferCount = m_maxFramesInFlight
        };
        vkResult = vkAllocateCommandBuffers(m_vkDevice, &commandBufferAllocateCI, m_vkCommandBuffers.data());
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to allocate command buffers", vkResult);
    }

    { /* Textures. */
    std::vector<VkDescriptorImageInfo> textureDescriptors{};
    for (auto i = 0; i < m_textures.size(); ++i)
    {
        ktxTexture* _ktxTexture{ nullptr };
        std::string filename = "../assets/suzanne" + std::to_string(i) + ".ktx";
        ktxTexture_CreateFromNamedFile(filename.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &_ktxTexture);

        VkImageCreateInfo texImgCI
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = ktxTexture_GetVkFormat(_ktxTexture),
            .extent = {
                .width = _ktxTexture->baseWidth,
                .height = _ktxTexture->baseHeight,
                .depth = 1
            },
            .mipLevels = _ktxTexture->numLevels,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VmaAllocationCreateInfo texImageAllocationCI{ .usage = VMA_MEMORY_USAGE_AUTO };
        vkResult = vmaCreateImage(
            m_vmaAllocator,
            &texImgCI,
            &texImageAllocationCI,
            &m_textures[i].image,
            &m_textures[i].allocation,
            nullptr
        );
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create textures", vkResult);

        VkImageViewCreateInfo texViewCI
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_textures[i].image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = texImgCI.format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = _ktxTexture->numLevels,
                .layerCount = 1
            }
        };
        vkResult = vkCreateImageView(m_vkDevice, &texViewCI, nullptr, &m_textures[i].view);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create texture image views", vkResult);

        VkBuffer imgSrcBuffer{};
        VmaAllocation imgSrcAllocation{};
        VkBufferCreateInfo imgSrcBufferCI
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
            .size = (uint32_t)_ktxTexture->dataSize,
            .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT
        };
        VmaAllocationCreateInfo imgSrcAllocationCI
        {
            .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                        | VMA_ALLOCATION_CREATE_MAPPED_BIT,
            .usage = VMA_MEMORY_USAGE_AUTO
        };
        VmaAllocationInfo imgSrcAllocationInfo{};
        vkResult = vmaCreateBuffer(
            m_vmaAllocator,
            &imgSrcBufferCI,
            &imgSrcAllocationCI,
            &imgSrcBuffer,
            &imgSrcAllocation,
            &imgSrcAllocationInfo
        );
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create texture buffer", vkResult);

        memcpy(imgSrcAllocationInfo.pMappedData, _ktxTexture->pData, _ktxTexture->dataSize);

        VkFenceCreateInfo fenceOneTimeCI
        {
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO
        };
        VkFence fenceOneTime{};
        vkResult = vkCreateFence(m_vkDevice, &fenceOneTimeCI, nullptr, &fenceOneTime);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create OneTime fence", vkResult);
        VkCommandBuffer cbOneTime{};
        VkCommandBufferAllocateInfo cbOneTimeAI
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = m_vkCommandPool,
            .commandBufferCount = 1
        };
        vkResult = vkAllocateCommandBuffers(m_vkDevice, &cbOneTimeAI, &cbOneTime);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create OneTime command buffer", vkResult);

        VkCommandBufferBeginInfo cbOneTimeBI
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };
        vkResult = vkBeginCommandBuffer(cbOneTime, &cbOneTimeBI);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to begin OneTime command buffer", vkResult);
        VkImageMemoryBarrier2 barrierTexImage
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .image = m_textures[i].image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = _ktxTexture->numLevels,
                .layerCount = 1
            }
        };
        VkDependencyInfo barrierTexInfo
        {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &barrierTexImage
        };
        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
        std::vector<VkBufferImageCopy> copyRegions{};
        for (auto j = 0; j < _ktxTexture->numLevels; ++j)
        {
            ktx_size_t mipOffset{ 0 };
            KTX_error_code ret = ktxTexture_GetImageOffset(_ktxTexture, j, 0, 0, &mipOffset);
            copyRegions.push_back({
                .bufferOffset = mipOffset,
                .imageSubresource{
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .mipLevel = (uint32_t)j,
                    .layerCount = 1
                },
                .imageExtent{
                    .width = _ktxTexture->baseWidth >> j,
                    .height = _ktxTexture->baseHeight >> j,
                    .depth = 1
                }
            });
        }
        vkCmdCopyBufferToImage(
            cbOneTime,
            imgSrcBuffer,
            m_textures[i].image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            static_cast<uint32_t>(copyRegions.size()),
            copyRegions.data()
        );
        VkImageMemoryBarrier2 barrierTexRead
        {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL,
            .image = m_textures[i].image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = _ktxTexture->numLevels,
                .layerCount = 1
            }
        };
        barrierTexInfo.pImageMemoryBarriers = &barrierTexRead;
        vkCmdPipelineBarrier2(cbOneTime, &barrierTexInfo);
        vkResult = vkEndCommandBuffer(cbOneTime);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to end OneTime command buffer", vkResult);
        VkCommandBufferSubmitInfo cbOneTimeSubmitInfo
        {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cbOneTime
        };
        VkSubmitInfo2 oneTimeSI
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cbOneTimeSubmitInfo
        };
        vkResult = vkQueueSubmit2(m_vkQueue, 1, &oneTimeSI, fenceOneTime);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to submit OneTime queue", vkResult);
        vkResult = vkWaitForFences(m_vkDevice, 1, &fenceOneTime, VK_TRUE, UINT64_MAX);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to wait for OneTime fence", vkResult);

        VkSamplerCreateInfo samplerCI
        {
            .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
            .magFilter = VK_FILTER_LINEAR,
            .minFilter = VK_FILTER_LINEAR,
            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
            .anisotropyEnable = VK_TRUE,
            .maxAnisotropy = 8.0f, // widely supported value for max anisotropy
            .maxLod = (float)_ktxTexture->numLevels
        };
        vkResult = vkCreateSampler(m_vkDevice, &samplerCI, nullptr, &m_textures[i].sampler);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create sampler", vkResult);

        ktxTexture_Destroy(_ktxTexture);
        textureDescriptors.push_back({
            .sampler = m_textures[i].sampler,
            .imageView = m_textures[i].view,
            .imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL
        });
        
        VkDescriptorBindingFlags descVariableFlag{ VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT };
        VkDescriptorSetLayoutBindingFlagsCreateInfo descBindingFlags
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
            .bindingCount = 1,
            .pBindingFlags = &descVariableFlag
        };
        VkDescriptorSetLayoutBinding descLayoutBindingTex
        {
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = static_cast<uint32_t>(m_textures.size()),
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
        };
        VkDescriptorSetLayoutCreateInfo descLayoutTexCI
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .pNext = &descBindingFlags,
            .bindingCount = 1,
            .pBindings = &descLayoutBindingTex
        };
        vkResult = vkCreateDescriptorSetLayout(m_vkDevice, &descLayoutTexCI, nullptr, &m_vkDescriptorSetLayoutTex);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create descriptor set", vkResult);

        VkDescriptorPoolSize poolSize
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = static_cast<uint32_t>(m_textures.size())
        };
        VkDescriptorPoolCreateInfo descPoolCI
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets = 1,
            .poolSizeCount = 1,
            .pPoolSizes = &poolSize
        };
        vkResult = vkCreateDescriptorPool(m_vkDevice, &descPoolCI, nullptr, &m_vkDescriptorPool);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to create descriptor pool", vkResult);

        uint32_t variableDescCount{ static_cast<uint32_t>(m_textures.size()) };
        VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescCountAI
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO_EXT,
            .descriptorSetCount = 1,
            .pDescriptorCounts = &variableDescCount
        };
        VkDescriptorSetAllocateInfo texDescSetAlloc
        {
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .pNext = &variableDescCountAI,
            .descriptorPool = m_vkDescriptorPool,
            .descriptorSetCount = 1,
            .pSetLayouts = &m_vkDescriptorSetLayoutTex
        };
        vkResult = vkAllocateDescriptorSets(m_vkDevice, &texDescSetAlloc, &m_vkDescriptorSetTex);
        RETURN_FALSE_ON_FAIL_VULKAN("Failed to allocate descriptor sets", vkResult);

        VkWriteDescriptorSet writeDescSet
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = m_vkDescriptorSetTex,
            .dstBinding = 0,
            .descriptorCount = static_cast<uint32_t>(textureDescriptors.size()),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = textureDescriptors.data()
        };
        vkUpdateDescriptorSets(m_vkDevice, 1, &writeDescSet, 0, nullptr);
    }
    }

    return true;
}

void SpaceInvadersGame::CleanupVulkan()
{

}

void SpaceInvadersGame::MainLoop()
{

}

void SpaceInvadersGame::Cleanup()
{
    CleanupVulkan();
    CleanupSDL();
}
