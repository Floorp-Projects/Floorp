[android-components](../../index.md) / [mozilla.components.lib.jexl](../index.md) / [Jexl](index.md) / [addTransform](./add-transform.md)

# addTransform

`fun addTransform(name: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, transform: `[`Transform`](../../mozilla.components.lib.jexl.evaluator/-transform.md)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/lib/jexl/src/main/java/mozilla/components/lib/jexl/Jexl.kt#L34)

Adds or replaces a transform function in this Jexl instance.

### Parameters

`name` - The name of the transform function, as it will be used within Jexl expressions.

`transform` - The function to be executed when this transform is invoked. It will be
    provided with two arguments:
     - value: The value to be transformed
     - arguments: The list of arguments for this transform.