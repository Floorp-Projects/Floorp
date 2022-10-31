# [Android Components](../../../README.md) > Feature > Downloads

Feature implementation for apps that want to use [Android downloads manager](https://developer.android.com/reference/android/app/DownloadManager).

## Usage

### Setting up the dependency

Use Gradle to download the library from [maven.mozilla.org](https://maven.mozilla.org/) ([Setup repository](../../../README.md#maven-repository)):

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
    fragmentManager /*Optional, if it is provided, before every download a dialog will be shown*/,
    dialog /*Optional, if it is not provided a simple dialog will be shown before every download, with a positive button and negative button.*/,
    sessionManager = sessionManager)

//Starts observing the selected session for new downloads and forward it to
// the download manager
downloadsFeature.start()

//Stop observing the selected session
downloadsFeature.stop()

```

### DownloadDialogFragment
 This is general representation of a dialog meant to be used in collaboration with `DownloadsFeature`
 to show a dialog before a download is triggered. If `SimpleDownloadDialogFragment` is not flexible enough for your use case you should inherit for this class.

```kotlin
class FocusDialogDownloadFragment : DownloadDialogFragment() {

    /*Creating a customized the dialog*/
    override fun onCreateDialog(bundle: Bundle?): AlertDialog {
        //DownloadsFeature will add these metadata before calling show() on the dialog.
        val fileName = arguments?.getString(KEY_FILE_NAME)
        //Not used, just for the sake you can use this metadata
        val url = arguments?.getString(KEY_URL)
        val contentLength = arguments?.getString(KEY_CONTENT_LENGTH)

        val builder = AlertDialog.Builder(requireContext())
        builder.setCancelable(true)
        builder.setTitle(getString(R.string.download_dialog_title))

        val inflater = activity!!.layoutInflater
        val dialogView = inflater.inflate(R.layout.download_dialog, null)
        builder.setView(dialogView)

        dialogView.download_dialog_icon.setImageResource(R.drawable.ic_download)
        dialogView.download_dialog_file_name.text = fileName
        dialogView.download_dialog_cancel.text = getString(R.string.download_dialog_action_cancel)
        dialogView.download_dialog_download.text =
            getString(R.string.download_dialog_action_download)

        dialogView.download_dialog_warning.text = getString(R.string.download_dialog_warning)

        setCancelButton(dialogView.download_dialog_cancel)
        setDownloadButton(dialogView.download_dialog_download)

        return builder.create()
    }

    private fun setDownloadButton(button: Button) {
        button.setOnClickListener {
            //Letting know DownloadFeature that can proceed with the download
            onStartDownload()
            TelemetryWrapper.downloadDialogDownloadEvent(true)
            dismiss()
        }
    }

    private fun setCancelButton(button: Button) {
        button.setOnClickListener {
           TelemetryWrapper.downloadDialogDownloadEvent(false)
            dismiss()
        }
    }
}

//Adding our dialog to DownloadsFeature
val downloadsFeature = DownloadsFeature(
            context(),
            sessionManager = sessionManager,
            fragmentManager = fragmentManager,
            dialog = FocusDialogDownloadFragment()
        )

downloadsFeature.start()
```

### SimpleDownloadDialogFragment

A confirmation dialog to be called before a download is triggered.

SimpleDownloadDialogFragment is the default dialog if you don't provide a value to DownloadsFeature.
It is composed by a title, a negative and a positive bottoms. When the positive button is clicked the download is triggered.

```kotlin
//To use the default behavior, just provide a fragmentManager/childFragmentManager.
        downloadsFeature = DownloadsFeature(
            requireContext(),
            sessionManager = components.sessionManager,
            fragmentManager = fragmentManager /*If you're inside a Fragment use childFragmentManager '*/
        )

        downloadsFeature.start()
```
Customizing SimpleDownloadDialogFragment.

```kotlin
        val dialog = SimpleDownloadDialogFragment.newInstance(
                dialogTitleText = R.string.dialog_title,
                positiveButtonText = R.string.download,
                negativeButtonText = R.string.cancel,
                cancelable = true,
                themeResId = R.style.your_theme
            )

        downloadsFeature = DownloadsFeature(
            requireContext(),
            sessionManager = components.sessionManager,
            fragmentManager = fragmentManager,
            dialog = dialog
        )

        downloadsFeature.start()
  ```

## Facts

This component emits the following [Facts](../../support/base/README.md#Facts):

| Action     | Item            |  Description                                      |
|------------|-----------------|---------------------------------------------------|
| RESUME     | notification    | The user resumes a download.                      |
| PAUSE      | notification    | The user pauses a download.                       |
| CANCEL     | notification    | The user cancels a download.                      |
| TRY_AGAIN  | notification    | The user taps on try again when a download fails. |
| OPEN       | notification    | The user opens a downloaded file.                 |

## License

    This Source Code Form is subject to the terms of the Mozilla Public
    License, v. 2.0. If a copy of the MPL was not distributed with this
    file, You can obtain one at http://mozilla.org/MPL/2.0/
