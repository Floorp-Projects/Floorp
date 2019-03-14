[android-components](../../../../index.md) / [mozilla.components.browser.session.manifest](../../../index.md) / [WebAppManifestParser](../../index.md) / [Result](../index.md) / [Failure](./index.md)

# Failure

`data class Failure : `[`Result`](../index.md) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/browser/session/src/main/java/mozilla/components/browser/session/manifest/WebAppManifestParser.kt#L30)

Parsing the JSON failed.

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `Failure(exception: `[`JSONException`](https://developer.android.com/reference/org/json/JSONException.html)`)`<br>Parsing the JSON failed. |

### Properties

| Name | Summary |
|---|---|
| [exception](exception.md) | `val exception: `[`JSONException`](https://developer.android.com/reference/org/json/JSONException.html)<br>The exception that was thrown while parsing the manifest. |
