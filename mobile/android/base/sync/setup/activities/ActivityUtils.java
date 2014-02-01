/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import java.util.Locale;

import org.mozilla.gecko.background.common.GlobalConstants;
import org.mozilla.gecko.background.common.log.Logger;
import org.mozilla.gecko.sync.SyncConstants;
import org.mozilla.gecko.sync.setup.InvalidSyncKeyException;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.Html;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextPaint;
import android.text.method.LinkMovementMethod;
import android.text.style.ClickableSpan;
import android.text.style.URLSpan;
import android.view.View;
import android.widget.TextView;

public class ActivityUtils {
  private static final String LOG_TAG = "ActivityUtils";

  public static void prepareLogging() {
    Logger.setThreadLogTag(SyncConstants.GLOBAL_LOG_TAG);
  }

  /**
   * Sync key should be a 26-character string, and can include arbitrary
   * capitalization and hyphenation.
   *
   * @param key
   *          Sync key entered by user in account setup.
   * @return Sync key in correct format (lower-case, no hyphens).
   * @throws InvalidSyncKeyException
   */
  public static String validateSyncKey(String key) throws InvalidSyncKeyException {
    String charKey = key.trim().replace("-", "").toLowerCase(Locale.US);
    if (!charKey.matches("^[abcdefghijkmnpqrstuvwxyz23456789]{26}$")) {
      throw new InvalidSyncKeyException();
    }
    return charKey;
  }

  /**
   * Open a URL in Fennec, if one is provided; or just open Fennec.
   *
   * @param context Android context.
   * @param url to visit, or null to just open Fennec.
   */
  public static void openURLInFennec(final Context context, final String url) {
    Intent intent;
    if (url != null) {
      intent = new Intent(Intent.ACTION_VIEW);
      intent.setData(Uri.parse(url));
    } else {
      intent = new Intent(Intent.ACTION_MAIN);
    }
    intent.setClassName(GlobalConstants.BROWSER_INTENT_PACKAGE, GlobalConstants.BROWSER_INTENT_CLASS);
    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
    context.startActivity(intent);
  }

  /**
   * Open a clicked span in Fennec with the provided URL.
   */
  public static class FennecClickableSpan extends ClickableSpan {
    private final String url;
    private final boolean underlining;

    public FennecClickableSpan(final String url, boolean underlining) {
      this.url = url;
      this.underlining = underlining;
    }

    @Override
    public void updateDrawState(TextPaint ds) {
      super.updateDrawState(ds);
      if (!this.underlining) {
        ds.setUnderlineText(false);
      }
    }

    @Override
    public void onClick(View widget) {
      openURLInFennec(widget.getContext(), this.url);
    }
  }

  /**
   * Replace the contents of a plain text view with the provided text wrapped in a link.
   * TODO: escape the URL!
   */
  public static void linkTextView(TextView view, int text, int link) {
    final Context context = view.getContext();
    linkTextView(view, context.getString(text), context.getString(link));
  }

  /**
   * Replace the contents of a plain text view with the provided text wrapped in a link.
   * TODO: escape the URL!
   */
  public static void linkTextView(TextView view, String text, String url) {
    view.setText("<a href=\"" + url + "\">" + text + "</a>");
    linkifyTextView(view, false);
  }

  public static void linkifyTextView(TextView textView, boolean underlining) {
    if (textView == null) {
      Logger.warn(LOG_TAG, "Could not process links for view.");
      return;
    }

    textView.setMovementMethod(LinkMovementMethod.getInstance());

    // Create spans.
    final Spanned spanned = Html.fromHtml(textView.getText().toString());

    // Replace the spans with Fennec-launching links.
    SpannableString replaced = new SpannableString(spanned);
    URLSpan[] spans = replaced.getSpans(0, replaced.length(), URLSpan.class);
    for (URLSpan span : spans) {
      final int start = replaced.getSpanStart(span);
      final int end = replaced.getSpanEnd(span);
      final int flags = replaced.getSpanFlags(span);

      replaced.removeSpan(span);
      replaced.setSpan(new FennecClickableSpan(span.getURL(), underlining), start, end, flags);
    }

    textView.setText(replaced);
  }
}
