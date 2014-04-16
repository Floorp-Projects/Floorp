/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.gecko.sync.setup.activities;

import org.mozilla.gecko.BrowserLocaleManager;
import org.mozilla.gecko.LocaleManager;

import android.annotation.TargetApi;
import android.app.Activity;
import android.content.Context;
import android.os.Build;
import android.os.Bundle;
import android.os.StrictMode;
import android.support.v4.app.FragmentActivity;

/**
 * This is a helper class to do typical locale switching operations
 * without hitting StrictMode errors or adding boilerplate to common
 * activity subclasses.
 *
 * Either call {@link LocaleAware#initializeLocale(Context)} in your
 * <code>onCreate</code> method, or inherit from <code>LocaleAwareFragmentActivity</code>
 * or <code>LocaleAwareActivity</code>.
 */
public class LocaleAware {
  @TargetApi(Build.VERSION_CODES.GINGERBREAD)
  public static void initializeLocale(Context context) {
    final LocaleManager localeManager = BrowserLocaleManager.getInstance();
    if (Build.VERSION.SDK_INT < Build.VERSION_CODES.GINGERBREAD) {
      localeManager.getAndApplyPersistedLocale(context);
    } else {
      final StrictMode.ThreadPolicy savedPolicy = StrictMode.allowThreadDiskReads();
      StrictMode.allowThreadDiskWrites();
      try {
        localeManager.getAndApplyPersistedLocale(context);
      } finally {
        StrictMode.setThreadPolicy(savedPolicy);
      }
    }
  }

  public static class LocaleAwareFragmentActivity extends FragmentActivity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
      LocaleAware.initializeLocale(getApplicationContext());
      super.onCreate(savedInstanceState);
    }
  }

  public static class LocaleAwareActivity extends Activity {
    @Override
    protected void onCreate(Bundle savedInstanceState) {
      LocaleAware.initializeLocale(getApplicationContext());
      super.onCreate(savedInstanceState);
    }
  }
}
