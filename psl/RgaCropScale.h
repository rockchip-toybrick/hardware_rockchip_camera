/*
 * Copyright (c) 2018, Fuzhou Rockchip Electronics Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef HAL_ROCKCHIP_PSL_RKISP1_RGACROPSCALE_H_
#define HAL_ROCKCHIP_PSL_RKISP1_RGACROPSCALE_H_
#if defined(ANDROID_VERSION_ABOVE_12_X)
#include <im2d_api/im2d.h>
#endif

#include <unordered_map>
#include <mutex>

#include <cutils/native_handle.h>

namespace android {
namespace camera2 {
#if defined(TARGET_RK312X)
#define RGA_VER (1.0)
#define RGA_ACTIVE_W (2048)
#define RGA_VIRTUAL_W (4096)
#define RGA_ACTIVE_H (2048)
#define RGA_VIRTUAL_H (2048)
#else
#if defined(TARGET_RK3588)
#define RGA_VER (3.0)
#define RGA_ACTIVE_W (8128)
#define RGA_VIRTUAL_W (8128)
#define RGA_ACTIVE_H (8128)
#define RGA_VIRTUAL_H (8128)
#elif defined(TARGET_RK3562)
#define RGA_VER (2.0)
#define RGA_ACTIVE_W (4096)
#define RGA_VIRTUAL_W (8192)
#define RGA_ACTIVE_H (4096)
#define RGA_VIRTUAL_H (8192)
#elif defined(TARGET_RK3576)
#define RGA_VER "2.0pro"
#define RGA_ACTIVE_W (8192)
#define RGA_VIRTUAL_W (8192)
#define RGA_ACTIVE_H (8192)
#define RGA_VIRTUAL_H (8192)
#else
#define RGA_VER (2.0)
#define RGA_ACTIVE_W (4096)
#define RGA_VIRTUAL_W (4096)
#define RGA_ACTIVE_H (4096)
#define RGA_VIRTUAL_H (4096)
#endif
#endif

struct RgaBufferContext {
    int cameraId;
    int dmaFd;
    int handle;
    int width;
    int height;
    int format;
};

typedef std::unordered_map<buffer_handle_t,
        std::unique_ptr<struct RgaBufferContext>>
        RgaBufferContextCache;

class RgaCropScale {
 public:
    struct Params {
      public:
        Params() {
            fd = -1;
            handle = -1;
            vir_addr = 0;
            offset_x = 0;
            offset_y = 0;
            width_stride = 0;
            height_stride = 0;
            width = 0;
            height = 0;
            blend = 0;
            translate_x = 0;
            translate_y = 0;
            rotation = 0;
            fmt = 0;
            mirror = false;
            acquire_fence_fd = -1;
            release_fence_fd = -1;
        }
        /* use share fd if it's valid */
        int fd;
        int handle;
        /* if fd == -1, use virtual address */
        char *vir_addr;
        int offset_x;
        int offset_y;
        int width_stride;
        int height_stride;
        int width;
        int height;
        int blend;
        int translate_x;
        int translate_y;
        int rotation;
        /* only support NV12,NV21 now */
        int fmt;
        /* just for src params */
        bool mirror;
        bool flip;
        int acquire_fence_fd;
        int release_fence_fd;
    };
    struct RkfaceRect {
        int x, y, width, height;
    };
    static int RectCheck(struct Params* out, im_rect& rect);
    static int ImDrawRectSingle(struct Params* out);
    static int ImDrawRectArray(struct Params* out, im_rect *dst_rect, int array_size);
    static int Im2dBlit(struct Params* in, struct Params* out);
    static RgaCropScale* GetInstance();
    static int CropScaleNV12Or21(struct Params* in, struct Params* out);
    static int WidthSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout);
    static int HeightSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout);
    static int WHSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout);
    static int CropScaleNV12Or21Async(struct Params* in, struct Params* out);
    static int WaitFenceDone(int fence_fd);
    int getRgaBufferHandle(int cameraId, buffer_handle_t handle, int width, int height, int format);
    void releaseRgaBufferHandle(int cameraId);
  private:
    std::mutex lock_;
    RgaBufferContextCache rga_buffer_context_;
};

} /* namespace camera2 */
} /* namespace android */

#endif  // HAL_ROCKCHIP_PSL_RKISP1_RGACROPSCALE_H_
