# [Android Components](../../../README.md) > Feature > Prompts

A feature that will subscribe to the selected session and will handle all the common prompt dialogs from web content like select, option and menu html elements.

## Usage

### Setting up the dependency
Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

```Groovy
implementation "org.mozilla.components:feature-prompts:{latest-version}"
```

### PromptFeature

  ```kotlin

  val onNeedToRequestPermissions : ( Session, Array<String>, Int) -> Unit = {
    session, permissions, requestCode ->
    /* You are in charge of triggering the request for the permissions needed,
     * this way you can control, when you request the permissions,
     * in case that you want to show an informative dialog,
     * to clarify the use of these permissions.
     */
    this.requestPermissions(permissions, requestCode)
  }

  val promptFeature = PromptFeature(fragment = this,
    sessionManager = sessionManager,
    fragmentManager= fragmentManager,
    onNeedToRequestPermissions = onNeedToRequestPermissions
  )

  //It will start listing for new prompt requests for web content.
  promptFeature.start()

  //It will stop listing for future prompt requests for web content.
  promptFeature.stop()

  /* There some requests that are not handled with dialogs, instead they are delegated to other apps
   * to perform the request, an example is a file picker request, that delegates to the OS file picker.
   * For this reason, you have to forward the results of these requests to the prompt feature by overriding,
   * onActivityResult in your Activity or Fragment and forward its calls to promptFeature.onActivityResult.
   */
  override fun onActivityResult(requestCode: Int, resultCode: Int, data: Intent?) {
    promptFeature.onActivityResult(requestCode, resultCode, data)
  }

  /* Additionally, there are requests that need to have some runtime permission before they can be performed,
   * like file pickers request that need access to read the selected files. As onActivityResult you need to override
   * onRequestPermissionsResult in your Activity or Fragment and forward its
   * calls to promptFeature.onRequestPermissionsResult.
   */
  override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<String>, grantResults: IntArray) {
    promptFeature.onActivityResult(requestCode, resultCode, data)
  }
  ```

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
