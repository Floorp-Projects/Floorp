[android-components](../../index.md) / [mozilla.components.concept.engine.profiler](../index.md) / [Profiler](index.md) / [addMarker](./add-marker.md)

# addMarker

`abstract fun addMarker(markerName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, startTime: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`?, endTime: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`?, text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/profiler/Profiler.kt#L90)

Add a profiler marker to Gecko Profiler with the given arguments.
It can be used for either adding a point-in-time marker or a duration marker.
No-op if profiler is not active.

### Parameters

`markerName` - Name of the event as a string.

`startTime` - Start time as Double. It can be null if you want to mark a point of time.

`endTime` - End time as Double. If it's null, this function implicitly gets the end time.

`text` - An optional string field for more information about the marker.`abstract fun addMarker(markerName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, startTime: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`?, text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/profiler/Profiler.kt#L102)

Add a profiler marker to Gecko Profiler with the given arguments.
End time will be added automatically with the current profiler time when the function is called.
No-op if profiler is not active.
This is an overload of [Profiler.addMarker](./add-marker.md) for convenience.

### Parameters

`aMarkerName` - Name of the event as a string.

`aStartTime` - Start time as Double. It can be null if you want to mark a point of time.

`aText` - An optional string field for more information about the marker.`abstract fun addMarker(markerName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, startTime: `[`Double`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-double/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/profiler/Profiler.kt#L113)

Add a profiler marker to Gecko Profiler with the given arguments.
End time will be added automatically with the current profiler time when the function is called.
No-op if profiler is not active.
This is an overload of [Profiler.addMarker](./add-marker.md) for convenience.

### Parameters

`markerName` - Name of the event as a string.

`startTime` - Start time as Double. It can be null if you want to mark a point of time.`abstract fun addMarker(markerName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, text: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`?): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/profiler/Profiler.kt#L124)

Add a profiler marker to Gecko Profiler with the given arguments.
Time will be added automatically with the current profiler time when the function is called.
No-op if profiler is not active.
This is an overload of [Profiler.addMarker](./add-marker.md) for convenience.

### Parameters

`markerName` - Name of the event as a string.

`text` - An optional string field for more information about the marker.`abstract fun addMarker(markerName: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/profiler/Profiler.kt#L134)

Add a profiler marker to Gecko Profiler with the given arguments.
Time will be added automatically with the current profiler time when the function is called.
No-op if profiler is not active.
This is an overload of [Profiler.addMarker](./add-marker.md) for convenience.

### Parameters

`markerName` - Name of the event as a string.