/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko;

import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ClipboardManager;
import android.content.Context;
import android.os.Build;
import android.text.TextUtils;
import android.util.Log;
import org.mozilla.gecko.annotation.WrapForJNI;

public final class Clipboard {
  private static final String HTML_MIME = "text/html";
  private static final String UNICODE_MIME = "text/unicode";
  private static final String LOGTAG = "GeckoClipboard";

  private Clipboard() {}

  /**
   * Get the text on the primary clip on Android clipboard
   *
   * @param context application context.
   * @return a plain text string of clipboard data.
   */
  public static String getText(final Context context) {
    return getData(context, UNICODE_MIME);
  }

  /**
   * Get the data on the primary clip on clipboard
   *
   * @param context application context
   * @param mimeType the mime type we want. This supports text/html and text/unicode only. If other
   *     type, we do nothing.
   * @return a string into clipboard.
   */
  @WrapForJNI(calledFrom = "gecko")
  public static String getData(final Context context, final String mimeType) {
    final ClipboardManager cm =
        (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
    if (cm.hasPrimaryClip()) {
      final ClipData clip = cm.getPrimaryClip();
      if (clip == null || clip.getItemCount() == 0) {
        return null;
      }

      final ClipDescription description = clip.getDescription();
      if (HTML_MIME.equals(mimeType)
          && description.hasMimeType(ClipDescription.MIMETYPE_TEXT_HTML)) {
        final CharSequence data = clip.getItemAt(0).getHtmlText();
        if (data == null) {
          return null;
        }
        return data.toString();
      }
      if (UNICODE_MIME.equals(mimeType)) {
        try {
          return clip.getItemAt(0).coerceToText(context).toString();
        } catch (final SecurityException e) {
          Log.e(LOGTAG, "Couldn't get clip data from clipboard", e);
        }
      }
    }
    return null;
  }

  /**
   * Set plain text to clipboard
   *
   * @param context application context
   * @param text a plain text to set to clipboard
   * @return true if copy is successful.
   */
  @WrapForJNI(calledFrom = "gecko")
  public static boolean setText(final Context context, final CharSequence text) {
    return setData(context, ClipData.newPlainText("text", text));
  }

  /**
   * Store HTML to clipboard
   *
   * @param context application context
   * @param text a plain text to set to clipboard
   * @param html a html text to set to clipboard
   * @return true if copy is successful.
   */
  @WrapForJNI(calledFrom = "gecko")
  public static boolean setHTML(
      final Context context, final CharSequence text, final String htmlText) {
    return setData(context, ClipData.newHtmlText("html", text, htmlText));
  }

  /**
   * Store {@link android.content.ClipData} to clipboard
   *
   * @param context application context
   * @param clipData a {@link android.content.ClipData} to set to clipboard
   * @return true if copy is successful.
   */
  private static boolean setData(final Context context, final ClipData clipData) {
    // In API Level 11 and above, CLIPBOARD_SERVICE returns android.content.ClipboardManager,
    // which is a subclass of android.text.ClipboardManager.
    final ClipboardManager cm =
        (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
    try {
      cm.setPrimaryClip(clipData);
    } catch (final NullPointerException e) {
      // Bug 776223: This is a Samsung clipboard bug. setPrimaryClip() can throw
      // a NullPointerException if Samsung's /data/clipboard directory is full.
      // Fortunately, the text is still successfully copied to the clipboard.
    } catch (final RuntimeException e) {
      // If clipData is too large, TransactionTooLargeException occurs.
      Log.e(LOGTAG, "Couldn't set clip data to clipboard", e);
      return false;
    }
    return true;
  }

  /**
   * Check whether primary clipboard has given MIME type.
   *
   * @param context application context
   * @param mimeType MIME type
   * @return true if the clipboard is nonempty, false otherwise.
   */
  @WrapForJNI(calledFrom = "gecko")
  public static boolean hasData(final Context context, final String mimeType) {
    if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
      if (HTML_MIME.equals(mimeType) || UNICODE_MIME.equals(mimeType)) {
        return !TextUtils.isEmpty(getData(context, mimeType));
      }
      return false;
    }

    // Calling getPrimaryClip causes a toast message from Android 12.
    // https://developer.android.com/about/versions/12/behavior-changes-all#clipboard-access-notifications

    final ClipboardManager cm =
        (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);

    if (!cm.hasPrimaryClip()) {
      return false;
    }

    final ClipDescription description = cm.getPrimaryClipDescription();
    if (description == null) {
      return false;
    }

    if (HTML_MIME.equals(mimeType)) {
      return description.hasMimeType(ClipDescription.MIMETYPE_TEXT_HTML);
    }

    if (UNICODE_MIME.equals(mimeType)) {
      // We cannot check content in data at this time to avoid toast message.
      return description.hasMimeType(ClipDescription.MIMETYPE_TEXT_HTML)
          || description.hasMimeType(ClipDescription.MIMETYPE_TEXT_PLAIN);
    }

    return false;
  }

  /** Deletes all text from the clipboard. */
  @WrapForJNI(calledFrom = "gecko")
  public static void clearText(final Context context) {
    if (Build.VERSION.SDK_INT <= Build.VERSION_CODES.P) {
      setText(context, null);
      return;
    }
    // Although we don't know more details of https://crbug.com/1203377, Blink doesn't use
    // clearPrimaryClip on Android P since this may throw an exception, even if it is supported
    // on Android P.
    final ClipboardManager cm =
        (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
    cm.clearPrimaryClip();
  }
}
