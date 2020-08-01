[android-components](../../index.md) / [mozilla.components.support.migration](../index.md) / [FxaMigrationException](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`FxaMigrationException(failure: `[`Failure`](../-fxa-migration-result/-failure/index.md)`)`

Wraps [FxaMigrationResult](../-fxa-migration-result/index.md) in an exception so that it can be returned via [Result.Failure](../-result/-failure/index.md).

PII note - be careful to not log this exception, as it may contain personal information (wrapped in a JSONException).

