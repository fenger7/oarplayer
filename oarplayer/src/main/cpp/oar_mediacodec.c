//
// Created by gutou on 2017/4/24.
//

#include <unistd.h>
#include <malloc.h>
#include "oar_mediacodec.h"
#include "oarplayer_type_def.h"
#define _JNILOG_TAG "mediacodec"
#include "_android.h"

/*#if __ANDROID_API__ >= NDK_MEDIACODEC_VERSION

int oar_mediacodec_send_packet(oarplayer * oar, OARPacket *packet) {
    oar_mediacodec_context *ctx = oar->mediacodec_ctx;
    if (packet == NULL) { return -2; }
    uint32_t keyframe_flag = 0;
//    av_packet_split_side_data(packet);
    int64_t time_stamp = packet->pts;
    if (packet->isKeyframe) {
        keyframe_flag |= 0x1;
    }
    ssize_t id = AMediaCodec_dequeueInputBuffer(ctx->codec, 1000000);
    media_status_t media_status;
    size_t size;
    if (id >= 0) {
        uint8_t *buf = AMediaCodec_getInputBuffer(ctx->codec, (size_t) id, &size);
        if (buf != NULL && size >= packet->size) {
            memcpy(buf, packet->data, (size_t) packet->size);
            media_status = AMediaCodec_queueInputBuffer(ctx->codec, (size_t) id, 0, (size_t) packet->size,
                                                        (uint64_t) time_stamp,
                                                        keyframe_flag);
            if (media_status != AMEDIA_OK) {
                LOGE("AMediaCodec_queueInputBuffer error. status ==> %d", media_status);
                return (int) media_status;
            }
        }
    }else if(id == AMEDIACODEC_INFO_TRY_AGAIN_LATER){
        return -1;
    }else{
        LOGE("input buffer id < 0  value == %zd", id);
    }
    return 0;
}

void oar_mediacodec_release_buffer(oarplayer * oar, OARFrame *frame) {
    AMediaCodec_releaseOutputBuffer(oar->mediacodec_ctx->codec, (size_t) frame->HW_BUFFER_ID, true);
}

int oar_mediacodec_receive_frame(oarplayer * oar, OARFrame *frame) {
    oar_mediacodec_context *ctx = oar->mediacodec_ctx;
    AMediaCodecBufferInfo info;
    int output_ret = 1;
    ssize_t outbufidx = AMediaCodec_dequeueOutputBuffer(ctx->codec, &info, 0);
    if (outbufidx >= 0) {
            frame->pts = info.presentationTimeUs;
            frame->format = OAR_PIX_FMT_EGL_EXT;
            frame->width = oar->metadata->width;
            frame->height = oar->metadata->height;
            frame->HW_BUFFER_ID = outbufidx;
            output_ret = 0;
    } else {
        switch (outbufidx) {
            case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED: {
                int pix_format = -1;
                AMediaFormat *format = AMediaCodec_getOutputFormat(ctx->codec);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &ctx->width);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &ctx->height);
                AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_COLOR_FORMAT, &pix_format);
                //todo 仅支持了两种格式
                switch (pix_format) {
                    case 19:
                        ctx->pix_format = PIX_FMT_YUV420P;
                        break;
                    case 21:
                        ctx->pix_format = PIX_FMT_NV12;
                        break;
                    default:
                        break;
                }
                output_ret = -2;
                break;
            }
            case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
                break;
            case AMEDIACODEC_INFO_TRY_AGAIN_LATER:
                break;
            default:
                break;
        }

    }
    return output_ret;
}

void oar_mediacodec_flush(oarplayer * oar) {
    oar_mediacodec_context *ctx = oar->mediacodec_ctx;
    AMediaCodec_flush(ctx->codec);
}

oar_mediacodec_context *oar_create_mediacodec_context(oarplayer * oar) {
    oar_mediacodec_context *ctx = (oar_mediacodec_context *) malloc(sizeof(oar_mediacodec_context));
    ctx->width = oar->metadata->width;
    ctx->height = oar->metadata->height;
    ctx->codec_id = oar->metadata->video_codec;
    ctx->nal_size = 0;
    ctx->format = AMediaFormat_new();
    ctx->pix_format = PIX_FMT_NONE;
//    "video/x-vnd.on2.vp8" - VP8 video (i.e. video in .webm)
//    "video/x-vnd.on2.vp9" - VP9 video (i.e. video in .webm)
//    "video/avc" - H.264/AVC video
//    "video/hevc" - H.265/HEVC video
//    "video/mp4v-es" - MPEG4 video
//    "video/3gpp" - H.263 video
    switch (ctx->codec_id) {
        case VIDEO_CODEC_AVC:
            ctx->codec = AMediaCodec_createDecoderByType("video/avc");
            AMediaFormat_setString(ctx->format, AMEDIAFORMAT_KEY_MIME, "video/avc");
            AMediaFormat_setBuffer(ctx->format, "csd-0", oar->metadata->video_sps, oar->metadata->video_sps_size);
            AMediaFormat_setBuffer(ctx->format, "csd-1", oar->metadata->video_pps, oar->metadata->video_pps_size);
            break;

        case VIDEO_CODEC_H263:
            ctx->codec = AMediaCodec_createDecoderByType("video/3gpp");
            AMediaFormat_setString(ctx->format, AMEDIAFORMAT_KEY_MIME, "video/3gpp");
            AMediaFormat_setBuffer(ctx->format, "csd-0", oar->metadata->video_sps, oar->metadata->video_sps_size);//TODO 获取H263 sps
            break;
        default:
            break;
    }
//    AMediaFormat_setInt32(ctx->format, "rotation-degrees", 90);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_WIDTH, ctx->width);
    AMediaFormat_setInt32(ctx->format, AMEDIAFORMAT_KEY_HEIGHT, ctx->height);
//    media_status_t ret = AMediaCodec_configure(ctx->codec, ctx->format, NULL, NULL, 0);

    return ctx;
}

void oar_mediacodec_start(oarplayer * oar){
    oar_mediacodec_context *ctx = oar->mediacodec_ctx;
    while(oar->video_render_ctx->texture_window == NULL){
        usleep(10000);
    }
    media_status_t ret = AMediaCodec_configure(ctx->codec, ctx->format, oar->video_render_ctx->texture_window, NULL, 0);
    if (ret != AMEDIA_OK) {
        LOGE("open mediacodec failed \n");
    }
    ret = AMediaCodec_start(ctx->codec);
    if (ret != AMEDIA_OK) {
        LOGE("open mediacodec failed \n");
    }
}

void oar_mediacodec_stop(oarplayer * oar){
    AMediaCodec_stop(oar->mediacodec_ctx->codec);
}

void oar_mediacodec_release_context(oarplayer * oar){
    oar_mediacodec_context *ctx = oar->mediacodec_ctx;
    AMediaCodec_delete(ctx->codec);
    AMediaFormat_delete(ctx->format);
    free(ctx);
    oar->mediacodec_ctx = NULL;
}
#else*/


static int get_int(uint8_t *buf) {
    return (buf[0] << 24) + (buf[1] << 16) + (buf[2] << 8) + buf[3];
}

static int64_t get_long(uint8_t *buf) {
    return (((int64_t) buf[0]) << 56)
           + (((int64_t) buf[1]) << 48)
           + (((int64_t) buf[2]) << 40)
           + (((int64_t) buf[3]) << 32)
           + (((int64_t) buf[4]) << 24)
           + (((int64_t) buf[5]) << 16)
           + (((int64_t) buf[6]) << 8)
           + ((int64_t) buf[7]);
}

oar_mediacodec_context *oar_create_mediacodec_context(
        oarplayer *oar) {
    oar_mediacodec_context *ctx = (oar_mediacodec_context *) malloc(sizeof(oar_mediacodec_context));
    ctx->width = oar->metadata->width;
    ctx->height = oar->metadata->height;
    ctx->codec_id = oar->metadata->video_codec;
    ctx->nal_size = 0;
    ctx->pix_format = PIX_FMT_NONE;
    return ctx;
}

void oar_mediacodec_start(oarplayer *oar){
    LOGE("oar_mediacodec_start...");
    oar_mediacodec_context *ctx = oar->mediacodec_ctx;
    JNIEnv *jniEnv = ctx->jniEnv;
    oar_java_class * jc = oar->jc;
    jobject codecName = NULL, csd_0 = NULL, csd_1 = NULL;
    while(oar->video_render_ctx->texture_window == NULL){
        usleep(10000);
    }
    switch (ctx->codec_id) {
        case VIDEO_CODEC_AVC:
            codecName = (*jniEnv)->NewStringUTF(jniEnv, "video/avc");
            if (oar->metadata->video_extradata) {
                size_t sps_size, pps_size;
                uint8_t *sps_buf;
                uint8_t *pps_buf;
                sps_buf = (uint8_t *) malloc((size_t) oar->metadata->video_extradata_size + 20);
                pps_buf = (uint8_t *) malloc((size_t) oar->metadata->video_extradata_size + 20);
                if (0 != convert_sps_pps2(oar->metadata->video_extradata, (size_t) oar->metadata->video_extradata_size,
                                          sps_buf, &sps_size, pps_buf, &pps_size, &ctx->nal_size)) {
                    LOGE("%s:convert_sps_pps: failed\n", __func__);
                }
                LOGE("extradata_size = %d, sps_size = %d, pps_size = %d, nal_size = %d",
                     (size_t) oar->metadata->video_extradata_size, sps_size, pps_size, ctx->nal_size);

                csd_0 = (*jniEnv)->NewDirectByteBuffer(jniEnv, sps_buf, sps_size);
                csd_1 = (*jniEnv)->NewDirectByteBuffer(jniEnv, pps_buf, pps_size);
                (*jniEnv)->CallStaticVoidMethod(jniEnv, jc->HwDecodeBridge, jc->codec_init,
                                                codecName, ctx->width, ctx->height, csd_0, csd_1);
                (*jniEnv)->DeleteLocalRef(jniEnv, csd_0);
                (*jniEnv)->DeleteLocalRef(jniEnv, csd_1);
            } else {
                (*jniEnv)->CallStaticVoidMethod(jniEnv, jc->HwDecodeBridge, jc->codec_init,
                                                codecName, ctx->width, ctx->height, NULL, NULL);
            }
            break;

        case VIDEO_CODEC_H263:
            codecName = (*jniEnv)->NewStringUTF(jniEnv, "video/3gpp");
            //TODO 解析h263格式
            /*csd_0 = (*jniEnv)->NewDirectByteBuffer(jniEnv, oar->metadata->video_sps, oar->metadata->video_sps_size);
            (*jniEnv)->CallStaticVoidMethod(jniEnv, jc->HwDecodeBridge, jc->codec_init, codecName,
                                            ctx->width, ctx->height, csd_0, NULL);
            (*jniEnv)->DeleteLocalRef(jniEnv, csd_0);*/
            break;
        default:
            break;
    }
    if (codecName != NULL) {
        (*jniEnv)->DeleteLocalRef(jniEnv, codecName);
    }
}

void oar_mediacodec_release_buffer(oarplayer *pd, OARFrame *frame) {
    JNIEnv *jniEnv = pd->video_render_ctx->jniEnv;
    oar_java_class * jc = pd->jc;
    (*jniEnv)->CallStaticVoidMethod(jniEnv, jc->HwDecodeBridge, jc->codec_releaseOutPutBuffer,
                                    (int)frame->HW_BUFFER_ID);
}

int oar_mediacodec_receive_frame(oarplayer *oar, OARFrame *frame) {
    JNIEnv *jniEnv = oar->mediacodec_ctx->jniEnv;
    oar_java_class * jc = oar->jc;
    oar_mediacodec_context *ctx = oar->mediacodec_ctx;
    int output_ret = 1;
    jobject deqret = (*jniEnv)->CallStaticObjectMethod(jniEnv, jc->HwDecodeBridge,
                                                       jc->codec_dequeueOutputBufferIndex,
                                                       (jlong) 0);
    uint8_t *retbuf = (*jniEnv)->GetDirectBufferAddress(jniEnv, deqret);
    int outbufidx = get_int(retbuf);
    int64_t pts = get_long(retbuf + 8);
    (*jniEnv)->DeleteLocalRef(jniEnv, deqret);
//    LOGE("outbufidx:%d" , outbufidx);
    if (outbufidx >= 0) {
        frame->pts = pts;
        frame->format = PIX_FMT_EGL_EXT;
        frame->width = oar->metadata->width;
        frame->height = oar->metadata->height;
        frame->HW_BUFFER_ID = outbufidx;
        frame->next=NULL;
        output_ret = 0;
    } else {
        switch (outbufidx) {
            // AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED
            case -2: {
                jobject newFormat = (*jniEnv)->CallStaticObjectMethod(jniEnv, jc->HwDecodeBridge,
                                                                      jc->codec_formatChange);
                uint8_t *fmtbuf = (*jniEnv)->GetDirectBufferAddress(jniEnv, newFormat);
                ctx->width = get_int(fmtbuf);
                ctx->height = get_int(fmtbuf + 4);
                int pix_format = get_int(fmtbuf + 8);
                (*jniEnv)->DeleteLocalRef(jniEnv, newFormat);

                //todo 仅支持了两种格式
                switch (pix_format) {
                    case 19:
                        ctx->pix_format = PIX_FMT_YUV420P;
                        break;
                    case 21:
                        ctx->pix_format = PIX_FMT_NV12;
                        break;
                    default:
                        break;
                }
                output_ret = -2;
                break;
            }
            // AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED
            case -3:
                break;
            // AMEDIACODEC_INFO_TRY_AGAIN_LATER
            case -1:
                break;
            default:
                break;
        }

    }
    return output_ret;
}

int oar_mediacodec_send_packet(oarplayer *oar, OARPacket *packet) {
    JNIEnv *jniEnv = oar->mediacodec_ctx->jniEnv;
    oar_java_class * jc = oar->jc;
    oar_mediacodec_context *ctx = oar->mediacodec_ctx;
    if (packet == NULL) { return -2; }
    int keyframe_flag = 0;
    int64_t time_stamp = packet->pts;

    if (ctx->codec_id == VIDEO_CODEC_AVC) {
        H264ConvertState convert_state = {0, 0};
        convert_h264_to_annexb(packet->data, packet->size, ctx->nal_size, &convert_state);
    }
    if (packet->isKeyframe) {
        keyframe_flag |= 0x1;
    }
    int id = (*jniEnv)->CallStaticIntMethod(jniEnv, jc->HwDecodeBridge,
                                            jc->codec_dequeueInputBuffer, (jlong) 1000000);
    if (id >= 0) {
        jobject inputBuffer = (*jniEnv)->CallStaticObjectMethod(jniEnv, jc->HwDecodeBridge,
                                                                jc->codec_getInputBuffer, id);
        uint8_t *buf = (*jniEnv)->GetDirectBufferAddress(jniEnv, inputBuffer);
        jlong size = (*jniEnv)->GetDirectBufferCapacity(jniEnv, inputBuffer);
        if (buf != NULL && size >= packet->size) {
            memcpy(buf, packet->data, (size_t) packet->size);
            (*jniEnv)->CallStaticVoidMethod(jniEnv, jc->HwDecodeBridge,
                                            jc->codec_queueInputBuffer,
                                            (jint) id, (jint) packet->size,
                                            (jlong) time_stamp, (jint) keyframe_flag);
        }
        (*jniEnv)->DeleteLocalRef(jniEnv, inputBuffer);
    } else if (id == -1) {
        return -1;
    } else {
        LOGE("input buffer id < 0  value == %zd", id);
    }
    return 0;
}

void oar_mediacodec_flush(oarplayer *oar) {
    JNIEnv *jniEnv = oar->mediacodec_ctx->jniEnv;
    oar_java_class * jc = oar->jc;
    (*jniEnv)->CallStaticVoidMethod(jniEnv, jc->HwDecodeBridge, jc->codec_flush);
}

void oar_mediacodec_release_context(oarplayer *oar) {
    JNIEnv *jniEnv = oar->jniEnv;
    oar_java_class * jc = oar->jc;
    (*jniEnv)->CallStaticVoidMethod(jniEnv, jc->HwDecodeBridge, jc->codec_release);
    oar_mediacodec_context *ctx = oar->mediacodec_ctx;
    free(ctx);
    oar->mediacodec_ctx = NULL;
}

void oar_mediacodec_stop(oarplayer *oar) {
    JNIEnv *jniEnv = oar->mediacodec_ctx->jniEnv;
    oar_java_class * jc = oar->jc;
    (*jniEnv)->CallStaticVoidMethod(jniEnv, jc->HwDecodeBridge, jc->codec_stop);
}

//#endif