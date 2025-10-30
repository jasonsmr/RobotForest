package com.robotforest.launcher;

import android.content.Context;
import org.json.JSONObject;
import java.io.*;

public final class RuntimeManifest {
    public final String url;
    public final String sha256;   // hex lowercase
    public final String subdir;   // e.g. "runtime"

    private RuntimeManifest(String url, String sha256, String subdir) {
        this.url = url;
        this.sha256 = sha256;
        this.subdir = subdir;
    }

    public static RuntimeManifest fromAssets(Context ctx) throws Exception {
        try (InputStream in = ctx.getAssets().open("runtime/manifest.json")) {
            String json = slurp(in);
            JSONObject o = new JSONObject(json);
            String url = o.getString("url");
            String sha = o.getString("sha256").toLowerCase();
            String subdir = o.optString("subdir", "runtime");
            return new RuntimeManifest(url, sha, subdir);
        }
    }

    private static String slurp(InputStream in) throws IOException {
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        byte[] buf = new byte[8192];
        int n;
        while ((n = in.read(buf)) >= 0) bos.write(buf, 0, n);
        return bos.toString("UTF-8");
    }
}
