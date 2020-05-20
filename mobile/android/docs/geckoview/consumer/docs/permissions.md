---
layout: default
title: Working with Site Permissions
summary: How to receive and respond to permission requests from websites.
nav_exclude: true
exclude: true
tags: [GeckoView,Gecko,mozilla,android,WebView,mobile,permissions,site,media,grant,notifications,storage,persistent storage,app]
---
# Working with Site Permissions

When a website wants to access certain services on a user's device, it will send out a permissions request. This document will explain how to use GeckoView to receive those requests, and respond to them by granting or denying those permissions.

## The Permission Delegate

The way an app interacts with site permissions in GeckoView is through the [`PermissionDelegate`][1]. There are three broad categories of permission that the `PermissionDelegate` handles, Android Permissions, Content Permissions and Media Permissions. All site permissions handled by GeckoView fall into one of these three categories.

To get notified about permission requests, you need to implement the `PermissionDelegate` interface:

```java
private class ExamplePermissionDelegate implements GeckoSession.PermissionDelegate {
    @Override
    public void onAndroidPermissionsRequest(final GeckoSession session, 
                                            final String[] permissions,
                                            final Callback callback) { }

    @Override
    public void onContentPermissionRequest(final GeckoSession session, 
                                           final String uri,
                                           final int type, final Callback callback) { }

    @Override
    public void onMediaPermissionRequest(final GeckoSession session, 
                                         final String uri,
                                         final MediaSource[] video, 
                                         final MediaSource[] audio,
                                         final MediaCallback callback) { }
}
```

You will then need to register the delegate with your [`GeckoSession`][3] instance.

```java
public class GeckoViewActivity extends AppCompatActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        ...

        final ExamplePermissionDelegate permission = new ExamplePermissionDelegate();
        session.setPermissionDelegate(permission);

        ...
    }
}
```

### Android Permissions

Android permissions are requested whenever a site wants access to a device's navigation or input capabilities.

The user will often need to grant these Android permissions to the app alongside granting the Content or Media site permissions.

When you receive an [`onAndroidPermissionsRequest`][2] call, you will also receive the `GeckoSession` the request was sent from, an array containing the permissions that are being requested, and a [`Callback`][4] to respond to the request. It is then up to the app to request those permissions from the device, which can be done using [`requestPermissions`][5]. 

Possible `permissions` values are; [`ACCESS_COARSE_LOCATION`][6], [`ACCESS_FINE_LOCATION`][7], [`CAMERA`][8] or [`RECORD_AUDIO`][9].

```java
private class ExamplePermissionDelegate implements GeckoSession.PermissionDelegate {
    private Callback mCallback;

    public void onRequestPermissionsResult(final String[] permissions,
                                           final int[] grantResults) {
        if (mCallback == null) { return; }

        final Callback cb = mCallback;
        mCallback = null;
        for (final int result : grantResults) {
            if (result != PackageManager.PERMISSION_GRANTED) {
                // At least one permission was not granted.
                cb.reject();
                return;
            }
        }
        cb.grant();
    }

    @Override
    public void onAndroidPermissionsRequest(final GeckoSession session, 
                                            final String[] permissions,
                                            final Callback callback) {
        mCallback = callback;
        requestPermissions(permissions, androidPermissionRequestCode);
    }
}

public class GeckoViewActivity extends AppCompatActivity {
    @Override
    public void onRequestPermissionsResult(final int requestCode,
                                           final String[] permissions,
                                           final int[] grantResults) {
        if (requestCode == REQUEST_PERMISSIONS ||
            requestCode == REQUEST_WRITE_EXTERNAL_STORAGE) {
            final ExamplePermissionDelegate permission = (ExamplePermissionDelegate)
                    getCurrentSession().getPermissionDelegate();
            permission.onRequestPermissionsResult(permissions, grantResults);
        } else {
            super.onRequestPermissionsResult(requestCode, permissions, grantResults);
        }
    }
}
```

### Content Permissions

Content permissions are requested whenever a site wants access to content that is stored on the device. The content permissions that can be requested through GeckoView are; [`Geolocation`][10], [`Site Notifications`][11] [`Persistent Storage`][12] and [`XR`][13] access.

When you receive an [`onContentPermissionRequest`][14] call, you will also receive the `GeckoSession` the request was sent from, the URI of the site that requested the permission, as a String, the type of the content permission requested (geolocation, site notification or persistent storage), and a [`Callback`][4] to respond to the request. It is then up to the app to present UI to the user asking for the permissions, and to notify GeckoView of the response via the `Callback`.

*Please note, in the case of `PERMISSION_DESKTOP_NOTIFICATION` and `PERMISSION_PERSISTENT_STORAGE`, GeckoView does not track accepted permissions and prevent further requests being sent for a particular site. It is therefore up to the calling app to do this if that is the desired behaviour. The code below demonstrates how to track storage permissions by site and track notification permission rejection for the whole app*

```java
private class ExamplePermissionDelegate implements GeckoSession.PermissionDelegate {
    private boolean showNotificationsRejected;
    private ArrayList<String> acceptedPersistentStorage = new ArrayList<String>();

    @Override
    public void onContentPermissionRequest(final GeckoSession session, 
                                           final String uri,
                                           final int type, 
                                           final Callback callback) {
        final int resId;
        Callback contentPermissionCallback = callback;
        if (PERMISSION_GEOLOCATION == type) {
            resId = R.string.request_geolocation;
        } else if (PERMISSION_DESKTOP_NOTIFICATION == type) {
            if (showNotificationsRejected) {
                callback.reject();
                return;
            }
            resId = R.string.request_notification;
        } else if (PERMISSION_PERSISTENT_STORAGE == type) {
            if (acceptedPersistentStorage.contains(uri)) {
                callback.grant();
                return;
            }
            resId = R.string.request_storage;
        } else if (PERMISSION_XR == type) {
            resId = R.string.request_xr;
        } else {    // unknown permission type
            callback.reject();
            return;
        }

        final String title = getString(resId, Uri.parse(uri).getAuthority());
        final AlertDialog.Builder builder = new AlertDialog.Builder(activity);
        builder.setTitle(title)
               .setNegativeButton(android.R.string.cancel, new DialogInterface.OnClickListener() {
                   @Override
                   public void onClick(final DialogInterface dialog, final int which) {
                        if (PERMISSION_DESKTOP_NOTIFICATION == type) {
                            showNotificationsRejected = false;
                        }
                       callback.reject();
                   }
               })
               .setPositiveButton(android.R.string.ok, new DialogInterface.OnClickListener() {
                   @Override
                   public void onClick(final DialogInterface dialog, final int which) {
                        if (PERMISSION_PERSISTENT_STORAGE == type) {
                            acceptedPersistentStorage.add(mUri);
                        } else if (PERMISSION_DESKTOP_NOTIFICATION == type) {
                            showNotificationsRejected = true;
                        }
                       callback.grant();
                   }
               });

        final AlertDialog dialog = builder.create();
        dialog.show();
    }
}
```

### Media Permissions
Media permissions are requested whenever a site wants access to play or record media from the device's camera and microphone.

When you receive an [`onMediaPermissionRequest`][15] call, you will also receive the `GeckoSession` the request was sent from, the URI of the site that requested the permission, as a String, the list of video devices available, if requesting video, the list of audio devices available, if requesting audio, and a [`MediaCallback`][17] to respond to the request. 

It is up to the app to present UI to the user asking for the permissions, and to notify GeckoView of the response via the `MediaCallback`.

*Please note, media permissions will still be requested if the associated device permissions have been denied if there are video or audio sources in that category that can still be accessed when listed. It is the responsibility of consumers to ensure that media permission requests are not displayed in this case.*

```java
private class ExamplePermissionDelegate implements GeckoSession.PermissionDelegate {
    @Override
    public void onMediaPermissionRequest(final GeckoSession session, 
                                         final String uri,
                                         final MediaSource[] video, 
                                         final MediaSource[] audio,
                                         final MediaCallback callback) {
        // Reject permission if Android permission has been previously denied.
        if ((audio != null
                && ContextCompat.checkSelfPermission(GeckoViewActivity.this,
                    Manifest.permission.RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED)
            || (video != null
                && ContextCompat.checkSelfPermission(GeckoViewActivity.this,
                    Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED)) {
            callback.reject();
            return;
        }

        final String host = Uri.parse(uri).getAuthority();
        final String title;
        if (audio == null) {
            title = getString(R.string.request_video, host);
        } else if (video == null) {
            title = getString(R.string.request_audio, host);
        } else {
            title = getString(R.string.request_media, host);
        }

        // Get the media device name from the `MediaDevice`
        String[] videoNames = normalizeMediaName(video); 
        String[] audioNames = normalizeMediaName(audio);

        final AlertDialog.Builder builder = new AlertDialog.Builder(activity);

        // Create drop down boxes to allow users to select which device to grant permission to
        final LinearLayout container = addStandardLayout(builder, title, null);
        final Spinner videoSpinner;
        if (video != null) {
            videoSpinner = addMediaSpinner(builder.getContext(), container, video, videoNames); // create spinner and add to alert UI
        } else {
            videoSpinner = null;
        }

        final Spinner audioSpinner;
        if (audio != null) {
            audioSpinner = addMediaSpinner(builder.getContext(), container, audio, audioNames); // create spinner and add to alert UI
        } else {
            audioSpinner = null;
        }

        builder.setNegativeButton(android.R.string.cancel, null)
               .setPositiveButton(android.R.string.ok,
                                  new DialogInterface.OnClickListener() {
                    @Override
                    public void onClick(final DialogInterface dialog, final int which) {
                        // gather selected media devices and grant access
                        final MediaSource video = (videoSpinner != null)
                                ? (MediaSource) videoSpinner.getSelectedItem() : null;
                        final MediaSource audio = (audioSpinner != null)
                                ? (MediaSource) audioSpinner.getSelectedItem() : null;
                        callback.grant(video, audio);
                    }
                });

        final AlertDialog dialog = builder.create();
        dialog.setOnDismissListener(new DialogInterface.OnDismissListener() {
                    @Override
                    public void onDismiss(final DialogInterface dialog) {
                        callback.reject();
                    }
                });
        dialog.show();
    }
}
```

To see the `PermissionsDelegate` in action, you can find the full example implementation in the [GeckoView example app][16].

[1]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html
[2]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#onAndroidPermissionsRequest-org.mozilla.geckoview.GeckoSession-java.lang.String:A-org.mozilla.geckoview.GeckoSession.PermissionDelegate.Callback-
[3]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.html
[4]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.Callback.html
[5]: https://developer.android.com/reference/android/app/Activity#requestPermissions(java.lang.String%5B%5D,%2520int)
[6]: https://developer.android.com/reference/android/Manifest.permission.html#ACCESS_COARSE_LOCATION
[7]: https://developer.android.com/reference/android/Manifest.permission.html#ACCESS_FINE_LOCATION
[8]: https://developer.android.com/reference/android/Manifest.permission.html#CAMERA
[9]: https://developer.android.com/reference/android/Manifest.permission.html#RECORD_AUDIO
[10]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_GEOLOCATION
[11]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_DESKTOP_NOTIFICATION
[12]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_PERSISTENT_STORAGE
[13]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_XR
[14]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#onContentPermissionRequest-org.mozilla.geckoview.GeckoSession-java.lang.String-int-org.mozilla.geckoview.GeckoSession.PermissionDelegate.Callback-
[15]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#onMediaPermissionRequest-org.mozilla.geckoview.GeckoSession-java.lang.String-org.mozilla.geckoview.GeckoSession.PermissionDelegate.MediaSource:A-org.mozilla.geckoview.GeckoSession.PermissionDelegate.MediaSource:A-org.mozilla.geckoview.GeckoSession.PermissionDelegate.MediaCallback-
[16]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.MediaCallback.html
[17]: https://searchfox.org/mozilla-central/source/mobile/android/geckoview_example/src/main/java/org/mozilla/geckoview_example/GeckoViewActivity.java#686
