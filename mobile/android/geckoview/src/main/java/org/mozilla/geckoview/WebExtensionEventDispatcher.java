package org.mozilla.geckoview;

import android.util.Log;

import org.json.JSONException;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.util.HashMap;
import java.util.Iterator;
import java.util.Map;

/* protected */ class WebExtensionEventDispatcher implements BundleEventListener,
        WebExtension.Port.DisconnectDelegate {
    private final static String LOGTAG = "WebExtension";

    private boolean mHandlerRegistered = false;

    private Map<String, WebExtension> mExtensions = new HashMap<>();
    private Map<Long, WebExtension.Port> mPorts = new HashMap<>();

    public void registerWebExtension(final WebExtension webExtension) {
        if (!mHandlerRegistered) {
            EventDispatcher.getInstance().registerUiThreadListener(
                    this,
                    "GeckoView:WebExtension:Message",
                    "GeckoView:WebExtension:PortMessage",
                    "GeckoView:WebExtension:Connect",
                    "GeckoView:WebExtension:Disconnect"
            );
            mHandlerRegistered = true;
        }

        mExtensions.put(webExtension.id, webExtension);
    }

    public void unregisterWebExtension(final WebExtension webExtension) {
        mExtensions.remove(webExtension.id);

        // Some ports may still be open so we need to go through the list and close all of the
        // ports tied to this web extension
        Iterator<Map.Entry<Long, WebExtension.Port>> it = mPorts.entrySet().iterator();
        while (it.hasNext()) {
            WebExtension.Port port = it.next().getValue();

            if (port.sender.webExtension.equals(webExtension)) {
                it.remove();
            }
        }
    }

    @Override
    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback) {
        handleMessage(event, message, callback, null);
    }

    private WebExtension.MessageSender fromBundle(final GeckoBundle sender,
                                                  final GeckoSession session) {
        final String extensionId = sender.getString("extensionId");
        WebExtension extension = mExtensions.get(extensionId);

        if (extension == null) {
            // All senders should have an extension
            return null;
        }

        final String envType = sender.getString("envType");
        @WebExtension.MessageSender.EnvType int environmentType;

        if ("content_child".equals(envType)) {
            environmentType = WebExtension.MessageSender.ENV_TYPE_CONTENT_SCRIPT;
        } else if ("addon_child".equals(envType)) {
            // TODO Bug 1554277: check that this message is coming from the right process
            environmentType = WebExtension.MessageSender.ENV_TYPE_EXTENSION;
        } else {
            environmentType = WebExtension.MessageSender.ENV_TYPE_UNKNOWN;
        }

        if (environmentType == WebExtension.MessageSender.ENV_TYPE_UNKNOWN) {
            if (BuildConfig.DEBUG) {
                throw new RuntimeException("Missing or unknown envType.");
            }

            return null;
        }

        final String url = sender.getString("url");
        boolean isTopLevel;
        if (session == null) {
            // This message is coming from the background page
            isTopLevel = true;
        } else {
            // If session is present we are either receiving this message from a content script or
            // an extension page, let's make sure we have the proper identification so that
            // embedders can check the origin of this message.
            if (!sender.containsKey("frameId") || !sender.containsKey("url") ||
                    // -1 is an invalid frame id
                    sender.getInt("frameId", -1) == -1) {
                if (BuildConfig.DEBUG) {
                    throw new RuntimeException("Missing sender information.");
                }

                // This message does not have the proper identification and may be compromised,
                // let's ignore it.
                return null;
            }

            isTopLevel = sender.getInt("frameId", -1) == 0;
        }

        return new WebExtension.MessageSender(extension, session, url, environmentType, isTopLevel);
    }

    @Override
    public void onDisconnectFromApp(final WebExtension.Port port) {
        // If the port has been disconnected from the app side, we don't need to notify anyone and
        // we just need to remove it from our list of ports.
        mPorts.remove(port.id);
    }

    private void disconnect(final long portId, final EventCallback callback) {
        final WebExtension.Port port = mPorts.get(portId);
        if (port == null) {
            Log.d(LOGTAG, "Could not find recipient for port " + portId);
            return;
        }

        port.delegate.onDisconnect(port);
        mPorts.remove(portId);

        if (callback != null) {
            callback.sendSuccess(true);
        }
    }

    private WebExtension.MessageDelegate getDelegate(
            final String nativeApp, final WebExtension.MessageSender sender,
            final EventCallback callback) {
        if ((sender.webExtension.flags & WebExtension.Flags.ALLOW_CONTENT_MESSAGING) == 0 &&
                sender.environmentType == WebExtension.MessageSender.ENV_TYPE_CONTENT_SCRIPT) {
            callback.sendError("This NativeApp can't receive messages from Content Scripts.");
            return null;
        }

        WebExtension.MessageDelegate delegate = null;

        if (sender.session != null) {
            delegate = sender.session.getMessageDelegate(nativeApp);
        } else if (sender.environmentType == WebExtension.MessageSender.ENV_TYPE_EXTENSION) {
            delegate = sender.webExtension.messageDelegates.get(nativeApp);
        }

        if (delegate == null) {
            callback.sendError("Native app not found or this WebExtension does not have permissions.");
            return null;
        }

        return delegate;
    }

    private void connect(final String nativeApp, final long portId, final EventCallback callback,
                         final WebExtension.MessageSender sender) {
        if (portId == -1) {
            callback.sendError("Missing portId.");
            return;
        }

        final WebExtension.Port port = new WebExtension.Port(nativeApp, portId, sender, this);
        mPorts.put(port.id, port);

        final WebExtension.MessageDelegate delegate = getDelegate(nativeApp, sender, callback);
        if (delegate == null) {
            return;
        }

        delegate.onConnect(port);
        callback.sendSuccess(true);
    }

    private void portMessage(final GeckoBundle message, final EventCallback callback) {
        final long portId = message.getLong("portId", -1);
        final WebExtension.Port port = mPorts.get(portId);
        if (port == null) {
            callback.sendError("Could not find recipient for port " + portId);
            return;
        }

        final Object content;
        try {
            content = message.toJSONObject().get("data");
        } catch (JSONException ex) {
            callback.sendError(ex);
            return;
        }

        port.delegate.onPortMessage(content, port);
        callback.sendSuccess(null);
    }

    private void message(final String nativeApp, final GeckoBundle message,
                         final EventCallback callback, final WebExtension.MessageSender sender) {
        final Object content;
        try {
            content = message.toJSONObject().get("data");
        } catch (JSONException ex) {
            callback.sendError(ex);
            return;
        }

        final WebExtension.MessageDelegate delegate = getDelegate(nativeApp, sender, callback);
        if (delegate == null) {
            return;
        }

        final GeckoResult<Object> response = delegate.onMessage(content, sender);
        if (response == null) {
            callback.sendSuccess(null);
            return;
        }

        response.then(
            value -> {
                callback.sendSuccess(value);
                return null;
            },
            exception -> {
                callback.sendError(exception);
                return null;
            });
    }

    public void handleMessage(final String event, final GeckoBundle message,
                              final EventCallback callback, final GeckoSession session) {
        if ("GeckoView:WebExtension:Disconnect".equals(event)) {
            disconnect(message.getLong("portId", -1), callback);
            return;
        } else if ("GeckoView:WebExtension:PortMessage".equals(event)) {
            portMessage(message, callback);
            return;
        }

        final String nativeApp = message.getString("nativeApp");
        if (nativeApp == null) {
            if (BuildConfig.DEBUG) {
                throw new RuntimeException("Missing required nativeApp message parameter.");
            }
            callback.sendError("Missing nativeApp parameter.");
            return;
        }

        final WebExtension.MessageSender sender = fromBundle(message.getBundle("sender"), session);
        if (sender == null) {
            if (callback != null) {
                callback.sendError("Could not find recipient for " + message.getBundle("sender"));
            }
            return;
        }

        if ("GeckoView:WebExtension:Connect".equals(event)) {
            connect(nativeApp, message.getLong("portId", -1), callback, sender);
        } else if ("GeckoView:WebExtension:Message".equals(event)) {
            message(nativeApp, message, callback, sender);
        }
    }
}
