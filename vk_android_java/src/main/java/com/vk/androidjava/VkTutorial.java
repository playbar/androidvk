package com.vk.androidjava;

import android.content.res.AssetManager;
import android.util.Log;
import android.view.Surface;

/**
 * Created by Piasy{github.com/Piasy} on 30/06/2017.
 */

public class VkTutorial {

    static {
//        System.loadLibrary("vulkan");
        System.loadLibrary("vktutorial");
    }

    private long mNativeHandle;

    public VkTutorial(AssetManager assetManager, String vertexShader,
                      String fragmentShader) {
        mNativeHandle = create(assetManager, vertexShader, fragmentShader);
    }

    private static native long create(AssetManager assetManager, String vertexShader,
            String fragmentShader);

    private static native void run(long nativeHandle, Surface surface);

    private static native void pause(long nativeHandle);

    private static native void resume(long nativeHandle);

    private static native void surfaceChanged(long nativeHandle);

    private static native void stop(long nativeHandle);

    public void run(final Surface surface) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                Log.e("java run", "tid=" + android.os.Process.myTid());
                VkTutorial.run(mNativeHandle, surface);
            }
        }).start();
    }

    public void pause() {
        pause(mNativeHandle);
    }

    public void resume() {
        resume(mNativeHandle);
    }

    public void surfaceChanged() {
        surfaceChanged(mNativeHandle);
    }

    public void stop() {
        stop(mNativeHandle);
    }
}
