package com.robotforest.launcher;

import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.Settings;
import android.view.View;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

public class MainActivity extends Activity {

    private TextView statusText;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        statusText = findViewById(R.id.txt_status);

        Button btnLaunch = findViewById(R.id.btn_launch_native);
        Button btnQuit   = findViewById(R.id.btn_quit);
        Button btnAbout  = findViewById(R.id.btn_about);

        Button btnBattery  = findViewById(R.id.btn_battery);
        Button btnOverlay  = findViewById(R.id.btn_overlay);
        Button btnAllFiles = findViewById(R.id.btn_all_files);

        Button btnPickFolder   = findViewById(R.id.btn_pick_folder);
        Button btnOpenTermux   = findViewById(R.id.btn_open_termux);
        Button btnCopyTermux   = findViewById(R.id.btn_copy_termux_cmd);

        btnLaunch.setOnClickListener(v -> {
            try {
                Intent i = new Intent();
                i.setClassName(this, "android.app.NativeActivity");
                startActivity(i);
            } catch (Throwable t) {
                toast("NativeActivity not available: " + t.getMessage());
            }
        });

        btnQuit.setOnClickListener(v -> finish());

        btnAbout.setOnClickListener(v ->
                toast("RobotForest Launcher\nVersion: pre-alpha\nNative test + Termux helper")
        );

        btnBattery.setOnClickListener(v -> {
            try {
                Intent i = new Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS);
                startActivity(i);
            } catch (ActivityNotFoundException e) {
                toast("Battery optimization settings not found.");
            }
        });

        btnOverlay.setOnClickListener(v -> requestOverlayPermission());

        btnAllFiles.setOnClickListener(v -> requestAllFilesAccess());

        btnPickFolder.setOnClickListener(v -> {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_WRITE_URI_PERMISSION
                    | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
            startActivity(i);
        });

        btnOpenTermux.setOnClickListener(v -> openTermuxApp());

        btnCopyTermux.setOnClickListener(v -> copyTermuxBootstrapCmd());

        refreshStatus();
    }

    private void refreshStatus() {
        StringBuilder sb = new StringBuilder();
        sb.append("Overlay: ").append(Settings.canDrawOverlays(this) ? "ON" : "OFF");
        if (Build.VERSION.SDK_INT >= 30) {
            sb.append("  |  All files: ").append(Environment.isExternalStorageManager() ? "ON" : "OFF");
        }
        statusText.setText(sb.toString());
    }

    private void requestOverlayPermission() {
        if (Settings.canDrawOverlays(this)) {
            toast("Overlay already allowed.");
            refreshStatus();
            return;
        }
        try {
            Intent i = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION,
                    Uri.parse("package:" + getPackageName()));
            startActivity(i);
            return;
        } catch (Throwable ignored) {}

        try {
            Intent i2 = new Intent(Settings.ACTION_MANAGE_OVERLAY_PERMISSION);
            startActivity(i2);
            return;
        } catch (Throwable ignored) {}

        try {
            Intent i3 = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS,
                    Uri.fromParts("package", getPackageName(), null));
            startActivity(i3);
        } catch (Throwable t) {
            toast("Overlay settings unavailable.");
        }
    }

    private void requestAllFilesAccess() {
        if (Build.VERSION.SDK_INT >= 30) {
            if (Environment.isExternalStorageManager()) {
                toast("All files access already granted.");
                refreshStatus();
                return;
            }
            try {
                Intent i = new Intent(Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
                i.setData(Uri.parse("package:" + getPackageName()));
                startActivity(i);
            } catch (Throwable t1) {
                try {
                    Intent i2 = new Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                    startActivity(i2);
                } catch (Throwable t2) {
                    toast("All files access settings unavailable.");
                }
            }
        } else {
            toast("All files access not applicable on this Android version.");
        }
    }

    private void openTermuxApp() {
        try {
            Intent i = getPackageManager().getLaunchIntentForPackage("com.termux");
            if (i == null) {
                toast("Termux not found. Please install it from a trusted source.");
                return;
            }
            startActivity(i);
        } catch (Throwable t) {
            toast("Unable to open Termux: " + t.getMessage());
        }
    }

    private void copyTermuxBootstrapCmd() {
        String sharedDir = "/sdcard/RobotForest/bootstrap";
        String scriptOnSd = sharedDir + "/bootstrap.sh";
        String cmd =
                "bash -lc 'set -e;" +
                        " mkdir -p \"$HOME/robotforest/bootstrap\";" +
                        " cp -f " + shQuote(scriptOnSd) + " \"$HOME/robotforest/bootstrap/\";" +
                        " bash \"$HOME/robotforest/bootstrap/bootstrap.sh\"'";

        copyToClipboard(cmd);
        toast("Termux command copied. Paste in Termux and run.");
        statusText.setText("Copied Termux cmd:\n" + cmd);
    }

    private String shQuote(String s) {
        return "'" + s.replace("'", "'\\''") + "'";
    }

    private void copyToClipboard(String text) {
        ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
        cm.setPrimaryClip(ClipData.newPlainText("cmd", text));
    }

    private void toast(String s) {
        Toast.makeText(this, s, Toast.LENGTH_SHORT).show();
    }

    @Override
    protected void onResume() {
        super.onResume();
        refreshStatus();
    }
}
