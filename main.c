#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <webgpu/webgpu.h>
#define GLFW_INCLUDE_NONE
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>


#define CHECK(success) do { if (!(success)) { fprintf(stderr, "Failed on line %d\n", __LINE__); exit(1); } } while (0)


typedef struct {
    bool operation_ended;
    bool mapping_successful;
} BufferMapContext;


void sleep_ms(uint64_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

uint32_t divide_and_ceil(uint32_t a, uint32_t b) {
    return (a + b - 1) / b;
}

WGPUStringView to_wgpu_string_view(const char *c_str) {
    return (WGPUStringView) {
        .data = c_str,
        .length = strlen(c_str),
    };
}

void instance_wait_on(WGPUInstance instance, bool *done) {
    while (!(*done)) {
        wgpuInstanceProcessEvents(instance);
        if (*done) {
            break;
        }
        else {
            sleep_ms(200);
        }
    }
}

void on_adapter_request_ended(
    WGPURequestAdapterStatus status,
    WGPUAdapter adapter,
    WGPUStringView message,
    void *userdata1,
    void *userdata2
) {
    CHECK(status == WGPURequestAdapterStatus_Success);
    *(bool*)userdata1 = true;
    *(WGPUAdapter*)userdata2 = adapter;
}

void on_device_request_end(
    WGPURequestDeviceStatus status,
    WGPUDevice device,
    WGPUStringView message,
    void *userdata1,
    void *userdata2
) {
    CHECK(status == WGPURequestDeviceStatus_Success);
    *(WGPUDevice*)userdata1 = device;
    *(bool*)userdata2 = true;
}

void on_queue_work_done(
    WGPUQueueWorkDoneStatus status,
    WGPUStringView message,
    void *userdata1,
    void *userdata2
) {
    CHECK(status == WGPUQueueWorkDoneStatus_Success);
    bool *work_done = (bool*)userdata1;
    *work_done = true;
}

WGPUAdapter get_adapter(WGPUInstance instance, WGPURequestAdapterOptions adapter_options) {
    bool adapterReady = false;
    WGPUAdapter adapter = NULL;

    WGPURequestAdapterCallbackInfo cb_info = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    cb_info.mode = WGPUCallbackMode_AllowProcessEvents;
    cb_info.callback = on_adapter_request_ended;
    cb_info.userdata1 = &adapterReady;
    cb_info.userdata2 = &adapter;

    wgpuInstanceRequestAdapter(instance, &adapter_options, cb_info);

    instance_wait_on(instance, &adapterReady);

    return adapter;
}

WGPUDevice get_device(WGPUInstance instance, WGPUAdapter adapter) {

    WGPUDevice device = NULL;
    bool device_ready = false;

    WGPURequestDeviceCallbackInfo cb_info = WGPU_REQUEST_DEVICE_CALLBACK_INFO_INIT;
    cb_info.mode = WGPUCallbackMode_AllowProcessEvents;
    cb_info.callback = on_device_request_end;
    cb_info.userdata1 = &device;
    cb_info.userdata2 = &device_ready;

    wgpuAdapterRequestDevice(adapter, NULL, cb_info);

    instance_wait_on(instance, &device_ready);

    return device;
}


WGPUSurface create_surface(WGPUInstance instance, GLFWwindow *window) {

    WGPUSurfaceDescriptor surface_desc = WGPU_SURFACE_DESCRIPTOR_INIT;

    switch (glfwGetPlatform()) {
        case GLFW_PLATFORM_X11: {
            printf("X11\n");

            WGPUSurfaceSourceXlibWindow source = WGPU_SURFACE_SOURCE_XLIB_WINDOW_INIT;
            source.display = glfwGetX11Display();
            source.window = glfwGetX11Window(window);

            surface_desc.nextInChain = &source.chain;

            return wgpuInstanceCreateSurface(instance, &surface_desc);

            break;
        }
        case GLFW_PLATFORM_WAYLAND: {
            printf("Wayland\n");

            WGPUSurfaceSourceWaylandSurface source = WGPU_SURFACE_SOURCE_WAYLAND_SURFACE_INIT;
            source.display = glfwGetWaylandDisplay();
            source.surface = glfwGetWaylandWindow(window);

            surface_desc.nextInChain = &source.chain;

            return wgpuInstanceCreateSurface(instance, &surface_desc);

            break;
        }
    }

    return NULL;
}


const char *shader_source =
    "@vertex\n"
    "fn vs_main(@builtin(vertex_index) in_vertex_index: u32) -> @builtin(position) vec4f {\n"
    "    if (in_vertex_index == 0u) {\n"
    "        return vec4f(-0.45, 0.5, 0.0, 1.0);\n"
    "    } else if (in_vertex_index == 1u) {\n"
    "        return vec4f(0.45, 0.5, 0.0, 1.0);\n"
    "    } else {\n"
    "        return vec4f(0.0, -0.5, 0.0, 1.0);\n"
    "    }\n"
    "}\n"
    "@fragment\n"
    "fn fs_main() -> @location(0) vec4f {\n"
    "    return vec4f(0.0, 0.4, 0.7, 1.0);\n"
    "}\n"
    ;


int main(int argc, char **argv) {

    int width = 0;
    int height = 0;

    WGPUInstanceDescriptor instance_desc = WGPU_INSTANCE_DESCRIPTOR_INIT;
    instance_desc.nextInChain = NULL;

    WGPUInstance instance = wgpuCreateInstance(&instance_desc);
    CHECK(instance);

    CHECK(glfwInit());

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow *window = glfwCreateWindow(640, 480, "Learn WebGPU", NULL, NULL);
    CHECK(window);

    glfwGetFramebufferSize(window, &width, &height);

    WGPUSurface surface = create_surface(instance, window);
    CHECK(surface);


    WGPURequestAdapterOptions adapter_options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    adapter_options.powerPreference = WGPUPowerPreference_HighPerformance;
    adapter_options.compatibleSurface = surface;

    WGPUAdapter adapter = get_adapter(instance, adapter_options);

    WGPUAdapterInfo adapter_info = WGPU_ADAPTER_INFO_INIT;
    wgpuAdapterGetInfo(adapter, &adapter_info);

    printf("Device: %.*s\n", (int)adapter_info.device.length, adapter_info.device.data);

    wgpuAdapterInfoFreeMembers(adapter_info);

    WGPUDevice device = get_device(instance, adapter);
    CHECK(device);

    WGPUSurfaceCapabilities surface_caps = WGPU_SURFACE_CAPABILITIES_INIT;
    WGPUStatus cap_status = wgpuSurfaceGetCapabilities(surface, adapter, &surface_caps);
    CHECK(WGPUStatus_Success == cap_status);

    WGPUSurfaceConfiguration surface_config = WGPU_SURFACE_CONFIGURATION_INIT;
    surface_config.width = width;
    surface_config.height = height;
    surface_config.device = device;
    surface_config.format = surface_caps.formats[0];
    surface_config.presentMode = WGPUPresentMode_Fifo;
    surface_config.alphaMode = WGPUCompositeAlphaMode_Auto;
    wgpuSurfaceConfigure(surface, &surface_config);

    wgpuSurfaceCapabilitiesFreeMembers(surface_caps);
    wgpuAdapterRelease(adapter);


    WGPUQueue queue = wgpuDeviceGetQueue(device);


    WGPUShaderSourceWGSL wgsl_desc = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl_desc.code = to_wgpu_string_view(shader_source);

    WGPUShaderModuleDescriptor module_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    module_desc.nextInChain = &wgsl_desc.chain;
    WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(device, &module_desc);

    WGPURenderPipelineDescriptor pipeline_desc = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    pipeline_desc.vertex.module = shader_module;
    pipeline_desc.vertex.entryPoint = to_wgpu_string_view("vs_main");


    WGPUFragmentState fragment_state = WGPU_FRAGMENT_STATE_INIT;
    fragment_state.module = shader_module;
    fragment_state.entryPoint = to_wgpu_string_view("fs_main");

    WGPUColorTargetState color_target = WGPU_COLOR_TARGET_STATE_INIT;
    color_target.format = surface_config.format;
    WGPUBlendState blend_state = WGPU_BLEND_STATE_INIT;
    color_target.blend = &blend_state;

    fragment_state.targetCount = 1;
    fragment_state.targets = &color_target;

    pipeline_desc.fragment = &fragment_state;


    WGPURenderPipeline render_pipeline = wgpuDeviceCreateRenderPipeline(device, &pipeline_desc);


    float red = 0.0;
    float step = 0.01;
    bool needs_reconfigure = false;

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        glfwGetFramebufferSize(window, &width, &height);

        if (surface_config.width != width || surface_config.height != height || needs_reconfigure) {
            needs_reconfigure = false;
            surface_config.width = width;
            surface_config.height = height;
            wgpuSurfaceConfigure(surface, &surface_config);
        }

        WGPUSurfaceTexture surface_tex = WGPU_SURFACE_TEXTURE_INIT;
        wgpuSurfaceGetCurrentTexture(surface, &surface_tex);

        switch (surface_tex.status) {
            case WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal:
            case WGPUSurfaceGetCurrentTextureStatus_SuccessSuboptimal:
                break;
            case WGPUSurfaceGetCurrentTextureStatus_Outdated:
                needs_reconfigure = true;
                continue;
            case WGPUSurfaceGetCurrentTextureStatus_Timeout:
                continue;
            default:
                CHECK(NULL);
                break;
        }

        WGPUTextureViewDescriptor tex_view_desc = WGPU_TEXTURE_VIEW_DESCRIPTOR_INIT;
        tex_view_desc.label = to_wgpu_string_view("Surface texture view");
        tex_view_desc.dimension = WGPUTextureViewDimension_2D;

        WGPUTextureView tex_view = wgpuTextureCreateView(surface_tex.texture, &tex_view_desc);

        WGPURenderPassColorAttachment color_attachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        color_attachment.view = tex_view;
        color_attachment.loadOp = WGPULoadOp_Clear;
        color_attachment.storeOp = WGPUStoreOp_Store;
        red += step;
        if (red > 1.0 || red < 0.0) {
            step *= -1.0;
        }
        color_attachment.clearValue = (WGPUColor) { red, 0.0, 1.0, 1.0 };

        WGPURenderPassDescriptor render_pass_desc = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
        render_pass_desc.colorAttachmentCount = 1;
        render_pass_desc.colorAttachments = &color_attachment;

        WGPUCommandEncoderDescriptor enc_desc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
        WGPUCommandEncoder render_encoder = wgpuDeviceCreateCommandEncoder(device, &enc_desc);
        WGPURenderPassEncoder render_pass = wgpuCommandEncoderBeginRenderPass(render_encoder, &render_pass_desc);

        wgpuRenderPassEncoderSetPipeline(render_pass, render_pipeline);
        wgpuRenderPassEncoderDraw(render_pass, 3, 1, 0, 0);

        wgpuRenderPassEncoderEnd(render_pass);
        wgpuRenderPassEncoderRelease(render_pass);

        WGPUCommandBufferDescriptor cmd_buf_desc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
        WGPUCommandBuffer render_cmd_buf = wgpuCommandEncoderFinish(render_encoder, &cmd_buf_desc);

        wgpuQueueSubmit(queue, 1, &render_cmd_buf);

        wgpuCommandBufferRelease(render_cmd_buf);
        wgpuCommandEncoderRelease(render_encoder);

        wgpuSurfacePresent(surface);

        wgpuTextureViewRelease(tex_view);
        wgpuTextureRelease(surface_tex.texture);
    }
 

    wgpuSurfaceUnconfigure(surface);
    wgpuSurfaceRelease(surface);

    glfwDestroyWindow(window);
    glfwTerminate();

    wgpuShaderModuleRelease(shader_module);
    wgpuRenderPipelineRelease(render_pipeline);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuInstanceRelease(instance);

    return 0;
}
