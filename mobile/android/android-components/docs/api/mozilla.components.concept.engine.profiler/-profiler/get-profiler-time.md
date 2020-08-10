[android-components](../../index.md) / [mozilla.components.concept.engine.profiler](../index.md) / [Profiler](index.md) / [getProfilerTime](./get-profiler-time.md)

# getProfilerTime

`abstract fun getProfilerTime(): `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/profiler/Profiler.kt#L78)

Get the profiler time to be able to mark the start of the marker events.
can be used like this:

**Return**
profiler time as Double or null if the profiler is not active.

