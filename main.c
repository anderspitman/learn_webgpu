#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>


#define CHECK(success) if (!success) { fprintf(stderr, "Failed on line %d\n", __LINE__); exit(1); }
#define CHECK_MSG(ptr, msg) if (!ptr) fprintf(stderr, msg); exit(1)


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

void wait(WGPUInstance instance, bool *done) {
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

void wait_queue_work_done(WGPUInstance instance, WGPUQueue queue) {
    bool work_done = false;

    WGPUQueueWorkDoneCallbackInfo cbInfo = WGPU_QUEUE_WORK_DONE_CALLBACK_INFO_INIT;
    cbInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    cbInfo.callback = on_queue_work_done;
    cbInfo.userdata1 = &work_done;

    wgpuQueueOnSubmittedWorkDone(queue, cbInfo);

    wait(instance, &work_done);
}

void buf_map_callback(
    WGPUMapAsyncStatus status,
    WGPUStringView message,
    void *userdata1,
    void *userdata2
) {
    BufferMapContext *ctx = (BufferMapContext*)userdata1;

    ctx->operation_ended = true;
    
    CHECK(status == WGPUMapAsyncStatus_Success);
    ctx->mapping_successful = true;
}

void read_buf(WGPUInstance instance, WGPUBuffer buf) {
    WGPUBufferMapCallbackInfo buf_cb_info = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;

    BufferMapContext ctx;

    buf_cb_info.mode = WGPUCallbackMode_AllowProcessEvents;
    buf_cb_info.callback = buf_map_callback;
    buf_cb_info.userdata1 = &ctx;

    // TODO: WGPU_WHOLE_MAP_SIZE doesn't work as of writing this code:
    // see https://github.com/gfx-rs/wgpu-native/issues/601
    //wgpuBufferMapAsync(buf, WGPUMapMode_Read, 0, WGPU_WHOLE_MAP_SIZE, buf_cb_info);
    wgpuBufferMapAsync(buf, WGPUMapMode_Read, 0, wgpuBufferGetSize(buf), buf_cb_info);

    wait(instance, &ctx.operation_ended);
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

    WGPUBufferDescriptor buf_desc_a = WGPU_BUFFER_DESCRIPTOR_INIT;
    buf_desc_a.size = 256;
    buf_desc_a.usage = WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc;
    buf_desc_a.mappedAtCreation = true;
    WGPUBuffer buf_a = wgpuDeviceCreateBuffer(device, &buf_desc_a);

    WGPUBufferDescriptor buf_desc_b = WGPU_BUFFER_DESCRIPTOR_INIT;
    buf_desc_b.size = 32;
    buf_desc_b.usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
    WGPUBuffer buf_b = wgpuDeviceCreateBuffer(device, &buf_desc_b);

    uint8_t *buf_data_a = (uint8_t *)wgpuBufferGetMappedRange(buf_a, 0, WGPU_WHOLE_MAP_SIZE);
    CHECK(buf_data_a);

    for (size_t i = 0; i < 256; ++i) {
        buf_data_a[i] = i;
    }

    wgpuBufferUnmap(buf_a);

    WGPUQueue queue = wgpuDeviceGetQueue(device);
    CHECK(queue);

    WGPUCommandEncoderDescriptor enc_desc = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    WGPUCommandEncoder encoder = wgpuDeviceCreateCommandEncoder(device, &enc_desc);
    CHECK(encoder);

    wgpuCommandEncoderCopyBufferToBuffer(encoder, buf_a, 16, buf_b, 0, buf_desc_b.size);

    WGPUCommandBufferDescriptor cmd_buf_desc = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    WGPUCommandBuffer cmd_buf = wgpuCommandEncoderFinish(encoder, &cmd_buf_desc);
    CHECK(cmd_buf);
    wgpuCommandEncoderRelease(encoder);

    printf("Submit command\n");
    wgpuQueueSubmit(queue, 1, &cmd_buf);
    wgpuCommandBufferRelease(cmd_buf);
    printf("Command submitted\n");


    read_buf(instance, buf_b);

    const uint8_t *buf_data_b = wgpuBufferGetConstMappedRange(buf_b, 0, WGPU_WHOLE_MAP_SIZE);
    CHECK(buf_data_b);

    printf("buf b data: [ ");
    for (size_t i = 0; i < buf_desc_b.size; ++i) {
        printf("%d ", buf_data_b[i]);
    }
    printf("]\n");

    wgpuBufferUnmap(buf_b);

    sleep_ms(10000);


    //wait_queue_work_done(instance, queue);

    //glfwInit();

    //glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    //GLFWwindow *window = glfwCreateWindow(640, 480, "test", NULL, NULL);

    //while (!glfwWindowShouldClose(window)) {
    //    glfwPollEvents();
    //}

    wgpuBufferRelease(buf_a);
    wgpuBufferRelease(buf_b);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuInstanceRelease(instance);

    return 0;
}
