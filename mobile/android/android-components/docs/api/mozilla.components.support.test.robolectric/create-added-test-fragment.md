[android-components](../index.md) / [mozilla.components.support.test.robolectric](index.md) / [createAddedTestFragment](./create-added-test-fragment.md)

# createAddedTestFragment

`inline fun <T : Fragment> createAddedTestFragment(fragmentTag: `[`String`](https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-string/index.html)` = "test", fragmentFactory: () -> `[`T`](create-added-test-fragment.md#T)`): `[`T`](create-added-test-fragment.md#T) [(source)](https://github.com/mozilla-mobile/android-components/blob/master/components/support/test/src/main/java/mozilla/components/support/test/robolectric/Fragments.kt#L20)

Set up an added [Fragment](#) to a [FragmentActivity](#) that has been initialized to a resumed state.

### Parameters

`fragmentTag` - the name that will be used to tag the fragment inside the [FragmentManager](#).

`fragmentFactory` - a lambda function that returns a Fragment that will be added to the Activity.

**Return**
The same [Fragment](#) that was returned from [fragmentFactory](create-added-test-fragment.md#mozilla.components.support.test.robolectric$createAddedTestFragment(kotlin.String, kotlin.Function0((mozilla.components.support.test.robolectric.createAddedTestFragment.T)))/fragmentFactory) after it got added to the
Activity.

