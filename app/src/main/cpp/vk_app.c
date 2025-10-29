#include "vk_app.h"
#include <android/log.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <vulkan/vulkan_android.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  "VK", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "VK", __VA_ARGS__)
#define VKC(x) do { VkResult _r = (x); if (_r != VK_SUCCESS) { LOGE("VK err %d at %s:%d", _r, __FILE__, __LINE__); abort(); } } while(0)

static void read_asset(AAssetManager* am, const char* path, void** data, size_t* sz) {
    AAsset* a = AAssetManager_open(am, path, AASSET_MODE_BUFFER);
    if (!a) { LOGE("asset not found: %s", path); abort(); }
    *sz = AAsset_getLength(a);
    *data = malloc(*sz);
    if (!*data) abort();
    int n = AAsset_read(a, *data, *sz);
    AAsset_close(a);
    if (n <= 0) { LOGE("asset read failed: %s", path); abort(); }
}

static uint32_t find_memtype(VkPhysicalDevice phys, uint32_t typeBits, VkMemoryPropertyFlags req) {
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(phys, &mp);
    for (uint32_t i=0;i<mp.memoryTypeCount;i++)
        if ((typeBits & (1u<<i)) && (mp.memoryTypes[i].propertyFlags & req) == req)
            return i;
    abort();
}

static void create_instance(VkApp* a) {
    const char* exts[] = { "VK_KHR_surface", "VK_KHR_android_surface" };
    VkApplicationInfo ai = { .sType=VK_STRUCTURE_TYPE_APPLICATION_INFO, .pApplicationName="Triangle", .apiVersion=VK_API_VERSION_1_1 };
    VkInstanceCreateInfo ci = { .sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, .pApplicationInfo=&ai,
        .enabledExtensionCount=2, .ppEnabledExtensionNames=exts };
    VKC(vkCreateInstance(&ci, 0, &a->instance));
}

static void pick_device(VkApp* a) {
    uint32_t n=0; VKC(vkEnumeratePhysicalDevices(a->instance,&n,0));
    VkPhysicalDevice devs[8]; VKC(vkEnumeratePhysicalDevices(a->instance,&n,devs));
    a->phys = devs[0];
    uint32_t qn=0; vkGetPhysicalDeviceQueueFamilyProperties(a->phys,&qn,0);
    VkQueueFamilyProperties qfp[16]; vkGetPhysicalDeviceQueueFamilyProperties(a->phys,&qn,qfp);
    for (uint32_t i=0;i<qn;i++) {
        VkBool32 present=VK_FALSE; VKC(vkGetPhysicalDeviceSurfaceSupportKHR(a->phys,i,a->surface,&present));
        if ((qfp[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) { a->qFamily=i; break; }
    }
}

static void create_device(VkApp* a) {
    float prio=1.0f;
    VkDeviceQueueCreateInfo qci = { .sType=VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, .queueFamilyIndex=a->qFamily, .queueCount=1, .pQueuePriorities=&prio };
    const char* exts[] = { "VK_KHR_swapchain" };
    VkDeviceCreateInfo dci = { .sType=VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, .queueCreateInfoCount=1, .pQueueCreateInfos=&qci,
        .enabledExtensionCount=1, .ppEnabledExtensionNames=exts };
    VKC(vkCreateDevice(a->phys, &dci, 0, &a->device));
    vkGetDeviceQueue(a->device, a->qFamily, 0, &a->queue);
}

static void create_surface(VkApp* a, ANativeWindow* win) {
    VkAndroidSurfaceCreateInfoKHR sci = { .sType=VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, .window = win };
    VKC(vkCreateAndroidSurfaceKHR(a->instance, &sci, 0, &a->surface));
}

static void create_swapchain(VkApp* a) {
    VkSurfaceCapabilitiesKHR caps; VKC(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(a->phys,a->surface,&caps));
    uint32_t fmtN=0; VKC(vkGetPhysicalDeviceSurfaceFormatsKHR(a->phys,a->surface,&fmtN,0));
    VkSurfaceFormatKHR fmts[16]; VKC(vkGetPhysicalDeviceSurfaceFormatsKHR(a->phys,a->surface,&fmtN,fmts));
    VkSurfaceFormatKHR fmt = fmts[0];
    a->swapFormat = fmt.format;
    a->extent = caps.currentExtent;

    VkSwapchainCreateInfoKHR sci = {
        .sType=VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface=a->surface,
        .minImageCount = caps.minImageCount+1 <= caps.maxImageCount ? caps.minImageCount+1 : caps.minImageCount,
        .imageFormat=fmt.format,
        .imageColorSpace=fmt.colorSpace,
        .imageExtent=a->extent,
        .imageArrayLayers=1,
        .imageUsage=VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode=VK_SHARING_MODE_EXCLUSIVE,
        .preTransform=caps.currentTransform,
        .compositeAlpha=VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        .presentMode=VK_PRESENT_MODE_FIFO_KHR, // always available
        .clipped=VK_TRUE
    };
    VKC(vkCreateSwapchainKHR(a->device,&sci,0,&a->swapchain));

    VKC(vkGetSwapchainImagesKHR(a->device,a->swapchain,&a->imageCount,0));
    a->images = calloc(a->imageCount,sizeof(VkImage));
    VKC(vkGetSwapchainImagesKHR(a->device,a->swapchain,&a->imageCount,a->images));
}

static VkShaderModule load_shader(VkApp* a, const char* assetPath) {
    void* bytes=0; size_t sz=0; read_asset(a->assets, assetPath,&bytes,&sz);
    VkShaderModuleCreateInfo sci = { .sType=VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .codeSize=sz, .pCode=(const uint32_t*)bytes };
    VkShaderModule m; VKC(vkCreateShaderModule(a->device,&sci,0,&m));
    free(bytes); return m;
}

static void create_render(VkApp* a) {
    // Image views
    a->views = calloc(a->imageCount,sizeof(VkImageView));
    for (uint32_t i=0;i<a->imageCount;i++) {
        VkImageViewCreateInfo vci = {
            .sType=VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image=a->images[i],
            .viewType=VK_IMAGE_VIEW_TYPE_2D,
            .format=a->swapFormat,
            .components={VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY},
            .subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1}
        };
        VKC(vkCreateImageView(a->device,&vci,0,&a->views[i]));
    }

    // Render pass
    VkAttachmentDescription att = {
        .format=a->swapFormat,
        .samples=VK_SAMPLE_COUNT_1_BIT,
        .loadOp=VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp=VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout=VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };
    VkAttachmentReference colorRef = { .attachment=0, .layout=VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
    VkSubpassDescription sub = { .pipelineBindPoint=VK_PIPELINE_BIND_POINT_GRAPHICS, .colorAttachmentCount=1, .pColorAttachments=&colorRef };
    VkRenderPassCreateInfo rpci = { .sType=VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO, .attachmentCount=1, .pAttachments=&att, .subpassCount=1, .pSubpasses=&sub };
    VKC(vkCreateRenderPass(a->device,&rpci,0,&a->renderPass));

    // Pipeline: no vertex buffers (gl_VertexIndex), viewport dynamic
    VkShaderModule vs = load_shader(a, "shaders/triangle.vert.spv");
    VkShaderModule fs = load_shader(a, "shaders/triangle.frag.spv");
    VkPipelineShaderStageCreateInfo stages[2] = {
        { .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage=VK_SHADER_STAGE_VERTEX_BIT,   .module=vs, .pName="main" },
        { .sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .stage=VK_SHADER_STAGE_FRAGMENT_BIT, .module=fs, .pName="main" },
    };

    VkPipelineLayoutCreateInfo plci = { .sType=VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    VKC(vkCreatePipelineLayout(a->device,&plci,0,&a->pipelineLayout));

    VkPipelineVertexInputStateCreateInfo vi = { .sType=VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    VkPipelineInputAssemblyStateCreateInfo ia = { .sType=VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO, .topology=VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST };
    VkPipelineViewportStateCreateInfo vp = { .sType=VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO, .viewportCount=1, .scissorCount=1 };
    VkPipelineRasterizationStateCreateInfo rs = { .sType=VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO, .polygonMode=VK_POLYGON_MODE_FILL, .cullMode=VK_CULL_MODE_BACK_BIT, .frontFace=VK_FRONT_FACE_CLOCKWISE, .lineWidth=1.0f };
    VkPipelineMultisampleStateCreateInfo ms = { .sType=VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO, .rasterizationSamples=VK_SAMPLE_COUNT_1_BIT };
    VkPipelineColorBlendAttachmentState cbatt = { .colorWriteMask=0xF, .blendEnable=VK_FALSE };
    VkPipelineColorBlendStateCreateInfo cb = { .sType=VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .attachmentCount=1, .pAttachments=&cbatt };
    VkDynamicState dyns[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dyn = { .sType=VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO, .dynamicStateCount=2, .pDynamicStates=dyns };

    VkPipelineRenderingCreateInfoKHR renderInfo = {0}; // not used (we use a traditional render pass)

    VkGraphicsPipelineCreateInfo gp = {
        .sType=VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount=2, .pStages=stages,
        .pVertexInputState=&vi,
        .pInputAssemblyState=&ia,
        .pViewportState=&vp,
        .pRasterizationState=&rs,
        .pMultisampleState=&ms,
        .pColorBlendState=&cb,
        .pDynamicState=&dyn,
        .layout=a->pipelineLayout,
        .renderPass=a->renderPass,
        .subpass=0
    };
    VKC(vkCreateGraphicsPipelines(a->device, VK_NULL_HANDLE, 1, &gp, 0, &a->pipeline));

    vkDestroyShaderModule(a->device, vs, 0);
    vkDestroyShaderModule(a->device, fs, 0);

    // Framebuffers
    a->framebuffers = calloc(a->imageCount,sizeof(VkFramebuffer));
    for (uint32_t i=0;i<a->imageCount;i++) {
        VkImageView atts[] = { a->views[i] };
        VkFramebufferCreateInfo fci = { .sType=VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO, .renderPass=a->renderPass, .attachmentCount=1, .pAttachments=atts, .width=a->extent.width, .height=a->extent.height, .layers=1 };
        VKC(vkCreateFramebuffer(a->device,&fci,0,&a->framebuffers[i]));
    }

    // Command/sync
    VkCommandPoolCreateInfo cpci = { .sType=VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO, .queueFamilyIndex=a->qFamily, .flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT };
    VKC(vkCreateCommandPool(a->device,&cpci,0,&a->cmdPool));
    a->cmdBufs = calloc(a->imageCount,sizeof(VkCommandBuffer));
    VkCommandBufferAllocateInfo cbai = { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, .commandPool=a->cmdPool, .level=VK_COMMAND_BUFFER_LEVEL_PRIMARY, .commandBufferCount=a->imageCount };
    VKC(vkAllocateCommandBuffers(a->device,&cbai,a->cmdBufs));

    VkSemaphoreCreateInfo sci2 = { .sType=VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VKC(vkCreateSemaphore(a->device,&sci2,0,&a->imageAvailable));
    VKC(vkCreateSemaphore(a->device,&sci2,0,&a->renderFinished));
    VkFenceCreateInfo fci2 = { .sType=VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags=VK_FENCE_CREATE_SIGNALED_BIT };
    VKC(vkCreateFence(a->device,&fci2,0,&a->inFlight));
}

static void destroy_swap(VkApp* a) {
    if (!a->device) return;
    vkDeviceWaitIdle(a->device);
    if (a->cmdBufs) { vkFreeCommandBuffers(a->device,a->cmdPool,a->imageCount,a->cmdBufs); free(a->cmdBufs); a->cmdBufs=NULL; }
    if (a->cmdPool) { vkDestroyCommandPool(a->device,a->cmdPool,0); a->cmdPool=VK_NULL_HANDLE; }
    if (a->renderFinished) { vkDestroySemaphore(a->device,a->renderFinished,0); a->renderFinished=VK_NULL_HANDLE; }
    if (a->imageAvailable) { vkDestroySemaphore(a->device,a->imageAvailable,0); a->imageAvailable=VK_NULL_HANDLE; }
    if (a->inFlight) { vkDestroyFence(a->device,a->inFlight,0); a->inFlight=VK_NULL_HANDLE; }
    if (a->framebuffers) { for (uint32_t i=0;i<a->imageCount;i++) vkDestroyFramebuffer(a->device,a->framebuffers[i],0); free(a->framebuffers); a->framebuffers=NULL; }
    if (a->pipeline) { vkDestroyPipeline(a->device,a->pipeline,0); a->pipeline=VK_NULL_HANDLE; }
    if (a->pipelineLayout) { vkDestroyPipelineLayout(a->device,a->pipelineLayout,0); a->pipelineLayout=VK_NULL_HANDLE; }
    if (a->renderPass) { vkDestroyRenderPass(a->device,a->renderPass,0); a->renderPass=VK_NULL_HANDLE; }
    if (a->views) { for (uint32_t i=0;i<a->imageCount;i++) vkDestroyImageView(a->device,a->views[i],0); free(a->views); a->views=NULL; }
    if (a->images) { free(a->images); a->images=NULL; }
    if (a->swapchain) { vkDestroySwapchainKHR(a->device,a->swapchain,0); a->swapchain=VK_NULL_HANDLE; }
}

static void recreate(VkApp* a) {
    destroy_swap(a);
    create_swapchain(a);
    create_render(a);
}

void vkapp_init(VkApp* a, AAssetManager* am, ANativeWindow* win) {
    a->assets = am; a->window = win;
    create_instance(a);
    create_surface(a, win);
    pick_device(a);
    create_device(a);
    recreate(a);
    a->ready = true;
}

void vkapp_term_surface(VkApp* a) {
    a->ready = false;
    destroy_swap(a);
    if (a->surface) { vkDestroySurfaceKHR(a->instance,a->surface,0); a->surface=VK_NULL_HANDLE; }
}

void vkapp_destroy(VkApp* a) {
    vkapp_term_surface(a);
    if (a->device) { vkDestroyDevice(a->device,0); a->device=VK_NULL_HANDLE; }
    if (a->instance) { vkDestroyInstance(a->instance,0); a->instance=VK_NULL_HANDLE; }
}

void vkapp_draw_frame(VkApp* a) {
    if (!a->ready) return;

    VKC(vkWaitForFences(a->device,1,&a->inFlight,VK_TRUE,UINT64_MAX));
    VKC(vkResetFences(a->device,1,&a->inFlight));

    uint32_t idx=0;
    VkResult acq = vkAcquireNextImageKHR(a->device,a->swapchain,UINT64_MAX,a->imageAvailable,VK_NULL_HANDLE,&idx);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) { recreate(a); return; }
    VKC(acq);

    VkCommandBuffer cb = a->cmdBufs[idx];
    VkCommandBufferBeginInfo bi = { .sType=VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    VKC(vkBeginCommandBuffer(cb,&bi));

    VkClearValue clr = { .color={{0.05f,0.06f,0.12f,1.0f}} };
    VkRenderPassBeginInfo rbi = {
        .sType=VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass=a->renderPass,
        .framebuffer=a->framebuffers[idx],
        .renderArea={{0,0},{a->extent.width,a->extent.height}},
        .clearValueCount=1,
        .pClearValues=&clr
    };
    vkCmdBeginRenderPass(cb,&rbi,VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp = {0,0,(float)a->extent.width,(float)a->extent.height,0.0f,1.0f};
    VkRect2D sc = {{0,0},{a->extent.width,a->extent.height}};
    vkCmdSetViewport(cb,0,1,&vp);
    vkCmdSetScissor(cb,0,1,&sc);

    vkCmdBindPipeline(cb,VK_PIPELINE_BIND_POINT_GRAPHICS,a->pipeline);
    vkCmdDraw(cb,3,1,0,0);

    vkCmdEndRenderPass(cb);
    VKC(vkEndCommandBuffer(cb));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si = {
        .sType=VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount=1, .pWaitSemaphores=&a->imageAvailable, .pWaitDstStageMask=&waitStage,
        .commandBufferCount=1, .pCommandBuffers=&cb,
        .signalSemaphoreCount=1, .pSignalSemaphores=&a->renderFinished
    };
    VKC(vkQueueSubmit(a->queue,1,&si,a->inFlight));

    VkPresentInfoKHR pi = {
        .sType=VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount=1, .pWaitSemaphores=&a->renderFinished,
        .swapchainCount=1, .pSwapchains=&a->swapchain, .pImageIndices=&idx
    };
    VkResult pr = vkQueuePresentKHR(a->queue,&pi);
    if (pr == VK_ERROR_OUT_OF_DATE_KHR || pr == VK_SUBOPTIMAL_KHR) { recreate(a); return; }
    VKC(pr);
}
