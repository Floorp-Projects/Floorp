[android-components](../../index.md) / [mozilla.components.service.experiments](../index.md) / [JSONExperimentParser](./index.md)

# JSONExperimentParser

`class JSONExperimentParser` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/service/experiments/src/main/java/mozilla/components/service/experiments/JSONExperimentParser.kt#L19)

Default JSON parsing implementation

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `JSONExperimentParser()`<br>Default JSON parsing implementation |

### Functions

| Name | Summary |
|---|---|
| [fromJson](from-json.md) | `fun fromJson(jsonObject: `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html)`): `[`Experiment`](../-experiment/index.md)<br>Creates an experiment from its json representation |
| [toJson](to-json.md) | `fun toJson(experiment: `[`Experiment`](../-experiment/index.md)`): `[`JSONObject`](https://developer.android.com/reference/org/json/JSONObject.html)<br>Converts the specified experiment to json |
