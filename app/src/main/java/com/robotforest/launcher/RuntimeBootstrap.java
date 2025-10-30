package com.robotforest.launcher;

import android.content.Context;
import android.content.res.AssetManager;
import android.net.Uri;
import android.os.Build;

import org.json.JSONObject;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.Closeable;
import java.io.File;
import java.io.FileInputStream;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.FileOutputStream;
import java.net.HttpURLConnection;
import java.net.URL;
import java.security.MessageDigest;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipInputStream;

public final class RuntimeBootstrap {

    public enum Status { OK_READY, FETCHED, VERIFIED, INSTALLED, ALREADY_PRESENT, ERROR }

    public static final class Result {
        public final Status status;
        public final File runtimeDir;
        Result(Status s, File dir) { this.status = s; this.runtimeDir = dir; }
    }

    public interface Logger { void log(String msg); }

    private RuntimeBootstrap() {}

    // Primary: remote JSON (kept in repo so you can change without shipping a new APK)
    private static final String MANIFEST_URL =
            "https://raw.githubusercontent.com/jasonsmr/RobotForest/main/scripts/runtime/runtime-manifest.json";

    // Fallback: a copy inside the APK (assets) so launches still work offline
    private static final String ASSET_MANIFEST_PATH = "runtime/manifest.json";

    private static final String RUNTIME_SUBDIR_DEFAULT = "runtime";

    public static Result ensureRuntimeInstalled(Context ctx, Logger log) throws Exception {
        final File root = new File(ctx.getFilesDir(), "rf_runtime");
        final File installDir = new File(root, RUNTIME_SUBDIR_DEFAULT);
        if (installDir.isDirectory() && hasSanity(installDir)) {
            if (log != null) log.log("[runtime] already present: " + installDir);
            return new Result(Status.ALREADY_PRESENT, installDir);
        }

        Manifest m = null;
        // 1) Try remote manifest
        try {
            if (log != null) log.log("[runtime] fetching manifest (remote) …");
            m = fetchManifestRemote(MANIFEST_URL, log);
        } catch (Throwable t) {
            if (log != null) log.log("[runtime] remote manifest failed: " + t);
        }
        // 2) Fall back to assets
        if (m == null) {
            if (log != null) log.log("[runtime] using embedded manifest from assets …");
            m = fetchManifestFromAssets(ctx.getAssets(), ASSET_MANIFEST_PATH, log);
        }
        if (m == null || m.url == null || m.url.isEmpty()) {
            throw new IllegalStateException("no runtime manifest available");
        }

        final String subdir = (m.subdir != null && !m.subdir.isEmpty()) ? m.subdir : RUNTIME_SUBDIR_DEFAULT;
        final File targetDir = new File(root, subdir);

        if (log != null) log.log("[runtime] downloading zip …");
        final File zip = downloadToCache(ctx, m.url, "rf-runtime.zip", log);

        // Resolve SHA-256: accept manifest sha, or auto-fetch from url+".sha256"
        String needSha = m.sha256;
        if (needSha == null || needSha.isEmpty() || "auto".equalsIgnoreCase(needSha)) {
            try {
                if (log != null) log.log("[runtime] fetching sha256 from: " + m.url + ".sha256");
                needSha = fetchText(m.url + ".sha256").trim().split("\\s+")[0];
            } catch (Throwable t) {
                throw new IllegalStateException("unable to resolve sha256 (no field and no .sha256 sidecar): " + t);
            }
        }

        if (log != null) log.log("[runtime] verifying sha256 …");
        final String got = sha256File(zip);
        if (!got.equalsIgnoreCase(needSha)) {
            throw new IllegalStateException("checksum mismatch: expected " + needSha + " got " + got);
        }
        if (log != null) log.log("[runtime] checksum OK.");

        // Clean target and unzip
        if (targetDir.exists()) deleteRecursively(targetDir);
        if (!targetDir.mkdirs() && !targetDir.isDirectory()) {
            throw new IllegalStateException("mkdirs failed: " + targetDir);
        }

        if (log != null) log.log("[runtime] extracting to " + targetDir);
        unzip(zip, targetDir);

        if (!hasSanity(targetDir)) {
            throw new IllegalStateException("runtime incomplete after unzip");
        }

        // Make bin/* executable
        File bin = new File(targetDir, "bin");
        if (bin.isDirectory()) {
            File[] all = bin.listFiles();
            if (all != null) for (File f : all) {
                try { f.setExecutable(true, false); } catch (Throwable ignore) {}
            }
        }

        return new Result(Status.OK_READY, targetDir);
    }

    private static boolean hasSanity(File dir) {
        File f = new File(new File(dir, "bin"), "box64");
        return f.isFile();
    }

    // Manifest model
    private static final class Manifest {
        final String url;
        final String sha256;
        final String subdir;
        Manifest(String url, String sha256, String subdir) {
            this.url = url; this.sha256 = sha256; this.subdir = subdir;
        }
    }

    private static Manifest fetchManifestRemote(String url, Logger log) throws Exception {
        String json = fetchText(url);
        JSONObject j = new JSONObject(json);
        String mUrl = j.getString("url");
        String sha = j.optString("sha256", "");
        String sub = j.optString("subdir", RUNTIME_SUBDIR_DEFAULT);
        if (log != null) log.log("[runtime] manifest url: " + mUrl + " (sha=" + (sha.isEmpty()?"<auto>":sha) + ")");
        return new Manifest(mUrl, sha, sub);
    }

    private static Manifest fetchManifestFromAssets(AssetManager am, String path, Logger log) throws Exception {
        try (InputStream in = am.open(path);
             BufferedReader br = new BufferedReader(new InputStreamReader(in))) {
            StringBuilder sb = new StringBuilder();
            String line; while ((line = br.readLine()) != null) sb.append(line).append('\n');
            JSONObject j = new JSONObject(sb.toString());
            String mUrl = j.optString("url", "");
            String sha = j.optString("sha256", "");
            String sub = j.optString("subdir", RUNTIME_SUBDIR_DEFAULT);
            if (log != null) log.log("[runtime] asset manifest url: " + mUrl);
            return new Manifest(mUrl, sha, sub);
        }
    }

    private static String fetchText(String url) throws Exception {
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(15000);
        c.setReadTimeout(20000);
        c.setRequestProperty("User-Agent", ua());
        try (BufferedReader br = new BufferedReader(new InputStreamReader(c.getInputStream()))) {
            StringBuilder sb = new StringBuilder();
            String line; while ((line = br.readLine()) != null) sb.append(line).append('\n');
            return sb.toString();
        } finally {
            c.disconnect();
        }
    }

    private static File downloadToCache(Context ctx, String url, String name, Logger log) throws Exception {
        File dst = new File(ctx.getCacheDir(), name);
        HttpURLConnection c = (HttpURLConnection) new URL(url).openConnection();
        c.setConnectTimeout(15000);
        c.setReadTimeout(60000);
        c.setRequestProperty("User-Agent", ua());
        try (BufferedInputStream in = new BufferedInputStream(c.getInputStream());
             BufferedOutputStream out = new BufferedOutputStream(new FileOutputStream(dst))) {
            byte[] buf = new byte[8192];
            int n;
            long total = 0;
            while ((n = in.read(buf)) >= 0) {
                out.write(buf, 0, n);
                total += n;
            }
        } finally {
            c.disconnect();
        }
        return dst;
    }

    private static void unzip(File zip, File toDir) throws Exception {
        try (ZipInputStream zis = new ZipInputStream(new BufferedInputStream(new FileInputStream(zip)))) {
            ZipEntry e;
            byte[] buf = new byte[8192];
            while ((e = zis.getNextEntry()) != null) {
                File out = new File(toDir, e.getName());
                if (!out.getCanonicalPath().startsWith(toDir.getCanonicalPath()))
                    throw new SecurityException("zip path traversal: " + e.getName());
                if (e.isDirectory()) {
                    if (!out.exists() && !out.mkdirs())
                        throw new IllegalStateException("mkdirs failed: " + out);
                } else {
                    File parent = out.getParentFile();
                    if (parent != null && !parent.isDirectory() && !parent.mkdirs())
                        throw new IllegalStateException("mkdirs failed: " + parent);
                    try (BufferedOutputStream bos = new BufferedOutputStream(new FileOutputStream(out))) {
                        int n;
                        while ((n = zis.read(buf)) > 0) bos.write(buf, 0, n);
                    }
                }
                zis.closeEntry();
            }
        }
    }

    private static String sha256File(File f) throws Exception {
        MessageDigest md = MessageDigest.getInstance("SHA-256");
        try (BufferedInputStream in = new BufferedInputStream(new FileInputStream(f))) {
            byte[] buf = new byte[8192];
            int n; while ((n = in.read(buf)) >= 0) md.update(buf, 0, n);
        }
        byte[] d = md.digest();
        StringBuilder sb = new StringBuilder(d.length * 2);
        for (byte b : d) sb.append(String.format(Locale.US, "%02x", b));
        return sb.toString();
    }

    private static void deleteRecursively(File f) {
        if (f.isDirectory()) {
            File[] kids = f.listFiles();
            if (kids != null) for (File k : kids) deleteRecursively(k);
        }
        try { f.delete(); } catch (Throwable ignore) {}
    }

    private static String ua() {
        return "RobotForest/" + (Build.VERSION.RELEASE != null ? Build.VERSION.RELEASE : "0") +
                " (Android; " + Build.SUPPORTED_ABIS[0] + ")";
    }

    @SuppressWarnings("unused")
    private static void closeQuietly(Closeable c) { try { if (c != null) c.close(); } catch (Throwable ignore) {} }
}
