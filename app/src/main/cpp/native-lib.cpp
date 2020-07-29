#include <jni.h>
#include <string>
#include <android/log.h>
#include <libavcodec/jni.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

// version.h
#include <libavcodec/version.h>
#include <libavformat/version.h>
#include <libavutil/version.h>
#include <libavfilter/version.h>
#include <libswresample/version.h>
#include <libswscale/version.h>
}

#define LOG_TAG "native-lib"
#define LOGCATE(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

extern "C" JNIEXPORT jstring JNICALL
Java_com_wyze_ffmpegnative_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}
extern "C"
JNIEXPORT jstring JNICALL
Java_com_wyze_ffmpegnative_MainActivity_GetFFmpegVersion(JNIEnv *env, jobject thiz) {
    // TODO: implement GetFFmpegVersion()
    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "libavcodec : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVCODEC_VERSION));
    strcat(strBuffer, "\nlibavformat : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFORMAT_VERSION));
    strcat(strBuffer, "\nlibavutil : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVUTIL_VERSION));
    strcat(strBuffer, "\nlibavfilter : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFILTER_VERSION));
    strcat(strBuffer, "\nlibswresample : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWRESAMPLE_VERSION));
    strcat(strBuffer, "\nlibswscale : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWSCALE_VERSION));
    strcat(strBuffer, "\navcodec_configure : \n");
    strcat(strBuffer, avcodec_configuration());
    strcat(strBuffer, "\navcodec_license : ");
    strcat(strBuffer, avcodec_license());
    LOGCATE("GetFFmpegVersion\n%s", strBuffer);
    return env->NewStringUTF(strBuffer);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_wyze_ffmpegnative_MainActivity_setSurface(JNIEnv *env, jobject thiz, jobject surface) {
    // TODO: implement setSurface()
    char *file_name = "/sdcard/Download/test.mp4";
    LOGE("NICK %s: %d", __func__, __LINE__);
    ANativeWindow *mANativeWindow = ANativeWindow_fromSurface(env, surface);
    AVFormatContext *pFormatCtx = avformat_alloc_context();
    if(avformat_open_input(&pFormatCtx, file_name, NULL, NULL) != 0) {
        LOGE("Couldn't open file:%s\n", file_name);
        return -1;
    }
    if(avformat_find_stream_info(pFormatCtx, NULL)<0) {
        LOGE("Couldn't find stream information.");
        return -1;
    }
    LOGE("NICK %s: %d", __func__, __LINE__);
    // Find the first video stream
    int videoStream = -1, i;
    for (i = 0; i < pFormatCtx->nb_streams; i++) {
        if (pFormatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
            && videoStream < 0) {
            videoStream = i;
        }
    }
    if(videoStream==-1) {
        LOGE("Didn't find a video stream.");
        return -1; // Didn't find a video stream
    }

    // Find the decoder for the video stream
    AVCodec * pCodec = avcodec_find_decoder(pFormatCtx->streams[videoStream]->codecpar->codec_id);
    if(pCodec==NULL) {
        LOGE("Codec not found.");
        return -1; // Codec not found
    }
    AVCodecContext *pCodecCtx = avcodec_alloc_context3(pCodec);
    if(pCodecCtx) {
        avcodec_parameters_to_context(pCodecCtx, pFormatCtx->streams[videoStream]->codecpar);
    }

    if(avcodec_open2(pCodecCtx, pCodec, NULL) < 0) {
        LOGE("Could not open codec.");
        return -1; // Could not open codec
    }

    ANativeWindow* nativeWindow = ANativeWindow_fromSurface(env, surface);
    int videoWidth = pCodecCtx->width;
    int videoHeight = pCodecCtx->height;
    ANativeWindow_setBuffersGeometry(nativeWindow,  videoWidth, videoHeight, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer windowBuffer;
    LOGE("NICK %s: %d", __func__, __LINE__);


    if(avcodec_open2(pCodecCtx, pCodec, NULL)<0) {
        LOGE("Could not open codec.");
        return -1; // Could not open codec
    }
    AVFrame * pFrame = av_frame_alloc();

    // 用于渲染
    AVFrame * pFrameRGBA = av_frame_alloc();
    if(pFrameRGBA == NULL || pFrame == NULL) {
        LOGE("Could not allocate video frame.");
        return -1;
    }

    // Determine required buffer size and allocate buffer
    int numBytes=av_image_get_buffer_size(AV_PIX_FMT_RGBA, pCodecCtx->width, pCodecCtx->height, 1);
    uint8_t * buffer=(uint8_t *)av_malloc(numBytes*sizeof(uint8_t));
    av_image_fill_arrays(pFrameRGBA->data, pFrameRGBA->linesize, buffer, AV_PIX_FMT_RGBA,
                         pCodecCtx->width, pCodecCtx->height, 1);
    LOGE("NICK %s: %d", __func__, __LINE__);
    // 由于解码出来的帧格式不是RGBA的,在渲染之前需要进行格式转换
    struct SwsContext *sws_ctx = sws_getContext(pCodecCtx->width,
                                                pCodecCtx->height,
                                                pCodecCtx->pix_fmt,
                                                pCodecCtx->width,
                                                pCodecCtx->height,
                                                AV_PIX_FMT_RGBA,
                                                SWS_BILINEAR,
                                                NULL,
                                                NULL,
                                                NULL);

    int frameFinished;
    AVPacket packet;
    LOGE("NICK %s: %d", __func__, __LINE__);
    while(av_read_frame(pFormatCtx, &packet)>=0) {
        int ret = 0;
        if(packet.stream_index==videoStream) {
            ret = avcodec_send_packet(pCodecCtx, &packet);
            if (ret < 0) {
                fprintf(stderr, "Error submitting a packet for decoding (%s)\n", av_err2str(ret));
                return ret;
            }
            while (ret >= 0) {
                ret = avcodec_receive_frame(pCodecCtx, pFrame);
                if (ret < 0) {
                    if (ret == AVERROR_EOF)
                        return 0;
                    else if(ret == AVERROR(EAGAIN))
                        break;

                    fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
                    return ret;
                }

                // write the frame data to output file
                ANativeWindow_lock(nativeWindow, &windowBuffer, 0);

                // 格式转换
                sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                          pFrame->linesize, 0, pCodecCtx->height,
                          pFrameRGBA->data, pFrameRGBA->linesize);

                // 获取stride
                uint8_t * dst = (uint8_t *)windowBuffer.bits;
                int dstStride = windowBuffer.stride * 4;
                uint8_t * src = (uint8_t*) (pFrameRGBA->data[0]);
                int srcStride = pFrameRGBA->linesize[0];
                // 由于window的stride和帧的stride不同,因此需要逐行复制
                int h;
                for (h = 0; h < videoHeight; h++) {
                    memcpy(dst + h * dstStride, src + h * srcStride, srcStride);
                }
                ANativeWindow_unlockAndPost(nativeWindow);
                av_frame_unref(pFrame);
                if (ret < 0)
                    return ret;
            }
        }
        av_packet_unref(&packet);
    }
    av_free(buffer);
    av_free(pFrameRGBA);

    // Free the YUV frame
    av_free(pFrame);

    // Close the codecs
    avcodec_close(pCodecCtx);

    // Close the video file
    avformat_close_input(&pFormatCtx);
    return 0;
}



