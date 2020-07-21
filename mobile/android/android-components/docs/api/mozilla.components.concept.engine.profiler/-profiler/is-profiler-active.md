[android-components](../../index.md) / [mozilla.components.concept.engine.profiler](../index.md) / [Profiler](index.md) / [isProfilerActive](./is-profiler-active.md)

# isProfilerActive

`abstract fun isProfilerActive(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/profiler/Profiler.kt#L64)

Returns true if profiler is active and it's allowed the add markers.
It's useful when it's computationally heavy to get startTime or the
additional text for the marker. That code can be wrapped with
isProfilerActive if check to reduce the overhead of it.

**Return**
true if profiler is active and safe to add a new marker.

