.. -*- Mode: rst; fill-column: 100; -*-

======================================
 Install tracking with the Adjust SDK
======================================

Fennec (Firefox for Android) tracks certain types of installs using a third party install tracking
framework called Adjust.  The intention is to determine the origin of Fennec installs by answering
the question, "Did this user on this device install Fennec in response to a specific advertising
campaign performed by Mozilla?"

Mozilla is using a third party framework in order to answer this question for the Firefox for
Android 38.0.5 release.  We hope to remove the framework from Fennec in the future.

The framework consists of a software development kit (SDK) built into Fennec and a
data-collecting Internet service backend run by the German company `adjust GmbH`_.  The Adjust SDK
is open source and MIT licensed: see the `github repository`_.  Fennec ships a copy of the SDK
(currently not modified from upstream) in ``mobile/android/thirdparty/com/adjust/sdk``.  The SDK is
documented at https://docs.adjust.com.

Data collection
~~~~~~~~~~~~~~~

When is data collected and sent to the Adjust backend?
======================================================

Data is never collected (or sent to the Adjust backend) unless

* the Fennec binary is an official Mozilla binary [#official]_; and
* the release channel is Release or Beta [#channel]_.

If both of the above conditions are true, then data is collected and sent to the Adjust backend in
the following two circumstances: first, when

* Fennec is started on the device [#started]_.

Second, when

* the Fennec binary was installed from the Google Play Store; and
* the Google Play Store sends the installed Fennec binary an `INSTALL_REFERRER Intent`_, and the
  received Intent includes Google Play Store campaign tracking information.  This happens when thea
  Google Play Store install is in response to a campaign-specific Google Play Store link.  For
  details, see the developer documentation at
  https://developers.google.com/analytics/devguides/collection/android/v4/campaigns.

In these two limited circumstances, data is collected and sent to the Adjust backend.

Where does data sent to the Adjust backend go?
==============================================

The Adjust SDK is hard-coded to send data to the endpoint https://app.adjust.com.  The endpoint is
defined by ``com.adjust.sdk.Constants.BASE_URL`` at
https://hg.mozilla.org/mozilla-central/file/f76f02793f7a/mobile/android/thirdparty/com/adjust/sdk/Constants.java#l27.

The Adjust backend then sends a limited subset of the collected data -- limited but sufficient to
uniquely identify the submitting device -- to a set of advertising network providers that Mozilla
elects to share the collected data with.  Those advertising networks then confirm or deny that the
identifying information corresponds to a specific advertising campaign performed by Mozilla.

What data is collected and sent to the Adjust backend?
======================================================

The Adjust SDK collects and sends two messages to the Adjust backend.  The messages have the
following parameters::

  V/Adjust  ( 6508): Parameters:
  V/Adjust  ( 6508): 	screen_format    normal
  V/Adjust  ( 6508): 	device_manufacturer samsung
  V/Adjust  ( 6508): 	session_count    1
  V/Adjust  ( 6508): 	device_type      phone
  V/Adjust  ( 6508): 	screen_size      normal
  V/Adjust  ( 6508): 	package_name     org.mozilla.firefox
  V/Adjust  ( 6508): 	app_version      39.0a1
  V/Adjust  ( 6508): 	android_uuid     <guid>
  V/Adjust  ( 6508): 	display_width    720
  V/Adjust  ( 6508): 	country          GB
  V/Adjust  ( 6508): 	os_version       18
  V/Adjust  ( 6508): 	needs_attribution_data 0
  V/Adjust  ( 6508): 	environment      sandbox
  V/Adjust  ( 6508): 	device_name      Galaxy Nexus
  V/Adjust  ( 6508): 	os_name          android
  V/Adjust  ( 6508): 	tracking_enabled 1
  V/Adjust  ( 6508): 	created_at       2015-03-24T17:53:38.452Z-0400
  V/Adjust  ( 6508): 	app_token        <private>
  V/Adjust  ( 6508): 	screen_density   high
  V/Adjust  ( 6508): 	language         en
  V/Adjust  ( 6508): 	display_height   1184
  V/Adjust  ( 6508): 	gps_adid         <guid>

  V/Adjust  ( 6508): Parameters:
  V/Adjust  ( 6508): 	needs_attribution_data 0
  V/Adjust  ( 6508): 	app_token        <private>
  V/Adjust  ( 6508): 	environment      production
  V/Adjust  ( 6508): 	android_uuid     <guid>
  V/Adjust  ( 6508): 	tracking_enabled 1
  V/Adjust  ( 6508): 	gps_adid         <guid>

The available parameters (including ones not exposed to Mozilla) are documented at
https://partners.adjust.com/placeholders/.

Notes on what data is collected
-------------------------------

The *android_uuid* uniquely identifies the device.

The *gps_adid* is a Google Advertising ID.  It is capable of uniquely identifying a device to any
advertiser, across all applications.  If a Google Advertising ID is not available, Adjust may fall
back to an Android ID, or, as a last resort, the device's WiFi MAC address.

The *tracking_enabled* flag is only used to allow or disallow contextual advertising to be sent to a
user. It can be, and is, ignored for general install tracking of the type Mozilla is using the
Adjust SDK for.  (This flag might be used by consumers using the Adjust SDK to provide in-App
advertising.)

It is not clear how much entropy their is in the set of per-device parameters that do not
*explicitly* uniquely identify the device.  That is, it is not known if the device parameters are
likely to uniquely fingerprint the device, in the way that user agent capabilities are likely to
uniquely fingerprint the user.

Technical notes
~~~~~~~~~~~~~~~

Build flags controlling the Adjust SDK integration
==================================================

Add the following to your mozconfig to compile with the Adjust SDK::

 export MOZ_INSTALL_TRACKING=1
 export MOZ_NATIVE_DEVICES=1
 ac_add_options --with-adjust-sdk-keyfile="$topsrcdir/mobile/android/base/adjust-sdk-sandbox.token"

``MOZ_NATIVE_DEVICES`` is required to provide some Google Play Services dependencies.

No trace of the Adjust SDK should be present in Fennec if
``MOZ_INSTALL_TRACKING`` is not defined.

Access to the Adjust backend is controlled by a private App-specific
token. Fennec's token is managed by Release Engineering and should not
be exposed if at all possible; for example, it should *not* leak to build
logs.  The value of the token is read from the file specified using the
``configure`` flag ``--with-adjust-sdk-keyfile=KEYFILE`` and stored in
the build variable ``MOZ_ADJUST_SDK_KEY``. The mozconfig specified above
defaults to submitting data to a special Adjust sandbox allowing a
developer to test Adjust without submitting false data to our backend.

We throw an assertion if ``MOZ_INSTALL_TRACKING`` is specified but
``--with-adjust-sdk-keyfile`` is not to ensure our builders have a proper
adjust token for release and beta builds.  It's great to catch some
errors at compile-time rather than in release. That being said, ideally
we'd specify a default ``--with-adjust-sdk-keyfile`` for developer builds
but I don't know how to do that.

Technical notes on the Adjust SDK integration
=============================================

The *Adjust install tracking SDK* is a pure-Java library that is conditionally compiled into Fennec.
It's not trivial to integrate such conditional feature libraries into Fennec without pre-processing.
To minimize such pre-processing, we define a trivial ``AdjustHelperInterface`` and define two
implementations: the real ``AdjustHelper``, which requires the Adjust SDK, and a no-op
``StubAdjustHelper``, which has no additional requirements.  We use the existing pre-processed
``AppConstants.java.in`` to switch, at build-time, between the two implementations.

Notes and links
===============

.. _adjust GmbH: http://www.adjust.com
.. _github repository: https://github.com/adjust/android_sdk
.. [#official] Data is not sent for builds not produced by Mozilla: this would include
  redistributors such as the Palemoon project.
.. [#channel] Data is not sent for Aurora, Nightly, or custom builds.
.. [#started] *Started* means more than just when the user taps the Fennec icon or otherwise causes
  the Fennec user interface to appear directly.  It includes, for example, when a Fennec service
  (like the Update Service, or Background Sync), starts and Fennec was not previously running on the
  device.  See http://developer.android.com/reference/android/app/Application.html#onCreate%28%29
  for details.
.. _INSTALL_REFERRER Intent: https://developer.android.com/reference/com/google/android/gms/tagmanager/InstallReferrerReceiver.html
