#pragma once

#include <array>

#include <glm/glm.hpp>
#include <vma/vk_mem_alloc.h>
#include <volk/volk.h>

class SDL_Window;

struct Vertex
{
    glm::vec3 pos;
    glm::vec2 uv;
};

struct Sprite
{
    static constexpr uint32_t NUM_VERTICES = 4;

    VkImage image{ VK_NULL_HANDLE };
    VkImageView imageView{ VK_NULL_HANDLE };
    VmaAllocation imageAllocation{ VK_NULL_HANDLE };
    VkSampler sampler{ VK_NULL_HANDLE };
};

struct PlayerShaderData
{
    glm::mat4 projection;
    glm::mat4 view;
    glm::mat4 model;
};

struct PlayerShaderBuffer
{
    VkBuffer buffer{ VK_NULL_HANDLE };
    VmaAllocation allocation{ VK_NULL_HANDLE };
    VmaAllocationInfo allocationInfo{};
    VkDeviceAddress deviceAddress{};
};

class Shape
{

};

struct Camera
{
    glm::vec3 position;
};

struct PlayerSpaceship
{
    /* Rendering. */
    Sprite sprite;
    VkBuffer buffer{ VK_NULL_HANDLE };
    VmaAllocation vmaBufferAllocation{ VK_NULL_HANDLE };

    float scale = 0.1f;
    std::array<Vertex, 4> vertices
    {{
        {
            .pos = scale * glm::vec3(-10.0f, -10.0f, 0.0f),
            .uv = glm::vec2(0.0f, 0.0f)
        },
        {
            .pos = scale * glm::vec3(-10.0f, 10.0f, 0.0f),
            .uv = glm::vec2(0.0f, 1.0f)
        },
        {
            .pos = scale * glm::vec3(10.0f, 10.0f, 0.0f),
            .uv = glm::vec2(1.0f, 1.0f)
        },
        {
            .pos = scale * glm::vec3(10.0f, -10.0f, 0.0f),
            .uv = glm::vec2(1.0f, 0.0f)
        }
    }};
    std::array<uint16_t, 6> vertexIndices{ 0, 1, 2, 0, 2, 3 };

    /* Logic. */
    glm::vec3 position;
    Shape hitbox;
};

class SpaceInvadersGame
{
public:
    SpaceInvadersGame() = default;
    ~SpaceInvadersGame();

    /**
     * Init a game of space invaders. This method must be called before SpaceInfacersGame::Run.
     */
    bool Init();

    /**
     * Run a game of space invaders.
     */
    bool Run();

private:
    /**
     * Init SDL.
     * 
     * As this is the only windowing library this game accounts for, failure of this method results
     * in immediate termination of the application.
     */
    bool InitSDL();
    bool CleanupSDL();

    /**
     * Init Vulkan.
     * 
     * As this is the only graphics API this game accounts for, failure  of this method results in
     * immediate termination of the application.
     */
    bool InitVulkan();
    bool CleanupVulkan();

    /**
     * Init player and enemies.
     */
    bool InitObjects();
    bool CleanupObjects();

    /**
     * Clean up the resources associated with this instance of the game.
     */
    bool Destroy();

    bool CheckSwapchain(VkResult result);
    bool m_updateSwapchain = true;

    const char* m_pApplicationName = "Space Invaders";

    const size_t m_windowHeight = 720u;
    const size_t m_windowWidth = 1280u;
    SDL_Window* m_sdlWindow = nullptr;

    static constexpr uint32_t m_maxFramesInFlight{ 2 };

    VkInstance m_vkInstance{ VK_NULL_HANDLE };
    VkPhysicalDevice m_vkPhysicalDevice{ VK_NULL_HANDLE };
    VkDevice m_vkDevice{ VK_NULL_HANDLE };
    VkQueue m_vkQueue{ VK_NULL_HANDLE };
    VkCommandPool m_vkCommandPool{ VK_NULL_HANDLE };
    std::array<VkCommandBuffer, m_maxFramesInFlight> m_vkCommandBuffers;

    VkSurfaceKHR m_vkSurface{ VK_NULL_HANDLE };
    VkSurfaceCapabilitiesKHR m_surfaceCapabilities{};
    VkSwapchainKHR m_vkSwapchain{ VK_NULL_HANDLE };
    std::vector<VkImage> m_vkSwapchainImages;
    std::vector<VkImageView> m_vkSwapchainImageViews;
    VkImage m_vkDepthImage{ VK_NULL_HANDLE };
    VkImageView m_vkDepthImageView{ VK_NULL_HANDLE };
    VkShaderModule m_vkShaderModule{};
    VkPipeline m_vkPipeline{ VK_NULL_HANDLE };
    VkPipelineLayout m_vkPipelineLayout{ VK_NULL_HANDLE };
    PlayerShaderData m_playerShaderData;
    std::array<PlayerShaderBuffer, m_maxFramesInFlight> m_shaderDataBuffers;
    uint32_t m_imageIndex{ 0 };
    uint32_t m_frameIndex{ 0 };

    VkDescriptorSetLayout m_vkDescriptorSetLayoutTex{ VK_NULL_HANDLE };
    VkDescriptorSet m_vkDescriptorSetTex{ VK_NULL_HANDLE };
    VkDescriptorPool m_vkDescriptorPool{ VK_NULL_HANDLE };

    VmaAllocator m_vmaAllocator{ VK_NULL_HANDLE };
    VmaAllocation m_vmaDepthImageAllocation{ VK_NULL_HANDLE };

    std::array<VkFence, m_maxFramesInFlight> m_vkFences;
    std::array<VkSemaphore, m_maxFramesInFlight> m_vkImageAcquiredSemaphores;
    std::vector<VkSemaphore> m_vkRenderCompleteSemaphores;

    PlayerSpaceship m_playerSpaceship;
    Camera m_camera;
};
