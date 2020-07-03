[android-components](../../index.md) / [mozilla.components.concept.engine.profiler](../index.md) / [Profiler](./index.md)

# Profiler

`interface Profiler` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/profiler/Profiler.kt#L55)

[Profiler](./index.md) is being used to manage Firefox Profiler related features.

If you want to add a profiler marker to mark a point in time (without a duration)
you can directly use `engine.profiler?.addMarker("marker name")`.
Or if you want to provide more information, you can use
`engine.profiler?.addMarker("marker name", "extra information")`.

If you want to add a profiler marker with a duration (with start and end time)
you can use it like this, it will automatically get the end time inside the addMarker:

```
    val startTime = engine.profiler?.getProfilerTime()
    ...some code you want to measure...
    engine.profiler?.addMarker("name", startTime)
```

Or you can capture start and end time in somewhere, then add the marker in somewhere else:

```
    val startTime = engine.profiler?.getProfilerTime()
    ...some code you want to measure (or end time can be collected in a callback)...
    val endTime = engine.profiler?.getProfilerTime()

    ...somewhere else in the codebase...
    engine.profiler?.addMarker("name", startTime, endTime)
```

Here's an [Profiler.addMarker](add-marker.md) example with all the possible parameters:

```
    val startTime = engine.profiler?.getProfilerTime()
    ...some code you want to measure...
    val endTime = engine.profiler?.getProfilerTime()

    ...somewhere else in the codebase...
    engine.profiler?.addMarker("name", startTime, endTime, "extra information")
```

[Profiler.isProfilerActive](is-profiler-active.md) method is handy when you want to get more information to
add inside the marker, but you think it's going to be computationally heavy (and useless)
when profiler is not running:

```
    val startTime = engine.profiler?.getProfilerTime()
    ...some code you want to measure...
    if (engine.profiler?.isProfilerActive()) {
        val info = aFunctionYouDoNotWantToCallWhenProfilerIsNotActive()
        engine.profiler?.addMarker("name", startTime, info)
    }
```

### Functions

| Name | Summary |
|---|---|
| [addMarker](add-marker.md) | `abstract fun addMarker(markerName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, startTime: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`?, endTime: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`?, text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Add a profiler marker to Gecko Profiler with the given arguments. It can be used for either adding a point-in-time marker or a duration marker. No-op if profiler is not active.`abstract fun addMarker(markerName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, startTime: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`?, text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun addMarker(markerName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, startTime: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Add a profiler marker to Gecko Profiler with the given arguments. End time will be added automatically with the current profiler time when the function is called. No-op if profiler is not active. This is an overload of [Profiler.addMarker](add-marker.md) for convenience.`abstract fun addMarker(markerName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>`abstract fun addMarker(markerName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)<br>Add a profiler marker to Gecko Profiler with the given arguments. Time will be added automatically with the current profiler time when the function is called. No-op if profiler is not active. This is an overload of [Profiler.addMarker](add-marker.md) for convenience. |
| [getProfilerTime](get-profiler-time.md) | `abstract fun getProfilerTime(): `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`?`<br>Get the profiler time to be able to mark the start of the marker events. can be used like this: |
| [isProfilerActive](is-profiler-active.md) | `abstract fun isProfilerActive(): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html)<br>Returns true if profiler is active and it's allowed the add markers. It's useful when it's computationally heavy to get startTime or the additional text for the marker. That code can be wrapped with isProfilerActive if check to reduce the overhead of it. |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |

### Inheritors

| Name | Summary |
|---|---|
| [Profiler](../../mozilla.components.browser.engine.gecko.profiler/-profiler/index.md) | `class Profiler : `[`Profiler`](./index.md)<br>Gecko-based implementation of [Profiler](./index.md), wrapping the ProfilerController object provided by GeckoView. |
