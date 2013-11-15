/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

package org.mozilla.gecko.background.testhelpers;

import java.io.IOException;

import org.json.simple.parser.ParseException;
import org.mozilla.gecko.sync.GlobalSession;
import org.mozilla.gecko.sync.NonObjectJSONException;
import org.mozilla.gecko.sync.SyncConfigurationException;
import org.mozilla.gecko.sync.crypto.KeyBundle;
import org.mozilla.gecko.sync.delegates.ClientsDataDelegate;
import org.mozilla.gecko.sync.delegates.GlobalSessionCallback;
import org.mozilla.gecko.sync.net.AuthHeaderProvider;
import org.mozilla.gecko.sync.net.BasicAuthHeaderProvider;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;

/**
 * GlobalSession touches the Android prefs system. Stub that out.
 */
public class MockPrefsGlobalSession extends GlobalSession {

  public MockSharedPreferences prefs;

  public MockPrefsGlobalSession(String serverURL,
      String username, String password, String prefsPath,
      KeyBundle syncKeyBundle, GlobalSessionCallback callback, Context context,
      Bundle extras, ClientsDataDelegate clientsDelegate)
      throws SyncConfigurationException, IllegalArgumentException, IOException,
      ParseException, NonObjectJSONException {
    this(serverURL, username, new BasicAuthHeaderProvider(username, password), prefsPath, syncKeyBundle, callback, context, extras, clientsDelegate);
  }

  public MockPrefsGlobalSession(String serverURL,
      String username, AuthHeaderProvider authHeaderProvider, String prefsPath,
      KeyBundle syncKeyBundle, GlobalSessionCallback callback, Context context,
      Bundle extras, ClientsDataDelegate clientsDelegate)
      throws SyncConfigurationException, IllegalArgumentException, IOException,
      ParseException, NonObjectJSONException {
    super(serverURL, username, authHeaderProvider, prefsPath, syncKeyBundle,
        callback, context, extras, clientsDelegate);
  }

  @Override
  public SharedPreferences getPrefs(String name, int mode) {
    if (prefs == null) {
      prefs = new MockSharedPreferences();
    }
    return prefs;
  }

  @Override
  public Context getContext() {
    return null;
  }

}
