---
title: Logger.measure - 
---

[mozilla.components.support.base.log.logger](../index.html) / [Logger](index.html) / [measure](./measure.html)

# measure

`fun measure(message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)
`fun measure(message: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, block: () -> `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html)

Measure the time it takes to execute the provided block and print a log message before and
after executing the block.

Example log message:
⇢ doSomething()
[..](#)
⇠ doSomething() [12ms](#)

