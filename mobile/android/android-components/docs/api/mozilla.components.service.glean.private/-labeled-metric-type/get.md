[android-components](../../index.md) / [mozilla.components.service.glean.private](../index.md) / [LabeledMetricType](index.md) / [get](./get.md)

# get

`operator fun get(label: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`T`](index.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/LabeledMetricType.kt#L193)

Get the specific metric for a given label.

If a set of acceptable labels were specified in the metrics.yaml file,
and the given label is not in the set, it will be recorded under the
special [OTHER_LABEL](#).

If a set of acceptable labels was not specified in the metrics.yaml file,
only the first 16 unique labels will be used. After that, any additional
labels will be recorded under the special [OTHER_LABEL](#) label.

Labels must be snake_case and less than 30 characters. If an invalid label
is used, the metric will be recorded in the special [OTHER_LABEL](#) label.

### Parameters

`label` - The label

**Return**
The specific metric for that label

`operator fun get(labelIndex: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`): `[`T`](index.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/glean/src/main/java/mozilla/components/service/glean/private/LabeledMetricType.kt#L228)

Get the specific metric for a given label index.

This only works if a set of acceptable labels were specified in the
metrics.yaml file. If static labels were not defined in that file or
the index of the given label is not in the set, it will be recorded under
the special [OTHER_LABEL](#).

### Parameters

`labelIndex` - The label

**Return**
The specific metric for that label

