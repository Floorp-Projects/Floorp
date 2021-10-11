/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.geckoview.test;

import android.app.Activity;
import android.content.ContextWrapper;
import android.os.Bundle;
import org.mozilla.geckoview.GeckoView;

public class GeckoViewTestActivity extends Activity {
  public GeckoView view;

  @Override
  protected void onCreate(final Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    view = new GeckoView(new ContextWrapper(this));
    setContentView(view);
  }
}
