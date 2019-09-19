[android-components](../../index.md) / [mozilla.components.service.glean.net](../index.md) / [ConceptFetchHttpUploader](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`ConceptFetchHttpUploader(client: `[`Lazy`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-lazy/index.html)`<`[`Client`](../../mozilla.components.concept.fetch/-client/index.md)`>)`

A simple ping Uploader, which implements a "send once" policy, never
storing or attempting to send the ping again. This uses Android Component's
`concept-fetch`.

