/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.example.messaging;

import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoView;
import org.mozilla.geckoview.WebExtension;

//
// The apk file for this example can be build using the following mach command:
//
//     mach gradle port_messaging_example:build
//
// Formatting error reported by Spotless as part of the build can be autofixed
// can be fixed using the mach command:
//
//     mach gradle port_messaging_example:spotlessApply
//
// After the gradle task messaging_example:build runs successfully, an apk
// file will be stored in the OBJDIR/gradle/build/mobile/android/examples/port_messaging_example.
// subdirectories

public class MainActivity extends AppCompatActivity {
  private static GeckoRuntime sRuntime;

  private WebExtension.Port mPort;

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    setContentView(R.layout.activity_main);

    GeckoView view = findViewById(R.id.geckoview);
    GeckoSession session = new GeckoSession();

    if (sRuntime == null) {
      GeckoRuntimeSettings settings =
          new GeckoRuntimeSettings.Builder().remoteDebuggingEnabled(true).build();
      sRuntime = GeckoRuntime.create(this, settings);
    }

    WebExtension.PortDelegate portDelegate =
        new WebExtension.PortDelegate() {
          @Override
          public void onPortMessage(
              final @NonNull Object message, final @NonNull WebExtension.Port port) {
            Log.d("PortDelegate", "Received message from extension: " + message);
          }

          @Override
          public void onDisconnect(final @NonNull WebExtension.Port port) {
            // This port is not usable anymore.
            if (port == mPort) {
              mPort = null;
            }
          }
        };

    WebExtension.MessageDelegate messageDelegate =
        new WebExtension.MessageDelegate() {
          @Override
          @Nullable
          public void onConnect(final @NonNull WebExtension.Port port) {
            mPort = port;
            mPort.setDelegate(portDelegate);
          }
        };

    sRuntime
        .getWebExtensionController()
        .ensureBuiltIn("resource://android/assets/messaging/", "messaging@example.com")
        .accept(
            // Register message delegate for background script
            extension ->
                ThreadUtils.runOnUiThread(
                    new Runnable() {
                      @Override
                      public void run() {
                        extension.setMessageDelegate(messageDelegate, "browser");
                      }
                    }),
            e -> Log.e("MessageDelegate", "Error registering WebExtension", e));

    session.open(sRuntime);
    view.setSession(session);
    session.loadUri("https://mobile.twitter.com");
  }

  @Override
  public boolean onKeyLongPress(int keyCode, KeyEvent event) {
    if (mPort == null) {
      // No extension registered yet, let's ignore this message
      return false;
    }

    JSONObject message = new JSONObject();
    try {
      message.put("keyCode", keyCode);
      message.put("event", KeyEvent.keyCodeToString(event.getKeyCode()));
    } catch (JSONException ex) {
      throw new RuntimeException(ex);
    }

    mPort.postMessage(message);
    return true;
  }
}
