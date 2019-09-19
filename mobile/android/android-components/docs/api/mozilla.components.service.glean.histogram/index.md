[android-components](../index.md) / [mozilla.components.service.glean.histogram](./index.md)

## Package mozilla.components.service.glean.histogram

### Types

| Name | Summary |
|---|---|
| [FunctionalHistogram](-functional-histogram/index.md) | `data class FunctionalHistogram`<br>This class represents a histogram where the bucketing is performed by a function, rather than pre-computed buckets. It is meant to help serialize and deserialize data to the correct format for transport and storage, as well as performing the calculations to determine the correct bucket for each sample. |
| [PrecomputedHistogram](-precomputed-histogram/index.md) | `data class PrecomputedHistogram`<br>This class represents the structure of a custom distribution. It is meant to help serialize and deserialize data to the correct format for transport and storage, as well as including helper functions to calculate the bucket sizes. |
