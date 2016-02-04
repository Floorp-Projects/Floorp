.. -*- Mode: rst; fill-column: 100; -*-

=========================================
 The Firefox for Android install bouncer
=========================================

`Bug 1234629 <https://bugzilla.mozilla.org/show_bug.cgi?id=1234629>`_ and `Bug 1163082
<https://bugzilla.mozilla.org/show_bug.cgi?id=1163082>`_ combine to allow building a very small
Fennec-like "bouncer" APK that redirects (bounces) a potential Fennec user to the marketplace of
their choice -- usually the Google Play Store -- to install the real Firefox for Android application
APK.

The real APK should install seamlessly over top of the bouncer APK.  Care is taken to keep the
bouncer and application APK <permission> manifest definitions identical, and to have the bouncer APK
<activity> manifest definitions look similar to the application APK <activity> manifest definitions.

In addition, the bouncer APK can carry a Fennec distribution, which it copies onto the device before
redirecting to the marketplace.  The application APK recognizes the installed distribution and
customizes itself accordingly on first run.

The motivation is to allow partners to pre-install the very small bouncer APK on shipping devices
and to have a smooth path to upgrade to the full application APK, with a partner-specific
distribution in place.

Technical details
=================

To build the bouncer APK, define ``MOZ_ANDROID_PACKAGE_INSTALL_BOUNCER``.  To pack a distribution
into the bouncer APK (and *not* into the application APK), add a line like::

  ac_add_options --with-android-distribution-directory=/path/to/fennec-distribution-sample

to your ``mozconfig`` file.  See the `general distribution documentation on the wiki
<https://wiki.mozilla.org/Mobile/Distribution_Files>`_ for more information.

The ``distribution`` directory should end up in the ``assets/distribution`` directory of the bouncer
APK.  It will be copied into ``/data/data/$ANDROID_PACKAGE_NAME/distribution`` when the bouncer
executes.
