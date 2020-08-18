# [Android Components](../../../README.md) > Browser > Thumbnails

A component for loading and storing website thumbnails (screenshot of the website).

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:browser-thumbnails:{latest-version}"
```

## Requesting thumbnails

To get thumbnail images from the browser, we need to request them from the `EngineView`. This can
be done easily by using `BrowserThumbnails` which will then notify the `BrowserStore` when a
thumbnail has been received:

```kotlin
browserThumbnails.set(
    feature = BrowserThumbnails(context, layout.engineView, browserStore),
    owner = this,
    view = layout
)
```

`BrowserThumbnails` tries to make requests as frequent as possible in order to get the most
up-to-date state of the site in the images.

The various situations when we try to request a thumbnail:
 - During the Android lifecycle event `onStart`.
 - When the selected tab's `loading` state changes.
 - More to be added..

## Storing to disk

When we receive new thumbnails, we may want to persist them to disk as these images can be quite large.

To do this, we need to add the `BrowserMiddleware` to receive the image from the store 
and put it in our storage:

```kotlin
val thumbnailStorage by lazy { ThumbnailStorage(applicationContext) }

val store by lazy {
    BrowserStore(middleware = listOf(
        ThumbnailsMiddleware(thumbnailStorage)
    ))
}
```

## Loading from disk

Now that we have the thumbnails stored to disk, we can access them via the `ThumbnailStorage`
directly:

```kotlin
runBlocking {
    val bitmap = thumbnailStorage.loadThumbnail(
      request = ImageLoadRequest("thumbnailId", maxPreferredImageDimen)
    )
}
```

A better way, is to use the `ThumbnailLoader`:

```kotlin
val thumbnailLoader = ThumbnailLoader(components.thumbnailStorage)
thumbnailLoader.loadIntoView(
    view = thumbnailView,
    request = ImageLoadRequest(id = tab.id, size = thumbnailSize)
)
```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
