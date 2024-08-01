/*
 * Copyright (c) 2024, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef __FACE_DETECTOR_H_
#define __FACE_DETECTOR_H_

#include <time.h>
#include<sys/time.h>
#include <utils/Timers.h>

#define FACEDETECT_INIT_BIAS (-20)
#define FACEDETECT_BIAS_INERVAL (5)
#define FACEDETECT_FRAME_INTERVAL (1)

#ifdef __cplusplus
extern "C" {
#endif

// Detector Type
enum {
    DETECTOR_OPENCV = 0,
    DETECTOR_OPENCL,
};

// Image Format
enum {
    IMAGE_RGBA8888 = 0,
    IMAGE_GRAYSCALE,
    IMAGE_YUV420SP,
    IMAGE_YUV420P,
};

// Image Orientation
enum {
    ROTATION_0 = 0,
    ROTATION_90,
    ROTATION_180,
    ROTATION_270,
};

struct RectFace {
    int x;
    int y;
    int width;
    int height;
};

#if 1
/*
  @type: Detector Type.
  @threshold: Detector Threshold, default = 10.0f.
*/
void *FaceDetector_initizlize(int type, float threshold, int smileMode);

/*
  @width: Initialize Image Width.
  @height: Initialize Image Height.
  @format: Image Format.
*/
void FaceDetector_start(void *context, int width, int height, int format);

void FaceDetector_stop(void *context);

void FaceDetector_destory(void *context);

int FaceDetector_prepare(void *context, void* src);

/*
  @src: Image Data.
  @orientation: Image Orientation.
  @angle: Image angle base on orientation.
  @faces: Output Find Face Rects.
  @num: Output Find Face Number.
*/
int FaceDetector_findFaces(void *context, void* src, int orientation, float angle, int isDrawRect, int *smileMode, RectFace** faces, int* num);


#endif

#ifdef __cplusplus
}
#endif

#endif
