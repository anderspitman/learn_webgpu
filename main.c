#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>

#define CHECK(success) if (!success) { fprintf(stderr, "Failed on line %d\n", __LINE__); exit(1); }
#define CHECK_MSG(ptr, msg) if (!ptr) fprintf(stderr, msg); exit(1)

void sleep_ms(uint64_t ms) {
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
}

void on_adapter_request_ended(
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

WGPUAdapter get_adapter(WGPUInstance instance) {
    bool adapterReady = false;
    WGPUAdapter adapter;

    WGPURequestAdapterCallbackInfo cbInfo = {
        .callback = on_adapter_request_ended,
        .userdata1 = &adapterReady,
        .userdata2 = &adapter
    };

    WGPURequestAdapterOptions adapterOptions = {
       .powerPreference = WGPUPowerPreference_Undefined
    };

    wgpuInstanceRequestAdapter(instance, &adapterOptions, cbInfo);

    while (!adapterReady) {
        wgpuInstanceProcessEvents(instance);
        sleep_ms(100);
    }

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

void wait_queue_work_done(WGPUInstance instance, WGPUQueue queue) {
    bool work_done = false;

    WGPUQueueWorkDoneCallbackInfo cbInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    cbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    cbInfo.callback = on_queue_work_done;
    cbInfo.userdata1 = &work_done;

    wgpuQueueOnSubmittedWorkDone(queue, cbInfo);

    while (!work_done) {
        wgpuInstanceProcessEvents(instance);
        sleep_ms(200);
    }
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
    CHECK(device);

    wgpuAdapterRelease(adapter);

    WGPUQueue queue = wgpuDeviceGetQueue(device);
    CHECK(queue);

    WGPUCommandEncoderDescriptor enc_desc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &enc_desc);
    CHECK(encoder);

    WGPUCommandBufferDescriptor cmd_buf_desc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    WGPUCommandBuffer cmd_buf = wgpuCommandEncoderFinish(encoder, &cmd_buf_desc);
    CHECK(cmd_buf);
    wgpuCommandEncoderRelease(encoder);

    printf("Submit command\n");
    wgpuQueueSubmit(queue, 1, &cmd_buf);
    wgpuCommandBufferRelease(cmd_buf);
    printf("Command submitted\n");

    wait_queue_work_done(instance, queue);

    //glfwInit();

    //glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    //GLFWwindow *window = glfwCreateWindow(640, 480, "test", NULL, NULL);

    //while (!glfwWindowShouldClose(window)) {
    //    glfwPollEvents();
    //}

    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuInstanceRelease(instance);

    return 0;
}
