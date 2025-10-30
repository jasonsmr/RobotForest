// app/src/main/java/com/robotforest/launcher/Hashes.java
package com.robotforest.launcher;

import java.security.MessageDigest;
import java.util.Locale;

final class Hashes {
    static boolean shouldVerify(String expected) {
        if (expected == null) return false;
        String e = expected.trim();
        return !(e.isEmpty() || e.equalsIgnoreCase("auto"));
    }
    static String sha256Hex(byte[] data) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            byte[] d = md.digest(data);
            StringBuilder sb = new StringBuilder(d.length * 2);
            for (byte b : d) sb.append(String.format(Locale.US, "%02x", b));
            return sb.toString();
        } catch (Exception ex) {
            return "";
        }
    }
}
