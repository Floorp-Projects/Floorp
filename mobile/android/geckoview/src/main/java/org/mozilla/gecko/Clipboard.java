/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import java.util.concurrent.SynchronousQueue;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.util.ThreadUtils;

import android.content.ClipboardManager;
import android.content.ClipData;
import android.content.Context;
import android.text.TextUtils;

public final class Clipboard {
    private final static String LOGTAG = "GeckoClipboard";

    private Clipboard() {
    }

    @WrapForJNI(calledFrom = "gecko")
    public static String getText(final Context context) {
        final ClipboardManager cm = (ClipboardManager)
                context.getSystemService(Context.CLIPBOARD_SERVICE);
        if (cm.hasPrimaryClip()) {
            ClipData clip = cm.getPrimaryClip();
            if (clip != null && clip.getItemCount() > 0) {
                ClipData.Item item = clip.getItemAt(0);
                return item.coerceToText(context).toString();
            }
        }
        return null;
    }

    @WrapForJNI(calledFrom = "gecko")
    public static void setText(final Context context, final CharSequence text) {
        // In API Level 11 and above, CLIPBOARD_SERVICE returns android.content.ClipboardManager,
        // which is a subclass of android.text.ClipboardManager.
        final ClipboardManager cm = (ClipboardManager)
                context.getSystemService(Context.CLIPBOARD_SERVICE);
        try {
            cm.setPrimaryClip(ClipData.newPlainText("Text", text));
        } catch (NullPointerException e) {
            // Bug 776223: This is a Samsung clipboard bug. setPrimaryClip() can throw
            // a NullPointerException if Samsung's /data/clipboard directory is full.
            // Fortunately, the text is still successfully copied to the clipboard.
        }
    }

    /**
     * @return true if the clipboard is nonempty, false otherwise.
     */
    @WrapForJNI(calledFrom = "gecko")
    public static boolean hasText(final Context context) {
        return !TextUtils.isEmpty(getText(context));
    }

    /**
     * Deletes all text from the clipboard.
     */
    @WrapForJNI(calledFrom = "gecko")
    public static void clearText(final Context context) {
        setText(context, null);
    }
}
