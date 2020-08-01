[android-components](../../index.md) / [mozilla.components.tooling.lint](../index.md) / [TextViewAndroidSrcXmlDetector](./index.md)

# TextViewAndroidSrcXmlDetector

`class TextViewAndroidSrcXmlDetector` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/tooling/lint/src/main/java/mozilla/components/tooling/lint/TextViewAndroidSrcXmlDetector.kt#L28)

A custom lint check that prohibits not using the app:srcCompat for ImageViews

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `TextViewAndroidSrcXmlDetector()`<br>A custom lint check that prohibits not using the app:srcCompat for ImageViews |

### Functions

| Name | Summary |
|---|---|
| [appliesTo](applies-to.md) | `fun appliesTo(folderType: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [getApplicableElements](get-applicable-elements.md) | `fun getApplicableElements(): `[`Collection`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-collection/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?` |
| [visitElement](visit-element.md) | `fun visitElement(context: <ERROR CLASS>, element: `[`Element`](https://kotlinlang.org/api/latest/jvm/stdlib/org.w3c.dom/-element/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [ERROR_MESSAGE](-e-r-r-o-r_-m-e-s-s-a-g-e.md) | `const val ERROR_MESSAGE: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [ISSUE_XML_SRC_USAGE](-i-s-s-u-e_-x-m-l_-s-r-c_-u-s-a-g-e.md) | `val ISSUE_XML_SRC_USAGE: <ERROR CLASS>` |
| [SCHEMA](-s-c-h-e-m-a.md) | `const val SCHEMA: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
