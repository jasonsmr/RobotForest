package com.robotforest.launcher;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.system.Os;
import android.system.OsConstants;
import android.util.Log;

import java.io.*;
import java.security.MessageDigest;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public final class RuntimeInstaller {
    private static final String TAG = "runtime";
    private static final Handler MAIN = new Handler(Looper.getMainLooper());

    public interface Listener {
        void onReady(File installDir);
        void onProgress(String msg);
        void onError(Exception e);
    }

    public static File getInstallDir(Context ctx, String subdir) {
        File base = ctx.getDir("runtime", Context.MODE_PRIVATE);
        return new File(base, subdir);
    }

    public static void ensureInstalled(Context ctx, Listener cb) {
        installInternal(ctx, cb, /*force*/ false);
    }

    public static void forceReinstall(Context ctx, Listener cb) {
        installInternal(ctx, cb, /*force*/ true);
    }

    private static void installInternal(Context ctx, Listener cb, boolean force) {
        try {
            RuntimeManifest mf = RuntimeManifest.fromAssets(ctx);
            File install = getInstallDir(ctx, mf.subdir);
            File stamp = new File(install, ".sha256");

            if (!force && install.isDirectory() && stamp.isFile()) {
                String existing = readAll(stamp).trim().toLowerCase();
                if (!existing.isEmpty() && !existing.equals("auto") && existing.equals(mf.sha256)) {
                    post(cb, () -> cb.onReady(install));
                    return;
                }
            }

            post(cb, () -> cb.onProgress("[runtime] downloading…"));
            Net.getBytesAsync(mf.url, 8000, 30000, new Net.Callback() {
                @Override public void onSuccess(byte[] data) {
                    try {
                        post(cb, () -> cb.onProgress("[runtime] verifying…"));
                        String got = sha256Hex(data);
                        String expected = (mf.sha256 == null) ? "" : mf.sha256.trim().toLowerCase();
                        if (!expected.isEmpty() && !expected.equals("auto") && !got.equals(expected)) {
                            throw new IOException("sha256 mismatch expected=" + expected + " got=" + got);
                        }

                        post(cb, () -> cb.onProgress("[runtime] unpacking…"));
                        unzipTo(data, install);

                        // Write stamp
                        if (!install.exists() && !install.mkdirs())
                            throw new IOException("mkdirs failed: " + install);
                        try (FileWriter w = new FileWriter(stamp, false)) {
                            w.write(got);
                            w.write("\n");
                        }

                        // **Critical**: ensure exec bits on dirs and bin/*
                        post(cb, () -> cb.onProgress("[runtime] fixing permissions…"));
                        fixExecBitsRecursive(install);
                        File bin = new File(install, "bin");
                        if (bin.isDirectory()) {
                            File[] xs = bin.listFiles();
                            if (xs != null) {
                                for (File f : xs) {
                                    try { Os.chmod(f.getAbsolutePath(), 0755); } catch (Throwable t) { Log.w(TAG, "chmod exec", t); }
                                }
                            }
                        }

                        post(cb, () -> cb.onReady(install));
                    } catch (Exception e) {
                        Log.e(TAG, "install failed", e);
                        post(cb, () -> cb.onError(e));
                    }
                }
                @Override public void onError(Exception e) {
                    Log.e(TAG, "download failed", e);
                    post(cb, () -> cb.onError(e));
                }
            });

        } catch (Exception e) {
            Log.e(TAG, "manifest/init failed", e);
            post(cb, () -> cb.onError(e));
        }
    }

    private static void unzipTo(byte[] zipBytes, File destDir) throws IOException {
        if (destDir.exists()) deleteRec(destDir);
        if (!destDir.mkdirs() && !destDir.isDirectory())
            throw new IOException("mkdirs failed: " + destDir);

        try (ByteArrayInputStream bin = new ByteArrayInputStream(zipBytes);
             ZipInputStream zin = new ZipInputStream(bin)) {
            ZipEntry e;
            byte[] buf = new byte[8192];
            while ((e = zin.getNextEntry()) != null) {
                File out = new File(destDir, e.getName());
                if (e.isDirectory()) {
                    if (!out.exists() && !out.mkdirs())
                        throw new IOException("mkdirs failed: " + out);
                } else {
                    File parent = out.getParentFile();
                    if (parent != null && !parent.exists() && !parent.mkdirs())
                        throw new IOException("mkdirs failed: " + parent);
                    try (FileOutputStream fos = new FileOutputStream(out)) {
                        int n;
                        while ((n = zin.read(buf)) >= 0) fos.write(buf, 0, n);
                    }
                    // default file mode 0644; we'll fix exec bits later
                    try { Os.chmod(out.getAbsolutePath(), 0644); } catch (Throwable ignore) {}
                }
                zin.closeEntry();
            }
        }
    }

    private static void fixExecBitsRecursive(File root) {
        // Ensure every directory has 0755 so traversal/execution is allowed
        if (root.isDirectory()) {
            try { Os.chmod(root.getAbsolutePath(), 0755); } catch (Throwable t) { Log.w(TAG, "chmod dir", t); }
            File[] kids = root.listFiles();
            if (kids != null) for (File k : kids) fixExecBitsRecursive(k);
        } else {
            // leave files at 0644; bin/* is bumped to 0755 above
        }
    }

    private static void deleteRec(File f) {
        if (f.isDirectory()) {
            File[] kids = f.listFiles();
            if (kids != null) for (File k : kids) deleteRec(k);
        }
        //noinspection ResultOfMethodCallIgnored
        f.delete();
    }

    private static String sha256Hex(byte[] data) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SHA-256");
        byte[] d = md.digest(data);
        StringBuilder sb = new StringBuilder(d.length * 2);
        for (byte b : d) sb.append(String.format("%02x", b));
        return sb.toString();
    }

    private static String readAll(File f) throws IOException {
        try (FileInputStream in = new FileInputStream(f);
             ByteArrayOutputStream bos = new ByteArrayOutputStream()) {
            byte[] buf = new byte[4096];
            int n;
            while ((n = in.read(buf)) >= 0) bos.write(buf, 0, n);
            return bos.toString("UTF-8");
        }
    }

    private static void post(Listener cb, Runnable r) {
        if (Looper.myLooper() == Looper.getMainLooper()) r.run(); else MAIN.post(r);
    }
}
