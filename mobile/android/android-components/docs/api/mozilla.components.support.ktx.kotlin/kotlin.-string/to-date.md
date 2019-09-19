[android-components](../../index.md) / [mozilla.components.support.ktx.kotlin](../index.md) / [kotlin.String](index.md) / [toDate](./to-date.md)

# toDate

`fun `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`.toDate(format: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`, locale: `[`Locale`](https://developer.android.com/reference/java/util/Locale.html)` = Locale.ROOT): `[`Date`](https://developer.android.com/reference/java/util/Date.html) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/kotlin/String.kt#L44)

Converts a [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) to a [Date](https://developer.android.com/reference/java/util/Date.html) object.

### Parameters

`format` - date format used for formatting the this given [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) object.

`locale` - the locale to use when converting the String, defaults to [Locale.ROOT](https://developer.android.com/reference/java/util/Locale.html#ROOT).

**Return**
a [Date](https://developer.android.com/reference/java/util/Date.html) object with the values in the provided in this string, if empty string was provided, a current date
will be returned.

`fun `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)`.toDate(vararg possibleFormats: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = arrayOf(
            "yyyy-MM-dd'T'HH:mm",
            "yyyy-MM-dd",
            "yyyy-'W'ww",
            "yyyy-MM",
            "HH:mm"
    )): `[`Date`](https://developer.android.com/reference/java/util/Date.html)`?` [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/ktx/src/main/java/mozilla/components/support/ktx/kotlin/String.kt#L71)

Tries to convert a [String](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html) to a [Date](https://developer.android.com/reference/java/util/Date.html) using a list of [possibleFormats](to-date.md#mozilla.components.support.ktx.kotlin$toDate(kotlin.String, kotlin.Array((kotlin.String)))/possibleFormats).

### Parameters

`possibleFormats` - one ore more possible format.

**Return**
a [Date](https://developer.android.com/reference/java/util/Date.html) object with the values in the provided in this string,
if the conversion is not possible null will be returned.

