package com.robotforest.launcher;

import android.os.Handler;
import android.os.Looper;

import java.io.*;
import java.util.*;
import java.util.concurrent.*;

public final class Exec {
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
