package com.wyze.ffmpegnative;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.app.ActivityCompat;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.graphics.Rect;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.CaptureResult;
import android.hardware.camera2.params.OutputConfiguration;
import android.hardware.camera2.params.SessionConfiguration;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.media.Image;
import android.media.ImageReader;
import android.opengl.GLSurfaceView;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.TextView;

import java.lang.reflect.Array;
import java.nio.ByteBuffer;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import jp.co.cyberagent.android.gpuimage.GPUImage;
import jp.co.cyberagent.android.gpuimage.GPUImageView;
import jp.co.cyberagent.android.gpuimage.filter.GPUImage3x3TextureSamplingFilter;
import jp.co.cyberagent.android.gpuimage.filter.GPUImageColorInvertFilter;
import jp.co.cyberagent.android.gpuimage.filter.GPUImageContrastFilter;
import jp.co.cyberagent.android.gpuimage.filter.GPUImageGrayscaleFilter;
import jp.co.cyberagent.android.gpuimage.filter.GPUImageSketchFilter;
import jp.co.cyberagent.android.gpuimage.filter.GPUImageSmoothToonFilter;
import jp.co.cyberagent.android.gpuimage.util.Rotation;

public class MainActivity extends AppCompatActivity {

    private static boolean VERBOSE = false;
    private static String TAG= "NICK: ";

    // Used to load the 'native-lib' library on application startup.

    static {
        System.loadLibrary("fdk-aac");
        System.loadLibrary("x264");
        System.loadLibrary("avutil");
        System.loadLibrary("swresample");
        System.loadLibrary("avformat");
        System.loadLibrary("avcodec");
        System.loadLibrary("swscale");
        System.loadLibrary("avfilter");
        System.loadLibrary("avresample");
        System.loadLibrary("avdevice");
        System.loadLibrary("native-lib");
    }
    HandlerThread mHandlerThread = new HandlerThread("Task");
    Handler mBackHandler;
    GPUImage gpuImage;
    GLSurfaceView glSurfaceView;
    GPUImageView gpuImageView;
    CameraDevice mCameraDevice;
    String cameraId = null;
    ImageReader imageReader;
    CaptureRequest.Builder requestBuilder;
    CameraManager cameraManager;
    private static final int COLOR_FormatI420 = 1;
    private static final int COLOR_FormatNV21 = 2;

    private static boolean isImageFormatSupported(Image image) {
        int format = image.getFormat();
        switch (format) {
            case ImageFormat.YUV_420_888:
            case ImageFormat.NV21:
            case ImageFormat.YV12:
                return true;
        }
        return false;
    }

    private static byte[] getDataFromImage(Image image, int colorFormat) {
        if (colorFormat != COLOR_FormatI420 && colorFormat != COLOR_FormatNV21) {
            throw new IllegalArgumentException("only support COLOR_FormatI420 " + "and COLOR_FormatNV21");
        }
        if (!isImageFormatSupported(image)) {
            throw new RuntimeException("can't convert Image to byte array, format " + image.getFormat());
        }
        Rect crop = image.getCropRect();
        int format = image.getFormat();
        int width = crop.width();
        int height = crop.height();
        Image.Plane[] planes = image.getPlanes();
        byte[] data = new byte[width * height * ImageFormat.getBitsPerPixel(format) / 8];
        byte[] rowData = new byte[planes[0].getRowStride()];
        if (VERBOSE) Log.v(TAG, "get data from " + planes.length + " planes");
        int channelOffset = 0;
        int outputStride = 1;
        for (int i = 0; i < planes.length; i++) {
            switch (i) {
                case 0:
                    channelOffset = 0;
                    outputStride = 1;
                    break;
                case 1:
                    if (colorFormat == COLOR_FormatI420) {
                        channelOffset = width * height;
                        outputStride = 1;
                    } else if (colorFormat == COLOR_FormatNV21) {
                        channelOffset = width * height + 1;
                        outputStride = 2;
                    }
                    break;
                case 2:
                    if (colorFormat == COLOR_FormatI420) {
                        channelOffset = (int) (width * height * 1.25);
                        outputStride = 1;
                    } else if (colorFormat == COLOR_FormatNV21) {
                        channelOffset = width * height;
                        outputStride = 2;
                    }
                    break;
            }
            ByteBuffer buffer = planes[i].getBuffer();
            int rowStride = planes[i].getRowStride();
            int pixelStride = planes[i].getPixelStride();
            if (VERBOSE) {
                Log.v(TAG, "pixelStride " + pixelStride);
                Log.v(TAG, "rowStride " + rowStride);
                Log.v(TAG, "width " + width);
                Log.v(TAG, "height " + height);
                Log.v(TAG, "buffer size " + buffer.remaining());
            }
            int shift = (i == 0) ? 0 : 1;
            int w = width >> shift;
            int h = height >> shift;
            buffer.position(rowStride * (crop.top >> shift) + pixelStride * (crop.left >> shift));
            for (int row = 0; row < h; row++) {
                int length;
                if (pixelStride == 1 && outputStride == 1) {
                    length = w;
                    buffer.get(data, channelOffset, length);
                    channelOffset += length;
                } else {
                    length = (w - 1) * pixelStride + 1;
                    buffer.get(rowData, 0, length);
                    for (int col = 0; col < w; col++) {
                        data[channelOffset] = rowData[col * pixelStride];
                        channelOffset += outputStride;
                    }
                }
                if (row < h - 1) {
                    buffer.position(buffer.position() + rowStride - length);
                }
            }
            if (VERBOSE) Log.v(TAG, "Finished reading data from plane " + i);
        }
        return data;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        cameraManager = (CameraManager) getSystemService(Context.CAMERA_SERVICE);
        setContentView(R.layout.activity_main);
        mHandlerThread.start();
        mBackHandler = new Handler(mHandlerThread.getLooper());

        gpuImage = new GPUImage(this);
//        glSurfaceView = findViewById(R.id.gpu_view);
        gpuImageView = findViewById(R.id.gpu_view);
        gpuImageView.setFilter(new GPUImageColorInvertFilter());
        gpuImageView.setRotation(Rotation.NORMAL);
        gpuImageView.setRenderMode(GPUImageView.RENDERMODE_CONTINUOUSLY);

//        gpuImage.setFilter(new GPUImageSketchFilter());
//        gpuImage.setGLSurfaceView(glSurfaceView);
        int img_width = 1920;
        int img_height = 1080;

//        imageReader = ImageReader.newInstance(img_width, img_height, ImageFormat.YUV_420_888, 5);
//        imageReader.setOnImageAvailableListener(new ImageReader.OnImageAvailableListener() {
//            @Override
//            public void onImageAvailable(ImageReader imageReader) {
//                if(imageReader != null){
//                    Image image = imageReader.acquireNextImage();
//                    if(image != null) {
////                        gpuImage.updatePreviewFrame(getDataFromImage(image, COLOR_FormatNV21), image.getWidth(), image.getHeight());
//                        gpuImageView.updatePreviewFrame(getDataFromImage(image, COLOR_FormatNV21), image.getWidth(), image.getHeight());
//                        image.close();
//                    }
//                }
//            }
//        }, null);
//
////        final Surface previewSurfaec = surfaceHolder.getSurface();
//        try {
//            String[] cameraList = cameraManager.getCameraIdList();
//            for (String camera : cameraList) {
//                CameraCharacteristics cameraCharacteristics = cameraManager.getCameraCharacteristics(camera);
//                Integer cameraFace = cameraCharacteristics.get(CameraCharacteristics.LENS_FACING);
//                if (cameraFace == null) {
//                    return;
//                }
//                if (cameraFace == CameraCharacteristics.LENS_FACING_FRONT) {
//                    cameraId = camera;
//                }
//            }
//        } catch (CameraAccessException e) {
//            e.printStackTrace();
//        }
//        SurfaceView tv = findViewById(R.id.sample_text);
//        SurfaceHolder surfaceHolder = tv.getHolder();
//        surfaceHolder.addCallback(new SurfaceHolder.Callback() {
//            @Override
//            public void surfaceCreated(@NonNull final SurfaceHolder surfaceHolder) {
//                final Surface surface = surfaceHolder.getSurface();
//                try {
//                    cameraManager.openCamera(cameraId,
//                            new CameraDevice.StateCallback() {
//                                @Override
//                                public void onOpened(@NonNull CameraDevice cameraDevice) {
//                                    mCameraDevice = cameraDevice;
//                                    try {
//                                        requestBuilder = mCameraDevice.createCaptureRequest(CameraDevice.TEMPLATE_PREVIEW);
//                                        requestBuilder.addTarget(surfaceHolder.getSurface());
//                                        requestBuilder.addTarget(imageReader.getSurface());
//                                        mCameraDevice.createCaptureSession(Arrays.asList(surfaceHolder.getSurface(), imageReader.getSurface()),
//                                                new CameraCaptureSession.StateCallback() {
//                                            @Override
//                                            public void onConfigured(@NonNull CameraCaptureSession cameraCaptureSession) {
//                                                try {
//                                                    cameraCaptureSession.setRepeatingRequest(requestBuilder.build(), null
//                                                            , null);
//                                                } catch (CameraAccessException e) {
//                                                    e.printStackTrace();
//                                                }
//                                            }
//
//                                            @Override
//                                            public void onConfigureFailed(@NonNull CameraCaptureSession cameraCaptureSession) {
//
//                                            }
//                                        }, null);
//                                    } catch (CameraAccessException e) {
//                                        e.printStackTrace();
//                                    }
//                                }
//
//                                @Override
//                                public void onDisconnected(@NonNull CameraDevice cameraDevice) {
//
//                                }
//
//                                @Override
//                                public void onError(@NonNull CameraDevice cameraDevice, int i) {
//
//                                }
//                            }, mBackHandler);
//                } catch (CameraAccessException e) {
//                    e.printStackTrace();
//                }
//            }
//
//            @Override
//            public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int i, int i1, int i2) {
//
//            }
//
//            @Override
//            public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {
//
//            }
//        });

        SurfaceView tv = findViewById(R.id.sample_text);
        SurfaceHolder surfaceHolder = tv.getHolder();
        surfaceHolder.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull final SurfaceHolder surfaceHolder) {
                new AsyncTask<Void, Integer, Integer>() {
                    @Override
                    protected Integer doInBackground(Void... voids) {
                        setSurface(surfaceHolder.getSurface());
                        return 0;
                    }
                }.execute();
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int i, int i1, int i2) {

            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {

            }
        });
    }

    private AudioTrack mAudioTrack;

    public void createTrack(int sampleRateInHz, int nb_channels) {
        int channelConfig;
        if(nb_channels == 1) {
            channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        } else if(nb_channels == 2) {
            channelConfig = AudioFormat.CHANNEL_OUT_STEREO;
        } else {
            channelConfig = AudioFormat.CHANNEL_OUT_MONO;
        }
        int bufferSize = AudioTrack.getMinBufferSize(sampleRateInHz, channelConfig, AudioFormat.ENCODING_PCM_16BIT);
        mAudioTrack = new AudioTrack(AudioManager.STREAM_MUSIC, sampleRateInHz, channelConfig,
                AudioFormat.ENCODING_PCM_16BIT, bufferSize, AudioTrack.MODE_STREAM);
        mAudioTrack.play();
    }

    public void playTrack(byte[] buffer, int lenght) {
        if(mAudioTrack != null) {
            mAudioTrack.write(buffer, 0, lenght);
        }
    }

    @Override
    protected void onStop() {
        super.onStop();
        stopPlay();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
    public native String GetFFmpegVersion();
    public native int setSurface(Surface surface);
    public native void stopPlay();
}
