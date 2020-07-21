[android-components](../../index.md) / [mozilla.components.support.base.log.logger](../index.md) / [Logger](index.md) / [measure](./measure.md)

# measure

`fun measure(message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/log/logger/Logger.kt#L55)
`fun measure(message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/base/src/main/java/mozilla/components/support/base/log/logger/Logger.kt#L100)

Measure the time it takes to execute the provided block and print a log message before and
after executing the block.

Example log message:
⇢ doSomething()
[..](#)
⇠ doSomething() [12ms](#)

