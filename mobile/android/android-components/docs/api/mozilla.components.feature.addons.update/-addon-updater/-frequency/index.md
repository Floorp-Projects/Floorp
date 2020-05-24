[android-components](../../../index.md) / [mozilla.components.feature.addons.update](../../index.md) / [AddonUpdater](../index.md) / [Frequency](./index.md)

# Frequency

`class Frequency` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/feature/addons/src/main/java/mozilla/components/feature/addons/update/AddonUpdater.kt#L138)

Indicates how often an extension should be updated.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Frequency(repeatInterval: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, repeatIntervalTimeUnit: `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html)`)`<br>Indicates how often an extension should be updated. |

### Properties

| Name | Summary |
|---|---|
| [repeatInterval](repeat-interval.md) | `val repeatInterval: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)<br>Integer indicating how often the update should happen. |
| [repeatIntervalTimeUnit](repeat-interval-time-unit.md) | `val repeatIntervalTimeUnit: `[`TimeUnit`](http://docs.oracle.com/javase/7/docs/api/java/util/concurrent/TimeUnit.html)<br>The time unit of the [repeatInterval](repeat-interval.md). |
