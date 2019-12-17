package org.mozilla.geckoview.example.messaging;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoView;
import org.mozilla.geckoview.WebExtension;

public class MainActivity extends AppCompatActivity {
    private static GeckoRuntime sRuntime;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        GeckoView view = findViewById(R.id.geckoview);
        GeckoSession session = new GeckoSession();

        if (sRuntime == null) {
            sRuntime = GeckoRuntime.create(this);
        }

        WebExtension.MessageDelegate messageDelegate = new WebExtension.MessageDelegate() {
            @Nullable
            @Override
            public GeckoResult<Object> onMessage(final @NonNull String nativeApp,
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

        WebExtension extension = new WebExtension(
                "resource://android/assets/messaging/",
                "myextension",
                WebExtension.Flags.ALLOW_CONTENT_MESSAGING,
                sRuntime.getWebExtensionController());

        sRuntime.registerWebExtension(extension).exceptionally(e -> {
            Log.e("MessageDelegate", "Error registering WebExtension", e);
            return null;
        });

        session.setMessageDelegate(extension, messageDelegate, "browser");

        session.open(sRuntime);
        view.setSession(session);
        session.loadUri("https://mobile.twitter.com");
    }
}
