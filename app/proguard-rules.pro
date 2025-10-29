# app/proguard-rules.pro

# Keep NativeActivity (Android loads it by class name)
-keep class android.app.NativeActivity { *; }

# Keep your package (no Java/Kotlin yet, but future-proof)
-keep class com.example.skyrimlauncher.** { *; }

# Don’t warn about vulkan or native-related classes
-dontwarn org.lwjgl.**
-dontwarn **.vulkan.**

# Keep native-lib loader patterns if you add Java later
-keepclasseswithmembers class * {
    native <methods>;
}
