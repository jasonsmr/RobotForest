package com.robotforest.launcher;

import android.os.Handler;
import android.os.Looper;
import java.io.*;
import java.net.*;
import java.util.concurrent.*;

public final class Net {
    private static final ExecutorService EXEC = Executors.newSingleThreadExecutor();
    private static final Handler MAIN = new Handler(Looper.getMainLooper());

    public interface Callback {
        void onSuccess(byte[] data);
        void onError(Exception e);
    }

    public static void getBytesAsync(String url, int connectMs, int readMs, Callback cb) {
        EXEC.submit(() -> {
            HttpURLConnection c = null;
            try {
                URL u = new URL(url);
                c = (HttpURLConnection) u.openConnection();
                c.setInstanceFollowRedirects(true);
                c.setConnectTimeout(connectMs);
                c.setReadTimeout(readMs);
                c.connect();
                if (c.getResponseCode() != HttpURLConnection.HTTP_OK)
                    throw new IOException("HTTP " + c.getResponseCode());

                try (InputStream in = new BufferedInputStream(c.getInputStream());
                     ByteArrayOutputStream bos = new ByteArrayOutputStream(64 * 1024)) {
                    byte[] buf = new byte[8192];
                    int n;
                    while ((n = in.read(buf)) >= 0) bos.write(buf, 0, n);
                    byte[] out = bos.toByteArray();
                    MAIN.post(() -> cb.onSuccess(out));
                }
            } catch (Exception e) {
                Exception ex = e;
                MAIN.post(() -> cb.onError(ex));
            } finally {
                if (c != null) c.disconnect();
            }
        });
    }
}
