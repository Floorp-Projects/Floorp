[android-components](../../../index.md) / [mozilla.components.lib.crash](../../index.md) / [Crash](../index.md) / [NativeCodeCrash](index.md) / [isFatal](./is-fatal.md)

# isFatal

`val isFatal: `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/Crash.kt#L69)

Whether or not the crash was fatal or not: If true, the main application process was affected
    by the crash. If false, only an internal process used by Gecko has crashed and the application
    may be able to recover.

### Property

`isFatal` - Whether or not the crash was fatal or not: If true, the main application process was affected
    by the crash. If false, only an internal process used by Gecko has crashed and the application
    may be able to recover.