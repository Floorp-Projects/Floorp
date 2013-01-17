/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import java.util.ArrayList;
import java.util.List;

import android.content.Intent;
import android.os.Bundle;

/**
 * A static factory that extracts (title, uri) pairs suitable for send tab from
 * Android intent instances.
 * <p>
 * Takes some care to extract likely "Web URLs" in preference to general URIs.
 */
public class SendTabData {
  public final String title;
  public final String uri;

  public SendTabData(String title, String uri) {
    this.title = title;
    this.uri = uri;
  }

  public static SendTabData fromIntent(Intent intent) {
    if (intent == null) {
      throw new IllegalArgumentException("intent must not be null");
    }

    return fromBundle(intent.getExtras());
  }

  protected static SendTabData fromBundle(Bundle bundle) {
    if (bundle == null) {
      throw new IllegalArgumentException("bundle must not be null");
    }

    String text = bundle.getString(Intent.EXTRA_TEXT);
    String subject = bundle.getString(Intent.EXTRA_SUBJECT);
    String title = bundle.getString(Intent.EXTRA_TITLE);

    // For title, prefer EXTRA_SUBJECT but accept EXTRA_TITLE.
    String theTitle = subject;
    if (theTitle == null) {
      theTitle = title;
    }

    // For URL, take first URL from EXTRA_TEXT, EXTRA_SUBJECT, and EXTRA_TITLE
    // (in that order).
    List<String> strings = new ArrayList<String>();
    strings.add(text);
    strings.add(subject);
    strings.add(title);
    String theUri = new WebURLFinder(strings).bestWebURL();

    return new SendTabData(theTitle, theUri);
  }
}
