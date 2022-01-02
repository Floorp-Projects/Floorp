.. -*- Mode: rst; fill-column: 80; -*-

=============================
Working with Site Permissions
=============================

When a website wants to access certain services on a user’s device, it
will send out a permissions request. This document will explain how to
use GeckoView to receive those requests, and respond to them by granting
or denying those permissions.

.. contents:: :local:

The Permission Delegate
-----------------------

The way an app interacts with site permissions in GeckoView is through
the
`PermissionDelegate <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html>`_.
There are three broad categories of permission that the
``PermissionDelegate`` handles, Android Permissions, Content Permissions
and Media Permissions. All site permissions handled by GeckoView fall
into one of these three categories.

To get notified about permission requests, you need to implement the
``PermissionDelegate`` interface:

.. code:: java

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

You will then need to register the delegate with your
`GeckoSession <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.html>`_
instance.

.. code:: java

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

Android Permissions
~~~~~~~~~~~~~~~~~~~

Android permissions are requested whenever a site wants access to a
device’s navigation or input capabilities.

The user will often need to grant these Android permissions to the app
alongside granting the Content or Media site permissions.

When you receive an
`onAndroidPermissionsRequest <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#onAndroidPermissionsRequest-org.mozilla.geckoview.GeckoSession-java.lang.String:A-org.mozilla.geckoview.GeckoSession.PermissionDelegate.Callback->`_
call, you will also receive the ``GeckoSession`` the request was sent
from, an array containing the permissions that are being requested, and
a
`Callback`_
to respond to the request. It is then up to the app to request those
permissions from the device, which can be done using
`requestPermissions <https://developer.android.com/reference/android/app/Activity#requestPermissions(java.lang.String%5B%5D,%2520int)>`_.

Possible ``permissions`` values are;

`ACCESS_COARSE_LOCATION <https://developer.android.com/reference/android/Manifest.permission.html#ACCESS_COARSE_LOCATION>`_,

`ACCESS_FINE_LOCATION <https://developer.android.com/reference/android/Manifest.permission.html#ACCESS_FINE_LOCATION>`_,

`CAMERA <https://developer.android.com/reference/android/Manifest.permission.html#CAMERA>`_

or

`RECORD_AUDIO <https://developer.android.com/reference/android/Manifest.permission.html#RECORD_AUDIO>`_.

.. code:: java

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

Content Permissions
~~~~~~~~~~~~~~~~~~~

Content permissions are requested whenever a site wants access to
content that is stored on the device. The content permissions that can
be requested through GeckoView are;

`Geolocation <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_GEOLOCATION>`_,

`Site Notifications <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_DESKTOP_NOTIFICATION>`_,

`Persistent Storage <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_PERSISTENT_STORAGE>`_,

`XR <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_XR>`_,

`Autoplay Inaudible <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_AUTOPLAY_INAUDIBLE>`_,

`Autoplay Audible <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_AUTOPLAY_AUDIBLE>`_,

and

`DRM Media <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_MEDIA_KEY_SYSTEM_ACCESS>`_

access. Additionally, `tracking protection exceptions <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#PERMISSION_TRACKING>`_
are treated as a type of content permission.

When you receive an
`onContentPermissionRequest <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#onContentPermissionRequest-org.mozilla.geckoview.GeckoSession-org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission->`_
call, you will also receive the ``GeckoSession`` the request was sent
from, and all relevant information about the permission being requested
stored in a `ContentPermission <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.ContentPermission.html>`_.
It is then up to the app to present UI to the user asking for the
permissions, and to notify GeckoView of the response via the returned
``GeckoResult``.

Once a permission has been set in this fashion, GeckoView will persist it
across sessions until it is cleared or modified. When a page is loaded,
the active permissions associated with it (both allowed and denied) will
be reported in `onLocationChange <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.NavigationDelegate.html#onLocationChange-org.mozilla.geckoview.GeckoSession-java.lang.String-java.util.List->`_
as a list of ``ContentPermission`` objects; additionally, one may check all stored
content permissions by calling `getAllPermissions <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/StorageController.html#getAllPermissions-->`_
and the content permissions associated with a given URI by calling
`getPermissions <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/StorageController.html#getPermissions-java.lang.String-java.lang.String->`_.
In order to modify an existing permission, you will need the associated
``ContentPermission`` (which can be retrieved from any of the above methods);
then, call `setPermission <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/StorageController.html#setPermission-org.mozilla.geckoview.GeckoSession.PermissionDelegate.ContentPermission-int->`_
with the desired new value, or `VALUE_PROMPT <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.ContentPermission.html#VALUE_PROMPT>`_
if you wish to unset the permission and let the site request it again in the future.

Media Permissions
~~~~~~~~~~~~~~~~~

Media permissions are requested whenever a site wants access to play or
record media from the device’s camera and microphone.

When you receive an
`onMediaPermissionRequest <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.html#onMediaPermissionRequest-org.mozilla.geckoview.GeckoSession-java.lang.String-org.mozilla.geckoview.GeckoSession.PermissionDelegate.MediaSource:A-org.mozilla.geckoview.GeckoSession.PermissionDelegate.MediaSource:A-org.mozilla.geckoview.GeckoSession.PermissionDelegate.MediaCallback->`_
call, you will also receive the ``GeckoSession`` the request was sent
from, the URI of the site that requested the permission, as a String,
the list of video devices available, if requesting video, the list of
audio devices available, if requesting audio, and a
`MediaCallback <https://searchfox.org/mozilla-central/source/mobile/android/geckoview_example/src/main/java/org/mozilla/geckoview_example/GeckoViewActivity.java#686>`_
to respond to the request.

It is up to the app to present UI to the user asking for the
permissions, and to notify GeckoView of the response via the
``MediaCallback``.

*Please note, media permissions will still be requested if the
associated device permissions have been denied if there are video or
audio sources in that category that can still be accessed when listed.
It is the responsibility of consumers to ensure that media permission
requests are not displayed in this case.*

.. code:: java

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

To see the ``PermissionsDelegate`` in action, you can find the full
example implementation in the `GeckoView example
app <https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.MediaCallback.html>`_.

.. _Callback: https://mozilla.github.io/geckoview/https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.PermissionDelegate.Callback.html