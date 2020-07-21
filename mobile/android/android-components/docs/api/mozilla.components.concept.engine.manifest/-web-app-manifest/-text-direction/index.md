[android-components](../../../index.md) / [mozilla.components.concept.engine.manifest](../../index.md) / [WebAppManifest](../index.md) / [TextDirection](./index.md)

# TextDirection

`enum class TextDirection` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/concept/engine/src/main/java/mozilla/components/concept/engine/manifest/WebAppManifest.kt#L151)

Specifies the primary text direction for the name, short_name, and description members. Together with the lang
member, it helps the correct display of right-to-left languages.

### Enum Values

| Name | Summary |
|---|---|
| [LTR](-l-t-r.md) | Left-to-right (LTR). |
| [RTL](-r-t-l.md) | Right-to-left (RTL). |
| [AUTO](-a-u-t-o.md) | If the value is set to auto, the browser will use the Unicode bidirectional algorithm to make a best guess about the text's direction. |
