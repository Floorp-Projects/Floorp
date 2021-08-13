
# We do not want to obfuscate - It's just painful to debug without the right mapping file.
# If we update this, we'll have to update our Sentry config to upload ProGuard mappings.
-dontobfuscate


##### Default proguard settings:

# Add project specific ProGuard rules here.
# By default, the flags in this file are appended to flags specified
# in /Users/sebastian/Library/Android/sdk/tools/proguard/proguard-android.txt
# You can edit the include path and order by changing the proguardFiles
# directive in build.gradle.
#
# For more details, see
#   http://developer.android.com/guide/developing/tools/proguard.html

####################################################################################################
# Adjust
####################################################################################################

-keep public class com.adjust.sdk.** { *; }
-keep class com.google.android.gms.common.ConnectionResult {
    int SUCCESS;
}
-keep class com.google.android.gms.ads.identifier.AdvertisingIdClient {
    com.google.android.gms.ads.identifier.AdvertisingIdClient$Info getAdvertisingIdInfo(android.content.Context);
}
-keep class com.google.android.gms.ads.identifier.AdvertisingIdClient$Info {
    java.lang.String getId();
    boolean isLimitAdTrackingEnabled();
}
-keep class dalvik.system.VMRuntime {
    java.lang.String getRuntime();
}
-keep class android.os.Build {
    java.lang.String[] SUPPORTED_ABIS;
    java.lang.String CPU_ABI;
}
-keep class android.content.res.Configuration {
    android.os.LocaledList getLocales();
    java.util.Locale locale;
}
-keep class android.os.LocaledList {
    java.util.Locale get(int);
}


####################################################################################################
# Okhttp
####################################################################################################

# JSR 305 annotations are for embedding nullability information.
-dontwarn javax.annotation.**

# A resource is loaded with a relative path so the package of this class must be preserved.
-keepnames class okhttp3.internal.publicsuffix.PublicSuffixDatabase

# Animal Sniffer compileOnly dependency to ensure APIs are compatible with older versions of Java.
-dontwarn org.codehaus.mojo.animal_sniffer.*

# OkHttp platform used only on JVM and when Conscrypt dependency is available.
-dontwarn okhttp3.internal.platform.ConscryptPlatform

####################################################################################################
# Sentry
####################################################################################################

# Recommended config via https://docs.sentry.io/clients/java/modules/android/#manual-integration
# Since we don't obfuscate, we don't need to use their Gradle plugin to upload ProGuard mappings.
-keepattributes LineNumberTable,SourceFile
-dontwarn org.slf4j.**
-dontwarn javax.**

# Our addition: this class is saved to disk via Serializable, which ProGuard doesn't like.
# If we exclude this, upload silently fails (Sentry swallows a NPE so we don't crash).
# I filed https://github.com/getsentry/sentry-java/issues/572
#
# If Sentry ever mysteriously stops working after we upgrade it, this could be why.
-keep class io.sentry.event.Event { *; }

####################################################################################################
# Android architecture components
####################################################################################################

-dontwarn android.**
-dontwarn androidx.**
-dontwarn com.google.**
-dontwarn org.mozilla.geckoview.**
-dontwarn mozilla.components.**

# https://developer.android.com/topic/libraries/architecture/release-notes.html
# According to the docs this won't be needed when 1.0 of the library is released.
-keep class * implements android.arch.lifecycle.GeneratedAdapter {<init>(...);}

# Temporary fix until we can use androidx
-dontwarn mozilla.components.service.fretboard.scheduler.workmanager.**

# Fix for ViewModels
-keep class * extends androidx.lifecycle.ViewModel {
    <init>();
}
-keep class * extends androidx.lifecycle.AndroidViewModel {
    <init>(android.app.Application);
}

####################################################################################################
# Kotlinx
####################################################################################################

-dontwarn kotlinx.atomicfu.**

####################################################################################################
# snakeyaml
####################################################################################################

-dontwarn java.beans.PropertyDescriptor
-dontwarn java.beans.Introspector
-dontwarn java.beans.BeanInfo
-dontwarn java.beans.IntrospectionException
-dontwarn java.beans.FeatureDescriptor

####################################################################################################
# REMOVE all Log messages except warnings and errors
####################################################################################################
-assumenosideeffects class android.util.Log {
    public static boolean isLoggable(java.lang.String, int);
    public static int v(...);
    public static int i(...);
    public static int d(...);
}

####################################################################################################
# kotlinx.coroutines: use the fast service loader to init MainDispatcherLoader by including a rule
# to rewrite this property to return true:
# https://github.com/Kotlin/kotlinx.coroutines/blob/8c98180f177bbe4b26f1ed9685a9280fea648b9c/kotlinx-coroutines-core/jvm/src/internal/MainDispatchers.kt#L19
#
# R8 is expected to optimize the default implementation to avoid a performance issue but a bug in R8
# as bundled with AGP v7.0.0 causes this optimization to fail so we use the fast service loader instead. See:
# https://github.com/mozilla-mobile/focus-android/issues/5102#issuecomment-897854121
#
# The fast service loader appears to be as performant as the R8 optimization so it's not worth the
# churn to later remove this workaround. If needed, the upstream fix is being handled in
# https://issuetracker.google.com/issues/196302685
####################################################################################################
-assumenosideeffects class kotlinx.coroutines.internal.MainDispatcherLoader {
    boolean FAST_SERVICE_LOADER_ENABLED return true;
}
