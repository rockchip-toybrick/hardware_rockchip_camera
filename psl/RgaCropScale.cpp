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
#include "RgaCropScale.h"
#include "LogHelper.h"
#include <utils/Singleton.h>
#include <RockchipRga.h>
#if defined(ANDROID_VERSION_ABOVE_12_X)
#include <hardware/hardware_rockchip.h>
#include <im2d_api/im2d.hpp>
#endif
#include "arc/camera_buffer_manager.h"

#define LOG_TAG "RgaCropScale"

namespace android {
namespace camera2 {

static void RkCamera_empty_structure(rga_buffer_t *src, rga_buffer_t *dst, rga_buffer_t *pat,
                                im_rect *srect, im_rect *drect, im_rect *prect, im_opt_t *opt) {
    if (src != NULL)
        memset(src, 0, sizeof(*src));
    if (dst != NULL)
        memset(dst, 0, sizeof(*dst));
    if (pat != NULL)
        memset(pat, 0, sizeof(*pat));
    if (srect != NULL)
        memset(srect, 0, sizeof(*srect));
    if (drect != NULL)
        memset(drect, 0, sizeof(*drect));
    if (prect != NULL)
        memset(prect, 0, sizeof(*prect));
    if (opt != NULL)
        memset(opt, 0, sizeof(*opt));
}

int RgaCropScale::Im2dBlit(struct Params* in, struct Params* out)
{
	im_rect         src_rect;
	im_rect         dst_rect;
	rga_buffer_t     src;
	rga_buffer_t     dst;
	int ret = 0;
	memset(&src_rect, 0, sizeof(src_rect));
	memset(&dst_rect, 0, sizeof(dst_rect));
	memset(&src, 0, sizeof(src));
	memset(&dst, 0, sizeof(dst));

	im_handle_param_t param_in,param_out;

	rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;
	param_in.width = in->width;
	param_in.height = in->height;
	param_in.format = in->fmt;

	param_out.width = out->width;
	param_out.height = out->height;
	param_out.format = out->fmt;

	if (in->fd == -1) {
	   src_handle = importbuffer_virtualaddr((void*)in->vir_addr, &param_in);
	} else {
	   src_handle = importbuffer_fd(in->fd, &param_in);
	}

	if (out->fd == -1) {
	   dst_handle = importbuffer_virtualaddr((void*)out->vir_addr, &param_out);
	} else {
	   dst_handle = importbuffer_fd(out->fd, &param_out);
	}

	src = wrapbuffer_handle(src_handle, in->width, in->height, in->fmt);
	dst = wrapbuffer_handle(dst_handle, out->width, out->height, out->fmt);

	if(src.width == 0 || dst.width == 0) {
        releasebuffer_handle(src_handle);
        releasebuffer_handle(dst_handle);
		return -1;
	}
	int usage = 0;

    if (in->rotation != 0) {
        int zoom_cropW = in->width,zoom_cropH = in->height;
        int ratio = 0;
        int zoom_top_offset = in->offset_y,zoom_left_offset = in->offset_x;
        switch (in->rotation) {
            case 90:
                usage = IM_HAL_TRANSFORM_ROT_90;
                ratio = out->width * 1000 / out->height;
                zoom_cropH = in->height & (~0x01);
                zoom_cropW = in->width & (~0x01);
                zoom_left_offset=((in->width-zoom_cropW)>>1) & (~0x01);
                zoom_top_offset=((in->height-zoom_cropH)>>1) & (~0x01);
                in->width = zoom_cropW;
                in->height = zoom_cropH;
                in->offset_x = zoom_left_offset + in->offset_x;
                in->offset_y = zoom_top_offset + in->offset_y;
                break;
            case 180:
                usage = IM_HAL_TRANSFORM_ROT_180;
                break;
            case 270:
                usage = IM_HAL_TRANSFORM_ROT_270;
                ratio = out->width * 1000 / out->height;
                zoom_cropH = in->height & (~0x01);
                zoom_cropW = in->width & (~0x01);
                zoom_left_offset=((in->width-zoom_cropW)>>1) & (~0x01);
                zoom_top_offset=((in->height-zoom_cropH)>>1) & (~0x01);
                in->width = zoom_cropW;
                in->height = zoom_cropH;
                in->offset_x = zoom_left_offset + in->offset_x;
                in->offset_y = zoom_top_offset + in->offset_y;
                break;
        }
        LOGE("crop:%dx%d, offset:%dx%d", zoom_cropW, zoom_cropH, zoom_left_offset, zoom_top_offset);
    }

	dst.width = src.width;
	dst.height = src.height;

	im_opt_t opt;
	rga_buffer_t pat;
	im_rect srect;
	im_rect drect;
	im_rect prect;
	RkCamera_empty_structure(NULL, NULL, &pat, &srect, &drect, &prect, &opt);

    src_rect.x = in->offset_x;
    src_rect.y = in->offset_y;
    src_rect.width = in->width;
    src_rect.height = in->height;

    dst_rect.x = out->offset_x;
    dst_rect.y = out->offset_y;
    dst_rect.width = out->width;
    dst_rect.height = out->height;

	usage |= IM_SYNC;

	ret = improcess(src, dst, {}, src_rect, dst_rect, {}, usage);


    releasebuffer_handle(src_handle);
    releasebuffer_handle(dst_handle);
	return ret;
}

//Check Face Rectangle is valid
int RgaCropScale::RectCheck(struct Params* out, im_rect& rect)
{
    if (!(rect.x || rect.y || rect.width || rect.height)) {
        ALOGD("@%s: rect all zero, do nothing!", __FUNCTION__);
        return -1;
    }
    if ((rect.x < 0) || (rect.y <0) ||
        (rect.width < 0) || (rect.height < 0)) {
        ALOGD("@%s: rect has nagetive value, do nothing", __FUNCTION__);
        return -1;
    }

    if (((rect.x + rect.width) > out->width_stride ) ||
        ((rect.y + rect.height) > out->height_stride)) {
        ALOGD("@%s: do nothing", __FUNCTION__);
        return -1;
    }
    return 0;
}

int RgaCropScale::ImDrawRectSingle(struct Params* out)
{

    im_rect         dst_rect;
    rga_buffer_t     dst;
    int ret = 0;

    memset(&dst_rect, 0, sizeof(dst_rect));
    memset(&dst, 0, sizeof(dst));
    im_handle_param_t param_out;
    rga_buffer_handle_t dst_handle;

    param_out.width = out->width_stride;
    param_out.height = out->height_stride;
    param_out.format = out->fmt;

    if (out->fd == -1) {
        dst_handle = importbuffer_virtualaddr((void*)out->vir_addr, &param_out);
    } else {
        dst_handle = importbuffer_fd(out->fd, &param_out);
    }

    dst = wrapbuffer_handle(dst_handle, out->width_stride, out->height_stride, out->fmt);

    if(dst.width == 0) {
        releasebuffer_handle(dst_handle);
        return -1;
    }

    dst_rect.x = out->offset_x;
    dst_rect.y = out->offset_y;
    dst_rect.width = out->width;
    dst_rect.height = out->height;

    if (RectCheck(out, dst_rect))
        return 0;

    dst.width = out->width_stride;
    dst.height = out->height_stride;
    LOGD("dst_rect:[%d %d %d %d], stride: [%d %d]",
           dst_rect.x, dst_rect.y, dst_rect.width, dst_rect.height,
           dst.width, dst.height);
#if 0
    ret = imcheck({}, dst, {}, dst_rect, IM_COLOR_FILL);
    if (IM_STATUS_NOERROR != ret) {
        ALOGD("%d, check error! %s", __FUNCTION__, imStrError((IM_STATUS)ret));
        goto release_buffer;
    }
#endif
    ret = imrectangle(dst, dst_rect, 0xff00ff00, 2);
    if (ret == IM_STATUS_SUCCESS) {
        LOGD("%s running success!\n", LOG_TAG);
    } else {
        LOGE("%s running failed, %s\n", LOG_TAG, imStrError((IM_STATUS)ret));
        goto release_buffer;
    }

release_buffer:

    releasebuffer_handle(dst_handle);
	return ret;
}

int RgaCropScale::ImDrawRectArray(struct Params* out, im_rect *dst_rect, int array_size)
{

    rga_buffer_t     dst;
    int ret = 0, i = 0;

    for (i = 0; i < array_size; i++) {
        // if one face rectangle abnormal, discard it;
        if (RectCheck(out, dst_rect[i])) {
            return 0;
        }
    }

    memset(&dst, 0, sizeof(dst));
    im_handle_param_t param_out;
    rga_buffer_handle_t dst_handle;

    param_out.width = out->width_stride;
    param_out.height = out->height_stride;
    param_out.format = out->fmt;

    if (out->fd == -1) {
        dst_handle = importbuffer_virtualaddr((void*)out->vir_addr, &param_out);
    } else {
        dst_handle = importbuffer_fd(out->fd, &param_out);
    }

    dst = wrapbuffer_handle(dst_handle, out->width_stride, out->height_stride, out->fmt);

    if(dst.width == 0) {
        releasebuffer_handle(dst_handle);
        return -1;
    }

    dst.width = out->width_stride;
    dst.height = out->height_stride;

    for (i = 0; i < array_size; i++) {
        LOGD("dst_rect[%d]:[%d %d %d %d], stride: [%d %d]",
               i, dst_rect[i].x, dst_rect[i].y, dst_rect[i].width, dst_rect[i].height,
               dst.width, dst.height);
    }
    LOGD("%s: array_size(%d)", __FUNCTION__, array_size);
    ret = imrectangleArray(dst, dst_rect, array_size, 0xff00ff00, 2);
    if (ret == IM_STATUS_SUCCESS) {
        LOGD("running success!\n");
    } else {
        LOGE("running failed: %s\n", imStrError((IM_STATUS)ret));
        goto release_buffer;
    }

release_buffer:

    releasebuffer_handle(dst_handle);
    return ret;
}

RgaCropScale* RgaCropScale::GetInstance(){
    static RgaCropScale instance;
    return &instance;
}

int RgaCropScale::CropScaleNV12Or21(struct Params* in, struct Params* out)
{
	HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
#if defined(ANDROID_VERSION_ABOVE_12_X)
    int ret = CropScaleNV12Or21Async(in,out);
    if (ret == IM_STATUS_SUCCESS )
    {
        return 0;
    }
#endif
	rga_info_t src, dst;
#if defined(ANDROID_VERSION_ABOVE_12_X)
	rga_buffer_handle_t src_handle;
	rga_buffer_handle_t dst_handle;
	im_handle_param_t param;
	memset(&param, 0, sizeof(im_handle_param_t));
	memset(&src_handle, 0, sizeof(rga_buffer_handle_t));
	memset(&dst_handle, 0, sizeof(rga_buffer_handle_t));
#endif
    memset(&src, 0, sizeof(rga_info_t));
    memset(&dst, 0, sizeof(rga_info_t));
    if (!in || !out)
        return -1;

	if((out->width > RGA_ACTIVE_W) || (out->height > RGA_ACTIVE_H)){
			ALOGE("%s(%d): out wxh %dx%d beyond rga capability",
                 __FUNCTION__, __LINE__,
                 out->width, out->height);
            return -1;
	}

    if ((in->fmt != HAL_PIXEL_FORMAT_YCrCb_NV12 &&
        in->fmt != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
		in->fmt != HAL_PIXEL_FORMAT_RGBA_8888) ||
        (out->fmt != HAL_PIXEL_FORMAT_YCrCb_NV12 &&
        out->fmt != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
		out->fmt != HAL_PIXEL_FORMAT_RGBA_8888)) {
        ALOGE("%s(%d): only accept NV12 or NV21 now. in fmt %d, out fmt %d",
             __FUNCTION__, __LINE__,
             in->fmt, out->fmt);
        return -1;
    }
	RockchipRga& rkRga(RockchipRga::get());

#if defined(ANDROID_VERSION_ABOVE_12_X)
	param.width = in->width;
	param.height = in->height;
	param.format = in->fmt;
#endif
	if (in->fd == -1) {
		src.fd = -1;
		src.virAddr = (void*)in->vir_addr;
#if defined(ANDROID_VERSION_ABOVE_12_X)
		LOGD("@%s,src virtual:%p",__FUNCTION__,src.virAddr);
		src_handle = importbuffer_virtualaddr(src.virAddr, &param);
#endif
	} else {
		src.fd = in->fd;
#if defined(ANDROID_VERSION_ABOVE_12_X)
		src_handle = importbuffer_fd(src.fd, &param);
		LOGD("@%s,src fd:%d,width:%d,height:%d,format:%d",__FUNCTION__,src.fd,param.width,param.height,param.format);
#endif
	}
	src.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);

    if (in->rotation != 0) {
        int zoom_cropW = in->width,zoom_cropH = in->height;
        int ratio = 0;
        int zoom_top_offset = in->offset_y,zoom_left_offset = in->offset_x;
        switch (in->rotation) {
            case 90:
                src.rotation = DRM_RGA_TRANSFORM_ROT_90;
                ratio = out->width * 1000 / out->height;
                zoom_cropH = in->height & (~0x01);
                zoom_cropW = in->width & (~0x01);
                zoom_left_offset=((in->width-zoom_cropW)>>1) & (~0x01);
                zoom_top_offset=((in->height-zoom_cropH)>>1) & (~0x01);
                in->width = zoom_cropW;
                in->height = zoom_cropH;
                in->offset_x = zoom_left_offset + in->offset_x;
                in->offset_y = zoom_top_offset + in->offset_y;
                break;
            case 180:
                src.rotation = DRM_RGA_TRANSFORM_ROT_180;
                break;
            case 270:
                src.rotation = DRM_RGA_TRANSFORM_ROT_270;
                ratio = out->width * 1000 / out->height;
                zoom_cropH = in->height & (~0x01);
                zoom_cropW = in->width & (~0x01);
                zoom_left_offset=((in->width-zoom_cropW)>>1) & (~0x01);
                zoom_top_offset=((in->height-zoom_cropH)>>1) & (~0x01);
                in->width = zoom_cropW;
                in->height = zoom_cropH;
                in->offset_x = zoom_left_offset + in->offset_x;
                in->offset_y = zoom_top_offset + in->offset_y;
                break;
        }
        LOGE("crop:%dx%d, offset:%dx%d", zoom_cropW, zoom_cropH, zoom_left_offset, zoom_top_offset);
    }

#if defined(ANDROID_VERSION_ABOVE_12_X)
	param.width = out->width;
	param.height = out->height;
	param.format = out->fmt;
#endif
	if (out->fd == -1 ) {
		dst.fd = -1;
		dst.virAddr = (void*)out->vir_addr;
#if defined(ANDROID_VERSION_ABOVE_12_X)
		LOGD("@%s,dst virtual:%p",__FUNCTION__,src.virAddr);
		dst_handle = importbuffer_virtualaddr(dst.virAddr, &param);
#endif
	} else {
		dst.fd = out->fd;
#if defined(ANDROID_VERSION_ABOVE_12_X)
		dst_handle = importbuffer_fd(dst.fd, &param);
		LOGD("@%s,dst fd:%d,width:%d,height:%d,format:%d",__FUNCTION__,dst.fd,param.width,param.height,param.format);
#endif
	}
	dst.mmuFlag = ((2 & 0x3) << 4) | 1 | (1 << 8) | (1 << 10);

	rga_set_rect(&src.rect,
		     in->offset_x,
		     in->offset_y,
		     in->width,
		     in->height,
		     in->width_stride,
		     in->height_stride,
		     in->fmt);

	rga_set_rect(&dst.rect,
		     out->offset_x,
		     out->offset_y,
		     out->width,
		     out->height,
		     out->width_stride,
		     out->height_stride,
		     out->fmt);
    if (in->mirror) {
        src.rotation |= src.rotation ?
				DRM_RGA_TRANSFORM_FLIP_H << 4 :
				DRM_RGA_TRANSFORM_FLIP_H;
        LOGD("---zc rga mirror====");
    }
    if (in->flip) {
        LOGD("---zc rga flip=====");
        src.rotation |= src.rotation ?
				DRM_RGA_TRANSFORM_FLIP_V << 4 :
				DRM_RGA_TRANSFORM_FLIP_V;
    }


#if defined(ANDROID_VERSION_ABOVE_12_X)
    src.handle = src_handle;
    src.fd = -1;
    src.in_fence_fd = -1;
    dst.handle = dst_handle;
    dst.fd = -1;
    dst.in_fence_fd = -1;
#endif

	if (rkRga.RkRgaBlit(&src, &dst, NULL)) {
	    ALOGE("%s:rga blit failed", __FUNCTION__);
#if defined(ANDROID_VERSION_ABOVE_12_X)
	    releasebuffer_handle(src_handle);
	    releasebuffer_handle(dst_handle);
#endif
	    return -1;
	}

#if defined(ANDROID_VERSION_ABOVE_12_X)
	releasebuffer_handle(src_handle);
	releasebuffer_handle(dst_handle);
#endif
    return 0;
}

/* split buffer to left right to do crop scale */
int RgaCropScale::WidthSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout) {
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
                /* test start */
    char *src = rgain->vir_addr;
    char *dst = rgaout->vir_addr;
    int src_fd = rgain->fd;
    int dst_fd = rgaout->fd;
    unsigned int in_w, in_h, in_offset_x, in_offset_y, out_w, out_h, out_offset_x, out_offset_y;
    unsigned int in_w_stride, in_h_stride, out_w_stride, out_h_stride;

    ALOGD("@%s: do split copy/scale start!", __FUNCTION__);
    in_w = rgain->width;
    in_h = rgain->height;
    in_offset_x = rgain->offset_x;
    in_offset_y = rgain->offset_y;
    out_w = rgaout->width;
    out_h = rgaout->height;
    out_offset_x = rgaout->offset_x;
    out_offset_y = rgaout->offset_y;
    in_w_stride = rgain->width_stride;
    in_h_stride = rgain->height_stride;
    out_w_stride = rgaout->width_stride;
    out_h_stride = rgaout->height_stride;

    rgain->width = in_w / 2;

    rgaout->width = out_w / 2;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: first split copy/scale failed!", __FUNCTION__);
        return -1;
    }
    if(rgaout->release_fence_fd != -1 ){
        RgaCropScale::WaitFenceDone(rgaout->release_fence_fd);
        rgaout->release_fence_fd = -1;
    }
    ALOGD("@%s: do second split copy/ scale", __FUNCTION__);

    rgain->offset_x = in_offset_x + in_w / 2;
    rgaout->offset_x = out_w_stride / 2;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: second split copy/scale failed!", __FUNCTION__);
        return -1;
    }
    if(rgaout->release_fence_fd != -1 ){
        RgaCropScale::WaitFenceDone(rgaout->release_fence_fd);
        rgaout->release_fence_fd = -1;
    }
    ALOGD("@%s: do split copy/scale end!", __FUNCTION__);
    return 0;
}

/* split buffer to up and down to do crop scale */
int RgaCropScale::HeightSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout) {
                /* test start */
    char *src = rgain->vir_addr;
    char *dst = rgaout->vir_addr;
    int src_fd = rgain->fd;
    int dst_fd = rgaout->fd;
    unsigned int in_w, in_h, in_offset_x, in_offset_y, out_w, out_h, out_offset_x, out_offset_y;
    unsigned int in_w_stride, in_h_stride, out_w_stride, out_h_stride;

    ALOGD("@%s: do split copy/scale start!", __FUNCTION__);
    in_w = rgain->width;
    in_h = rgain->height;
    in_offset_x = rgain->offset_x;
    in_offset_y = rgain->offset_y;
    out_w = rgaout->width;
    out_h = rgaout->height;
    out_offset_x = rgaout->offset_x;
    out_offset_y = rgaout->offset_y;
    in_w_stride = rgain->width_stride;
    in_h_stride = rgain->height_stride;
    out_w_stride = rgaout->width_stride;
    out_h_stride = rgaout->height_stride;

    rgain->height = in_h / 2;
    rgain->width_stride = in_w_stride;
    rgain->height_stride = in_h_stride;

    rgaout->height = out_h / 2;
    rgaout->width_stride = out_w_stride;
    rgaout->height_stride  = out_h_stride;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: first split copy/scale failed!", __FUNCTION__);
        return -1;
    }
    if(rgaout->release_fence_fd != -1 ){
        RgaCropScale::WaitFenceDone(rgaout->release_fence_fd);
        rgaout->release_fence_fd = -1;
    }
    ALOGD("@%s: do second split copy/ scale", __FUNCTION__);

    rgain->offset_y = in_offset_y + in_h / 2;
    rgaout->offset_y = out_h_stride / 2;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: second split copy/scale failed!", __FUNCTION__);
        return -1;
    }
    if(rgaout->release_fence_fd != -1 ){
        RgaCropScale::WaitFenceDone(rgaout->release_fence_fd);
        rgaout->release_fence_fd = -1;
    }
    ALOGD("@%s: do split copy/scale end!", __FUNCTION__);
    return 0;
}

/* split buffer to up and down to do crop scale */
int RgaCropScale::WHSplit_CropScaleNV12Or21(struct Params* rgain, struct Params* rgaout) {
                /* test start */
    char *src = rgain->vir_addr;
    char *dst = rgaout->vir_addr;
    int src_fd = rgain->fd;
    int dst_fd = rgaout->fd;
    unsigned int in_w, in_h, in_offset_x, in_offset_y, out_w, out_h, out_offset_x, out_offset_y;
    unsigned int in_w_stride, in_h_stride, out_w_stride, out_h_stride;

    ALOGD("@%s: do split copy/scale start!", __FUNCTION__);

    /* left top */
    ALOGD("@%s: left top split copy/scale start!", __FUNCTION__);

    in_w = rgain->width;
    in_h = rgain->height;
    in_offset_x = rgain->offset_x;
    in_offset_y = rgain->offset_y;
    out_w = rgaout->width;
    out_h = rgaout->height;
    out_offset_x = rgaout->offset_x;
    out_offset_y = rgaout->offset_y;
    in_w_stride = rgain->width_stride;
    in_h_stride = rgain->height_stride;
    out_w_stride = rgaout->width_stride;
    out_h_stride = rgaout->height_stride;

    rgain->width = in_w / 2;
    rgain->height = in_h / 2;
    rgain->width_stride = in_w_stride;
    rgain->height_stride = in_h_stride;


    rgaout->width = out_w / 2;
    rgaout->height = out_h / 2;
    rgaout->width_stride = out_w_stride;
    rgaout->height_stride  = out_h_stride;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: first split copy/scale failed!", __FUNCTION__);
        return -1;
    }
    if(rgaout->release_fence_fd != -1 ){
        RgaCropScale::WaitFenceDone(rgaout->release_fence_fd);
        rgaout->release_fence_fd = -1;
    }
    ALOGD("@%s: do right top split copy/ scale", __FUNCTION__);
    rgain->offset_x = in_offset_x + in_w / 2;
    rgain->offset_y = in_offset_y;

    rgaout->offset_x = out_w_stride / 2;
    rgaout->offset_y = 0;
    rgaout->width = out_w / 2;
    rgaout->height = out_h / 2;
    rgaout->width_stride = out_w_stride;
    rgaout->height_stride  = out_h_stride;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: second split copy/scale failed!", __FUNCTION__);
        return -1;
    }
    if(rgaout->release_fence_fd != -1 ){
        RgaCropScale::WaitFenceDone(rgaout->release_fence_fd);
        rgaout->release_fence_fd = -1;
    }
    ALOGD("@%s: do left bottom split copy/ scale", __FUNCTION__);
    rgain->offset_x = in_offset_x;
    rgain->offset_y = in_offset_y + in_h / 2;

    rgaout->offset_x = 0;
    rgaout->offset_y = out_h_stride / 2;
    rgaout->width = out_w / 2;
    rgaout->height = out_h / 2;
    rgaout->width_stride = out_w_stride;
    rgaout->height_stride  = out_h_stride;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: second split copy/scale failed!", __FUNCTION__);
        return -1;
    }
    if(rgaout->release_fence_fd != -1 ){
        RgaCropScale::WaitFenceDone(rgaout->release_fence_fd);
        rgaout->release_fence_fd = -1;
    }
    ALOGD("@%s: do right bottom split copy/ scale", __FUNCTION__);
    rgain->offset_x = in_offset_x + in_w / 2;
    rgain->offset_y = in_offset_y + in_h / 2;

    rgaout->offset_x = out_w_stride / 2;;
    rgaout->offset_y = out_h_stride / 2;
    rgaout->width = out_w / 2;
    rgaout->height = out_h / 2;
    rgaout->width_stride = out_w_stride;
    rgaout->height_stride  = out_h_stride;

    if (RgaCropScale::CropScaleNV12Or21(rgain, rgaout)) {
        ALOGE("@%s: second split copy/scale failed!", __FUNCTION__);
        return -1;
    }
    if(rgaout->release_fence_fd != -1 ){
        RgaCropScale::WaitFenceDone(rgaout->release_fence_fd);
        rgaout->release_fence_fd = -1;
    }
    ALOGD("@%s: do split copy/scale end!", __FUNCTION__);
    return 0;
}


int RgaCropScale::CropScaleNV12Or21Async(struct Params* in, struct Params* out)
{
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);
    int ret = 0;
#if defined(ANDROID_VERSION_ABOVE_12_X)
    im_rect         src_rect;
    im_rect         dst_rect;
    rga_buffer_t     src;
    rga_buffer_t     dst;

    memset(&src_rect, 0, sizeof(src_rect));
    memset(&dst_rect, 0, sizeof(dst_rect));
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));
    out->release_fence_fd = -1;

    im_handle_param_t param_in,param_out;

    rga_buffer_handle_t src_handle;
    rga_buffer_handle_t dst_handle;
    param_in.width = in->width_stride;
    param_in.height = in->height_stride;
    param_in.format = in->fmt;

    param_out.width = out->width_stride;
    param_out.height = out->height_stride;
    param_out.format = out->fmt;

    if (in->handle != -1)
    {
       src_handle = in->handle;
    } else if (in->fd == -1) {
        src_handle = importbuffer_virtualaddr((void*)in->vir_addr, &param_in);
    } else {
        src_handle = importbuffer_fd(in->fd, &param_in);
    }

    if (out->handle != -1)
    {
       dst_handle = out->handle;
    } else if (out->fd == -1) {
        dst_handle = importbuffer_virtualaddr((void*)out->vir_addr, &param_out);
    } else {
        dst_handle = importbuffer_fd(out->fd, &param_out);
    }

    src = wrapbuffer_handle(src_handle, in->width_stride, in->height_stride, in->fmt);
    dst = wrapbuffer_handle(dst_handle, out->width_stride, out->height_stride, out->fmt);

    if(src.width == 0 || dst.width == 0)
    {
        if (in->handle == -1){
            releasebuffer_handle(src_handle);
        }
        if (out->handle == -1){
            releasebuffer_handle(dst_handle);
        }
        return -1;
    }

    im_opt_t opt;
    rga_buffer_t pat;
    im_rect srect;
    im_rect drect;
    im_rect prect;
    int usage = 0;
    srect.x = in->offset_x;
    srect.y = in->offset_y;
    srect.width = in->width;
    srect.height = in->height;
    drect.x = out->offset_x;
    drect.y = out->offset_y;
    drect.width = out->width;
    drect.height = out->height;
    usage |= IM_SYNC;
    if (in->mirror)
    {
        usage |= IM_HAL_TRANSFORM_FLIP_H;
    }
    if (in->flip)
    {
        usage |= IM_HAL_TRANSFORM_FLIP_V;
    }
#if 0
    if (in->acquire_fence_fd != -1)
    {
       imsync(in->acquire_fence_fd);
    }
#endif

    ret = improcess(src, dst, {}, srect, drect, {}, in->acquire_fence_fd, &out->release_fence_fd, NULL, usage);
    if (ret != IM_STATUS_SUCCESS) {
        ALOGE("%s improcess failed, %s\n", LOG_TAG, imStrError((IM_STATUS)ret));
    }

    if (in->handle == -1){
        releasebuffer_handle(src_handle);
    }
    if (out->handle == -1){
        releasebuffer_handle(dst_handle);
    }
	LOGD("%s improcess ret = %d in.handle:%d out.handle:%d acquire_fence_fd:%d release_fence_fd:%d\n", __FUNCTION__, ret,in->handle,out->handle,in->acquire_fence_fd,out->release_fence_fd);
#endif
    return ret;
}

int RgaCropScale::WaitFenceDone(int in_fence_fd){
    int ret = 0;
    HAL_TRACE_CALL(CAM_GLBL_DBG_HIGH);

    return 0;
#if defined(ANDROID_VERSION_ABOVE_12_X)
    ret = imsync(in_fence_fd);
    if (ret != IM_STATUS_SUCCESS) {
        ALOGE("%s imsync failed, %s\n", LOG_TAG, imStrError((IM_STATUS)ret));
    }
#endif
	LOGD("%s imsync ret = %d in_fence_fd:%d\n", __FUNCTION__, ret,in_fence_fd);

    return ret;
}

int RgaCropScale::getRgaBufferHandle(int cameraId, buffer_handle_t handle, int width, int height, int format){
    //LOGD("%s cameraId = %d, handle = %d, width = %d, height = %d, format = %d\n", __FUNCTION__, cameraId, handle, width, height, format);
    int dmaBufRgaFd = -1;
#if defined(ANDROID_VERSION_ABOVE_12_X)
    std::lock_guard<std::mutex> l(lock_);
    auto context_it = rga_buffer_context_.find(handle);
    if (context_it != rga_buffer_context_.end()) {
        return context_it->second->handle;
    }
    int dmaFd = arc::CameraBufferManager::GetInstance()->GetHandleFd(handle);
    std::unique_ptr<RgaBufferContext> rga_buffer_context(new struct RgaBufferContext);
    im_handle_param_t param;
    param.width = width;
    param.height = height;
    if (V4L2_PIX_FMT_NV12 == format)
    {
        param.format = HAL_PIXEL_FORMAT_YCrCb_NV12;
    }else{
        param.format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
    }
    dmaBufRgaFd = importbuffer_fd(dmaFd, &param);
    rga_buffer_context->cameraId = cameraId;
    rga_buffer_context->dmaFd = dmaFd;
    rga_buffer_context->handle = dmaBufRgaFd;
    rga_buffer_context->width = width;
    rga_buffer_context->height = height;
    rga_buffer_context->format = format;

    rga_buffer_context_[handle] = std::move(rga_buffer_context);
    LOGD("%s import new rga buffer handle = %p dmaFd:%d dmaBufRgaFd:%d, width = %d, height = %d, format = %d\n", __FUNCTION__, handle, dmaFd, dmaBufRgaFd, width, height, format);
#endif
    return dmaBufRgaFd;
}

void RgaCropScale::releaseRgaBufferHandle(int cameraId){
#if defined(ANDROID_VERSION_ABOVE_12_X)
    std::lock_guard<std::mutex> l(lock_);
    std::vector<buffer_handle_t> handles;

    for (const auto& context : rga_buffer_context_) {
        if (context.second->cameraId == cameraId) {
            handles.push_back(context.first);
            LOGD("%s cameraId:%d release rga buffer handle = %p dmaFd = %d rga handle = %d\n", __FUNCTION__, cameraId, context.first, context.second->dmaFd, context.second->handle);
            releasebuffer_handle(context.second->handle);
        }
    }
    for (const auto& handle : handles) {
        rga_buffer_context_.erase(handle);
    }
#endif
}

} /* namespace camera2 */
} /* namespace android */
