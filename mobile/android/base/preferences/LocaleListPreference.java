/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import java.text.Collator;
import java.util.Arrays;
import java.util.Collection;
import java.util.Locale;

import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.R;

import android.content.Context;
import android.preference.ListPreference;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;

public class LocaleListPreference extends ListPreference {
    private static final String LOG_TAG = "GeckoLocaleList";

    private volatile Locale entriesLocale;

    public LocaleListPreference(Context context) {
        this(context, null);
    }

    public LocaleListPreference(Context context, AttributeSet attributes) {
        super(context, attributes);
        buildList();
    }

    private static final class LocaleDescriptor implements Comparable<LocaleDescriptor> {
        // We use Locale.US here to ensure a stable ordering of entries.
        private static final Collator COLLATOR = Collator.getInstance(Locale.US);

        public final String tag;
        private final String nativeName;

        public LocaleDescriptor(String tag) {
            this(BrowserLocaleManager.parseLocaleCode(tag), tag);
        }

        public LocaleDescriptor(Locale locale, String tag) {
            this.nativeName = locale.getDisplayName(locale);
            this.tag = tag;
        }

        public String getTag() {
            return this.tag;
        }

        public String getDisplayName() {
            return this.nativeName;
        }

        @Override
        public String toString() {
            return this.nativeName;
        }


        @Override
        public int compareTo(LocaleDescriptor another) {
            // We sort by name, so we use Collator.
            return COLLATOR.compare(this.nativeName, another.nativeName);
        }
    }

    private LocaleDescriptor[] getShippingLocales() {
        Collection<String> shippingLocales = BrowserLocaleManager.getPackagedLocaleTags(getContext());

        // Future: single-locale builds should be specified, too.
        if (shippingLocales == null) {
            final String fallbackTag = BrowserLocaleManager.getFallbackLocaleTag();
            return new LocaleDescriptor[] { new LocaleDescriptor(fallbackTag) };
        }

        final int count = shippingLocales.size();
        final LocaleDescriptor[] descriptors = new LocaleDescriptor[count];

        int i = 0;
        for (String tag : shippingLocales) {
            descriptors[i++] = new LocaleDescriptor(tag);
        }

        Arrays.sort(descriptors, 0, count);
        return descriptors;
    }

    @Override
    protected void onDialogClosed(boolean positiveResult) {
        // The superclass will take care of persistence.
        super.onDialogClosed(positiveResult);

        // Use this hook to try to fix up the environment ASAP.
        // Do this so that the redisplayed fragment is inflated
        // with the right locale.
        final Locale selectedLocale = getSelectedLocale();
        final Context context = getContext();
        BrowserLocaleManager.getInstance().updateConfiguration(context, selectedLocale);
    }

    private Locale getSelectedLocale() {
        final String tag = getValue();
        if (tag == null || tag.equals("")) {
            return Locale.getDefault();
        }
        return BrowserLocaleManager.parseLocaleCode(tag);
    }

    @Override
    public CharSequence getSummary() {
        final String value = getValue();

        if (TextUtils.isEmpty(value)) {
            return getContext().getString(R.string.locale_system_default);
        }

        // We can't trust super.getSummary() across locale changes,
        // apparently, so let's do the same work.
        final Locale loc = new Locale(value);
        return loc.getDisplayName(loc);
    }

    private void buildList() {
        final Locale currentLocale = Locale.getDefault();
        Log.d(LOG_TAG, "Building locales list. Current locale: " + currentLocale);

        if (currentLocale.equals(this.entriesLocale) &&
            getEntries() != null) {
            Log.v(LOG_TAG, "No need to build list.");
            return;
        }

        final LocaleDescriptor[] descriptors = getShippingLocales();
        final int count = descriptors.length;

        this.entriesLocale = currentLocale;

        // We leave room for "System default".
        final String[] entries = new String[count + 1];
        final String[] values = new String[count + 1];

        entries[0] = getContext().getString(R.string.locale_system_default);
        values[0] = "";

        for (int i = 0; i < count; ++i) {
            entries[i + 1] = descriptors[i].getDisplayName();
            values[i + 1] = descriptors[i].getTag();
        }

        setEntries(entries);
        setEntryValues(values);
    }
}
