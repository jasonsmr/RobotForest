package com.robotforest.launcher;

import android.os.Handler;
import android.os.Looper;
import android.util.Log;

import java.io.BufferedInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.InputStream;
import java.io.IOException;
import java.util.List;
import java.util.Map;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class Exec {
    private static final String TAG = "RF.Exec";

    private static android.content.Context currentAppContext() {
        try {
            Class<?> at = Class.forName("android.app.ActivityThread");
            Object app = at.getMethod("currentApplication").invoke(null);
            if (app instanceof android.content.Context) return (android.content.Context) app;
        } catch (Throwable ignored) {}
        return null;
    }

    public static String nativeBox64Path() {
        try {
            android.content.Context ctx = currentAppContext();
            if (ctx == null) return null;
            String nld = ctx.getApplicationInfo().nativeLibraryDir;
            if (nld == null) return null;
            File f = new File(nld, "libbox64.so");
            if (f.exists() && f.isFile()) {
                try { if (!f.canExecute()) f.setExecutable(true, false); } catch (Throwable ignored) {}
                if (f.canExecute()) return f.getAbsolutePath();
            }
        } catch (Throwable ignored) {}
        return null;
    }

    private static File preferNativeLibDir(String toolName) {
        boolean isBox = false;
        String candidate = null;
        try {
            if (toolName != null) {
                if ("box64".equals(toolName) || toolName.endsWith("/box64") || "libbox64.so".equals(toolName)) {
                    isBox = true; candidate = "libbox64.so";
                } else if ("box86".equals(toolName) || toolName.endsWith("/box86") || "libbox86.so".equals(toolName)) {
                    isBox = true; candidate = "libbox86.so";
                }
            }
        } catch (Throwable ignored) {}
        if (!isBox) return null;

        try {
            android.content.Context ctx = currentAppContext();
            if (ctx == null) return null;
            String nld = ctx.getApplicationInfo().nativeLibraryDir;
            if (nld == null) return null;
            File f = new File(nld, candidate);
            boolean ok = (f.exists() && f.isFile());
            Log.i(TAG, "nativeBox path probe for " + toolName + ": " + f.getAbsolutePath() + " (exists=" + ok + ")");
            if (ok) {
                try { if (!f.canExecute()) f.setExecutable(true, false); } catch (Throwable ignored) {}
                if (f.canExecute()) return f;
            }
        } catch (Throwable ignored) {}
        return null;
    }

    public interface Callback {
        void onCompleted(int exitCode, String stdout, String stderr);
        void onError(Exception e);
    }

    private static final ExecutorService EXEC = Executors.newCachedThreadPool();
    private static final Handler MAIN = new Handler(Looper.getMainLooper());

    public static void runAsync(
            List<String> cmd,
            File workDir,
            Map<String, String> extraEnv,
            Callback cb
    ) {
        EXEC.submit(() -> {
            Process p = null;
            try {
                if (cmd != null && !cmd.isEmpty()) {
                    String argv0 = cmd.get(0);
                    File nldTool = preferNativeLibDir(argv0);
                    if (nldTool != null) {
                        cmd.set(0, nldTool.getAbsolutePath());
                        Log.i(TAG, "redirected to nativeLibraryDir: " + cmd.get(0));
                    } else {
                        Log.i(TAG, "no redirect; argv0=" + argv0);
                    }
                }

                ProcessBuilder pb = new ProcessBuilder(cmd);
                if (workDir != null) pb.directory(workDir);
                pb.redirectErrorStream(false);
                Map<String, String> env = pb.environment();
                if (extraEnv != null) env.putAll(extraEnv);
                p = pb.start();

                String stdout = slurp(p.getInputStream());
                String stderr = slurp(p.getErrorStream());
                int code = p.waitFor();

                final int fcode = code;
                final String fOut = stdout;
                final String fErr = stderr;
                MAIN.post(() -> cb.onCompleted(fcode, fOut, fErr));
            } catch (Exception e) {
                MAIN.post(() -> cb.onError(e));
            } finally {
                if (p != null) p.destroy();
            }
        });
    }

    private static String slurp(InputStream in) throws IOException {
        try (BufferedInputStream bin = new BufferedInputStream(in);
             ByteArrayOutputStream bos = new ByteArrayOutputStream()) {
            byte[] buf = new byte[8192];
            int n;
            while ((n = bin.read(buf)) >= 0) bos.write(buf, 0, n);
            return bos.toString("UTF-8");
        }
    }
}
