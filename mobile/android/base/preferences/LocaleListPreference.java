/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.preferences;

import java.nio.ByteBuffer;
import java.text.Collator;
import java.util.Arrays;
import java.util.Collection;
import java.util.HashSet;
import java.util.Locale;
import java.util.Set;

import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.R;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.preference.ListPreference;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.util.Log;

public class LocaleListPreference extends ListPreference {
    private static final String LOG_TAG = "GeckoLocaleList";

    /**
     * With thanks to <http://stackoverflow.com/a/22679283/22003> for the
     * initial solution.
     *
     * This class encapsulates an approach to checking whether a script
     * is usable on a device. We attempt to draw a character from the
     * script (e.g., ব). If the fonts on the device don't have the correct
     * glyph, Android typically renders whitespace (rather than .notdef).
     *
     * Pass in part of the name of the locale in its local representation,
     * and a whitespace character; this class performs the graphical comparison.
     *
     * See Bug 1023451 Comment 24 for extensive explanation.
     */
    private static class CharacterValidator {
        private static final int BITMAP_WIDTH = 32;
        private static final int BITMAP_HEIGHT = 48;

        private final Paint paint = new Paint();
        private final byte[] missingCharacter;

        public CharacterValidator(String missing) {
            this.missingCharacter = getPixels(drawBitmap(missing));
        }

        private Bitmap drawBitmap(String text){
            Bitmap b = Bitmap.createBitmap(BITMAP_WIDTH, BITMAP_HEIGHT, Bitmap.Config.ALPHA_8);
            Canvas c = new Canvas(b);
            c.drawText(text, 0, BITMAP_HEIGHT / 2, this.paint);
            return b;
        }

        private static byte[] getPixels(final Bitmap b) {
            final int byteCount;
            if (android.os.Build.VERSION.SDK_INT >= 19) {
                // TODO: when Bug 1042829 lands, do the right thing for KitKat devices.
                // Which is:
                // byteCount = b.getAllocationByteCount();
                byteCount = b.getRowBytes() * b.getHeight();
            } else {
                // Close enough for government work.
                // Equivalent to getByteCount, but works on <12.
                byteCount = b.getRowBytes() * b.getHeight();
            }

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
    private final CharacterValidator characterValidator;

    public LocaleListPreference(Context context) {
        this(context, null);
    }

    public LocaleListPreference(Context context, AttributeSet attributes) {
        super(context, attributes);

        // Thus far, missing glyphs are replaced by whitespace, not a box
        // or other Unicode codepoint.
        this.characterValidator = new CharacterValidator(" ");
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
            this.tag = tag;

            final String displayName = locale.getDisplayName(locale);
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
                this.nativeName = displayName.substring(0, 1).toUpperCase(locale) +
                                  displayName.substring(1);
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
        public int compareTo(LocaleDescriptor another) {
            // We sort by name, so we use Collator.
            return COLLATOR.compare(this.nativeName, another.nativeName);
        }

        /**
         * See Bug 1023451 Comment 10 for the research that led to
         * this method.
         *
         * @return true if this locale can be used for displaying UI
         *         on this device without known issues.
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

    /**
     * Not every locale we ship can be used on every device, due to
     * font or rendering constraints.
     *
     * This method filters down the list before generating the descriptor array.
     */
    private LocaleDescriptor[] getUsableLocales() {
        Collection<String> shippingLocales = BrowserLocaleManager.getPackagedLocaleTags(getContext());

        // Future: single-locale builds should be specified, too.
        if (shippingLocales == null) {
            final String fallbackTag = BrowserLocaleManager.getFallbackLocaleTag();
            return new LocaleDescriptor[] { new LocaleDescriptor(fallbackTag) };
        }

        final int initialCount = shippingLocales.size();
        final Set<LocaleDescriptor> locales = new HashSet<LocaleDescriptor>(initialCount);
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

        final LocaleDescriptor[] descriptors = getUsableLocales();
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
