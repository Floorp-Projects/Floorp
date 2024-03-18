# [Android Components](../../../README.md) > Feature > Site Permissions

A feature for showing site permission request prompts.

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-sitepermissions:{latest-version}"
```

### SitePermissionsFeature

 ```
 Add these permissions to your ``AndroidManifest.xml`` file.
 ```XML
 <uses-permission android:name="android.permission.CAMERA" />
 <uses-permission android:name="android.permission.RECORD_AUDIO" />
 <uses-permission android:name="android.permission.ACCESS_FINE_LOCATION"/>
 <uses-permission android:name="android.permission.ACCESS_COARSE_LOCATION"/>
 ```

 ```kotlin
  val onNeedToRequestPermissions : (Array<String>) -> Unit = { permissions ->
    /* You are in charge of triggering the request for the permissions needed,
     * this way you can control, when you request the permissions,
     * in case that you want to show an informative dialog,
     * to clarify the use of these permissions.
     */
    this.requestPermissions(permissions, REQUEST_CODE_APP_PERMISSIONS)
  }

  val sitePermissionsFeature = SitePermissionsFeature(
        anchorView = toolbar,
        sessionManager = components.sessionManager,
        fragmentManager = requireFragmentManager(),
        onNeedToRequestPermissions = onNeedToRequestPermissions
  )

    // It will start listing for new permissionRequest.
    sitePermissionsFeature.start()

    // It will stop listing for new permissionRequest.
    sitePermissionsFeature.stop()


  // Notify the feature if the permissions requested were granted or rejected.
  override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
    when (requestCode) {
         REQUEST_CODE_APP_PERMISSIONS -> sitePermissionsFeature.onPermissionsResult(grantResults)
    }
  }

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
