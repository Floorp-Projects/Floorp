[android-components](../../../index.md) / [mozilla.components.lib.crash](../../index.md) / [Crash](../index.md) / [NativeCodeCrash](index.md) / [extrasPath](./extras-path.md)

# extrasPath

`val extrasPath: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/crash/src/main/java/mozilla/components/lib/crash/Crash.kt#L81)

Path to a file containing extra metadata about the crash. The file contains key-value pairs
    in the form `Key=Value`. Be aware, it may contain sensitive data such as the URI that was
    loaded at the time of the crash.

### Property

`extrasPath` - Path to a file containing extra metadata about the crash. The file contains key-value pairs
    in the form `Key=Value`. Be aware, it may contain sensitive data such as the URI that was
    loaded at the time of the crash.