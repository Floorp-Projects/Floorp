package org.mozilla.geckoview;

import android.support.annotation.AnyThread;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.util.Log;

import org.json.JSONException;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.MultiMap;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

public class WebExtensionController {
    private final static String LOGTAG = "WebExtension";

    private PromptDelegate mPromptDelegate;
    private final WebExtension.Listener mListener;

    // Map [ portId -> Message ]
    private final MultiMap<Long, Message> mPendingPortMessages;
    // Map [ extensionId -> Message ]
    private final MultiMap<String, Message> mPendingMessages;

    private static class Message {
        final GeckoBundle bundle;
        final EventCallback callback;
        final String event;
        final GeckoSession session;
        public Message(final String event, final GeckoBundle bundle, final EventCallback callback,
                       final GeckoSession session) {
            this.bundle = bundle;
            this.callback = callback;
            this.event = event;
            this.session = session;
        }
    }

    private static class ExtensionStore {
        final private Map<String, WebExtension> mData = new HashMap<>();

        public GeckoResult<WebExtension> get(final String id) {
            final WebExtension extension = mData.get(id);
            if (extension == null) {
                final WebExtensionResult result = new WebExtensionResult("extension");

                final GeckoBundle bundle = new GeckoBundle(1);
                bundle.putString("extensionId", id);

                EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Get",
                    bundle, result);

                return result.then(ext -> {
                    mData.put(ext.id, ext);
                    return GeckoResult.fromValue(ext);
                });
            }

            return GeckoResult.fromValue(extension);
        }

        public void remove(final String id) {
            mData.remove(id);
        }

        /**
         * Add this extension to the store and update it's current value if it's already present.
         *
         * @param id the {@link WebExtension} id.
         * @param extension the {@link WebExtension} to add to the store.
         */
        public void update(final String id, final WebExtension extension) {
            mData.put(id, extension);
        }
    }

    private ExtensionStore mExtensions = new ExtensionStore();

    private Map<Long, WebExtension.Port> mPorts = new HashMap<>();

    private Internals mInternals = new Internals();

    // Avoids exposing listeners to the API
    private class Internals implements BundleEventListener,
            WebExtension.Port.Observer {
        @Override
        // BundleEventListener
        public void handleMessage(final String event, final GeckoBundle message,
                                  final EventCallback callback) {
            WebExtensionController.this.handleMessage(event, message, callback, null);
        }

        @Override
        // WebExtension.Port.Observer
        public void onDisconnectFromApp(final WebExtension.Port port) {
            // If the port has been disconnected from the app side, we don't need to notify anyone and
            // we just need to remove it from our list of ports.
            mPorts.remove(port.id);
        }

        @Override
        // WebExtension.Port.Observer
        public void onDelegateAttached(final WebExtension.Port port) {
            if (port.delegate == null) {
                return;
            }

            for (final Message message : mPendingPortMessages.get(port.id)) {
                WebExtensionController.this.portMessage(message);
            }

            mPendingPortMessages.remove(port.id);
        }
    }

    private class DelegateController implements WebExtension.DelegateController {
        private WebExtension mExtension;

        public DelegateController(final WebExtension extension) {
            mExtension = extension;
        }

        @Override
        public void onMessageDelegate(final String nativeApp,
                                      final WebExtension.MessageDelegate delegate) {
            mListener.setMessageDelegate(mExtension, delegate, nativeApp);

            if (delegate == null) {
                return;
            }

            for (final Message message : mPendingMessages.get(mExtension.id)) {
                WebExtensionController.this.handleMessage(message.event, message.bundle,
                        message.callback, message.session);
            }

            mPendingMessages.remove(mExtension.id);
        }

        @Override
        public void onActionDelegate(final WebExtension.ActionDelegate delegate) {
            mListener.setActionDelegate(mExtension, delegate);
        }

        @Override
        public WebExtension.ActionDelegate getActionDelegate() {
            return mListener.getActionDelegate(mExtension);
        }
    }

    // TODO: remove once we remove GeckoRuntime#registerWebExtension
    /* package */ DelegateController delegateFor(final WebExtension extension) {
        return new DelegateController(extension);
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
        return mListener.getTabDelegate();
    }

    @UiThread
    public void setTabDelegate(final @Nullable TabDelegate delegate) {
        mListener.setTabDelegate(delegate);
    }

    /**
     * This delegate will be called whenever an extension is about to be installed or it needs
     * new permissions, e.g during an update or because it called <code>permissions.request</code>
     */
    @UiThread
    public interface PromptDelegate {
        /**
         * Called whenever a new extension is being installed. This is intended as an
         * opportunity for the app to prompt the user for the permissions required by
         * this extension.
         *
         * @param extension The {@link WebExtension} that is about to be installed.
         *                  You can use {@link WebExtension#metaData} to gather information
         *                  about this extension when building the user prompt dialog.
         * @return A {@link GeckoResult} that completes to either {@link AllowOrDeny#ALLOW ALLOW}
         *         if this extension should be installed or {@link AllowOrDeny#DENY DENY} if
         *         this extension should not be installed. A null value will be interpreted as
         *         {@link AllowOrDeny#DENY DENY}.
         */
        @Nullable
        default GeckoResult<AllowOrDeny> onInstallPrompt(final @NonNull WebExtension extension) {
            return null;
        }

        /*
        TODO: Bug 1599581
        default GeckoResult<AllowOrDeny> onUpdatePrompt(
                WebExtension currentlyInstalled,
                WebExtension updatedExtension,
                String[] newPermissions) {
            return null;
        }
        TODO: Bug 1601420
        default GeckoResult<AllowOrDeny> onOptionalPrompt(
                WebExtension extension,
                String[] optionalPermissions) {
            return null;
        } */
    }

    /**
     * @return the current {@link PromptDelegate} instance.
     * @see PromptDelegate
     */
    @UiThread
    @Nullable
    public PromptDelegate getPromptDelegate() {
        return mPromptDelegate;
    }

    /** Set the {@link PromptDelegate} for this instance. This delegate will be used
     * to be notified whenever an extension is being installed or needs new permissions.
     *
     * @param delegate the delegate instance.
     * @see PromptDelegate
     */
    @UiThread
    public void setPromptDelegate(final @Nullable PromptDelegate delegate) {
        if (delegate == null && mPromptDelegate != null) {
            EventDispatcher.getInstance().unregisterUiThreadListener(
                    mInternals,
                    "GeckoView:WebExtension:InstallPrompt",
                    "GeckoView:WebExtension:UpdatePrompt",
                    "GeckoView:WebExtension:OptionalPrompt"
            );
        } else if (delegate != null && mPromptDelegate == null) {
            EventDispatcher.getInstance().registerUiThreadListener(
                    mInternals,
                    "GeckoView:WebExtension:InstallPrompt",
                    "GeckoView:WebExtension:UpdatePrompt",
                    "GeckoView:WebExtension:OptionalPrompt"
            );
        }

        mPromptDelegate = delegate;
    }

    private static class WebExtensionResult extends GeckoResult<WebExtension>
            implements EventCallback {
        private final String mFieldName;

        public WebExtensionResult(final String fieldName) {
            mFieldName = fieldName;
        }

        @Override
        public void sendSuccess(final Object response) {
            final GeckoBundle bundle = (GeckoBundle) response;
            complete(new WebExtension(bundle.getBundle(mFieldName)));
        }

        @Override
        public void sendError(final Object response) {
            if (response instanceof GeckoBundle
                    && ((GeckoBundle) response).containsKey("installError")) {
                final GeckoBundle bundle = (GeckoBundle) response;
                final int errorCode = bundle.getInt("installError");
                completeExceptionally(new WebExtension.InstallException(errorCode));
            } else {
                completeExceptionally(new Exception(response.toString()));
            }
        }
    }

    /**
     * Install an extension.
     *
     * An installed extension will persist and will be available even when restarting the
     * {@link GeckoRuntime}.
     *
     * Installed extensions through this method need to be signed by Mozilla, see
     * <a href="https://extensionworkshop.com/documentation/publish/signing-and-distribution-overview/#distributing-your-addon">
     *     Distributing your add-on
     * </a>.
     *
     * When calling this method, the GeckoView library will download the extension, validate
     * its manifest and signature, and give you an opportunity to verify its permissions through
     * {@link PromptDelegate#installPrompt}, you can use this method to prompt the user if
     * appropriate.
     *
     * @param uri URI to the extension's <code>.xpi</code> package. This can be a remote
     *            <code>https:</code> URI or a local <code>file:</code> or <code>resource:</code>
     *            URI. Note: the app needs the appropriate permissions for local URIs.
     *
     * @return A {@link GeckoResult} that will complete when the installation process finishes.
     *         For successful installations, the GeckoResult will return the {@link WebExtension}
     *         object that you can use to set delegates and retrieve information about the
     *         WebExtension using {@link WebExtension#metaData}.
     *
     *         If an error occurs during the installation process, the GeckoResult will complete
     *         exceptionally with a
     *         {@link WebExtension.InstallException InstallException} that will contain
     *         the relevant error code in
     *         {@link WebExtension.InstallException#code InstallException#code}.
     *
     * @see PromptDelegate#installPrompt
     * @see WebExtension.InstallException.ErrorCodes
     * @see WebExtension#metaData
     */
    @NonNull
    @AnyThread
    public GeckoResult<WebExtension> install(final @NonNull String uri) {
        final WebExtensionResult result = new WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("locationUri", uri);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Install",
                bundle, result);

        return result.then(extension -> {
            registerWebExtension(extension);
            return GeckoResult.fromValue(extension);
        });
    }

    // TODO: Bug 1601067 make public
    GeckoResult<WebExtension> installBuiltIn(final String uri) {
        final WebExtensionResult result = new WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("locationUri", uri);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:InstallBuiltIn",
                bundle, result);

        return result.then(extension -> {
            registerWebExtension(extension);
            return GeckoResult.fromValue(extension);
        });
    }

    /**
     * Uninstall an extension.
     *
     * Uninstalling an extension will remove it from the current {@link GeckoRuntime} instance,
     * delete all its data and trigger a request to close all extension pages currently open.
     *
     * @param extension The {@link WebExtension} to be uninstalled.
     *
     * @return A {@link GeckoResult} that will complete when the uninstall process is completed.
     */
    @NonNull
    @AnyThread
    public GeckoResult<Void> uninstall(final @NonNull WebExtension extension) {
        final CallbackResult<Void> result = new CallbackResult<Void>() {
            @Override
            public void sendSuccess(final Object response) {
                complete(null);
            }
        };

        unregisterWebExtension(extension);

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("webExtensionId", extension.id);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Uninstall",
                bundle, result);

        return result;
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ EnableSource.USER })
    @interface EnableSources {}

    /** Contains the possible values for the <code>source</code> parameter in {@link #enable} and
     * {@link #disable}. */
    public static class EnableSource {
        /** Action has been requested by the user. */
        public final static int USER = 1;

        // TODO: Bug 1604222
        /** Action requested by the app itself, e.g. to disable an extension that is
         * not supported in this version of the app. */
        final static int APP = 2;

        static String toString(final @EnableSources int flag) {
            if (flag == USER) {
                return  "user";
            } else {
                throw new IllegalArgumentException("Value provided in flags is not valid.");
            }
        }
    }

    /**
     * Enable an extension that has been disabled. If the extension is already enabled, this
     * method has no effect.
     *
     * @param extension The {@link WebExtension} to be enabled.
     * @param source The agent that initiated this action, e.g. if the action has been initiated
     *               by the user,use {@link EnableSource#USER}.
     * @return the new {@link WebExtension} instance, updated to reflect the enablement.
     */
    @AnyThread
    @NonNull
    public GeckoResult<WebExtension> enable(final @NonNull WebExtension extension,
                                            final @EnableSources int source) {
        final WebExtensionResult result = new WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(2);
        bundle.putString("webExtensionId", extension.id);
        bundle.putString("source", EnableSource.toString(source));

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Enable",
                bundle, result);

        return result.then(newExtension -> {
            registerWebExtension(newExtension);
            return GeckoResult.fromValue(newExtension);
        });
    }

    /**
     * Disable an extension that is enabled. If the extension is already disabled, this
     * method has no effect.
     *
     * @param extension The {@link WebExtension} to be disabled.
     * @param source The agent that initiated this action, e.g. if the action has been initiated
     *               by the user, use {@link EnableSource#USER}.
     * @return the new {@link WebExtension} instance, updated to reflect the disablement.
     */
    @AnyThread
    @NonNull
    public GeckoResult<WebExtension> disable(final @NonNull WebExtension extension,
                                             final @EnableSources int source) {
        final WebExtensionResult result = new WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(2);
        bundle.putString("webExtensionId", extension.id);
        bundle.putString("source", EnableSource.toString(source));

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Disable",
                bundle, result);

        return result.then(newExtension -> {
            registerWebExtension(newExtension);
            return GeckoResult.fromValue(newExtension);
        });
    }

    /**
     * List installed extensions for this {@link GeckoRuntime}.
     *
     * The returned list can be used to set delegates on the {@link WebExtension} objects using
     * {@link WebExtension#setActionDelegate}, {@link WebExtension#setMessageDelegate}.
     *
     * @return a {@link GeckoResult} that will resolve when the list of extensions is available.
     */
    @AnyThread
    @NonNull
    public GeckoResult<List<WebExtension>> list() {
        final CallbackResult<List<WebExtension>> result = new CallbackResult<List<WebExtension>>() {
            @Override
            public void sendSuccess(final Object response) {
                final GeckoBundle[] bundles = ((GeckoBundle) response)
                        .getBundleArray("extensions");
                final List<WebExtension> list = new ArrayList<>(bundles.length);

                for (GeckoBundle bundle : bundles) {
                    final WebExtension extension = new WebExtension(bundle);
                    registerWebExtension(extension);
                    list.add(extension);
                }

                complete(list);
            }
        };

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:List",
                null, result);

        return result;
    }

    // TODO: Bug 1599581 make public
    GeckoResult<WebExtension> update(final WebExtension extension) {
        final WebExtensionResult result = new WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("webExtensionId", extension.id);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Update",
                bundle, result);

        return result.then(newExtension -> {
            registerWebExtension(newExtension);
            return GeckoResult.fromValue(newExtension);
        });
    }

    /* package */ WebExtensionController(final GeckoRuntime runtime) {
        mListener = new WebExtension.Listener(runtime);
        mPendingPortMessages = new MultiMap<>();
        mPendingMessages = new MultiMap<>();
    }

    /* package */ void registerWebExtension(final WebExtension webExtension) {
        webExtension.setDelegateController(new DelegateController(webExtension));
        mExtensions.update(webExtension.id, webExtension);
    }

    /* package */ void handleMessage(final String event, final GeckoBundle bundle,
                                     final EventCallback callback, final GeckoSession session) {
        final Message message = new Message(event, bundle, callback, session);

        if ("GeckoView:WebExtension:Disconnect".equals(event)) {
            disconnect(bundle.getLong("portId", -1), callback);
            return;
        } else if ("GeckoView:WebExtension:PortMessage".equals(event)) {
            portMessage(message);
            return;
        } else if ("GeckoView:BrowserAction:Update".equals(event)) {
            actionUpdate(bundle, session, WebExtension.Action.TYPE_BROWSER_ACTION);
            return;
        } else if ("GeckoView:PageAction:Update".equals(event)) {
            actionUpdate(bundle, session, WebExtension.Action.TYPE_PAGE_ACTION);
            return;
        } else if ("GeckoView:BrowserAction:OpenPopup".equals(event)) {
            openPopup(bundle, session, WebExtension.Action.TYPE_BROWSER_ACTION);
            return;
        } else if ("GeckoView:PageAction:OpenPopup".equals(event)) {
            openPopup(bundle, session, WebExtension.Action.TYPE_PAGE_ACTION);
            return;
        } else if ("GeckoView:WebExtension:InstallPrompt".equals(event)) {
            installPrompt(bundle, callback);
            return;
        }

        final String nativeApp = bundle.getString("nativeApp");
        if (nativeApp == null) {
            if (BuildConfig.DEBUG) {
                throw new RuntimeException("Missing required nativeApp message parameter.");
            }
            callback.sendError("Missing nativeApp parameter.");
            return;
        }

        final GeckoBundle senderBundle = bundle.getBundle("sender");
        final String extensionId = senderBundle.getString("extensionId");

        mExtensions.get(extensionId).accept(extension -> {
            final WebExtension.MessageSender sender = fromBundle(extension, senderBundle, session);
            if (sender == null) {
                if (callback != null) {
                    if (BuildConfig.DEBUG) {
                        try {
                            Log.e(LOGTAG, "Could not find recipient for message: " + bundle.toJSONObject());
                        } catch (JSONException ex) {
                        }
                    }
                    callback.sendError("Could not find recipient for " + bundle.getBundle("sender"));
                }
                return;
            }

            if ("GeckoView:WebExtension:Connect".equals(event)) {
                connect(nativeApp, bundle.getLong("portId", -1), message, sender);
            } else if ("GeckoView:WebExtension:Message".equals(event)) {
                message(nativeApp, message, sender);
            }
        });
    }

    private void installPrompt(final GeckoBundle message, final EventCallback callback) {
        final GeckoBundle extensionBundle = message.getBundle("extension");
        if (extensionBundle == null || !extensionBundle.containsKey("webExtensionId")
                || !extensionBundle.containsKey("locationURI")) {
            if (BuildConfig.DEBUG) {
                throw new RuntimeException("Missing webExtensionId or locationURI");
            }

            Log.e(LOGTAG, "Missing webExtensionId or locationURI");
            return;
        }

        final WebExtension extension = new WebExtension(extensionBundle);
        extension.setDelegateController(new DelegateController(extension));

        if (mPromptDelegate == null) {
            Log.e(LOGTAG, "Tried to install extension " + extension.id +
                    " but no delegate is registered");
            return;
        }

        final GeckoResult<AllowOrDeny> promptResponse = mPromptDelegate.onInstallPrompt(extension);
        if (promptResponse == null) {
            return;
        }

        promptResponse.accept(allowOrDeny -> {
            GeckoBundle response = new GeckoBundle(1);
            if (AllowOrDeny.ALLOW.equals(allowOrDeny)) {
                response.putBoolean("allow", true);
            } else {
                response.putBoolean("allow", false);
            }
            callback.sendSuccess(response);
        });
    }

    /* package */ void newTab(final GeckoBundle message, final EventCallback callback,
                              final TabDelegate delegate) {
        mExtensions.get(message.getString("extensionId")).then(extension ->
            delegate.onNewTab(extension, message.getString("uri"))
        ).accept(session -> {
            if (session == null) {
                callback.sendSuccess(null);
                return;
            }

            if (session.isOpen()) {
                throw new IllegalArgumentException("Must use an unopened GeckoSession instance");
            }

            session.open(mListener.runtime);

            callback.sendSuccess(session.getId());
        });
    }

    /* package */ void closeTab(final GeckoBundle message,
                                final EventCallback callback,
                                final TabDelegate delegate,
                                final GeckoSession session) {
        mExtensions.get(message.getString("extensionId")).then(
            extension -> delegate.onCloseTab(extension, session),
            // On uninstall, we close all extension pages, in that case
            // the extension object may be gone already so we can't
            // send it to the delegate
            exception -> delegate.onCloseTab(null, session)
        ).accept(value -> {
            if (value == AllowOrDeny.ALLOW) {
                callback.sendSuccess(null);
            } else {
                callback.sendError(null);
            }
        });
    }


    /* package */ void unregisterWebExtension(final WebExtension webExtension) {
        mExtensions.remove(webExtension.id);
        webExtension.setDelegateController(null);

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

    private WebExtension.MessageSender fromBundle(final WebExtension extension,
                                                  final GeckoBundle sender,
                                                  final GeckoSession session) {
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

        if (port.delegate != null) {
            port.delegate.onDisconnect(port);
        }
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
            delegate = mListener.getMessageDelegate(sender.webExtension, nativeApp);
        }

        if (delegate == null) {
            callback.sendError("Native app not found or this WebExtension does not have permissions.");
            return null;
        }

        return delegate;
    }

    private void connect(final String nativeApp, final long portId, final Message message,
                         final WebExtension.MessageSender sender) {
        if (portId == -1) {
            message.callback.sendError("Missing portId.");
            return;
        }

        final WebExtension.Port port = new WebExtension.Port(nativeApp, portId, sender, mInternals);
        mPorts.put(port.id, port);

        final WebExtension.MessageDelegate delegate = getDelegate(nativeApp, sender,
                message.callback);
        if (delegate == null) {
            mPendingMessages.add(sender.webExtension.id, message);
            return;
        }

        delegate.onConnect(port);
        message.callback.sendSuccess(true);
    }

    private void portMessage(final Message message) {
        final GeckoBundle bundle = message.bundle;

        final long portId = bundle.getLong("portId", -1);
        final WebExtension.Port port = mPorts.get(portId);
        if (port == null) {
            if (BuildConfig.DEBUG) {
                try {
                    Log.e(LOGTAG, "Could not find recipient for message: " + bundle.toJSONObject());
                } catch (JSONException ex) {
                }
            }

            mPendingPortMessages.add(portId, message);
            return;
        }

        final Object content;
        try {
            content = bundle.toJSONObject().get("data");
        } catch (JSONException ex) {
            message.callback.sendError(ex);
            return;
        }

        if (port.delegate == null) {
            mPendingPortMessages.add(portId, message);
            return;
        }

        port.delegate.onPortMessage(content, port);
        message.callback.sendSuccess(null);
    }

    private void message(final String nativeApp, final Message message,
                         final WebExtension.MessageSender sender) {
        final EventCallback callback = message.callback;

        final Object content;
        try {
            content = message.bundle.toJSONObject().get("data");
        } catch (JSONException ex) {
            callback.sendError(ex);
            return;
        }

        final WebExtension.MessageDelegate delegate = getDelegate(nativeApp, sender,
                callback);
        if (delegate == null) {
            mPendingMessages.add(sender.webExtension.id, message);
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

    private GeckoResult<WebExtension> extensionFromBundle(final GeckoBundle message) {
        final String extensionId = message.getString("extensionId");
        return mExtensions.get(extensionId);
    }

    private void openPopup(final GeckoBundle message, final GeckoSession session,
                           final @WebExtension.Action.ActionType int actionType) {
        extensionFromBundle(message).accept(extension -> {
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
        });
    }

    private WebExtension.ActionDelegate actionDelegateFor(final WebExtension extension,
                                                          final GeckoSession session) {
        if (session == null) {
            return mListener.getActionDelegate(extension);
        }

        return session.getWebExtensionActionDelegate(extension);
    }

    private void actionUpdate(final GeckoBundle message, final GeckoSession session,
                              final @WebExtension.Action.ActionType int actionType) {
        extensionFromBundle(message).accept(extension -> {
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
        });
    }
}
