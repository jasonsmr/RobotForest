package org.robotforest.runtime.installer

import android.content.Context
import android.util.Log
import java.io.File
import java.io.FileOutputStream

object RuntimeInstaller {

    private const val TAG = "RF.RuntimeInstaller"
    private const val ASSET_NAME = "rf-runtime-dev.tar.zst"
    private const val RUNTIME_DIR = "rf_runtime"

    fun ensureRuntimeInstalled(context: Context): File {
        val appFiles = context.filesDir
        val runtimeRoot = File(appFiles, RUNTIME_DIR)

        if (runtimeRoot.exists()) {
            Log.i(TAG, "Runtime already installed at: ${runtimeRoot.absolutePath}")
            return runtimeRoot
        }

        Log.i(TAG, "Installing runtime...")

        // 1. Copy archive from assets → local tmp path
        val archiveFile = File(appFiles, ASSET_NAME)
        context.assets.open(ASSET_NAME).use { input ->
            FileOutputStream(archiveFile).use { output ->
                input.copyTo(output)
            }
        }

        Log.i(TAG, "Copied runtime archive to: ${archiveFile.absolutePath}")

        // 2. Extract using bundled installer shell script
        val installer = File(context.applicationInfo.nativeLibraryDir)
            .resolve("rf_install_runtime.sh")

        if (!installer.exists()) {
            throw IllegalStateException("Missing rf_install_runtime.sh in native libs!")
        }

        installer.setExecutable(true)

        val process = ProcessBuilder(
            installer.absolutePath,
            archiveFile.absolutePath,
            runtimeRoot.absolutePath
        )
            .redirectErrorStream(true)
            .start()

        val output = process.inputStream.bufferedReader().readText()
        Log.i(TAG, output)

        val code = process.waitFor()
        if (code != 0) {
            throw RuntimeException("Runtime install failed (exit $code)")
        }

        Log.i(TAG, "Runtime installed to: ${runtimeRoot.absolutePath}")
        archiveFile.delete()

        return runtimeRoot
    }
}
