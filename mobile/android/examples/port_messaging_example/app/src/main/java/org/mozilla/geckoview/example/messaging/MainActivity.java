package org.mozilla.geckoview.example.messaging;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.KeyEvent;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.geckoview.GeckoResult;
import org.mozilla.geckoview.GeckoRuntime;
import org.mozilla.geckoview.GeckoSession;
import org.mozilla.geckoview.GeckoView;
import org.mozilla.geckoview.WebExtension;

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
            sRuntime = GeckoRuntime.create(this);
        }

        WebExtension.PortDelegate portDelegate = new WebExtension.PortDelegate() {
            @Override
            public void onPortMessage(final @NonNull Object message,
                                      final @NonNull WebExtension.Port port) {
                Log.d("PortDelegate", "Received message from WebExtension: "
                        + message);
            }

            @Override
            public void onDisconnect(final @NonNull WebExtension.Port port) {
                // This port is not usable anymore.
                if (port == mPort) {
                    mPort = null;
                }
            }
        };

        WebExtension.MessageDelegate messageDelegate = new WebExtension.MessageDelegate() {
            @Override
            @Nullable
            public void onConnect(final @NonNull WebExtension.Port port) {
                mPort = port;
                mPort.setDelegate(portDelegate);
            }
        };

        WebExtension extension = new WebExtension(
                "resource://android/assets/messaging/",
                sRuntime.getWebExtensionController());

        extension.setMessageDelegate(messageDelegate, "browser");

        sRuntime.registerWebExtension(extension).exceptionally(e -> {
            Log.e("MessageDelegate", "Error registering WebExtension", e);
            return null;
        });

        session.open(sRuntime);
        view.setSession(session);
        session.loadUri("https://mobile.twitter.com");
    }

    @Override
    public boolean onKeyLongPress(int keyCode, KeyEvent event) {
        if (mPort == null) {
            // No WebExtension registered yet, let's ignore this message
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
