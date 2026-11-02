#define _POSIX_C_SOURCE 200809L
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <webgpu/webgpu.h>
#include <GLFW/glfw3.h>


#define CHECK(success) if (!success) { fprintf(stderr, "Failed on line %d\n", __LINE__); exit(1); }


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

WGPUAdapter get_adapter(WGPUInstance instance) {
    bool adapterReady = false;
    WGPUAdapter adapter = NULL;

    WGPURequestAdapterCallbackInfo cb_info = WGPU_REQUEST_ADAPTER_CALLBACK_INFO_INIT;
    cb_info.mode = WGPUCallbackMode_AllowProcessEvents;
    cb_info.callback = on_adapter_request_ended;
    cb_info.userdata1 = &adapterReady;
    cb_info.userdata2 = &adapter;

    WGPURequestAdapterOptions adapter_options = WGPU_REQUEST_ADAPTER_OPTIONS_INIT;
    adapter_options.powerPreference = WGPUPowerPreference_Undefined;

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

void map_buf_sync(WGPUInstance instance, WGPUBuffer buf) {
    WGPUBufferMapCallbackInfo buf_cb_info = WGPU_BUFFER_MAP_CALLBACK_INFO_INIT;

    BufferMapContext ctx = {0};

    buf_cb_info.mode = WGPUCallbackMode_AllowProcessEvents;
    buf_cb_info.callback = buf_map_callback;
    buf_cb_info.userdata1 = &ctx;

    // TODO: WGPU_WHOLE_MAP_SIZE doesn't work as of writing this code:
    // see https://github.com/gfx-rs/wgpu-native/issues/601
    //wgpuBufferMapAsync(buf, WGPUMapMode_Read, 0, WGPU_WHOLE_MAP_SIZE, buf_cb_info);
    wgpuBufferMapAsync(buf, WGPUMapMode_Read, 0, wgpuBufferGetSize(buf), buf_cb_info);

    instance_wait_on(instance, &ctx.operation_ended);
}


const char *shader_source =
    "@group(0) @binding(0)\n"
    "var<storage,read> input_buffer: array<f32>;\n"
    "@group(0) @binding(1)\n"
    "var<storage,read_write> output_buffer: array<f32>;\n"
    "@compute @workgroup_size(32)\n"
    "fn compute_stuff(@builtin(global_invocation_id) thread_id: vec3u) {\n"
    "    output_buffer[thread_id.x] = 2.0 * input_buffer[thread_id.x];\n"
    "}\n"
    ;

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

    WGPUDevice device = get_device(instance, adapter);
    CHECK(device);

    wgpuAdapterRelease(adapter);


    WGPUQueue queue = wgpuDeviceGetQueue(device);

    size_t element_count = 64;

    WGPUBufferDescriptor in_buf_desc = WGPU_BUFFER_DESCRIPTOR_INIT;
    in_buf_desc.label = to_wgpu_string_view("Input buffer");
    in_buf_desc.size = element_count * sizeof(float);
    in_buf_desc.usage = WGPUBufferUsage_Storage;
    in_buf_desc.mappedAtCreation = true;
    WGPUBuffer in_buf = wgpuDeviceCreateBuffer(device, &in_buf_desc);

    WGPUBufferDescriptor out_buf_desc = WGPU_BUFFER_DESCRIPTOR_INIT;
    out_buf_desc.label = to_wgpu_string_view("Output buffer");
    out_buf_desc.size = element_count * sizeof(float);
    out_buf_desc.usage = WGPUBufferUsage_Storage | WGPUBufferUsage_CopySrc;
    WGPUBuffer out_buf = wgpuDeviceCreateBuffer(device, &out_buf_desc);

    WGPUBufferDescriptor staging_buf_desc = WGPU_BUFFER_DESCRIPTOR_INIT;
    staging_buf_desc.label = to_wgpu_string_view("Staging buffer");
    staging_buf_desc.size = element_count * sizeof(float);
    staging_buf_desc.usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_MapRead;
    WGPUBuffer staging_buf = wgpuDeviceCreateBuffer(device, &staging_buf_desc);


    float *in_buf_data = (float*)wgpuBufferGetMappedRange(in_buf, 0, WGPU_WHOLE_MAP_SIZE);
    for (size_t i = 0; i < element_count; ++i) {
        in_buf_data[i] = (float)i * 0.1f;
    }
    wgpuBufferUnmap(in_buf);


    WGPUShaderSourceWGSL wgsl_source_desc = WGPU_SHADER_SOURCE_WGSL_INIT;
    wgsl_source_desc.code = to_wgpu_string_view(shader_source);

    WGPUShaderModuleDescriptor module_desc = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
    module_desc.nextInChain = &wgsl_source_desc.chain;
    module_desc.label = to_wgpu_string_view("Our first compute shader");

    WGPUShaderModule shader_module = wgpuDeviceCreateShaderModule(device, &module_desc);

    WGPUComputePipelineDescriptor pipeline_desc = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
    pipeline_desc.compute.module = shader_module;
    pipeline_desc.compute.entryPoint = to_wgpu_string_view("compute_stuff");
    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(device, &pipeline_desc);
    wgpuShaderModuleRelease(shader_module);


    WGPUBindGroupEntry bind_group_entries[2] = {
        WGPU_BIND_GROUP_ENTRY_INIT,
        WGPU_BIND_GROUP_ENTRY_INIT
    };

    bind_group_entries[0].binding = 0;
    bind_group_entries[0].buffer = in_buf;
    bind_group_entries[1].binding = 1;
    bind_group_entries[1].buffer = out_buf;

    WGPUBindGroupDescriptor bind_group_desc = WGPU_BIND_GROUP_DESCRIPTOR_INIT;
    bind_group_desc.entries = bind_group_entries;
    bind_group_desc.entryCount = 2;
    bind_group_desc.layout = wgpuComputePipelineGetBindGroupLayout(pipeline, 0);

    WGPUBindGroup bind_group = wgpuDeviceCreateBindGroup(device, &bind_group_desc);

    wgpuBindGroupLayoutRelease(bind_group_desc.layout);


    WGPUCommandEncoderDescriptor enc_desc2 = WGPU_COMMAND_ENCODER_DESCRIPTOR_INIT;
    WGPUCommandEncoder encoder2 = wgpuDeviceCreateCommandEncoder(device, &enc_desc2);

    WGPUComputePassEncoder compute_pass = wgpuCommandEncoderBeginComputePass(encoder2, NULL);


    wgpuComputePassEncoderSetPipeline(compute_pass, pipeline);
    wgpuComputePassEncoderSetBindGroup(compute_pass, 0, bind_group, 0, NULL);

    uint32_t workgroup_size_x = 32;
    uint32_t workgroup_count_x = divide_and_ceil(element_count, workgroup_size_x);

    wgpuComputePassEncoderDispatchWorkgroups(compute_pass, workgroup_count_x, 1, 1);
    wgpuComputePassEncoderEnd(compute_pass);
    wgpuComputePassEncoderRelease(compute_pass);

    wgpuCommandEncoderCopyBufferToBuffer(encoder2, out_buf, 0, staging_buf, 0, staging_buf_desc.size);

    WGPUCommandBufferDescriptor cmd_buf_desc2 = WGPU_COMMAND_BUFFER_DESCRIPTOR_INIT;
    WGPUCommandBuffer cmd_buf2 = wgpuCommandEncoderFinish(encoder2, &cmd_buf_desc2);

    wgpuQueueSubmit(queue, 1, &cmd_buf2);
    wgpuCommandBufferRelease(cmd_buf2);

    wgpuCommandEncoderRelease(encoder2);

    map_buf_sync(instance, staging_buf);

    const float *staging_buf_data = (float*)wgpuBufferGetConstMappedRange(staging_buf, 0, WGPU_WHOLE_MAP_SIZE);

    printf("staging buf data: [ ");
    for (size_t i = 0; i < element_count; ++i) {
        printf("%.1f ", staging_buf_data[i]);
    }
    printf("]\n");

    wgpuBufferUnmap(staging_buf);


    //glfwInit();

    //glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    //GLFWwindow *window = glfwCreateWindow(640, 480, "test", NULL, NULL);

    //while (!glfwWindowShouldClose(window)) {
    //    glfwPollEvents();
    //}

    wgpuBufferRelease(in_buf);
    wgpuBufferRelease(out_buf);
    wgpuBufferRelease(staging_buf);

    wgpuComputePipelineRelease(pipeline);
    wgpuBindGroupRelease(bind_group);
    wgpuQueueRelease(queue);
    wgpuDeviceRelease(device);
    wgpuInstanceRelease(instance);

    return 0;
}
