package com.wyze.ffmpegnative;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AppCompatActivity;

import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.TextView;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
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

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        mHandlerThread.start();
        mBackHandler = new Handler(mHandlerThread.getLooper());

        // Example of a call to a native method
        SurfaceView tv = findViewById(R.id.sample_text);
        SurfaceHolder surfaceHolder = tv.getHolder();
        surfaceHolder.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder surfaceHolder) {
                final Surface surface = surfaceHolder.getSurface();
                mBackHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        setSurface(surface);
                    }
                });
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder surfaceHolder, int i, int i1, int i2) {

            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder surfaceHolder) {

            }
        });
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
    public native String GetFFmpegVersion();
    public native int setSurface(Surface surface);
}
