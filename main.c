#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>

#define CHECK(success) if (!success) { fprintf(stderr, "Failed on line %d\n", __LINE__); exit(1); }
#define CHECK_MSG(ptr, msg) if (!ptr) fprintf(stderr, msg); exit(1)

void onAdapterRequestEnded(
    WGPURequestAdapterStatus status,
    WGPUAdapter adapter,
    WGPUStringView message,
    void *userdata1,
    void *userdata2
) {
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
    *(WGPUDevice*)userdata1 = device;
}

WGPUAdapter get_adapter(WGPUInstance instance) {
    bool adapterReady = false;
    WGPUAdapter adapter;

    WGPURequestAdapterCallbackInfo cbInfo = {
        .callback = onAdapterRequestEnded,
        .userdata1 = &adapterReady,
        .userdata2 = &adapter
    };

    WGPURequestAdapterOptions adapterOptions = {
       .powerPreference = WGPUPowerPreference_Undefined
    };

    wgpuInstanceRequestAdapter(instance, &adapterOptions, cbInfo);

    CHECK(adapterReady);

    return adapter;
}

WGPUDevice get_device(WGPUAdapter adapter) {

    WGPUDevice device;

    WGPURequestDeviceCallbackInfo cbInfo = {
        .callback = on_device_request_end,
        .userdata1 = &device,
    };

    wgpuAdapterRequestDevice(adapter, NULL, cbInfo);

    return device;
}

int main(int argc, char **argv) {

    WGPUInstanceDescriptor desc = {
        .nextInChain = NULL
    };

    WGPUInstance instance = wgpuCreateInstance(&desc);
    CHECK(instance);
    
    WGPUAdapter adapter = get_adapter(instance);

    WGPUAdapterInfo adapter_info = {0};
    wgpuAdapterGetInfo(adapter, &adapter_info);

    printf("Device: %.*s\n", adapter_info.device.length, adapter_info.device.data);

    WGPUDevice device = get_device(adapter);
    wgpuAdapterRelease(adapter);

    //glfwInit();

    //glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    //GLFWwindow *window = glfwCreateWindow(640, 480, "test", NULL, NULL);

    //while (!glfwWindowShouldClose(window)) {
    //    glfwPollEvents();
    //}

    wgpuDeviceRelease(device);
    wgpuInstanceRelease(instance);

    return 0;
}
