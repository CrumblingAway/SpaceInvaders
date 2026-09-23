#pragma once

#include <glm/glm.hpp>
#include <vma/vk_mem_alloc.h>
#include <volk/volk.h>

class SDL_Window;

struct Texture
{
    VkImage image{ VK_NULL_HANDLE };
    VkImageView imageView{ VK_NULL_HANDLE };
    VmaAllocation imageAllocation{ VK_NULL_HANDLE };
    VkSampler sampler{ VK_NULL_HANDLE };
};

class Shape
{

};

struct PlayerSpaceship
{
    /* Rendering. */
    Texture* sprite;

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
     * Clean up the resources associated with this instance of the game.
     */
    bool Destroy();

    const char* m_pApplicationName = "Space Invaders";

    const size_t m_windowHeight = 720u;
    const size_t m_windowWidth = 1280u;
    SDL_Window* m_sdlWindow = nullptr;

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
    static constexpr uint32_t m_maxFramesInFlight{ 2 };

    VkDescriptorSetLayout m_vkDescriptorSetLayoutTex{ VK_NULL_HANDLE };
    VkDescriptorSet m_vkDescriptorSetTex{ VK_NULL_HANDLE };
    VkDescriptorPool m_vkDescriptorPool{ VK_NULL_HANDLE };

    VmaAllocator m_vmaAllocator{ VK_NULL_HANDLE };
    VmaAllocation m_vmaDepthImageAllocation{ VK_NULL_HANDLE };

    std::array<VkFence, m_maxFramesInFlight> m_vkFences;
    std::array<VkSemaphore, m_maxFramesInFlight> m_vkImageAcquiredSemaphores;
    std::vector<VkSemaphore> m_vkRenderCompleteSemaphores;

    PlayerSpaceship m_playerSpaceship;
};
