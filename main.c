#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3/SDL_gpu.h>
#include <SDL3_image/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define WDITH 900
#define HIGHT 700
#define MAX_VERT_COUNT 2048

typedef struct Vec3{
    float x,y,z;
} Vec3;

typedef struct Vec2{
    float x,y;
} Vec2;

typedef struct Vertex{
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
} Vertex;

typedef struct {
    float m[16];
} Mat4;

typedef struct CameraUBO{
    Mat4 model;
    Mat4 view;
    Mat4 proj;
} CameraUBO;

typedef struct Mesh{
    Vec3 position;
    Vertex* vertices;
    int vertex_count;
    size_t size;
} Mesh;

Mesh Meshes[5];
SDL_GPUBuffer* vertexBuffers[5];

SDL_GPUShader* LoadTexture(SDL_GPUDevice *device, const char *filePath, SDL_GPUShaderStage stage){
    FILE *file = fopen(filePath, "rb");
    if(!file){
        printf("[ERROR]: could not open file: %s", filePath);
        return 0;
    }

    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);

    char *source = (char *)malloc(length + 1);
    fread(source, 1, length, file);
    source[length] = '\0';
    fclose(file);

    SDL_GPUShaderCreateInfo createInfo = {0};
    createInfo.code_size = length;
    createInfo.code = source;
    createInfo.entrypoint = "main";
    createInfo.format = SDL_GPU_SHADERFORMAT_SPIRV;
    createInfo.stage = stage; // or SDL_GPU_SHADER_TYPE_VERTEX, SDL_GPU_SHADER_TYPE_FRAGMENT
    createInfo.num_samplers = (stage == SDL_GPU_SHADERSTAGE_FRAGMENT) ? 1 : 0;
    createInfo.num_storage_textures = 0;
    createInfo.num_storage_buffers = 0;
    createInfo.num_uniform_buffers = (stage == SDL_GPU_SHADERSTAGE_VERTEX) ? 1 : 0;

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &createInfo);
    if (!shader) {
        SDL_Log("Failed to create shader: %s", SDL_GetError());
    }

    free(source);
    return shader;
}

static int resolve_index(int idx, int count) {
    if (idx < 0) return count + idx;
    return idx - 1;
}

Mesh LoadObjFromFile(const char *filePath, Vec3 pos){
    FILE *file = fopen(filePath, "r");
    if(!file){
        printf("[ERROR]: could not open file: %s\n", filePath);
        return (Mesh){0};
    }

    Vec3* positions = malloc(MAX_VERT_COUNT * sizeof(Vec3));
    Vec3* normals = malloc(MAX_VERT_COUNT * sizeof(Vec3));
    Vec2* uvs = malloc(MAX_VERT_COUNT * sizeof(Vec2));

    int pos_count = 0, pos_capacity = MAX_VERT_COUNT;
    int norm_count = 0, norm_capacity = MAX_VERT_COUNT;
    int uv_count = 0, uv_capacity = MAX_VERT_COUNT;

    Vertex* vertices = malloc(MAX_VERT_COUNT * sizeof(Vertex));
    int vert_count = 0, vert_cap = MAX_VERT_COUNT;

    char line[256];
    float x, y, z;

    while (fgets(line, sizeof(line), file)) {
        if (line[0] == 'v' && line[1] == ' ') {
            if (sscanf(line, "v %f %f %f", &x, &y, &z) == 3) {
                if (pos_count >= pos_capacity) {
                    pos_capacity *= 2;
                    positions = realloc(positions, pos_capacity * sizeof(Vec3));
                }
                positions[pos_count++] = (Vec3){x, y, z};
            }
        }
        else if (line[0] == 'v' && line[1] == 'n') {
            if (sscanf(line, "vn %f %f %f", &x, &y, &z) == 3) {
                if (norm_count >= norm_capacity) {
                    norm_capacity *= 2;
                    normals = realloc(normals, norm_capacity * sizeof(Vec3));
                }
                normals[norm_count++] = (Vec3){x, y, z};
            }
        }
        else if (line[0] == 'v' && line[1] == 't') {
            float u, v;
            if (sscanf(line, "vt %f %f", &u, &v) == 2) {
                if (uv_count >= uv_capacity) {
                    uv_capacity *= 2;
                    uvs = realloc(uvs, uv_capacity * sizeof(Vec2));
                }
                uvs[uv_count++] = (Vec2){u, v};
            }
        }
        else if (line[0] == 'f' && line[1] == ' ') {
            int v[3], vt[3], vn[3];
            int matched = sscanf(line, "f %d/%d/%d %d/%d/%d %d/%d/%d",
                                &v[0], &vt[0], &vn[0],
                                &v[1], &vt[1], &vn[1],
                                &v[2], &vt[2], &vn[2]);
            if (matched != 9) continue;
            for (int i = 0; i < 3; i++) {
                if (vert_count >= vert_cap) {
                    vert_cap *= 2;
                    vertices = realloc(vertices, vert_cap * sizeof(Vertex));
                }
                int vi = resolve_index(v[i], pos_count);
                int vti = resolve_index(vt[i], uv_count);
                int vni = resolve_index(vn[i], norm_count);
                if (vi < 0 || vi >= pos_count) continue;
                vertices[vert_count].position.x = positions[vi].x;
                vertices[vert_count].position.y = positions[vi].y;
                vertices[vert_count].position.z = positions[vi].z;
                if (vni >= 0 && vni < norm_count) {
                    vertices[vert_count].normal.x = normals[vni].x;
                    vertices[vert_count].normal.y = normals[vni].y;
                    vertices[vert_count].normal.z = normals[vni].z;
                } else {
                    vertices[vert_count].normal.x = 0.0f;
                    vertices[vert_count].normal.y = 1.0f;
                    vertices[vert_count].normal.z = 0.0f;
                }
                if (vti >= 0 && vti < uv_count) {
                    vertices[vert_count].uv.x = uvs[vti].x;
                    vertices[vert_count].uv.y = uvs[vti].y;
                } else {
                    vertices[vert_count].uv.x = 0.0f;
                    vertices[vert_count].uv.y = 0.0f;
                }
                vert_count++;
            }
        }
    }

    fclose(file);
    free(positions);
    free(normals);
    free(uvs);

    return (Mesh){
        .position = pos,
        .vertices = vertices,
        .vertex_count = vert_count,
        .size = vert_count * sizeof(Vertex)
    };
}

Mesh CreateDefaultCube(Vec3 pos){

    static Vertex vertices[] = {

        // ===== Front (+Z) =====
        {{-0.5f,-0.5f, 0.5f}, {0,0,1}, {0,0}},
        {{ 0.5f,-0.5f, 0.5f}, {0,0,1}, {1,0}},
        {{ 0.5f, 0.5f, 0.5f}, {0,0,1}, {1,1}},
        {{-0.5f,-0.5f, 0.5f}, {0,0,1}, {0,0}},
        {{ 0.5f, 0.5f, 0.5f}, {0,0,1}, {1,1}},
        {{-0.5f, 0.5f, 0.5f}, {0,0,1}, {0,1}},

        // ===== Back (-Z) =====
        {{ 0.5f,-0.5f,-0.5f}, {0,0,-1}, {0,0}},
        {{-0.5f,-0.5f,-0.5f}, {0,0,-1}, {1,0}},
        {{-0.5f, 0.5f,-0.5f}, {0,0,-1}, {1,1}},
        {{ 0.5f,-0.5f,-0.5f}, {0,0,-1}, {0,0}},
        {{-0.5f, 0.5f,-0.5f}, {0,0,-1}, {1,1}},
        {{ 0.5f, 0.5f,-0.5f}, {0,0,-1}, {0,1}},

        // ===== Left (-X) =====
        {{-0.5f,-0.5f,-0.5f}, {-1,0,0}, {0,0}},
        {{-0.5f,-0.5f, 0.5f}, {-1,0,0}, {1,0}},
        {{-0.5f, 0.5f, 0.5f}, {-1,0,0}, {1,1}},
        {{-0.5f,-0.5f,-0.5f}, {-1,0,0}, {0,0}},
        {{-0.5f, 0.5f, 0.5f}, {-1,0,0}, {1,1}},
        {{-0.5f, 0.5f,-0.5f}, {-1,0,0}, {0,1}},

        // ===== Right (+X) =====
        {{ 0.5f,-0.5f, 0.5f}, {1,0,0}, {0,0}},
        {{ 0.5f,-0.5f,-0.5f}, {1,0,0}, {1,0}},
        {{ 0.5f, 0.5f,-0.5f}, {1,0,0}, {1,1}},
        {{ 0.5f,-0.5f, 0.5f}, {1,0,0}, {0,0}},
        {{ 0.5f, 0.5f,-0.5f}, {1,0,0}, {1,1}},
        {{ 0.5f, 0.5f, 0.5f}, {1,0,0}, {0,1}},

        // ===== Top (+Y) =====
        {{-0.5f, 0.5f, 0.5f}, {0,1,0}, {0,0}},
        {{ 0.5f, 0.5f, 0.5f}, {0,1,0}, {1,0}},
        {{ 0.5f, 0.5f,-0.5f}, {0,1,0}, {1,1}},
        {{-0.5f, 0.5f, 0.5f}, {0,1,0}, {0,0}},
        {{ 0.5f, 0.5f,-0.5f}, {0,1,0}, {1,1}},
        {{-0.5f, 0.5f,-0.5f}, {0,1,0}, {0,1}},

        // ===== Bottom (-Y) =====
        {{-0.5f,-0.5f,-0.5f}, {0,-1,0}, {0,0}},
        {{ 0.5f, 0.5f,-0.5f}, {0,-1,0}, {1,0}},
        {{ 0.5f,-0.5f, 0.5f}, {0,-1,0}, {1,1}},
        {{-0.5f,-0.5f,-0.5f}, {0,-1,0}, {0,0}},
        {{ 0.5f,-0.5f, 0.5f}, {0,-1,0}, {1,1}},
        {{-0.5f,-0.5f, 0.5f}, {0,-1,0}, {0,1}},
    };

    return (Mesh){
        .position = pos,
        .vertices = vertices,
        .vertex_count = sizeof(vertices) / sizeof(Vertex),
        .size = sizeof(vertices)
    };
}

int main(){

    SDL_Init(SDL_INIT_VIDEO);

    SDL_Window *window = SDL_CreateWindow("GPU test", WDITH, HIGHT, 0);
    if(!window){
        printf("[ERROR]: Did not create window, %s\n", SDL_GetError());
        return -1;
    }

    SDL_GPUDevice *gpuDevice =  SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV , false, NULL);
    if(!gpuDevice){
        printf("[ERROR]: Did not create GPU device, %s\n", SDL_GetError());
        return -1;
    }

    if(!SDL_ClaimWindowForGPUDevice(gpuDevice, window)){
        printf("[ERROR]: Did not claim a window, %s\n", SDL_GetError());
        return -1;
    }

    SDL_GPUSwapchainComposition swapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
    if (!SDL_SetGPUSwapchainParameters(
        gpuDevice,
        window,
        swapchainComposition,
        SDL_GPU_PRESENTMODE_IMMEDIATE
    )) {
        printf("SDL_SetGPUSwapchainParameters failed: %s\n", SDL_GetError());
    }

    SDL_Surface *textureSurface_ = SDL_LoadBMP("texture.bmp");
    if(!textureSurface_){
        printf("[ERROR]: Could not load texture.png\n");
        return -1;
    }
    if(textureSurface_->w < 0 || textureSurface_->h < 0){
        printf("Surface size invalid: \ntx w: %i , tx h: %i\n", textureSurface_->w, textureSurface_->h);
        return -1;
    }

    SDL_Surface *textureSurface = SDL_ConvertSurface(textureSurface_, SDL_PIXELFORMAT_RGBA32);
    if (!textureSurface) {
        printf("[ERROR]: Could not convert surface: %s\n", SDL_GetError());
        return -1;
    }

    uint8_t* pixels = (uint8_t*)textureSurface->pixels;
    printf("First 16 bytes: ");
    for (int i = 0; i < 16; i++) {
        printf("%02X ", pixels[i]);
    }
    printf("\n");

    printf("Creatin GPU texture\n");

    SDL_GPUTextureCreateInfo texture_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM,
        .usage = SDL_GPU_TEXTUREUSAGE_SAMPLER,
        .width = textureSurface->w,
        .height = textureSurface->h,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1,
        .props = 0
    };
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(gpuDevice, &texture_info);

    printf("GPU texture created\n");

    SDL_GPUTextureRegion transfer_dst = {
        .texture = texture,
        .mip_level = 0,
        .layer = 0,
        .x = 0, .y = 0, .z = 0,
        .w = textureSurface->w,
        .h = textureSurface->h,
        .d = 1
    };

    SDL_GPUTransferBufferCreateInfo xfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = textureSurface->w * textureSurface->h * 4,
        .props = 0
    };
    SDL_GPUTransferBuffer* transfer_buffer = SDL_CreateGPUTransferBuffer(gpuDevice, &xfer_info);

    if (!transfer_buffer) {
        printf("Failed to create transfer buffer: %s\n", SDL_GetError());
        return 1;
    }

    printf("Mapping transfer_buffer\n");

    void* textureData = SDL_MapGPUTransferBuffer(gpuDevice, transfer_buffer, false);
    if(!textureData){
        printf("[ERROR]: dod not map tansfBuf, %s", SDL_GetError());
        return -1;
    }

    printf("Surface format: %s\n", SDL_GetPixelFormatName(textureSurface->format));

    SDL_memcpy(textureData, textureSurface->pixels, textureSurface->w * textureSurface->h * 4);
    SDL_UnmapGPUTransferBuffer(gpuDevice, transfer_buffer);
    SDL_DestroySurface(textureSurface);

    printf("Mapped transfer_buffer\n");

    SDL_GPUCommandBuffer* upload_cmd = SDL_AcquireGPUCommandBuffer(gpuDevice);
    SDL_GPUCopyPass* copy_pass = SDL_BeginGPUCopyPass(upload_cmd);

    SDL_GPUTextureTransferInfo textureSrc = {
        .transfer_buffer = transfer_buffer,
        .offset = 0,
        .pixels_per_row = texture_info.width,
        .rows_per_layer = texture_info.height
    };
    SDL_UploadToGPUTexture(copy_pass, &textureSrc, &transfer_dst, false);
    SDL_EndGPUCopyPass(copy_pass);
    SDL_SubmitGPUCommandBuffer(upload_cmd);
    SDL_WaitForGPUIdle(gpuDevice);
    SDL_ReleaseGPUTransferBuffer(gpuDevice, transfer_buffer);

    printf("Texture loaded and setup\n");

    SDL_GPUSamplerCreateInfo sampler_info = {
        .min_filter = SDL_GPU_FILTER_LINEAR,
        .mag_filter = SDL_GPU_FILTER_LINEAR,
        .mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR,
        .address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_REPEAT,
        .mip_lod_bias = 0.0f,
        .max_anisotropy = 1.0f,
        .compare_op = SDL_GPU_COMPAREOP_NEVER,
        .min_lod = 0.0f,
        .max_lod = 1000.0f,
        .enable_anisotropy = false,
        .enable_compare = false,
        .props = 0
    };
    SDL_GPUSampler* sampler = SDL_CreateGPUSampler(gpuDevice, &sampler_info);

    printf("Sampler created\n");

    Meshes[0] = LoadObjFromFile("ship.obj",(Vec3){0.0f, 0.0f, 15.0f});
    Meshes[1] = LoadObjFromFile("monkey.obj",(Vec3){0.0f, 0.0f, 15.0f});
    Meshes[2] = LoadObjFromFile("ship_2.obj",(Vec3){0.0f, 0.0f, -15.0f});
    Meshes[3] = LoadObjFromFile("sphere.obj",(Vec3){0.0f, 0.0f, -15.0f});
    Meshes[4] = CreateDefaultCube((Vec3){0.0f, -2.0f, 4.0f});

    printf("Meshes created\n");

    for(int i=0;i<5;i++){
        SDL_GPUBufferCreateInfo bufferInfo = {
            .usage = SDL_GPU_BUFFERUSAGE_VERTEX,
            .size = Meshes[i].size,
            .props = 0
        };
        vertexBuffers[i] = SDL_CreateGPUBuffer(gpuDevice, &bufferInfo);

        SDL_GPUTransferBufferCreateInfo transferInfo = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
            .size = Meshes[i].size,
            .props = 0
        };
        SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(gpuDevice, &transferInfo);

        void* data = SDL_MapGPUTransferBuffer(gpuDevice, transferBuffer, false);
        SDL_memcpy(data, Meshes[i].vertices, Meshes[i].size);
        SDL_UnmapGPUTransferBuffer(gpuDevice, transferBuffer);

        SDL_GPUCommandBuffer* uploadCmd = SDL_AcquireGPUCommandBuffer(gpuDevice);
        SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(uploadCmd);

        SDL_GPUTransferBufferLocation vertSrc = {
            .transfer_buffer = transferBuffer,
            .offset = 0
        };
        SDL_GPUBufferRegion vertDst = {
            .buffer = vertexBuffers[i],
            .offset = 0,
            .size = Meshes[i].size
        };

        SDL_UploadToGPUBuffer(copyPass, &vertSrc, &vertDst, false);

        SDL_EndGPUCopyPass(copyPass);
        SDL_SubmitGPUCommandBuffer(uploadCmd);

        SDL_ReleaseGPUTransferBuffer(gpuDevice, transferBuffer);
    }

    printf("Verticles loaded\n");

    SDL_GPUShader *vertShader = LoadTexture(gpuDevice, "vert.spv", SDL_GPU_SHADERSTAGE_VERTEX);
    SDL_GPUShader *fragShader = LoadTexture(gpuDevice, "frag.spv", SDL_GPU_SHADERSTAGE_FRAGMENT);

    printf("Shaders loaded\n");

    SDL_GPUVertexAttribute vertex_attributes[] = {
        {
            .location = 0,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = 0
        },
        {
            .location = 1,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3,
            .offset = sizeof(float) * 3
        },
        {
            .location = 2,
            .buffer_slot = 0,
            .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2,
            .offset = sizeof(float) * 6
        }
    };

    SDL_GPUVertexBufferDescription vertex_buffer_desc = {
        .slot = 0,
        .pitch = sizeof(Vertex),
        .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX,
        .instance_step_rate = 0
    };

    SDL_GPUVertexInputState vertex_input_state = {
        .vertex_buffer_descriptions = &vertex_buffer_desc,
        .num_vertex_buffers = 1,
        .vertex_attributes = vertex_attributes,
        .num_vertex_attributes = 3
    };

    SDL_GPUColorTargetDescription color_target = {
        .format = SDL_GetGPUSwapchainTextureFormat(gpuDevice, window),
        .blend_state = {
            .enable_blend = false,
            .alpha_blend_op = SDL_GPU_BLENDOP_ADD,
            .color_blend_op = SDL_GPU_BLENDOP_ADD,
            .src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
            .src_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE,
            .dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO,
            .color_write_mask = 0xF
        }
    };

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {
        .vertex_shader = vertShader,
        .fragment_shader = fragShader,
        .vertex_input_state = vertex_input_state,
        .primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST,
        .rasterizer_state = {
            .fill_mode = SDL_GPU_FILLMODE_FILL,
            .cull_mode = SDL_GPU_CULLMODE_BACK,
            .front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE,
            .depth_bias_constant_factor = 0.0f,
            .depth_bias_clamp = 0.0f,
            .depth_bias_slope_factor = 0.0f,
            .enable_depth_bias = false,
            .enable_depth_clip = true
        },
        .multisample_state = {
            .sample_count = SDL_GPU_SAMPLECOUNT_1,
            .sample_mask = 0xFFFFFFFF,
            .enable_mask = false
        },
        .depth_stencil_state = {
            .compare_op = SDL_GPU_COMPAREOP_LESS,
            .back_stencil_state = {
                .fail_op = SDL_GPU_STENCILOP_KEEP,
                .pass_op = SDL_GPU_STENCILOP_KEEP,
                .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
                .compare_op = SDL_GPU_COMPAREOP_LESS
            },
            .front_stencil_state = {
                .fail_op = SDL_GPU_STENCILOP_KEEP,
                .pass_op = SDL_GPU_STENCILOP_KEEP,
                .depth_fail_op = SDL_GPU_STENCILOP_KEEP,
                .compare_op = SDL_GPU_COMPAREOP_LESS
            },
            .compare_mask = 0,
            .write_mask = 0,
            .enable_depth_test = true,
            .enable_depth_write = true,
            .enable_stencil_test = false
        },
        .target_info = {
            .color_target_descriptions = &color_target,
            .num_color_targets = 1,
            .depth_stencil_format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
            .has_depth_stencil_target = true
        },
        .props = 0
    };

    SDL_GPUTextureCreateInfo depth_info = {
        .type = SDL_GPU_TEXTURETYPE_2D,
        .format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        .usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET,
        .width = WDITH,
        .height = HIGHT,
        .layer_count_or_depth = 1,
        .num_levels = 1,
        .sample_count = SDL_GPU_SAMPLECOUNT_1
    };
    SDL_GPUTexture* depthTexture = SDL_CreateGPUTexture(gpuDevice, &depth_info);

    printf("Setting up UBO\n");

    float fov = 70.0f * (3.14159265f / 180.0f);
    float aspect = (float)WDITH / (float)HIGHT;
    float near = 0.1f;
    float far  = 1000.0f;
    float f = 1.0f / tanf(fov * 0.5f);

    CameraUBO cameraData = {
        .view = {
            .m = {
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 0.0f,
                0.0f, 0.0f, -8.0f, 1.0f
            }
        },
        .proj = {
            .m = {
                f / aspect, 0, 0, 0,
                0, f, 0, 0,
                0, 0, far / (near - far), -1,
                0, 0, (near * far) / (near - far), 0
            }
        }
    };

    SDL_GPUBufferCreateInfo uboInfo = {
        .usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ,
        .size = sizeof(CameraUBO)
    };
    SDL_GPUBuffer *cameraBuffer = SDL_CreateGPUBuffer(gpuDevice, &uboInfo);

    SDL_GPUTransferBufferCreateInfo transfer_info = {
        .usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD,
        .size = sizeof(CameraUBO)
    };
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(gpuDevice, &transfer_info);

    void *mapped = SDL_MapGPUTransferBuffer(gpuDevice, transfer, false);
    SDL_memcpy(mapped, &cameraData, sizeof(CameraUBO));
    SDL_UnmapGPUTransferBuffer(gpuDevice, transfer);

    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpuDevice);
    SDL_GPUCopyPass *uboCopyPass = SDL_BeginGPUCopyPass(cmd);

    SDL_GPUTransferBufferLocation uboSrc = {
        .transfer_buffer = transfer,
        .offset = 0
    };
    SDL_GPUBufferRegion uboDst = {
        .buffer = cameraBuffer,
        .offset = 0,
        .size = sizeof(CameraUBO)
    };

    SDL_UploadToGPUBuffer(uboCopyPass, &uboSrc, &uboDst, false);
    SDL_EndGPUCopyPass(uboCopyPass);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_WaitForGPUIdle(gpuDevice);

    printf("Creating graphics pipeline\n");

    SDL_GPUGraphicsPipeline* pipeline = SDL_CreateGPUGraphicsPipeline(gpuDevice, &pipeline_info);

    printf("All setup done\n");

    bool quit = false;
    SDL_Event event;
    float rotation;

    while(!quit){
        float startTickNS = SDL_GetTicksNS();

        while (SDL_PollEvent(&event)){
            if (event.type == SDL_EVENT_QUIT) {
                quit = true;
            }
        }

        rotation += 0.0005f;
        float cos_y = cosf(rotation);
        float sin_y = sinf(rotation);

        SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(gpuDevice);

        SDL_GPUTexture* swapchainTexture;
        if (!SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, &swapchainTexture, NULL, NULL)){
            printf("[ERROR]: WaitAndAcquireGPUSwapchainTexture failed, %s\n", SDL_GetError());
            return -1;
        }

        if (swapchainTexture){
            SDL_GPUColorTargetInfo colorTargetInfo = {0};
            colorTargetInfo.texture = swapchainTexture;
            colorTargetInfo.clear_color = (SDL_FColor){0.5f, 0.5f, 0.5f, 1.0f};
            colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
            colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

            SDL_GPUDepthStencilTargetInfo depthTarget = {
                .texture = depthTexture,
                .clear_depth = 1.0f,
                .load_op = SDL_GPU_LOADOP_CLEAR,
                .store_op = SDL_GPU_STOREOP_DONT_CARE,
                .stencil_load_op = SDL_GPU_LOADOP_DONT_CARE,
                .stencil_store_op = SDL_GPU_STOREOP_DONT_CARE,
                .cycle = false
            };

            SDL_GPURenderPass* renderPass = SDL_BeginGPURenderPass(cmd, &colorTargetInfo, 1, &depthTarget);

            SDL_BindGPUGraphicsPipeline(renderPass, pipeline);

            SDL_GPUTextureSamplerBinding texture_binding = {
                .texture = texture,
                .sampler = sampler
            };

            for(int i=0; i<5;i++){

                cameraData.model = (Mat4){
                    .m = {
                        cos_y,  0.0f, sin_y, 0.0f,
                        0.0f,   1.0f, 0.0f,  0.0f,
                        -sin_y,  0.0f, cos_y, 0.0f,
                        0.0f, 0.0f, -120.0f, 1.0f
                    }
                };

                SDL_PushGPUVertexUniformData(cmd, 1, &cameraData, sizeof(CameraUBO));
                SDL_GPUBufferBinding vertex_binding = {
                    .buffer = vertexBuffers[i],
                    .offset = 0
                };
                SDL_BindGPUVertexBuffers(renderPass, 0, &vertex_binding, 1);
                SDL_BindGPUFragmentSamplers(renderPass, 0, &texture_binding, 1);
                SDL_DrawGPUPrimitives(renderPass, Meshes[i].vertex_count, 1, 0, 0);
            }

            SDL_EndGPURenderPass(renderPass);
        }

        SDL_SubmitGPUCommandBuffer(cmd);

        //float tickDeltaNS = SDL_GetTicksNS() - startTickNS;
        //if(tickDeltaNS > 0){
        //    int Fps = 1000000000.0f/tickDeltaNS;
        //    printf("FPS: %i\n", Fps);
        //}
    }

    SDL_ReleaseGPUGraphicsPipeline(gpuDevice, pipeline);
    SDL_ReleaseGPUShader(gpuDevice, vertShader);
    SDL_ReleaseGPUShader(gpuDevice, fragShader);
    SDL_ReleaseGPUSampler(gpuDevice, sampler);
    SDL_ReleaseGPUTexture(gpuDevice, texture);
    SDL_ReleaseGPUTransferBuffer(gpuDevice, transfer);
    for(int i=0;i<5;i++){
        SDL_ReleaseGPUBuffer(gpuDevice, vertexBuffers[i]);
    }
    SDL_ReleaseGPUBuffer(gpuDevice, cameraBuffer);
    SDL_DestroyGPUDevice(gpuDevice);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
