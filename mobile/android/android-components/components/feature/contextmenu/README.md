# [Android Components](../../../README.md) > Feature > Context Menu

A component for displaying context menus when *long-pressing* web content.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-contextmenu:{latest-version}"
```

### Integration

`ContextMenuFeature` subscribes to the selected `Session` automatically and displays context menus when web content is `long-pressed`.

Initializing the feature in a [Fragment](https://developer.android.com/reference/androidx/fragment/app/Fragment) (`onViewCreated`) or in an [Activity](https://developer.android.com/reference/android/app/Activity) (`onCreate`):

```Kotlin
contextMenuFeature = ContextMenuFeature(
    fragmentManager,
    sessionManager,

    // Use default context menu items:
    ContextMenuCandidate.defaultCandidates(context, tabsUseCases, snackbarParentView)
)
```

### Forwarding lifecycle events

Start/Stop events need to be forwarded to the feature:

```Kotlin
// From onStart():
feature.start()

// From onStop():
feature.stop()
```

### Customizing context menu items

When initializing the feature a list of `ContextMenuCandidate` objects need to be passed to the feature. Instead of using the default list (`ContextMenuCandidate.defaultCandidates()`) a customized list can be passed to the feature.

For every observed `HitResult` (`Session.Observer.onLongPress()`) the feature will query all candidates (`ContextMenuCandidate.showFor()`) in order to determine which candidates want to show up in the context menu. If a context menu item was selected by the user the feature will invoke the `ContextMenuCandidate.action()` method of the related candidate.

`ContextMenuCandidate` contains methods (`create*()`) for creating a variety of standard context menu items that can be used when customizing the list.

```Kotlin
val customCandidates = listOf(
    // Item from the list of standard items
    ContextMenuCandidate.createOpenInNewTabCandidate(context, tabsUseCases),

    // Custom item
    object : ContextMenuCandidate(
        id = "org.mozilla.custom.contextmenu.toast",
        label = "Show a toast",
        showFor = { session, hitResult -> hitResult.src.isNotEmpty() },
        action = { session, hitResult ->
            Toast.makeText(context, hitResult.src, Toast.LENGTH_SHORT).show()
        }
    )
)

contextMenuFeature = ContextMenuFeature(
    fragmentManager,
    sessionManager,
    customCandidates)
```

## Facts

This component emits the following [Facts](../../support/base/README.md#Facts):

| Action | Item    | Extras         | Description                              |
|--------|---------|----------------|------------------------------------------|
| CLICK  | item    | `itemExtras`   | The user clicked on a context menu item. |


#### `itemExtras`

| Key          | Type    | Value                                      |
|--------------|---------|--------------------------------------------|
| item         | String  | The `id` of the menu item that was clicked |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
