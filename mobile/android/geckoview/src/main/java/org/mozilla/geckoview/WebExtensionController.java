package org.mozilla.geckoview;

import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.util.Log;

import org.json.JSONException;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

public class WebExtensionController {
    private final static String LOGTAG = "WebExtension";

    private GeckoRuntime mRuntime;

    private boolean mHandlerRegistered = false;

    private TabDelegate mTabDelegate;
    private PromptDelegate mPromptDelegate;

    private Map<String, WebExtension> mExtensions = new HashMap<>();
    private Map<Long, WebExtension.Port> mPorts = new HashMap<>();

    private Internals mInternals = new Internals();

    // Avoids exposing listeners to the API
    private class Internals implements BundleEventListener,
            WebExtension.Port.DisconnectDelegate {
        @Override
        public void handleMessage(final String event, final GeckoBundle message,
                                  final EventCallback callback) {
            WebExtensionController.this.handleMessage(event, message, callback, null);
        }

        @Override
        public void onDisconnectFromApp(final WebExtension.Port port) {
            // If the port has been disconnected from the app side, we don't need to notify anyone and
            // we just need to remove it from our list of ports.
            mPorts.remove(port.id);
        }
    }

    public interface TabDelegate {
        /**
         * Called when tabs.create is invoked, this method returns a *newly-created* session
         * that GeckoView will use to load the requested page on. If the returned value
         * is null the page will not be opened.
         *
         * @param source An instance of {@link WebExtension} or null if extension was not registered
         *               with GeckoRuntime.registerWebextension
         * @param uri The URI to be loaded. This is provided for informational purposes only,
         *            do not call {@link GeckoSession#loadUri} on it.
         * @return A {@link GeckoResult} which holds the returned GeckoSession. May be null, in
         *        which case the request for a new tab by the extension will fail.
         *        The implementation of onNewTab is responsible for maintaining a reference
         *        to the returned object, to prevent it from being garbage collected.
         */
        @UiThread
        @Nullable
        default GeckoResult<GeckoSession> onNewTab(@Nullable WebExtension source, @Nullable String uri) {
            return null;
        }
        /**
         * Called when tabs.remove is invoked, this method decides if WebExtension can close the
         * tab. In case WebExtension can close the tab, it should close passed GeckoSession and
         * return GeckoResult.ALLOW or GeckoResult.DENY in case tab cannot be closed.
         *
         * @param source An instance of {@link WebExtension} or null if extension was not registered
         *               with GeckoRuntime.registerWebextension
         * @param session An instance of {@link GeckoSession} to be closed.
         * @return GeckoResult.ALLOW if the tab will be closed, GeckoResult.DENY otherwise
         */
        @UiThread
        @NonNull
        default GeckoResult<AllowOrDeny> onCloseTab(@Nullable WebExtension source, @NonNull GeckoSession session)  {
            return GeckoResult.DENY;
        }
    }

    @UiThread
    public @Nullable TabDelegate getTabDelegate() {
        return mTabDelegate;
    }

    @UiThread
    public void setTabDelegate(final @Nullable TabDelegate delegate) {
        if (delegate == null) {
            if (mTabDelegate != null) {
                EventDispatcher.getInstance().unregisterUiThreadListener(
                        mInternals,
                        "GeckoView:WebExtension:NewTab"
                );
            }
        } else {
            if (mTabDelegate == null) {
                EventDispatcher.getInstance().registerUiThreadListener(
                        mInternals,
                        "GeckoView:WebExtension:NewTab"
                );
            }
        }
        mTabDelegate = delegate;
    }

    // TODO: make public
    interface PromptDelegate {
        default GeckoResult<AllowOrDeny> onInstallPrompt(WebExtension extension) {
            return null;
        }
        default GeckoResult<AllowOrDeny> onUpdatePrompt(
                WebExtension currentlyInstalled,
                WebExtension updatedExtension,
                String[] newPermissions) {
            return null;
        }
        default GeckoResult<AllowOrDeny> onOptionalPrompt(
                WebExtension extension,
                String[] optionalPermissions) {
            return null;
        }
    }

    // TODO: make public
    PromptDelegate getPromptDelegate() {
        return mPromptDelegate;
    }

    // TODO: make public
    void setPromptDelegate(final PromptDelegate delegate) {
        if (delegate == null && mPromptDelegate != null) {
            EventDispatcher.getInstance().unregisterUiThreadListener(
                    mInternals,
                    "GeckoView:WebExtension:InstallPrompt",
                    "GeckoView:WebExtension:UpdatePrompt",
                    "GeckoView:WebExtension:OptionalPrompt"
            );
        } else if (delegate != null && mPromptDelegate == null) {
            EventDispatcher.getInstance().unregisterUiThreadListener(
                    mInternals,
                    "GeckoView:WebExtension:InstallPrompt",
                    "GeckoView:WebExtension:UpdatePrompt",
                    "GeckoView:WebExtension:OptionalPrompt"
            );
        }

        mPromptDelegate = delegate;
    }

    private static class WebExtensionResult extends CallbackResult<WebExtension> {
        private final String mFieldName;

        public WebExtensionResult(final String fieldName) {
            mFieldName = fieldName;
        }

        @Override
        public void sendSuccess(final Object response) {
            final GeckoBundle bundle = (GeckoBundle) response;
            complete(new WebExtension(bundle.getBundle(mFieldName)));
        }
    }

    // TODO: make public
    GeckoResult<WebExtension> install(final String uri) {
        final CallbackResult<WebExtension> result = new WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("locationUri", uri);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Install",
                bundle, result);

        return result;
    }

    // TODO: make public
    GeckoResult<WebExtension> installBuiltIn(final String uri) {
        final CallbackResult<WebExtension> result = new WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("locationUri", uri);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:InstallBuiltIn",
                bundle, result);

        return result;
    }

    // TODO: make public
    GeckoResult<Void> uninstall(final WebExtension extension) {
        final CallbackResult<Void> result = new CallbackResult<Void>() {
            @Override
            public void sendSuccess(final Object response) {
                complete(null);
            }
        };

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("webExtensionId", extension.id);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Uninstall",
                bundle, result);

        return result;
    }

    // TODO: make public
    GeckoResult<WebExtension> enable(final WebExtension extension) {
        final CallbackResult<WebExtension> result = new WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("webExtensionId", extension.id);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Enable",
                bundle, result);

        return result;
    }

    // TODO: make public
    GeckoResult<WebExtension> disable(final WebExtension extension) {
        final CallbackResult<WebExtension> result = new WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("webExtensionId", extension.id);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Disable",
                bundle, result);

        return result;
    }

    // TODO: make public
    GeckoResult<List<WebExtension>> listInstalled() {
        final CallbackResult<List<WebExtension>> result = new CallbackResult<List<WebExtension>>() {
            @Override
            public void sendSuccess(final Object response) {
                final GeckoBundle[] bundles = ((GeckoBundle) response)
                        .getBundleArray("extensions");
                final List<WebExtension> list = new ArrayList<>(bundles.length);
                for (GeckoBundle bundle : bundles) {
                    list.add(new WebExtension(bundle));
                }

                complete(list);
            }
        };

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:List",
                null, result);

        return result;
    }

    // TODO: make public
    GeckoResult<WebExtension> update(final WebExtension extension) {
        final CallbackResult<WebExtension> result = new WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("webExtensionId", extension.id);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Update",
                bundle, result);

        return result;
    }

    /* package */ WebExtensionController(final GeckoRuntime runtime) {
        mRuntime = runtime;
    }

    /* package */ void registerWebExtension(final WebExtension webExtension) {
        if (!mHandlerRegistered) {
            EventDispatcher.getInstance().registerUiThreadListener(
                    mInternals,
                    "GeckoView:WebExtension:Message",
                    "GeckoView:WebExtension:PortMessage",
                    "GeckoView:WebExtension:Connect",
                    "GeckoView:WebExtension:Disconnect",

                    // {Browser,Page}Actions
                    "GeckoView:BrowserAction:Update",
                    "GeckoView:BrowserAction:OpenPopup",
                    "GeckoView:PageAction:Update",
                    "GeckoView:PageAction:OpenPopup"
            );
            mHandlerRegistered = true;
        }

        mExtensions.put(webExtension.id, webExtension);
    }

    /* package */ void handleMessage(final String event, final GeckoBundle message,
                                     final EventCallback callback, final GeckoSession session) {
        if ("GeckoView:WebExtension:NewTab".equals(event)) {
            newTab(message, callback);
            return;
        } else if ("GeckoView:WebExtension:Disconnect".equals(event)) {
            disconnect(message.getLong("portId", -1), callback);
            return;
        } else if ("GeckoView:WebExtension:PortMessage".equals(event)) {
            portMessage(message, callback);
            return;
        } else if ("GeckoView:BrowserAction:Update".equals(event)) {
            actionUpdate(message, session, WebExtension.Action.TYPE_BROWSER_ACTION);
            return;
        } else if ("GeckoView:PageAction:Update".equals(event)) {
            actionUpdate(message, session, WebExtension.Action.TYPE_PAGE_ACTION);
            return;
        } else if ("GeckoView:BrowserAction:OpenPopup".equals(event)) {
            openPopup(message, session, WebExtension.Action.TYPE_BROWSER_ACTION);
            return;
        } else if ("GeckoView:PageAction:OpenPopup".equals(event)) {
            openPopup(message, session, WebExtension.Action.TYPE_PAGE_ACTION);
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

    private void newTab(final GeckoBundle message, final EventCallback callback) {
        if (mTabDelegate == null) {
            callback.sendSuccess(null);
            return;
        }

        WebExtension extension = mExtensions.get(message.getString("extensionId"));

        final GeckoResult<GeckoSession> result = mTabDelegate.onNewTab(extension, message.getString("uri"));

        if (result == null) {
            callback.sendSuccess(null);
            return;
        }

        result.accept(session -> {
            if (session == null) {
                callback.sendSuccess(null);
                return;
            }

            if (session.isOpen()) {
                throw new IllegalArgumentException("Must use an unopened GeckoSession instance");
            }

            session.open(mRuntime);

            callback.sendSuccess(session.getId());
        });
    }

    /* package */ void closeTab(final GeckoBundle message, final EventCallback callback, final GeckoSession session) {
        if (mTabDelegate == null) {
            callback.sendError(null);
            return;
        }

        WebExtension extension = mExtensions.get(message.getString("extensionId"));

        GeckoResult<AllowOrDeny> result = mTabDelegate.onCloseTab(extension, session);

        result.accept(value -> {
            if (value == AllowOrDeny.ALLOW) {
                callback.sendSuccess(null);
            } else {
                callback.sendError(null);
            }
        });
    }


    /* package */ void unregisterWebExtension(final WebExtension webExtension) {
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

    /* package */ @Nullable WebExtension getWebExtension(final String id) {
        return mExtensions.get(id);
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
            delegate = sender.session.getMessageDelegate(sender.webExtension, nativeApp);
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

        final WebExtension.Port port = new WebExtension.Port(nativeApp, portId, sender, mInternals);
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

        final GeckoResult<Object> response = delegate.onMessage(nativeApp, content, sender);
        if (response == null) {
            callback.sendSuccess(null);
            return;
        }

        response.accept(
            value -> callback.sendSuccess(value),
            exception -> callback.sendError(exception));
    }

    private WebExtension extensionFromBundle(final GeckoBundle message) {
        final String extensionId = message.getString("extensionId");

        final WebExtension extension = mExtensions.get(extensionId);
        if (extension == null) {
            if (BuildConfig.DEBUG) {
                // TODO: Bug 1582185 Some gecko tests install WebExtensions that we
                // don't know about and cause this to trigger.
                // throw new RuntimeException("Could not find extension: " + extensionId);
            }
            Log.e(LOGTAG, "Could not find extension: " + extensionId);
        }

        return extension;
    }

    private void openPopup(final GeckoBundle message, final GeckoSession session,
                           final @WebExtension.Action.ActionType int actionType) {
        final WebExtension extension = extensionFromBundle(message);
        if (extension == null) {
            return;
        }

        final WebExtension.Action action = new WebExtension.Action(
                actionType, message.getBundle("action"), extension);

        final WebExtension.ActionDelegate delegate = actionDelegateFor(extension, session);
        if (delegate == null) {
            return;
        }

        final GeckoResult<GeckoSession> popup = delegate.onOpenPopup(extension, action);
        action.openPopup(popup);
    }

    private WebExtension.ActionDelegate actionDelegateFor(final WebExtension extension,
                                                          final GeckoSession session) {
        if (session == null) {
            return extension.actionDelegate;
        }

        return session.getWebExtensionActionDelegate(extension);
    }

    private void actionUpdate(final GeckoBundle message, final GeckoSession session,
                              final @WebExtension.Action.ActionType int actionType) {
        final WebExtension extension = extensionFromBundle(message);
        if (extension == null) {
            return;
        }

        final WebExtension.ActionDelegate delegate = actionDelegateFor(extension, session);
        if (delegate == null) {
            return;
        }

        final WebExtension.Action action = new WebExtension.Action(
                actionType, message.getBundle("action"), extension);
        if (actionType == WebExtension.Action.TYPE_BROWSER_ACTION) {
            delegate.onBrowserAction(extension, session, action);
        } else if (actionType == WebExtension.Action.TYPE_PAGE_ACTION) {
            delegate.onPageAction(extension, session, action);
        }
    }
}
