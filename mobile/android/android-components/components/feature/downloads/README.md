# [Android Components](../../../README.md) > Feature > Downloads

Feature implementation for apps that want to use [Android downloads manager](https://developer.android.com/reference/android/app/DownloadManager).

## Usage

### Setting up the dependency

Use gradle to download the library from JCenter:

```Groovy
implementation "org.mozilla.components:feature-downloads:{latest-version}"
```

### DownloadsFeature
Feature implementation for proving download functionality for the selected session.

```kotlin


//This will be called before starting each download in case you don't have the right permissions
// or if the user removed your permissions.
val onNeedToRequestPermissions = {
        session: Session, download: Download ->
    //request permission e.g (WRITE_EXTERNAL_STORAGE)

    // After you get granted the permissions, remember to call downloadsFeature.onPermissionsGranted()
    // to start downloading the pending download.
}

//This will be called after every download is completed.
val onDownloadCompleted = {
        download: Download, downloadId: Long ->
    //Show some UI to let user know the download was completed.
}

val downloadsFeature =
    DownloadsFeature(context,
    onNeedToRequestPermissions /*Optional*/,
    onDownloadCompleted /*Optional*/,
    sessionManager = sessionManager)

//Starts observing the selected session for new downloads and forward it to
// the download manager
downloadsFeature.start()

//Stop observing the selected session
downloadsFeature.stop()

```

### DownloadManager
It is our representation to of the [Android downloads manager](https://developer.android.com/reference/android/app/DownloadManager). It allows you to download files and be subscribed for download events.

```kotlin
 val onDownloadComplete = { download: Download, downloadId: Long ->
    //Show some UI.
}

val manager = DownloadManager(applicationContext, onDownloadComplete /*Optional*/)

val download = Download(
    "http://ipv4.download.thinkbroadband.com/5MB.zip",
    null, "application/zip", 5242880,
    "Mozilla/5.0 (Linux; Android 7.1.1) AppleWebKit/537.36 (KHTML, like Gecko) Version/4.0 Focus/8.0 Chrome/69.0.3497.100 Mobile Safari/537.36"
)

val refererURL = "https://www.mozilla.org/"
val cookie = "yummy_cookie=choco"

val downloadId = manager.download(
    download,
    refererURL /*Optional*/,
    cookie /*Optional*/
)

//If you want to stop listening.
manager.unregisterListener()
```



## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
