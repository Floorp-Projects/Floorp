/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.focus.widget;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.os.AsyncTask;
import android.support.v7.preference.ListPreference;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;

import org.mozilla.focus.R;
import org.mozilla.focus.locale.LocaleManager;
import org.mozilla.focus.locale.Locales;

import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.text.Collator;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Locale;
import java.util.Map;
import java.util.Set;

import kotlin.Pair;

public class LocaleListPreference extends ListPreference {
    private static final String LOG_TAG = "GeckoLocaleList";

    private static final Map<String, String> languageCodeToNameMap = new HashMap<>();
    private static final Map<String, String> localeToNameMap = new HashMap<>();

    static {
        // Only ICU 57 actually contains the Asturian name for Asturian, even Android 7.1 is still
        // shipping with ICU 56, so we need to override the Asturian name (otherwise displayName will
        // be the current locales version of Asturian, see:
        // https://github.com/mozilla-mobile/focus-android/issues/634#issuecomment-303886118
        languageCodeToNameMap.put("ast", "Asturianu");
        // On an Android 8.0 device those languages are not known and we need to add the names
        // manually. Loading the resources at runtime works without problems though.
        languageCodeToNameMap.put("cak", "Kaqchikel");
        languageCodeToNameMap.put("ia", "Interlingua");
        languageCodeToNameMap.put("meh", "Tu´un savi ñuu Yasi'í Yuku Iti");
        languageCodeToNameMap.put("mix", "Tu'un savi");
        languageCodeToNameMap.put("trs", "Triqui");
        languageCodeToNameMap.put("zam", "DíɁztè");
        languageCodeToNameMap.put("oc", "occitan");
        languageCodeToNameMap.put("an", "Aragonés");
        languageCodeToNameMap.put("tt", "татарча");
        languageCodeToNameMap.put("wo", "Wolof");
        languageCodeToNameMap.put("anp", "अंगिका");
        languageCodeToNameMap.put("ixl", "Ixil");
        languageCodeToNameMap.put("pai", "Paa ipai");
        languageCodeToNameMap.put("quy", "Chanka Qhichwa");
        languageCodeToNameMap.put("ay", "Aimara");
        languageCodeToNameMap.put("quc", "K'iche'");
        languageCodeToNameMap.put("tsz", "P'urhepecha");
        languageCodeToNameMap.put("jv", "Basa Jawa");
        languageCodeToNameMap.put("ppl", "Náhuat Pipil");
        languageCodeToNameMap.put("su", "Basa Sunda");
    }
    static {
        // Override the native name for certain locale regions based on language community needs.
        localeToNameMap.put("zh-CN", "中文 (中国大陆)");
    }

    public LocaleListPreference(Context context, AttributeSet attrs, int defStyleAttr, int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);
    }

    /**
     * With thanks to <http://stackoverflow.com/a/22679283/22003> for the
     * initial solution.
     * <p>
     * This class encapsulates an approach to checking whether a script
     * is usable on a device. We attempt to draw a character from the
     * script (e.g., ব). If the fonts on the device don't have the correct
     * glyph, Android typically renders whitespace (rather than .notdef).
     * <p>
     * Pass in part of the name of the locale in its local representation,
     * and a whitespace character; this class performs the graphical comparison.
     * <p>
     * See Bug 1023451 Comment 24 for extensive explanation.
     */
    private static class CharacterValidator {
        private static final int BITMAP_WIDTH = 32;
        private static final int BITMAP_HEIGHT = 48;

        private final Paint paint = new Paint();
        private final byte[] missingCharacter;

        // Note: this constructor fails when running in Robolectric: robolectric only supports bitmaps
        // with 4 bytes per pixel ( https://github.com/robolectric/robolectric/blob/master/robolectric-shadows/shadows-core/src/main/java/org/robolectric/shadows/ShadowBitmap.java#L540 ).
        // We need to either make this code test-aware, or fix robolectric.
        public CharacterValidator(String missing) {
            this.missingCharacter = getPixels(drawBitmap(missing));
        }

        private Bitmap drawBitmap(String text) {
            Bitmap b = Bitmap.createBitmap(BITMAP_WIDTH, BITMAP_HEIGHT, Bitmap.Config.ALPHA_8);
            Canvas c = new Canvas(b);
            c.drawText(text, 0, BITMAP_HEIGHT / 2, this.paint);
            return b;
        }

        private static byte[] getPixels(final Bitmap b) {
            final int byteCount = b.getAllocationByteCount();

            final ByteBuffer buffer = ByteBuffer.allocate(byteCount);
            try {
                b.copyPixelsToBuffer(buffer);
            } catch (RuntimeException e) {
                // Android throws this if there's not enough space in the buffer.
                // This should never occur, but if it does, we don't
                // really care -- we probably don't need the entire image.
                // This is awful. I apologize.
                if ("Buffer not large enough for pixels".equals(e.getMessage())) {
                    return buffer.array();
                }
                throw e;
            }

            return buffer.array();
        }

        public boolean characterIsMissingInFont(String ch) {
            byte[] rendered = getPixels(drawBitmap(ch));
            return Arrays.equals(rendered, missingCharacter);
        }
    }

    private volatile Locale entriesLocale;
    private CharacterValidator characterValidator;
    private BuildLocaleListTask buildLocaleListTask;

    public LocaleListPreference(Context context) {
        this(context, null);
    }

    public LocaleListPreference(Context context, AttributeSet attributes) {
        super(context, attributes);

    }

    @Override
    public void onAttached() {
        super.onAttached();
        // Thus far, missing glyphs are replaced by whitespace, not a box
        // or other Unicode codepoint.
        this.characterValidator = new CharacterValidator(" ");

        initializeLocaleList();
    }

    private void initializeLocaleList() {
        final Locale currentLocale = Locale.getDefault();
        Log.d(LOG_TAG, "Building locales list. Current locale: " + currentLocale);

        if (currentLocale.equals(this.entriesLocale) && getEntries() != null) {
            Log.v(LOG_TAG, "No need to build list.");
            return;
        }

        this.entriesLocale = currentLocale;

        String defaultLanguage = getContext().getString(R.string.preference_language_systemdefault);
        this.buildLocaleListTask = new BuildLocaleListTask(this, defaultLanguage,
                characterValidator, LocaleManager.getPackagedLocaleTags(getContext()));
        this.buildLocaleListTask.execute();
    }

    @Override
    protected void onPrepareForRemoval() {
        super.onPrepareForRemoval();
        if (buildLocaleListTask != null) {
            buildLocaleListTask.cancel(true);
        }
    }

    private static final class LocaleDescriptor implements Comparable<LocaleDescriptor> {
        // We use Locale.US here to ensure a stable ordering of entries.
        private static final Collator COLLATOR = Collator.getInstance(Locale.US);

        public final String tag;
        private final String nativeName;

        public LocaleDescriptor(String tag) {
            this(Locales.parseLocaleCode(tag), tag);
        }

        public LocaleDescriptor(Locale locale, String tag) {
            this.tag = tag;

            final String displayName;

            if (languageCodeToNameMap.containsKey(locale.getLanguage())) {
                displayName = languageCodeToNameMap.get(locale.getLanguage());
            } else if (localeToNameMap.containsKey(locale)) {
                displayName = localeToNameMap.get(locale);
            } else {
                displayName = locale.getDisplayName(locale);
            }

            if (TextUtils.isEmpty(displayName)) {
                // There's nothing sane we can do.
                Log.w(LOG_TAG, "Display name is empty. Using " + locale.toString());
                this.nativeName = locale.toString();
                return;
            }

            // For now, uppercase the first character of LTR locale names.
            // This is pretty much what Android does. This is a reasonable hack
            // for Bug 1014602, but it won't generalize to all locales.
            final byte directionality = Character.getDirectionality(displayName.charAt(0));
            if (directionality == Character.DIRECTIONALITY_LEFT_TO_RIGHT) {
                String firstLetter = displayName.substring(0, 1);

                // Android OS creates an instance of Transliterator to convert the first letter
                // of the Greek locale. See CaseMapper.toUpperCase(Locale locale, String s, int count)
                // Since it's already in upper case, we don't need it
                if (!Character.isUpperCase(firstLetter.charAt(0))) {
                    firstLetter = firstLetter.toUpperCase(locale);
                }
                this.nativeName = firstLetter + displayName.substring(1);
                return;
            }

            this.nativeName = displayName;
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
        public boolean equals(Object obj) {
            if (obj instanceof LocaleDescriptor) {
                return compareTo((LocaleDescriptor) obj) == 0;
            } else {
                return false;
            }
        }

        @Override
        public int hashCode() {
            return tag.hashCode();
        }

        @Override
        public int compareTo(LocaleDescriptor another) {
            // We sort by name, so we use Collator.
            return COLLATOR.compare(this.nativeName, another.nativeName);
        }

        /**
         * See Bug 1023451 Comment 10 for the research that led to
         * this method.
         *
         * @return true if this locale can be used for displaying UI
         * on this device without known issues.
         */
        public boolean isUsable(CharacterValidator validator) {
            // Oh, for Java 7 switch statements.
            if (this.tag.equals("bn-IN")) {
                // Bengali sometimes has an English label if the Bengali script
                // is missing. This prevents us from simply checking character
                // rendering for bn-IN; we'll get a false positive for "B", not "ব".
                //
                // This doesn't seem to affect other Bengali-script locales
                // (below), which always have a label in native script.
                if (!this.nativeName.startsWith("বাংলা")) {
                    // We're on an Android version that doesn't even have
                    // characters to say বাংলা. Definite failure.
                    return false;
                }
            }

            // These locales use a script that is often unavailable
            // on common Android devices. Make sure we can show them.
            // See documentation for CharacterValidator.
            // Note that bn-IN is checked here even if it passed above.
            if (this.tag.equals("or") ||
                    this.tag.equals("my") ||
                    this.tag.equals("pa-IN") ||
                    this.tag.equals("gu-IN") ||
                    this.tag.equals("bn-IN")) {
                if (validator.characterIsMissingInFont(this.nativeName.substring(0, 1))) {
                    return false;
                }
            }

            return true;
        }
    }

    @Override
    protected void onClick() {
        super.onClick();

        // Use this hook to try to fix up the environment ASAP.
        // Do this so that the redisplayed fragment is inflated
        // with the right locale.
        final Locale selectedLocale = getSelectedLocale();
        final Context context = getContext();
        LocaleManager.getInstance().updateConfiguration(context, selectedLocale);
    }

    private Locale getSelectedLocale() {
        final String tag = getValue();
        if (tag == null || tag.equals("")) {
            return Locale.getDefault();
        }
        return Locales.parseLocaleCode(tag);
    }

    @Override
    public CharSequence getSummary() {
        final String value = getValue();

        if (TextUtils.isEmpty(value)) {
            return getContext().getString(R.string.preference_language_systemdefault);
        }

        // We can't trust super.getSummary() across locale changes,
        // apparently, so let's do the same work.
        return new LocaleDescriptor(value).getDisplayName();
    }

    static final class BuildLocaleListTask extends AsyncTask<Void, Void, Pair<String[], String[]>> {

        private final WeakReference<ListPreference> weakListPreference;
        private final CharacterValidator characterValidator;
        private final Collection<String> shippingLocales;
        private final String systemDefaultLanguage;

        BuildLocaleListTask(ListPreference listPreference, String systemDefaultLanguage,
                            CharacterValidator characterValidator, Collection<String> shippingLocales) {
            this.characterValidator = characterValidator;
            this.shippingLocales = shippingLocales;
            this.systemDefaultLanguage = systemDefaultLanguage;
            this.weakListPreference = new WeakReference<>(listPreference);
        }

        @Override
        protected Pair<String[], String[]> doInBackground(Void... voids) {
            final LocaleDescriptor[] descriptors = getUsableLocales();
            final int count = descriptors.length;

            // We leave room for "System default".
            final String[] entries = new String[count + 1];
            final String[] values = new String[count + 1];

            entries[0] = systemDefaultLanguage;
            values[0] = "";

            for (int i = 0; i < count; ++i) {
                final String displayName = descriptors[i].getDisplayName();
                final String tag = descriptors[i].getTag();
                Log.v(LOG_TAG, displayName + " => " + tag);
                entries[i + 1] = displayName;
                values[i + 1] = tag;
            }
            return new Pair<>(entries, values);
        }

        /**
         * Not every locale we ship can be used on every device, due to
         * font or rendering constraints.
         * <p>
         * This method filters down the list before generating the descriptor array.
         */
        private LocaleDescriptor[] getUsableLocales() {
            final int initialCount = shippingLocales.size();
            final Set<LocaleDescriptor> locales = new HashSet<>(initialCount);
            for (String tag : shippingLocales) {
                final LocaleDescriptor descriptor = new LocaleDescriptor(tag);
                if (!descriptor.isUsable(this.characterValidator)) {
                    Log.w(LOG_TAG, "Skipping locale " + tag + " on this device.");
                    continue;
                }

                locales.add(descriptor);
            }
            final int usableCount = locales.size();
            final LocaleDescriptor[] descriptors = locales.toArray(new LocaleDescriptor[usableCount]);
            Arrays.sort(descriptors, 0, usableCount);
            return descriptors;
        }

        @Override
        protected void onPostExecute(Pair<String[], String[]> pair) {
            if (isCancelled()) {
                return;
            }

            final ListPreference preference = weakListPreference.get();
            if (preference != null) {
                preference.setEntries(pair.getFirst());
                preference.setEntryValues(pair.getSecond());
            }
        }
    }
}
