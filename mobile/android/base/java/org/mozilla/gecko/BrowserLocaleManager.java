/* -*- Mode: Java; c-basic-offset: 4; tab-width: 4; indent-tabs-mode: nil; -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicReference;

import org.mozilla.gecko.annotation.ReflectionTarget;
import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.GeckoBundle;
import org.mozilla.gecko.util.GeckoJarReader;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.os.Build;
import android.os.LocaleList;
import android.util.Log;

/**
 * This class manages persistence, application, and otherwise handling of
 * user-specified locales.
 *
 * Of note:
 *
 * * It's a singleton, because its scope extends to that of the application,
 *   and definitionally all changes to the locale of the app must go through
 *   this.
 * * It's lazy.
 * * It has ties into the Gecko event system, because it has to tell Gecko when
 *   to switch locale.
 * * It relies on using the SharedPreferences file owned by the browser (in
 *   Fennec's case, "GeckoApp") for performance.
 */
public class BrowserLocaleManager implements LocaleManager {
    private static final String LOG_TAG = "GeckoLocales";

    private static final String EVENT_LOCALE_CHANGED = "Locale:Changed";
    private static final String PREF_LOCALE = "locale";

    private static final String FALLBACK_LOCALE_TAG = "en-US";

    // These are volatile because we don't impose restrictions
    // over which thread calls our methods.
    private volatile Locale currentLocale;
    private volatile Locale systemLocale = Locale.getDefault();

    private final AtomicBoolean inited = new AtomicBoolean(false);
    private boolean systemLocaleDidChange;
    private BroadcastReceiver receiver;

    private static final AtomicReference<LocaleManager> instance = new AtomicReference<LocaleManager>();

    @ReflectionTarget
    public static LocaleManager getInstance() {
        LocaleManager localeManager = instance.get();
        if (localeManager != null) {
            return localeManager;
        }

        localeManager = new BrowserLocaleManager();
        if (instance.compareAndSet(null, localeManager)) {
            return localeManager;
        } else {
            return instance.get();
        }
    }

    /**
     * Ensure that you call this early in your application startup,
     * and with a context that's sufficiently long-lived (typically
     * the application context).
     *
     * Calling multiple times is harmless.
     */
    @Override
    public void initialize(final Context context) {
        if (!inited.compareAndSet(false, true)) {
            return;
        }

        receiver = new BroadcastReceiver() {
            @Override
            public void onReceive(Context context, Intent intent) {
                final Locale current = systemLocale;

                // We don't trust Locale.getDefault() here, because we make a
                // habit of mutating it! Use the one Android supplies, because
                // that gets regularly reset.
                // The default value of systemLocale is fine, because we haven't
                // yet swizzled Locale during static initialization.
                systemLocale = context.getResources().getConfiguration().locale;
                systemLocaleDidChange = true;

                Log.d(LOG_TAG, "System locale changed from " + current + " to " + systemLocale);

                // If the OS locale changed, we need to tell Gecko.
                final SharedPreferences prefs = GeckoSharedPrefs.forProfile(context);
                BrowserLocaleManager.storeAndNotifyOSLocale(prefs, systemLocale);
            }
        };
        context.registerReceiver(receiver, new IntentFilter(Intent.ACTION_LOCALE_CHANGED));
    }

    @Override
    public boolean systemLocaleDidChange() {
        return systemLocaleDidChange;
    }

    /**
     * Every time the system gives us a new configuration, it
     * carries the external locale. Fix it.
     */
    @Override
    public void correctLocale(Context context, Resources res, Configuration config) {
        final Locale current = getCurrentLocale(context);
        if (current == null) {
            Log.d(LOG_TAG, "No selected locale. No correction needed.");
            return;
        }

        // I know it's tempting to short-circuit here if the config seems to be
        // up-to-date, but the rest is necessary.

        config.locale = current;

        // The following two lines are heavily commented in case someone
        // decides to chase down performance improvements and decides to
        // question what's going on here.
        // Both lines should be cheap, *but*...

        // This is unnecessary for basic string choice, but it almost
        // certainly comes into play when rendering numbers, deciding on RTL,
        // etc. Take it out if you can prove that's not the case.
        Locale.setDefault(current);

        // This seems to be a no-op, but every piece of documentation under the
        // sun suggests that it's necessary, and it certainly makes sense.
        res.updateConfiguration(config, null);
    }

    /**
     * We can be in one of two states.
     *
     * If the user has not explicitly chosen a Firefox-specific locale, we say
     * we are "mirroring" the system locale.
     *
     * When we are not mirroring, system locale changes do not impact Firefox
     * and are essentially ignored; the user's locale selection is the only
     * thing we care about, and we actively correct incoming configuration
     * changes to reflect the user's chosen locale.
     *
     * By contrast, when we are mirroring, system locale changes cause Firefox
     * to reflect the new system locale, as if the user picked the new locale.
     *
     * If we're currently mirroring the system locale, this method returns the
     * supplied configuration's locale, unless the current activity locale is
     * correct. If we're not currently mirroring, this method updates the
     * configuration object to match the user's currently selected locale, and
     * returns that, unless the current activity locale is correct.
     *
     * If the current activity locale is correct, returns null.
     *
     * The caller is expected to redisplay themselves accordingly.
     *
     * This method is intended to be called from inside
     * <code>onConfigurationChanged(Configuration)</code> as part of a strategy
     * to detect and either apply or undo system locale changes.
     */
    @Override
    public Locale onSystemConfigurationChanged(final Context context, final Resources resources, final Configuration configuration, final Locale currentActivityLocale) {
        if (!isMirroringSystemLocale(context)) {
            correctLocale(context, resources, configuration);
        }

        final Locale changed = configuration.locale;
        if (changed.equals(currentActivityLocale)) {
            return null;
        }

        return changed;
    }

    /**
     * Gecko needs to know the OS locale to compute a useful Accept-Language
     * header. If it changed since last time, send a message to Gecko and
     * persist the new value. If unchanged, returns immediately.
     *
     * @param prefs the SharedPreferences instance to use. Cannot be null.
     * @param osLocale the new locale instance. Safe if null.
     */
    public static void storeAndNotifyOSLocale(final SharedPreferences prefs,
                                              final Locale osLocale) {
        if (osLocale == null) {
            return;
        }

        final String lastOSLocale = prefs.getString("osLocale", null);
        final String osLocaleString = osLocale.toString();

        if (osLocaleString.equals(lastOSLocale)) {
            Log.d(LOG_TAG, "Previous locale " + lastOSLocale + " same as new. Doing nothing.");
            return;
        }

        // Store the Java-native form.
        prefs.edit().putString("osLocale", osLocaleString).apply();

        // The value we send to Gecko should be a language tag, not
        // a Java locale string.
        final GeckoBundle data = new GeckoBundle(1);
        data.putString("languageTag", Locales.getLanguageTag(osLocale));

        EventDispatcher.getInstance().dispatch("Locale:OS", data);

        if (GeckoThread.isRunning()) {
            refreshLocales();
        } else {
            GeckoThread.queueNativeCall(BrowserLocaleManager.class, "refreshLocales");
        }
    }

    @Override
    public String getAndApplyPersistedLocale(Context context) {
        initialize(context);

        final long t1 = android.os.SystemClock.uptimeMillis();
        final String localeCode = getPersistedLocale(context);
        if (localeCode == null) {
            return null;
        }

        // Note that we don't tell Gecko about this. We notify Gecko when the
        // locale is set, not when we update Java.
        final String resultant = updateLocale(context, localeCode);

        if (resultant == null) {
            // Update the configuration anyway.
            updateConfiguration(context, currentLocale);
        }

        final long t2 = android.os.SystemClock.uptimeMillis();
        Log.i(LOG_TAG, "Locale read and update took: " + (t2 - t1) + "ms.");
        return resultant;
    }

    /**
     * Returns the set locale if it changed.
     *
     * Always persists and notifies Gecko.
     */
    @Override
    public String setSelectedLocale(Context context, String localeCode) {
        final String resultant = updateLocale(context, localeCode);

        // We always persist and notify Gecko, even if nothing seemed to
        // change. This might happen if you're picking a locale that's the same
        // as the current OS locale. The OS locale might change next time we
        // launch, and we need the Gecko pref and persisted locale to have been
        // set by the time that happens.
        persistLocale(context, localeCode);

        // Tell Gecko.
        final GeckoBundle data = new GeckoBundle(1);
        data.putString("languageTag", Locales.getLanguageTag(getCurrentLocale(context)));
        EventDispatcher.getInstance().dispatch(EVENT_LOCALE_CHANGED, data);

        return resultant;
    }

    @Override
    public void resetToSystemLocale(Context context) {
        // Wipe the pref.
        final SharedPreferences settings = getSharedPreferences(context);
        settings.edit().remove(PREF_LOCALE).apply();

        // Apply the system locale.
        updateLocale(context, systemLocale);

        // Tell Gecko.
        EventDispatcher.getInstance().dispatch(EVENT_LOCALE_CHANGED, null);
    }

    /**
     * This is public to allow for an activity to force the
     * current locale to be applied if necessary (e.g., when
     * a new activity launches).
     */
    @Override
    public void updateConfiguration(Context context, Locale locale) {
        Resources res = context.getResources();
        Configuration config = res.getConfiguration();

        // We should use setLocale, but it's unexpectedly missing
        // on real devices.
        config.locale = locale;
        //  LayoutDirection is also updated in setLocale, do this manually.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.JELLY_BEAN_MR1) {
            config.setLayoutDirection(locale);
        }

        res.updateConfiguration(config, null);
    }

    private SharedPreferences getSharedPreferences(Context context) {
        // We should be using per-profile prefs here, because we're tracking against
        // a Gecko pref. The same applies to the locale switcher!
        // Bug 940575, Bug 873166 are relevant, and see Bug 1378501 for the commit
        // that added this comment.
        return GeckoSharedPrefs.forApp(context);
    }

    /**
     * @return the persisted locale in Java format: "en_US".
     */
    private String getPersistedLocale(Context context) {
        final SharedPreferences settings = getSharedPreferences(context);
        final String locale = settings.getString(PREF_LOCALE, "");

        if ("".equals(locale)) {
            return null;
        }
        return locale;
    }

    private void persistLocale(Context context, String localeCode) {
        final SharedPreferences settings = getSharedPreferences(context);
        settings.edit().putString(PREF_LOCALE, localeCode).apply();
    }

    @Override
    public Locale getCurrentLocale(Context context) {
        if (currentLocale != null) {
            return currentLocale;
        }

        final String current = getPersistedLocale(context);
        if (current == null) {
            return null;
        }
        return currentLocale = Locales.parseLocaleCode(current);
    }

    @Override
    public Locale getDefaultSystemLocale() {
        return systemLocale;
    }

    /**
     * Updates the Java locale and the Android configuration.
     *
     * Returns the persisted locale if it differed.
     *
     * Does not notify Gecko.
     *
     * @param localeCode a locale string in Java format: "en_US".
     * @return if it differed, a locale string in Java format: "en_US".
     */
    private String updateLocale(Context context, String localeCode) {
        // Fast path.
        final Locale defaultLocale = Locale.getDefault();
        if (defaultLocale.toString().equals(localeCode)) {
            return null;
        }

        final Locale locale = Locales.parseLocaleCode(localeCode);

        return updateLocale(context, locale);
    }

    /**
     * @return the Java locale string: e.g., "en_US".
     */
    private String updateLocale(Context context, final Locale locale) {
        // Fast path.
        if (Locale.getDefault().equals(locale)) {
            return null;
        }

        Locale.setDefault(locale);
        currentLocale = locale;

        // Update resources.
        updateConfiguration(context, locale);

        return locale.toString();
    }

    @Override
    public boolean isMirroringSystemLocale(Context context) {
        return getPersistedLocale(context) == null;
    }

    /**
     * Examines <code>multilocale.txt</code>, returning the included list of
     * locale codes.
     *
     * If <code>multilocale.txt</code> is not present, returns
     * <code>null</code>. In that case, consider {@link #getFallbackLocaleTag()}.
     *
     * multilocale.txt currently looks like this:
     *
     * <code>
     * en-US,be,ca,cs,da,de,en-GB,en-ZA,es-AR,es-ES,es-MX,et,fi
     * </code>
     */
    public static Collection<String> getPackagedLocaleTags(final Context context) {
        final String resPath = "res/multilocale.txt";
        final String jarURL = GeckoJarReader.getJarURL(context, resPath);

        final String contents = GeckoJarReader.getText(context, jarURL);
        if (contents == null) {
            // GeckoJarReader logs and swallows exceptions.
            return null;
        }

        String[] values = contents.trim().split("\\s*,\\s*");
        final Set<String> out = new HashSet<String>(Arrays.asList(values));

        return out;
    }

    /**
     * @return the single default locale baked into this application.
     *         Applicable when there is no multilocale.txt present.
     */
    @SuppressWarnings("static-method")
    public String getFallbackLocaleTag() {
        return FALLBACK_LOCALE_TAG;
    }

    @WrapForJNI(dispatchTo = "Gecko")
    private static native void refreshLocales();


    @WrapForJNI
    private static String[] getLocales() {
        try {
            ArrayList<String> locales = new ArrayList<String>();
            LocaleManager localeManager = Locales.getLocaleManager();
            Context context = GeckoAppShell.getApplicationContext();
            if (!localeManager.isMirroringSystemLocale(context)) {
                // User uses specific browser locale instead of system locale
                locales.add(Locales.getLanguageTag(localeManager.getCurrentLocale(context)));
                return locales.toArray(new String[locales.size()]);
            }
            // Since user selects system default for browser locale, we should return system locales too.
            if (Build.VERSION.SDK_INT >= 24) {
                LocaleList localeList = LocaleList.getDefault();
                for (int i = 0; i < localeList.size(); i++) {
                    // Some locales such as "he" need conversion.
                    locales.add(Locales.getLanguageTag(localeList.get(i)));
                }
            } else {
                locales.add(Locales.getLanguageTag(localeManager.getDefaultSystemLocale()));
            }
            return locales.toArray(new String[locales.size()]);
        } catch (NullPointerException e) {
            Log.i(LOG_TAG, "Couldn't get current locale.");
            return null;
        }
    }
}
