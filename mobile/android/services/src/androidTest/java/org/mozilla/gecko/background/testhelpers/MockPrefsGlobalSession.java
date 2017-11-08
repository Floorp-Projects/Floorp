/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import android.content.Context;
import android.content.SharedPreferences;

import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SyncConfiguration;
import org.mozilla.gecko.sync.SyncConfigurationException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.delegates.GlobalSessionCallback;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;
import org.mozilla.gecko.sync.telemetry.TelemetryCollector;

import java.io.IOException;

/**
 * GlobalSession touches the Android prefs system. Stub that out.
 */
public class MockPrefsGlobalSession extends GlobalSession {

  public MockSharedPreferences prefs;

  public MockPrefsGlobalSession(
      SyncConfiguration config, GlobalSessionCallback callback, Context context,
      ClientsDataDelegate clientsDelegate)
      throws SyncConfigurationException, IllegalArgumentException, IOException,
      NonObjectJSONException {
    super(config, callback, context, clientsDelegate, new TelemetryCollector());
  }

  public static MockPrefsGlobalSession getSession(
      String username, String password,
      KeyBundle syncKeyBundle, GlobalSessionCallback callback, Context context,
      ClientsDataDelegate clientsDelegate)
      throws SyncConfigurationException, IllegalArgumentException, IOException,
      NonObjectJSONException {
    return getSession(username, new BasicAuthHeaderProvider(username, password), null,
         syncKeyBundle, callback, context, clientsDelegate);
  }

  public static MockPrefsGlobalSession getSession(
      String username, AuthHeaderProvider authHeaderProvider, String prefsPath,
      KeyBundle syncKeyBundle, GlobalSessionCallback callback, Context context,
      ClientsDataDelegate clientsDelegate)
      throws SyncConfigurationException, IllegalArgumentException, IOException,
      NonObjectJSONException {

    final SharedPreferences prefs = new MockSharedPreferences();
    final SyncConfiguration config = new SyncConfiguration(username, authHeaderProvider, prefs);
    config.syncKeyBundle = syncKeyBundle;
    return new MockPrefsGlobalSession(config, callback, context, clientsDelegate);
  }

  @Override
  public Context getContext() {
    return null;
  }
}
