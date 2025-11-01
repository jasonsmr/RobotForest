package com.robotforest.launcher;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Bundle;
import android.system.Os;
import android.system.OsConstants;
import android.text.method.ScrollingMovementMethod;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.File;
import java.util.*;

public class MainActivity extends Activity {
    private static final String TAG = "RF.Main";
    private TextView logView;
    private File installDir;

    @Override protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(12);
        root.setPadding(pad, pad, pad, pad);

        logView = new TextView(this);
        logView.setText("Preparing runtime…\n");
        logView.setTextIsSelectable(true);
        logView.setMovementMethod(new ScrollingMovementMethod());
        logView.setMinLines(8);

        Button btnCheck64 = new Button(this); btnCheck64.setText("Run: libbox64.so -v");
        Button btnCheck86 = new Button(this); btnCheck86.setText("Run: libbox86.so -v");
        Button btnWv = new Button(this);      btnWv.setText("wine64 --version");
        Button btnCfg = new Button(this);     btnCfg.setText("winecfg (WoW64)");
        Button btnSteam = new Button(this);   btnSteam.setText("Steam (Windows)");
        Button btnList = new Button(this);    btnList.setText("List runtime/bin");
        Button btnFix  = new Button(this);    btnFix.setText("Fix permissions");
        Button btnCopy = new Button(this);    btnCopy.setText("Copy log");
        Button btnExit = new Button(this);    btnExit.setText("Exit");
        Button btnRein = new Button(this);    btnRein.setText("Force reinstall runtime");

        ScrollView scroller = new ScrollView(this); scroller.addView(logView);
        root.addView(scroller, new LinearLayout.LayoutParams(
                LinearLayout.LayoutParams.MATCH_PARENT, 0, 1f));
        root.addView(btnCheck64); root.addView(btnCheck86);
        root.addView(btnWv); root.addView(btnCfg); root.addView(btnSteam);
        root.addView(btnList); root.addView(btnFix);
        root.addView(btnCopy); root.addView(btnExit); root.addView(btnRein);

        setContentView(root);

        RuntimeInstaller.ensureInstalled(this, new RuntimeInstaller.Listener() {
            @Override public void onReady(File dir) {
                installDir = dir;
                append("[ready] " + dir.getAbsolutePath());
                String n = Exec.nativeBox64Path();
                if (n != null) append("[pref] nativeLibraryDir/libbox64.so -> " + n);
            }
            @Override public void onProgress(String msg) { append(msg); }
            @Override public void onError(Exception e) { append("[error] " + e.getMessage()); }
        });

        btnList.setOnClickListener(v -> {
            if (installDir == null) { append("[warn] runtime not ready yet"); return; }
            File bin = new File(installDir, "bin");
            File[] list = bin.listFiles();
            if (list == null) { append("[error] cannot list " + bin.getAbsolutePath()); return; }
            StringBuilder sb = new StringBuilder("runtime/bin:\n");
            for (File f : list) {
                sb.append("  ").append(f.getName());
                if (f.canExecute()) sb.append(" *");
                sb.append("  (").append(f.length()).append(" bytes)");
                sb.append("\n");
            }
            append(sb.toString());
        });

        btnFix.setOnClickListener(v -> {
            if (installDir == null) { append("[warn] runtime not ready yet"); return; }
            try {
                walkChmod(installDir, true);
                File bin = new File(installDir, "bin");
                File[] xs = bin.listFiles();
                if (xs != null) for (File f : xs) try { Os.chmod(f.getAbsolutePath(), 0755); } catch (Throwable ignore) {}
                append("[fix] permissions updated");
                String n = Exec.nativeBox64Path();
                if (n != null) append("[pref] nativeLibraryDir/libbox64.so -> " + n);
            } catch (Throwable t) {
                append("[fix error] " + t);
            }
        });

        btnCheck64.setOnClickListener(v -> {
            if (installDir == null) { append("[warn] runtime not ready yet"); return; }
            String prefer = Exec.nativeBox64Path();
            if (prefer == null) { append("[error] libbox64.so not visible"); return; }
            append("$ libbox64.so -v");
            Exec.runAsync(Arrays.asList(prefer, "-v"), installDir, withPath(new File(installDir,"bin")),
                    new Exec.Callback() {
                        @Override public void onCompleted(int code, String out, String err) { append("[exit " + code + "]\n" + trimBoth(out, err)); }
                        @Override public void onError(Exception e) { append("[exec error] " + e); }
                    });
        });

        btnCheck86.setOnClickListener(v -> {
            if (installDir == null) { append("[warn] runtime not ready yet"); return; }
            append("$ box86 -v");
            Exec.runAsync(Arrays.asList("box86", "-v"), installDir, withPath(new File(installDir,"bin")),
                    new Exec.Callback() {
                        @Override public void onCompleted(int code, String out, String err) { append("[exit " + code + "]\n" + trimBoth(out, err)); }
                        @Override public void onError(Exception e) { append("[exec error] " + e); }
                    });
        });

        btnWv.setOnClickListener(v -> {
            if (installDir == null) { append("[warn] runtime not ready yet"); return; }
            File wine64 = new File(new File(installDir,"bin"), "wine64.sh");
            append("$ wine64.sh --version");
            Exec.runAsync(Arrays.asList(wine64.getAbsolutePath(), "--version"), installDir, withPath(new File(installDir,"bin")),
                    new Exec.Callback() {
                        @Override public void onCompleted(int code, String out, String err) { append("[exit " + code + "]\n" + trimBoth(out, err)); }
                        @Override public void onError(Exception e) { append("[exec error] " + e); }
                    });
        });

        btnCfg.setOnClickListener(v -> {
            if (installDir == null) { append("[warn] runtime not ready yet"); return; }
            File wine64 = new File(new File(installDir,"bin"), "wine64.sh");
            append("$ wine64.sh winecfg");
            Exec.runAsync(Arrays.asList(wine64.getAbsolutePath(), "winecfg"), installDir, withPath(new File(installDir,"bin")),
                    new Exec.Callback() {
                        @Override public void onCompleted(int code, String out, String err) { append("[exit " + code + "]\n" + trimBoth(out, err)); }
                        @Override public void onError(Exception e) { append("[exec error] " + e); }
                    });
        });

        btnSteam.setOnClickListener(v -> {
            if (installDir == null) { append("[warn] runtime not ready yet"); return; }
            File steam = new File(new File(installDir,"bin"), "steam-win.sh");
            append("$ steam-win.sh");
            Exec.runAsync(Arrays.asList(steam.getAbsolutePath()), installDir, withPath(new File(installDir,"bin")),
                    new Exec.Callback() {
                        @Override public void onCompleted(int code, String out, String err) { append("[exit " + code + "]\n" + trimBoth(out, err)); }
                        @Override public void onError(Exception e) { append("[exec error] " + e); }
                    });
        });

        btnCopy.setOnClickListener(v -> {
            ClipboardManager cm = (ClipboardManager) getSystemService(Context.CLIPBOARD_SERVICE);
            cm.setPrimaryClip(ClipData.newPlainText("RobotForest log", logView.getText()));
            append("[copied log to clipboard]");
        });

        btnExit.setOnClickListener(v -> finish());

        btnRein.setOnClickListener(v -> {
            append("[action] force reinstall requested…");
            RuntimeInstaller.forceReinstall(this, new RuntimeInstaller.Listener() {
                @Override public void onReady(File dir) { installDir = dir; append("[ready] " + dir.getAbsolutePath()); }
                @Override public void onProgress(String msg) { append(msg); }
                @Override public void onError(Exception e) { append("[error] " + e.getMessage()); }
            });
        });
    }

    private Map<String,String> withPath(File bin) {
        Map<String,String> env = new HashMap<>();
        String cur = System.getenv("PATH");
        env.put("PATH", bin.getAbsolutePath() + (cur!=null?":"+cur:""));
        return env;
    }

    private String diag(File f) {
        StringBuilder sb = new StringBuilder();
        try {
            sb.append("[diag] ").append(f.getAbsolutePath()).append("\n");
            sb.append("  exists=").append(f.exists())
              .append(" dir=").append(f.isDirectory())
              .append(" len=").append(f.length())
              .append(" canExec=").append(f.canExecute()).append("\n");
            try { sb.append("  access(X_OK)=").append(Os.access(f.getAbsolutePath(), OsConstants.X_OK)).append("\n"); }
            catch (Throwable t) { sb.append("  access(X_OK) threw ").append(t).append("\n"); }
            try { Os.chmod(f.getAbsolutePath(), 0755); sb.append("  chmod 0755 ok\n"); }
            catch (Throwable t) { sb.append("  chmod 0755 threw ").append(t).append("\n"); }
        } catch (Throwable t) {
            sb.append("  diag threw ").append(t).append("\n");
        }
        return sb.toString();
    }

    private void walkChmod(File root, boolean dirs0755) {
        if (root.isDirectory()) {
            try { Os.chmod(root.getAbsolutePath(), 0755); } catch (Throwable ignore) {}
            File[] kids = root.listFiles();
            if (kids != null) for (File k : kids) walkChmod(k, dirs0755);
        } else {
            try { Os.chmod(root.getAbsolutePath(), 0644); } catch (Throwable ignore) {}
        }
    }

    private void append(String s) {
        runOnUiThread(() -> {
            logView.append(s + "\n");
            int bottom = logView.getLayout() != null ? logView.getLayout().getLineTop(logView.getLineCount()) : 0;
            logView.scrollTo(0, Math.max(0, bottom - logView.getHeight()));
        });
    }
    private int dp(int v) { float d = getResources().getDisplayMetrics().density; return Math.round(d * v); }
    private String trimBoth(String out, String err) {
        String o = (out == null ? "" : out.trim());
        String e = (err == null ? "" : err.trim());
        if (!e.isEmpty()) return o + (o.isEmpty() ? "" : "\n") + "[stderr]\n" + e;
        return o;
    }
}
