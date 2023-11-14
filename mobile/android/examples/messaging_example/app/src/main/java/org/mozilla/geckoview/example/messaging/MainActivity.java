/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview.example.messaging;

import android.os.Bundle;
import android.util.Log;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.appcompat.app.AppCompatActivity;
import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.util.ThreadUtils;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoRuntimeSettings;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoView;
import org.mozilla.geckoview.WebExtension;

// The apk file for this example can be build using the following mach command:
//
//     mach gradle messaging_example:build
//
// Formatting error reported by Spotless as part of the build can be autofixed
// can be fixed using the mach command:
//
//     mach gradle messaging_example:spotlessApply
//
// After the gradle task messaging_example:build runs successfully, an apk
// file will be stored in the OBJDIR/gradle/build/mobile/android/examples/messaging_example.
// subdirectories

public class MainActivity extends AppCompatActivity {
  private static GeckoRuntime sRuntime;

  private static final String EXTENSION_LOCATION = "resource://android/assets/messaging/";
  private static final String EXTENSION_ID = "messaging@example.com";
  // If you make changes to the extension you need to update this
  private static final String EXTENSION_VERSION = "1.0";

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

    WebExtension.MessageDelegate messageDelegate =
        new WebExtension.MessageDelegate() {
          @Nullable
          @Override
          public GeckoResult<Object> onMessage(
              final @NonNull String nativeApp,
              final @NonNull Object message,
              final @NonNull WebExtension.MessageSender sender) {
            if (message instanceof JSONObject) {
              JSONObject json = (JSONObject) message;
              try {
                if (json.has("type") && "WPAManifest".equals(json.getString("type"))) {
                  JSONObject manifest = json.getJSONObject("manifest");
                  Log.d("MessageDelegate", "Found WPA manifest: " + manifest);
                }
              } catch (JSONException ex) {
                Log.e("MessageDelegate", "Invalid manifest", ex);
              }
            }
            return null;
          }
        };

    // Let's make sure the extension is installed
    sRuntime
        .getWebExtensionController()
        .ensureBuiltIn(EXTENSION_LOCATION, "messaging@example.com")
        .accept(
            // Set delegate that will receive messages coming from this extension.
            extension ->
                ThreadUtils.runOnUiThread(
                    new Runnable() {
                      @Override
                      public void run() {
                        session
                            .getWebExtensionController()
                            .setMessageDelegate(extension, messageDelegate, "browser");
                      }
                    }),
            // Something bad happened, let's log an error
            e -> Log.e("MessageDelegate", "Error registering extension", e));

    session.open(sRuntime);
    view.setSession(session);
    session.loadUri("https://mobile.twitter.com");
  }
}
