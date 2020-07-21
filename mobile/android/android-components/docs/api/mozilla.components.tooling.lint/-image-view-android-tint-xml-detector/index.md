[android-components](../../index.md) / [mozilla.components.tooling.lint](../index.md) / [ImageViewAndroidTintXmlDetector](./index.md)

# ImageViewAndroidTintXmlDetector

`class ImageViewAndroidTintXmlDetector` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/tooling/lint/src/main/java/mozilla/components/tooling/lint/ImageViewAndroidTintXmlDetector.kt#L26)

A custom lint check that prohibits not using the app:tint for ImageViews

### Constructors

| Name | Summary |
|---|---|
| [&lt;init&gt;](-init-.md) | `ImageViewAndroidTintXmlDetector()`<br>A custom lint check that prohibits not using the app:tint for ImageViews |

### Functions

| Name | Summary |
|---|---|
| [appliesTo](applies-to.md) | `fun appliesTo(folderType: <ERROR CLASS>): `[`Boolean`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-boolean/index.html) |
| [getApplicableElements](get-applicable-elements.md) | `fun getApplicableElements(): `[`Collection`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin.collections/-collection/index.html)`<`[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`>?` |
| [visitElement](visit-element.md) | `fun visitElement(context: <ERROR CLASS>, element: `[`Element`](https://kotlinlang.org/api/latest/jvm/stdlib/org.w3c.dom/-element/index.html)`): `[`Unit`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-unit/index.html) |

### Companion Object Properties

| Name | Summary |
|---|---|
| [APP_COMPAT_IMAGE_BUTTON](-a-p-p_-c-o-m-p-a-t_-i-m-a-g-e_-b-u-t-t-o-n.md) | `const val APP_COMPAT_IMAGE_BUTTON: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [APP_COMPAT_IMAGE_VIEW](-a-p-p_-c-o-m-p-a-t_-i-m-a-g-e_-v-i-e-w.md) | `const val APP_COMPAT_IMAGE_VIEW: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [ERROR_MESSAGE](-e-r-r-o-r_-m-e-s-s-a-g-e.md) | `const val ERROR_MESSAGE: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [FULLY_QUALIFIED_APP_COMPAT_IMAGE_BUTTON](-f-u-l-l-y_-q-u-a-l-i-f-i-e-d_-a-p-p_-c-o-m-p-a-t_-i-m-a-g-e_-b-u-t-t-o-n.md) | `const val FULLY_QUALIFIED_APP_COMPAT_IMAGE_BUTTON: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [FULLY_QUALIFIED_APP_COMPAT_VIEW_CLASS](-f-u-l-l-y_-q-u-a-l-i-f-i-e-d_-a-p-p_-c-o-m-p-a-t_-v-i-e-w_-c-l-a-s-s.md) | `const val FULLY_QUALIFIED_APP_COMPAT_VIEW_CLASS: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |
| [ISSUE_XML_SRC_USAGE](-i-s-s-u-e_-x-m-l_-s-r-c_-u-s-a-g-e.md) | `val ISSUE_XML_SRC_USAGE: <ERROR CLASS>` |
| [SCHEMA](-s-c-h-e-m-a.md) | `const val SCHEMA: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) |

### Extension Functions

| Name | Summary |
|---|---|
| [loadResourceAsString](../../mozilla.components.support.test.file/kotlin.-any/load-resource-as-string.md) | `fun `[`Any`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html)`.loadResourceAsString(path: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`): `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)<br>Loads a file from the resources folder and returns its content as a string object. |
