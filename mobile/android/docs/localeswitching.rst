.. -*- Mode: rst; fill-column: 80; -*-

====================================
 Runtime locale switching in Fennec
====================================

`Bug 917480 <https://bugzilla.mozilla.org/show_bug.cgi?id=917480>`_ built on `Bug 936756 <https://bugzilla.mozilla.org/show_bug.cgi?id=936756>`_ to allow users to switch between supported locales at runtime, within Fennec, without altering the system locale.

This document aims to describe the overall architecture of the solution, along with guidelines for Fennec developers.

Overview
========

There are two places that locales are relevant to an Android application: the Java ``Locale`` object and the Android configuration itself.

Locale switching involves manipulating these values (to affect future UI), persisting them for future activities, and selectively redisplaying existing UI elements to give the appearance of responsive switching.

The user's choice of locale is stored in a per-app pref, ``"locale"``. If missing, the system default locale is used. If set, it should be a locale code like ``"es"`` or ``"en-US"``.

``BrowserLocaleManager`` takes care of updating the active locale when asked to do so. It also manages persistence and retrieval of the locale preference.

The question, then, is when to do so.

Locale events
=============

One might imagine that we need only set the locale when our Application is instantiated, and when a new locale is set. Alas, that's not the case: whenever there's a configuration change (*e.g.*, screen rotation), when a new activity is started, and at other apparently random times, Android will supply our activities with a configuration that's been reset to the system locale.

For this reason, each starting activity must ask ``BrowserLocaleManager`` to fix its locale.

Ideally, we also need to perform some amount of work when our configuration changes, when our activity is resumed, and perhaps when a result is returned from another activity, if that activity can change the app locale (as is the case for any activity that calls out to ``GeckoPreferences`` -- see ``BrowserApp#onActivityResult``).

``GeckoApp`` itself does some additional work, because it has particular performance constraints, and also is the typical root of the preferences activity.

Here's an example of the work that a typical activity should do::

  // This is cribbed from o.m.g.sync.setup.activities.LocaleAware.
  public static void initializeLocale(Context context) {
    final LocaleManager localeManager = BrowserLocaleManager.getInstance();
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.GINGERBREAD) {
      localeManager.getAndApplyPersistedLocale(context);
    } else {
      final StrictMode.ThreadPolicy savedPolicy = StrictMode.allowThreadDiskReads();
      StrictMode.allowThreadDiskWrites();
      try {
        localeManager.getAndApplyPersistedLocale(context);
      } finally {
        StrictMode.setThreadPolicy(savedPolicy);
      }
    }
  }

  @Override
  public void onConfigurationChanged(Configuration newConfig) {
    final LocaleManager localeManager = BrowserLocaleManager.getInstance();
    final Locale changed = localeManager.onSystemConfigurationChanged(this, getResources(), newConfig, mLastLocale);
    if (changed != null) {
      // Redisplay to match the locale.
      onLocaleChanged(BrowserLocaleManager.getLanguageTag(changed));
    }
  }

  @Override
  public void onCreate(Bundle icicle) {
    // Note that we don't do this in onResume. We should,
    // but it's an edge case that we feel free to ignore.
    // We also don't have a hook in this example for when
    // the user picks a new locale.
    initializeLocale(this);

    super.onCreate(icicle);
  }

``GeckoApplication`` itself handles correcting locales when the configuration changes; your activity shouldn't need to do this itself. See ``GeckoApplication``'s and ``GeckoApp``'s ``onConfigurationChanged`` methods.

System locale changes
=====================

Fennec can be in one of two states.

If the user has not explicitly chosen a Fennec-specific locale, we say
we are "mirroring" the system locale.

When we are not mirroring, system locale changes do not impact Fennec
and are essentially ignored; the user's locale selection is the only
thing we care about, and we actively correct incoming configuration
changes to reflect the user's chosen locale.

By contrast, when we are mirroring, system locale changes cause Fennec
to reflect the new system locale, as if the user picked the new locale.

When the system locale changes when we're mirroring, your activity will receive an ``onConfigurationChanged`` call. Simply pass this on to ``BrowserLocaleManager``, and then handle the response appropriately.

Further reference
=================

``GeckoPreferences``, ``GeckoApp``, and ``BrowserApp`` are excellent resources for figuring out what you should do.
