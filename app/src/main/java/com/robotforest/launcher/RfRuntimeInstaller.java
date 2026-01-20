package com.robotforest.launcher;

import android.content.Context;
import android.content.res.AssetManager;
import android.util.Log;

import java.io.*;

public class RfRuntimeInstaller {

    private static final String TAG = "RF.RfRuntimeInstaller";

    // Must match the asset name we just copied into app/src/main/assets/
    private static final String ASSET_NAME = "rf-runtime-dev.tar.zst";

    // Subdir under context.getFilesDir() where the runtime will live
    private static final String RUNTIME_DIR = "rf_runtime";

    public static File ensureInstalled(Context context) throws IOException, InterruptedException {
        File appFiles = context.getFilesDir();
        File runtimeRoot = new File(appFiles, RUNTIME_DIR);

        if (runtimeRoot.exists()) {
            Log.i(TAG, "Runtime already installed at: " + runtimeRoot.getAbsolutePath());
            return runtimeRoot;
        }

        Log.i(TAG, "Runtime not found, installing...");

        // 1) Copy archive from assets → app-private file
        File archiveFile = new File(appFiles, ASSET_NAME);
        copyAssetToFile(context, ASSET_NAME, archiveFile);
        Log.i(TAG, "Copied runtime archive to: " + archiveFile.getAbsolutePath());

        // 2) Locate installer script (we bundled it in jniLibs/arm64-v8a/)
        File nativeLibDir = new File(context.getApplicationInfo().nativeLibraryDir);
        File installer = new File(nativeLibDir, "rf_install_runtime.sh");

        if (!installer.exists()) {
            throw new IllegalStateException(
                    "Missing rf_install_runtime.sh in native libs: " + installer.getAbsolutePath()
            );
        }

        // Make sure it is executable
        // (On Android it should already be 0755 from packaging, but this is harmless)
        //noinspection ResultOfMethodCallIgnored
        installer.setExecutable(true);

        // 3) Run installer:
        //    rf_install_runtime.sh <archive> <runtimeRoot>
        ProcessBuilder pb = new ProcessBuilder(
                installer.getAbsolutePath(),
                archiveFile.getAbsolutePath(),
                runtimeRoot.getAbsolutePath()
        );
        pb.redirectErrorStream(true);

        Process p = pb.start();
        String output = readAll(p.getInputStream());
        int code = p.waitFor();

        Log.i(TAG, "rf_install_runtime.sh output:\n" + output);
        if (code != 0) {
            throw new RuntimeException(
                    "rf_install_runtime.sh failed with exit code " + code
            );
        }

        // Clean up archive after successful install
        //noinspection ResultOfMethodCallIgnored
        archiveFile.delete();

        Log.i(TAG, "Runtime installed to: " + runtimeRoot.getAbsolutePath());
        return runtimeRoot;
    }

    private static void copyAssetToFile(Context context, String assetName, File outFile) throws IOException {
        AssetManager am = context.getAssets();
        InputStream in = null;
        OutputStream out = null;
        try {
            in = am.open(assetName);
            out = new FileOutputStream(outFile);
            byte[] buf = new byte[8192];
            int r;
            while ((r = in.read(buf)) != -1) {
                out.write(buf, 0, r);
            }
            out.flush();
        } finally {
            if (in != null) try { in.close(); } catch (IOException ignored) {}
            if (out != null) try { out.close(); } catch (IOException ignored) {}
        }
    }

    private static String readAll(InputStream is) throws IOException {
        StringBuilder sb = new StringBuilder();
        BufferedReader br = new BufferedReader(new InputStreamReader(is));
        String line;
        while ((line = br.readLine()) != null) {
            sb.append(line).append('\n');
        }
        return sb.toString();
    }
}
