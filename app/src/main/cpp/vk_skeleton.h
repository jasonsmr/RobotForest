#pragma once
#include <android/native_window.h>
#include <vulkan/vulkan.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct VkApp {
    ANativeWindow* window;

    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice phys;
    uint32_t gfx_idx;

    VkDevice dev;
    VkQueue  gfx_q;

    VkFormat swap_format;
    VkColorSpaceKHR swap_colorspace;
    VkExtent2D swap_extent;

    VkSwapchainKHR swapchain;
    uint32_t swap_count;
    VkImage* swap_images;
    VkImageView* swap_views;

    VkRenderPass render_pass;
    VkFramebuffer* framebuffers;

    VkCommandPool cmd_pool;
    VkCommandBuffer* cmds;

    VkSemaphore img_avail;
    VkSemaphore render_done;
    VkFence* in_flight;          // one per swap image

    bool ready;
    bool frame_pending;
} VkApp;

bool vkapp_init(VkApp* app, ANativeWindow* win);
void vkapp_resize(VkApp* app, int32_t w, int32_t h);
void vkapp_draw(VkApp* app);
void vkapp_destroy(VkApp* app);

#ifdef __cplusplus
}
#endif
