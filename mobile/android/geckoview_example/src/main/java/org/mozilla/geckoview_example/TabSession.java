/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview_example;

import androidx.annotation.NonNull;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoSessionSettings;
import org.mozilla.geckoview.WebExtension;

public class TabSession extends GeckoSession {
  private String mTitle;
  private String mUri;
  public WebExtension.Action action;

  public TabSession() {
    super();
  }

  public TabSession(GeckoSessionSettings settings) {
    super(settings);
  }

  public String getTitle() {
    return mTitle == null || mTitle.length() == 0 ? "about:blank" : mTitle;
  }

  public void setTitle(String title) {
    this.mTitle = title;
  }

  public String getUri() {
    return mUri;
  }

  @Override
  public void loadUri(@NonNull String uri) {
    super.loadUri(uri);
    mUri = uri;
  }

  public void onLocationChange(@NonNull String uri) {
    mUri = uri;
  }
}
