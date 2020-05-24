[android-components](../index.md) / [mozilla.components.support.utils](index.md) / [logElapsedTime](./log-elapsed-time.md)

# logElapsedTime

`inline fun <T> logElapsedTime(logger: `[`Logger`](../mozilla.components.support.base.log.logger/-logger/index.md)`, op: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`T`](log-elapsed-time.md#T)`): `[`T`](log-elapsed-time.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/utils/src/main/java/mozilla/components/support/utils/Performance.kt#L20)

Executes the given [block](log-elapsed-time.md#mozilla.components.support.utils$logElapsedTime(mozilla.components.support.base.log.logger.Logger, kotlin.String, kotlin.Function0((mozilla.components.support.utils.logElapsedTime.T)))/block) and logs the elapsed time in milliseconds.
Uses [System.nanoTime](http://docs.oracle.com/javase/7/docs/api/java/lang/System.html#nanoTime()) for measurements, since it isn't tied to a wall-clock.

### Parameters

`logger` - [Logger](../mozilla.components.support.base.log.logger/-logger/index.md) to use for logging.

`op` - Name of the operation [block](log-elapsed-time.md#mozilla.components.support.utils$logElapsedTime(mozilla.components.support.base.log.logger.Logger, kotlin.String, kotlin.Function0((mozilla.components.support.utils.logElapsedTime.T)))/block) performs.

`block` - A lambda to measure.

**Return**
[T](log-elapsed-time.md#T) result of running [block](log-elapsed-time.md#mozilla.components.support.utils$logElapsedTime(mozilla.components.support.base.log.logger.Logger, kotlin.String, kotlin.Function0((mozilla.components.support.utils.logElapsedTime.T)))/block).

