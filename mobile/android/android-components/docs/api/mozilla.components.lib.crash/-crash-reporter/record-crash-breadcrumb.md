[android-components](../../index.md) / [mozilla.components.lib.crash](../index.md) / [CrashReporter](index.md) / [recordCrashBreadcrumb](./record-crash-breadcrumb.md)

# recordCrashBreadcrumb

`fun recordCrashBreadcrumb(breadcrumb: `[`Breadcrumb`](../-breadcrumb/index.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/CrashReporter.kt#L98)

Add a crash breadcrumb to all registered services with breadcrumb support.

``` Kotlin
crashReporter.recordCrashBreadcrumb(
    Breadcrumb("Settings button clicked", data, "UI", Level.INFO, Type.USER)
)
```

