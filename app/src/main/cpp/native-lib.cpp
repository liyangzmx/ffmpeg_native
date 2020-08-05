#include <jni.h>
#include <string>
#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>

extern "C" {
#include <libavcodec/jni.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>

// version.h
#include <libavcodec/version.h>
#include <libavformat/version.h>
#include <libavutil/version.h>
#include <libavfilter/version.h>
#include <libswresample/version.h>
#include <libswscale/version.h>
}

#define LOG_TAG "native-lib"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static bool runFlag = true;

void av_log_callback(void*ptr, int level, const char* fmt, va_list vl) {
    va_list vl2;
    char line[1024];
    static int print_prefix = 1;


    va_copy(vl2, vl);
    // av_log_default_callback(ptr, level, fmt, vl);
    av_log_format_line(ptr, level, fmt, vl2, line, sizeof(line), &print_prefix);
    va_end(vl2);
    LOGI("FFMPEG: %s", line);
}

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
    LOGI("GetFFmpegVersion\n%s", strBuffer);
    return env->NewStringUTF(strBuffer);
}

extern "C"
JNIEXPORT jint JNICALL
Java_com_wyze_ffmpegnative_MainActivity_setSurface(JNIEnv *env, jobject thiz, jobject surface) {
    // TODO: implement setSurface()
    char *file_name = "/sdcard/Download/test.mp4";

    av_log_set_level(AV_LOG_DEBUG);
    av_log_set_callback(av_log_callback);
    runFlag = true;
    
    ANativeWindow *mANativeWindow = ANativeWindow_fromSurface(env, surface);
    AVFormatContext *format_ctx = avformat_alloc_context();
    if(avformat_open_input(&format_ctx, file_name, NULL, NULL) != 0) {
        LOGE("Couldn't open file:%s\n", file_name);
        return -1;
    }
    jclass main_activity_class = env->GetObjectClass(thiz);
    jfieldID gpu_image_view_mid = env->GetFieldID(main_activity_class, "gpuImageView", "Ljp/co/cyberagent/android/gpuimage/GPUImageView;");
    jobject gpuimage_object = env->GetObjectField(thiz, gpu_image_view_mid);
    jclass gpuimage_class = env->GetObjectClass(gpuimage_object);
    jmethodID update_preview_frame_mid = env->GetMethodID(gpuimage_class, "updatePreviewFrame", "([BII)V");

    jmethodID create_track_mid = env->GetMethodID(main_activity_class, "createTrack", "(II)V");
    jmethodID play_track_mid = env->GetMethodID(main_activity_class, "playTrack", "([BI)V");
    int out_channel_nb = av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO);
    env->CallVoidMethod(thiz, create_track_mid, 44100, out_channel_nb);

    if(avformat_find_stream_info(format_ctx, NULL) < 0) {
        LOGE("Couldn't find stream information.");
        return -1;
    }
    
    // Find the first video stream
    AVCodec *video_codec, *audio_codec;
    int video_stream_idx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &video_codec, 0);
    if(video_codec == NULL) {
        LOGE("Codec not found.");
        return -1; // Codec not found
    }

    int audio_stream_idx = av_find_best_stream(format_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &audio_codec, 0);
    audio_codec = avcodec_find_decoder_by_name("libfdk_aac");
    if(audio_codec == NULL) {
        LOGE("Codec not found.");
        return -1; // Codec not found
    }

    if(0 > audio_stream_idx) {
        LOGE("Didn't find a video stream.");
        return -1; // Didn't find a video stream
    }
    if(0 > video_stream_idx) {
        LOGE("Didn't find a video stream.");
        return -1; // Didn't find a video stream
    }

    LOGI("Video stream id: %d", video_stream_idx);
    LOGI("Audio stream id: %d", audio_stream_idx);

    AVCodecContext *video_dec_ctx = avcodec_alloc_context3(video_codec);
    if(video_dec_ctx) {
        avcodec_parameters_to_context(video_dec_ctx, format_ctx->streams[video_stream_idx]->codecpar);
    }

    if(avcodec_open2(video_dec_ctx, video_codec, NULL) < 0) {
        LOGE("Could not open video codec.");
        return -1;
    }

    AVCodecContext *audio_dec_ctx = avcodec_alloc_context3(audio_codec);
    if(audio_dec_ctx) {
        avcodec_parameters_to_context(audio_dec_ctx, format_ctx->streams[audio_stream_idx]->codecpar);
    }
    if(avcodec_open2(audio_dec_ctx, audio_codec, NULL) < 0) {
        LOGE("Could not open audio codec.");
        return -1;
    }

    SwrContext *swr_ctx = swr_alloc();
    uint8_t *audio_out_buffer = (uint8_t *)malloc(44100 * 2);
    uint64_t out_ch_layout = AV_CH_LAYOUT_STEREO;
    enum AVSampleFormat out_format = AV_SAMPLE_FMT_S16;
    int out_sample_rate = audio_dec_ctx->sample_rate;
    swr_alloc_set_opts(swr_ctx, out_ch_layout, out_format, out_sample_rate, audio_dec_ctx->channel_layout,
                       audio_dec_ctx->sample_fmt, audio_dec_ctx->sample_rate, 0, NULL);
    swr_init(swr_ctx);

    ANativeWindow* native_window = ANativeWindow_fromSurface(env, surface);
    int videoWidth = video_dec_ctx->width;
    int videoHeight = video_dec_ctx->height;
    ANativeWindow_setBuffersGeometry(native_window, videoWidth, videoHeight, WINDOW_FORMAT_RGBA_8888);
    ANativeWindow_Buffer window_buffer;
    

    AVFrame * video_frame = av_frame_alloc();

    // 用于渲染
    AVFrame * rgba_frame = av_frame_alloc();
    if(rgba_frame == NULL || video_frame == NULL) {
        LOGE("Could not allocate video frame.");
        return -1;
    }

    AVFrame *audio_frame = av_frame_alloc();
    if(audio_frame == NULL) {
        LOGE("Could note allocate audio frame.");
        return -1;
    }

    AVFrame * yuv_frame = av_frame_alloc();
    if(yuv_frame == NULL) {
        LOGE("Could not allocate yuv_frame frame.");
        return -1;
    }

    // Determine required rgba_buffer size and allocate rgba_buffer
    int numBytes=av_image_get_buffer_size(AV_PIX_FMT_RGBA, video_dec_ctx->width, video_dec_ctx->height, 1);
    uint8_t * rgba_buffer=(uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(rgba_frame->data, rgba_frame->linesize, rgba_buffer, AV_PIX_FMT_RGBA,
                         video_dec_ctx->width, video_dec_ctx->height, 1);

    int yuv_numBytes=av_image_get_buffer_size(AV_PIX_FMT_NV21, video_dec_ctx->width, video_dec_ctx->height, 1);
    uint8_t * yuv_buffer=(uint8_t *)av_malloc(yuv_numBytes * sizeof(uint8_t));
    av_image_fill_arrays(yuv_frame->data, yuv_frame->linesize, yuv_buffer, AV_PIX_FMT_NV21,
                         video_dec_ctx->width, video_dec_ctx->height, 1);
    
    // 由于解码出来的帧格式不是RGBA的,在渲染之前需要进行格式转换
    struct SwsContext *sws_rgba_ctx = sws_getContext(video_dec_ctx->width,
                                                     video_dec_ctx->height,
                                                     video_dec_ctx->pix_fmt,
                                                     video_dec_ctx->width,
                                                     video_dec_ctx->height,
                                                     AV_PIX_FMT_RGBA,
                                                     SWS_BILINEAR,
                                                     NULL,
                                                     NULL,
                                                     NULL);

    struct SwsContext *sws_yuv_ctx = sws_getContext(video_dec_ctx->width,
                                                    video_dec_ctx->height,
                                                    video_dec_ctx->pix_fmt,
                                                    video_dec_ctx->width,
                                                    video_dec_ctx->height,
                                                    AV_PIX_FMT_NV21,
                                                    SWS_BILINEAR,
                                                    NULL,
                                                    NULL,
                                                    NULL);
    AVPacket av_packet;

    while((av_read_frame(format_ctx, &av_packet) >= 0) && runFlag) {
        int ret = 0;
        if(av_packet.stream_index == video_stream_idx) {
            ret = avcodec_send_packet(video_dec_ctx, &av_packet);
            if (ret < 0) {
                fprintf(stderr, "Error submitting a av_packet for decoding (%s)\n", av_err2str(ret));
                return ret;
            }
            while (ret >= 0) {
                ret = avcodec_receive_frame(video_dec_ctx, video_frame);
                if (ret < 0) {
                    if (ret == AVERROR_EOF)
                        return 0;
                    else if(ret == AVERROR(EAGAIN))
                        break;

                    fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
                    return ret;
                }

                ANativeWindow_lock(native_window, &window_buffer, 0);
                sws_scale(sws_rgba_ctx, (uint8_t const * const *)video_frame->data,
                          video_frame->linesize, 0, video_frame->height,
                          rgba_frame->data, rgba_frame->linesize);
                uint8_t * dst = (uint8_t *)window_buffer.bits;
                int dst_stride = window_buffer.stride * 4;
                uint8_t * src = (uint8_t*) (rgba_frame->data[0]);
                int src_stride = rgba_frame->linesize[0];
                int h;
                for (h = 0; h < videoHeight; h++) {
                    memcpy(dst + h * dst_stride, src + h * src_stride, src_stride);
                }
                ANativeWindow_unlockAndPost(native_window);

                ret = sws_scale(sws_yuv_ctx, (uint8_t const * const *)video_frame->data,
                                video_frame->linesize, 0, video_frame->height,
                                yuv_frame->data, yuv_frame->linesize);
                if (ret < 0) {
                    return ret;
                }
                av_image_copy_to_buffer(yuv_buffer, yuv_numBytes,
                                        yuv_frame->data,
                                        yuv_frame->linesize, AV_PIX_FMT_NV21,
                                        yuv_frame->width, yuv_frame->height, 1);
                jbyteArray yuv_array = env->NewByteArray(yuv_numBytes);
                env->SetByteArrayRegion(yuv_array, 0, yuv_numBytes,
                                        (const jbyte *)yuv_buffer);
                env->CallVoidMethod(gpuimage_object, update_preview_frame_mid, yuv_array, video_dec_ctx->width, video_dec_ctx->height);
                env->DeleteLocalRef(yuv_array);
                av_frame_unref(video_frame);
            }
        } else if(av_packet.stream_index == audio_stream_idx) {
            ret = avcodec_send_packet(audio_dec_ctx, &av_packet);
            if (ret < 0) {
                fprintf(stderr, "Error submitting a av_packet for audio decoding (%s)\n", av_err2str(ret));
                return ret;
            }
            while (ret >= 0) {
                ret = avcodec_receive_frame(audio_dec_ctx, audio_frame);
                if (ret < 0) {
                    if (ret == AVERROR_EOF)
                        return 0;
                    else if (ret == AVERROR(EAGAIN))
                        break;

                    fprintf(stderr, "Error during decoding (%s)\n", av_err2str(ret));
                    return ret;
                }
                swr_convert(swr_ctx, &audio_out_buffer, 44100 * 2, (const uint8_t **)audio_frame->data, audio_frame->nb_samples);
                int buffer_size = av_samples_get_buffer_size(NULL, out_channel_nb, audio_frame->nb_samples, AV_SAMPLE_FMT_S16, 1);
                jbyteArray audio_sample_array = env->NewByteArray(buffer_size);
                env->SetByteArrayRegion(audio_sample_array, 0, buffer_size, (const jbyte *)audio_out_buffer);
                env->CallVoidMethod(thiz, play_track_mid, audio_sample_array, buffer_size);
                env->DeleteLocalRef(audio_sample_array);
                av_frame_unref(audio_frame);
            }
        }
//        av_packet_unref(&av_packet);
    }
    av_free(rgba_buffer);
    av_free(yuv_buffer);
    av_free(audio_frame);
    av_free(rgba_frame);
    av_free(yuv_frame);
    av_free(video_frame);

    // Close the codecs
    avcodec_close(video_dec_ctx);
    avcodec_close(audio_dec_ctx);

    // Close the video file
    avformat_close_input(&format_ctx);
    return 0;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_wyze_ffmpegnative_MainActivity_stopPlay(JNIEnv *env, jobject thiz) {
    // TODO: implement stopPlay()
    runFlag = false;
}