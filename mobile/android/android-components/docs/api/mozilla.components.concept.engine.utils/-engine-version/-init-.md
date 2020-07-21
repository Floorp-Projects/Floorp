[android-components](../../index.md) / [mozilla.components.concept.engine.utils](../index.md) / [EngineVersion](index.md) / [&lt;init&gt;](./-init-.md)

# &lt;init&gt;

`EngineVersion(major: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, minor: `[`Int`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-int/index.html)`, patch: `[`Long`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-long/index.html)`, metadata: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`? = null)`

Data class for engine versions using semantic versioning (major.minor.patch).

### Parameters

`major` - Major version number

`minor` - Minor version number

`patch` - Patch version number

`metadata` - Additional and optional metadata appended to the version number, e.g. for a version number of
"68.0a1" [metadata](metadata.md) will contain "a1".