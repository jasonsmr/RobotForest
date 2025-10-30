package com.robotforest.launcher;

import android.app.Application;
import android.util.Log;

public final class RobotForestApp extends Application {
    @Override public void onCreate() {
        super.onCreate();
        Thread.setDefaultUncaughtExceptionHandler((t, e) -> {
            Log.e("AndroidRuntime", "Uncaught in " + t.getName(), e);
            // Let system default handler still crash the app:
            System.exit(1);
        });
    }
}
