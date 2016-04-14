/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.util;

import java.util.concurrent.SynchronousQueue;

import org.mozilla.gecko.annotation.WrapForJNI;
import org.mozilla.gecko.AppConstants.Versions;

import android.content.ClipData;
import android.content.Context;
import android.util.Log;

public final class Clipboard {
    // Volatile but not synchronized: we don't care about the race condition in
    // init, because both app contexts will be the same, but we do care about a
    // thread having a stale null value of mContext.
    volatile static Context mContext;
    private final static String LOGTAG = "GeckoClipboard";
    private final static SynchronousQueue<String> sClipboardQueue = new SynchronousQueue<String>();

    private Clipboard() {
    }

    public static void init(final Context c) {
        if (mContext != null) {
            Log.w(LOGTAG, "Clipboard.init() called twice!");
            return;
        }
        mContext = c.getApplicationContext();
    }

    @WrapForJNI(stubName = "GetClipboardTextWrapper")
    public static String getText() {
        // If we're on the UI thread or the background thread, we have a looper on the thread
        // and can just call this directly. For any other threads, post the call to the
        // background thread.

        if (ThreadUtils.isOnUiThread() || ThreadUtils.isOnBackgroundThread()) {
            return getClipboardTextImpl();
        }

        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            public void run() {
                String text = getClipboardTextImpl();
                try {
                    sClipboardQueue.put(text != null ? text : "");
                } catch (InterruptedException ie) { }
            }
        });

        try {
            return sClipboardQueue.take();
        } catch (InterruptedException ie) {
            return "";
        }
    }

    @WrapForJNI(stubName = "SetClipboardText")
    public static void setText(final CharSequence text) {
        ThreadUtils.postToBackgroundThread(new Runnable() {
            @Override
            @SuppressWarnings("deprecation")
            public void run() {
                // In API Level 11 and above, CLIPBOARD_SERVICE returns android.content.ClipboardManager,
                // which is a subclass of android.text.ClipboardManager.
                if (Versions.feature11Plus) {
                    final android.content.ClipboardManager cm = (android.content.ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
                    final ClipData clip = ClipData.newPlainText("Text", text);
                    try {
                        cm.setPrimaryClip(clip);
                    } catch (NullPointerException e) {
                        // Bug 776223: This is a Samsung clipboard bug. setPrimaryClip() can throw
                        // a NullPointerException if Samsung's /data/clipboard directory is full.
                        // Fortunately, the text is still successfully copied to the clipboard.
                    }
                    return;
                }

                // Deprecated.
                android.text.ClipboardManager cm = (android.text.ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
                cm.setText(text);
            }
        });
    }

    /**
     * @return true if the clipboard is nonempty, false otherwise.
     */
    @WrapForJNI
    public static boolean hasText() {
        if (Versions.feature11Plus) {
            android.content.ClipboardManager cm = (android.content.ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
            return cm.hasPrimaryClip();
        }

        // Deprecated.
        android.text.ClipboardManager cm = (android.text.ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
        return cm.hasText();
    }

    /**
     * Deletes all text from the clipboard.
     */
    @WrapForJNI
    public static void clearText() {
        setText(null);
    }

    /**
     * On some devices, access to the clipboard service needs to happen
     * on a thread with a looper, so this function requires a looper is
     * present on the thread.
     */
    @SuppressWarnings("deprecation")
    static String getClipboardTextImpl() {
        if (Versions.feature11Plus) {
            android.content.ClipboardManager cm = (android.content.ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm.hasPrimaryClip()) {
                ClipData clip = cm.getPrimaryClip();
                if (clip != null) {
                    ClipData.Item item = clip.getItemAt(0);
                    return item.coerceToText(mContext).toString();
                }
            }
        } else {
            android.text.ClipboardManager cm = (android.text.ClipboardManager) mContext.getSystemService(Context.CLIPBOARD_SERVICE);
            if (cm.hasText()) {
                return cm.getText().toString();
            }
        }
        return null;
    }
}
