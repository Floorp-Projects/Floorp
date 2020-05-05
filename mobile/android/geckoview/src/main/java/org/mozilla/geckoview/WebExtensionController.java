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
import java.util.UUID;

import static org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_POSTPONED;
import static org.mozilla.geckoview.WebExtension.InstallException.ErrorCodes.ERROR_USER_CANCELED;

public class WebExtensionController {
    private final static String LOGTAG = "WebExtension";

    private DebuggerDelegate mDebuggerDelegate;
    private PromptDelegate mPromptDelegate;
    private final WebExtension.Listener<WebExtension.TabDelegate> mListener;

    // Map [ portId -> Message ]
    private final MultiMap<Long, Message> mPendingPortMessages;
    // Map [ extensionId -> Message ]
    private final MultiMap<String, Message> mPendingMessages;
    private final MultiMap<String, Message> mPendingNewTab;

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
        private Observer mObserver;

        interface Observer {
            /***
             * This event is fired every time a new extension object is created by the store.
             *
             * @param extension the newly-created extension object
             */
            void onNewExtension(final WebExtension extension);
        }

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
                    mObserver.onNewExtension(ext);
                    return GeckoResult.fromValue(ext);
                });
            }

            return GeckoResult.fromValue(extension);
        }

        public void setObserver(final Observer observer) {
            mObserver = observer;
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
            WebExtension.Port.Observer,
            ExtensionStore.Observer {
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

        @Override
        public void onNewExtension(final WebExtension extension) {
            extension.setDelegateController(new DelegateController(extension));
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

        @Override
        public void onTabDelegate(final WebExtension.TabDelegate delegate) {
            mListener.setTabDelegate(mExtension, delegate);

            for (final Message message : mPendingNewTab.get(mExtension.id)) {
                WebExtensionController.this.handleMessage(message.event, message.bundle,
                        message.callback, message.session);
            }

            mPendingNewTab.remove(mExtension.id);
        }

        @Override
        public WebExtension.TabDelegate getTabDelegate() {
            return mListener.getTabDelegate(mExtension);
        }
    }

    // TODO: remove once we remove GeckoRuntime#registerWebExtension
    /* package */ DelegateController delegateFor(final WebExtension extension) {
        return new DelegateController(extension);
    }

    /**
     * @deprecated Use {@link WebExtension.TabDelegate} and {@link WebExtension.SessionTabDelegate}.
     */
    @Deprecated
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

    /**
     * @deprecated Use {@link WebExtension#getTabDelegate} and
     * {@link WebExtension.SessionController#getTabDelegate}.
     * @return The {@link TabDelegate} instance.
     */
    @UiThread
    @Nullable
    @Deprecated
    public TabDelegate getTabDelegate() {
        return mListener.getTabDelegate();
    }

    /**
     * @deprecated Use {@link WebExtension#setTabDelegate} and
     * {@link WebExtension.SessionController#setTabDelegate}.
     * @param delegate {@link TabDelegate} instance.
     */
    @Deprecated
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

        /**
         * Called whenever an updated extension has new permissions. This is intended as an
         * opportunity for the app to prompt the user for the new permissions required by
         * this extension.
         *
         * @param currentlyInstalled The {@link WebExtension} that is currently installed.
         * @param updatedExtension The {@link WebExtension} that will replace the previous extension.
         * @param newPermissions The new permissions that are needed.
         * @param newOrigins The new origins that are needed.
         *
         * @return A {@link GeckoResult} that completes to either {@link AllowOrDeny#ALLOW ALLOW}
         *         if this extension should be update or {@link AllowOrDeny#DENY DENY} if
         *         this extension should not be update. A null value will be interpreted as
         *         {@link AllowOrDeny#DENY DENY}.
         */
        @Nullable
        default GeckoResult<AllowOrDeny> onUpdatePrompt(
                @NonNull WebExtension currentlyInstalled,
                @NonNull WebExtension updatedExtension,
                @NonNull String[] newPermissions,
                @NonNull String[] newOrigins) {
            return null;
        }

        /*
        TODO: Bug 1601420
        default GeckoResult<AllowOrDeny> onOptionalPrompt(
                WebExtension extension,
                String[] optionalPermissions) {
            return null;
        } */
    }

    public interface DebuggerDelegate {
        /**
         * Called whenever the list of installed extensions has been modified using the debugger
         * with tools like web-ext.
         *
         * This is intended as an opportunity to refresh the list of installed extensions using
         * {@link WebExtensionController#list} and to set delegates on the new {@link WebExtension}
         * objects, e.g. using {@link WebExtension#setActionDelegate} and
         * {@link WebExtension#setMessageDelegate}.
         *
         * @see <a href="https://extensionworkshop.com/documentation/develop/getting-started-with-web-ext">
         *     Getting started with web-ext</a>
         */
        @UiThread
        default void onExtensionListUpdated() {}
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

    /**
     * Set the {@link DebuggerDelegate} for this instance. This delegate will receive updates
     * about extension changes using developer tools.
     *
     * @param delegate the Delegate instance
     */
    @UiThread
    public void setDebuggerDelegate(final @NonNull DebuggerDelegate delegate) {
        if (delegate == null && mDebuggerDelegate != null) {
            EventDispatcher.getInstance().unregisterUiThreadListener(
                    mInternals,
                    "GeckoView:WebExtension:DebuggerListUpdated"
            );
        } else if (delegate != null && mDebuggerDelegate == null) {
            EventDispatcher.getInstance().registerUiThreadListener(
                    mInternals,
                    "GeckoView:WebExtension:DebuggerListUpdated"
            );
        }

        mDebuggerDelegate = delegate;
    }

    private static class WebExtensionResult extends GeckoResult<WebExtension>
            implements EventCallback {
        /** These states should match gecko's AddonManager.STATE_* constants. */
        private static class StateCodes {
            public static final int STATE_POSTPONED = 7;
            public static final int STATE_CANCELED = 12;
        }

        private final String mFieldName;

        public WebExtensionResult(final String fieldName) {
            mFieldName = fieldName;
        }

        @Override
        public void sendSuccess(final Object response) {
            if (response == null) {
                complete(null);
                return;
            }
            final GeckoBundle bundle = (GeckoBundle) response;
            complete(new WebExtension(bundle.getBundle(mFieldName)));
        }

        @Override
        public void sendError(final Object response) {
            if (response instanceof GeckoBundle
                    && ((GeckoBundle) response).containsKey("installError")) {
                final GeckoBundle bundle = (GeckoBundle) response;
                int errorCode = bundle.getInt("installError");
                final int installState = bundle.getInt("state");
                if (errorCode == 0 && installState == StateCodes.STATE_CANCELED) {
                    errorCode = ERROR_USER_CANCELED;
                } else if (errorCode == 0 && installState == StateCodes.STATE_POSTPONED) {
                    errorCode = ERROR_POSTPONED;
                }
                completeExceptionally(new WebExtension.InstallException(errorCode));
            } else {
                completeExceptionally(new Exception(response.toString()));
            }
        }
    }

    private static class WebExtensionInstallResult extends WebExtensionResult {
        private static class InstallCanceller implements GeckoResult.CancellationDelegate {
            private static class CancelResult extends GeckoResult<Boolean>
                    implements EventCallback {
                @Override
                public void sendSuccess(final Object response) {
                    final boolean result = ((GeckoBundle) response).getBoolean("cancelled");
                    complete(result);
                }

                @Override
                public void sendError(final Object response) {
                    completeExceptionally(new Exception(response.toString()));
                }
            }

            private final String mInstallId;
            private boolean mCancelled;

            public InstallCanceller(@NonNull final String aInstallId) {
                mInstallId = aInstallId;
                mCancelled = false;
            }

            @Override
            public GeckoResult<Boolean> cancel() {
                CancelResult result = new CancelResult();

                final GeckoBundle bundle = new GeckoBundle(1);
                bundle.putString("installId", mInstallId);

                EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:CancelInstall",
                        bundle, result);

                return result.then(wasCancelled -> {
                    mCancelled = wasCancelled;
                    return GeckoResult.fromValue(wasCancelled);
                });
            }
        }

        /* package */ final @NonNull String installId;

        private final InstallCanceller mInstallCanceller;

        public WebExtensionInstallResult() {
            super("extension");

            installId = UUID.randomUUID().toString();
            mInstallCanceller = new InstallCanceller(installId);
            setCancellationDelegate(mInstallCanceller);
        }

        @Override
        public void sendError(final Object response) {
            if (!mInstallCanceller.mCancelled) {
                super.sendError(response);
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
        WebExtensionInstallResult result = new WebExtensionInstallResult();
        final GeckoBundle bundle = new GeckoBundle(2);
        bundle.putString("locationUri", uri);
        bundle.putString("installId", result.installId);
        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Install",
                        bundle, result);
        return result.then(extension -> {
            registerWebExtension(extension);
            return GeckoResult.fromValue(extension);
        });
    }

    /**
     * Set whether an extension should be allowed to run in private browsing or not.
     *
     * @param extension the {@link WebExtension} instance to modify.
     * @param allowed true if this extension should be allowed to run in private browsing pages,
     *                false otherwise.
     * @return the updated {@link WebExtension} instance.
     */
    @NonNull
    @AnyThread
    public GeckoResult<WebExtension> setAllowedInPrivateBrowsing(
            final @NonNull WebExtension extension,
            final boolean allowed) {
        final WebExtensionController.WebExtensionResult result =
                new WebExtensionController.WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(2);
        bundle.putString("extensionId", extension.id);
        bundle.putBoolean("allowed", allowed);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:SetPBAllowed",
                bundle, result);

        return result.then(newExtension -> {
            registerWebExtension(newExtension);
            return GeckoResult.fromValue(newExtension);
        });
    }

    /**
     * Install a built-in extension.
     *
     * Built-in extensions have access to native messaging, don't need to be
     * signed and are installed from a folder in the APK instead of a .xpi
     * bundle.
     *
     * Example: <p><code>
     *    controller.installBuiltIn("resource://android/assets/example/");
     * </code></p>
     *
     * Will install the built-in extension located at
     * <code>/assets/example/</code> in the app's APK.
     *
     * @param uri Folder where the extension is located. To ensure this folder
     *            is inside the APK, only <code>resource://android</code> URIs
     *            are allowed.
     *
     * @see WebExtension.MessageDelegate
     * @return A {@link GeckoResult} that completes with the extension once
     *         it's installed.
     */
    @NonNull
    @AnyThread
    public GeckoResult<WebExtension> installBuiltIn(final @NonNull String uri) {
        WebExtensionInstallResult result = new WebExtensionInstallResult();
        final GeckoBundle bundle = new GeckoBundle(2);
        bundle.putString("locationUri", uri);
        bundle.putString("installId", result.installId);
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

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("webExtensionId", extension.id);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Uninstall",
                bundle, result);

        return result.then(success -> {
            unregisterWebExtension(extension);
            return GeckoResult.fromValue(success);
        });
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ EnableSource.USER, EnableSource.APP })
    @interface EnableSources {}

    /** Contains the possible values for the <code>source</code> parameter in {@link #enable} and
     * {@link #disable}. */
    public static class EnableSource {
        /** Action has been requested by the user. */
        public final static int USER = 1;

        /** Action requested by the app itself, e.g. to disable an extension that is
         * not supported in this version of the app. */
        public final static int APP = 2;

        static String toString(final @EnableSources int flag) {
            if (flag == USER) {
                return  "user";
            } else if (flag == APP) {
                return "app";
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

    /**
     * Update a web extension.
     *
     * When checking for an update, GeckoView will download the update manifest that is defined by the
     * web extension's manifest property
     * <a href="https://extensionworkshop.com/documentation/manage/updating-your-extension/">browser_specific_settings.gecko.update_url</a>.
     * If an update is found it will be downloaded and installed. If the extension needs any new
     * permissions the {@link PromptDelegate#updatePrompt} will be triggered.
     *
     * More information about the update manifest format is available
     * <a href="https://extensionworkshop.com/documentation/manage/updating-your-extension/#manifest-structure">here</a>.
     *
     * @param extension The extension to update.
     *
     * @return A {@link GeckoResult} that will complete when the update process finishes. If an
     *         update is found and installed successfully, the GeckoResult will return the updated
     *         {@link WebExtension}. If no update is available, null will be returned. If the updated
     *         extension requires new permissions, the {@link PromptDelegate#installPrompt}
     *         will be called.
     *
     * @see PromptDelegate#updatePrompt
     */
    @AnyThread
    @NonNull
    public GeckoResult<WebExtension> update(final @NonNull WebExtension extension) {
        final WebExtensionResult result = new WebExtensionResult("extension");

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("webExtensionId", extension.id);

        EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:Update",
                bundle, result);

        return result.then(newExtension -> {
            if (newExtension != null) {
                registerWebExtension(newExtension);
            }
            return GeckoResult.fromValue(newExtension);
        });
    }

    /* package */ WebExtensionController(final GeckoRuntime runtime) {
        mListener = new WebExtension.Listener<>(runtime);
        mPendingPortMessages = new MultiMap<>();
        mPendingMessages = new MultiMap<>();
        mPendingNewTab = new MultiMap<>();
        mExtensions.setObserver(mInternals);
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
        } else if ("GeckoView:WebExtension:InstallPrompt".equals(event)) {
            installPrompt(bundle, callback);
            return;
        } else if ("GeckoView:WebExtension:UpdatePrompt".equals(event)) {
            updatePrompt(bundle, callback);
            return;
        } else if ("GeckoView:WebExtension:DebuggerListUpdated".equals(event)) {
            if (mDebuggerDelegate != null) {
                mDebuggerDelegate.onExtensionListUpdated();
            }
            return;
        }

        final GeckoBundle senderBundle;
        if ("GeckoView:WebExtension:Connect".equals(event) ||
                "GeckoView:WebExtension:Message".equals(event)) {
            senderBundle = bundle.getBundle("sender");
        } else {
            senderBundle = bundle;
        }

        extensionFromBundle(senderBundle).accept(extension -> {
            if ("GeckoView:WebExtension:NewTab".equals(event)) {
                newTab(message, extension);
                return;
            } else if ("GeckoView:WebExtension:UpdateTab".equals(event)) {
                updateTab(message, extension);
                return;
            } else if ("GeckoView:WebExtension:CloseTab".equals(event)) {
                closeTab(message, extension);
                return;
            } else if ("GeckoView:BrowserAction:Update".equals(event)) {
                actionUpdate(message, extension, WebExtension.Action.TYPE_BROWSER_ACTION);
                return;
            } else if ("GeckoView:PageAction:Update".equals(event)) {
                actionUpdate(message, extension, WebExtension.Action.TYPE_PAGE_ACTION);
                return;
            } else if ("GeckoView:BrowserAction:OpenPopup".equals(event)) {
                openPopup(message, extension, WebExtension.Action.TYPE_BROWSER_ACTION);
                return;
            } else if ("GeckoView:PageAction:OpenPopup".equals(event)) {
                openPopup(message, extension, WebExtension.Action.TYPE_PAGE_ACTION);
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
            final GeckoBundle response = new GeckoBundle(1);
            response.putBoolean("allow", AllowOrDeny.ALLOW.equals(allowOrDeny));
            callback.sendSuccess(response);
        });
    }

    private void updatePrompt(final GeckoBundle message, final EventCallback callback) {
        final GeckoBundle currentBundle = message.getBundle("currentlyInstalled");
        final GeckoBundle updatedBundle = message.getBundle("updatedExtension");
        final String[] newPermissions = message.getStringArray("newPermissions");
        final String[] newOrigins = message.getStringArray("newOrigins");
        if (currentBundle == null || updatedBundle == null) {
            if (BuildConfig.DEBUG) {
                throw new RuntimeException("Missing bundle");
            }

            Log.e(LOGTAG, "Missing bundle");
            return;
        }

        final WebExtension currentExtension = new WebExtension(currentBundle);
        currentExtension.setDelegateController(new DelegateController(currentExtension));

        final WebExtension updatedExtension = new WebExtension(updatedBundle);
        updatedExtension.setDelegateController(new DelegateController(updatedExtension));

        if (mPromptDelegate == null) {
            Log.e(LOGTAG, "Tried to update extension " + currentExtension.id +
                    " but no delegate is registered");
            return;
        }

        final GeckoResult<AllowOrDeny> promptResponse = mPromptDelegate.onUpdatePrompt(
                currentExtension, updatedExtension, newPermissions, newOrigins);
        if (promptResponse == null) {
            return;
        }

        promptResponse.accept(allowOrDeny -> {
            final GeckoBundle response = new GeckoBundle(1);
            response.putBoolean("allow", AllowOrDeny.ALLOW.equals(allowOrDeny));
            callback.sendSuccess(response);
        });
    }

    /* package */ void newTab(final Message message, final WebExtension extension) {
        newTab(message, null, extension);
    }

    // TODO: remove Bug 1618987
    /* package */ void newTab(final GeckoBundle bundle, final EventCallback callback,
                              final TabDelegate legacyDelegate) {
        final Message message = new Message("GeckoView:WebExtension:NewTab",
                bundle, callback, null);
        extensionFromBundle(bundle).accept(extension ->
            newTab(message, legacyDelegate, extension)
        );
    }

    // TODO: remove legacyDelegate Bug 1618987
    /* package */ void newTab(final Message message, final TabDelegate legacyDelegate,
                              final WebExtension extension) {
        final GeckoBundle bundle = message.bundle;

        final WebExtension.TabDelegate delegate = mListener.getTabDelegate(extension);
        final WebExtension.CreateTabDetails details =
                new WebExtension.CreateTabDetails(bundle.getBundle("createProperties"));

        final GeckoResult<GeckoSession> result;
        if (delegate != null) {
            result = delegate.onNewTab(extension, details);
        } else if (legacyDelegate != null) {
            result = legacyDelegate.onNewTab(extension, details.url);
        } else {
            mPendingNewTab.add(extension.id, message);
            return;
        }

        if (result == null) {
            message.callback.sendSuccess(null);
            return;
        }

        result.accept(session -> {
            if (session == null) {
                message.callback.sendSuccess(null);
                return;
            }

            if (session.isOpen()) {
                throw new IllegalArgumentException("Must use an unopened GeckoSession instance");
            }

            session.open(mListener.runtime);

            message.callback.sendSuccess(session.getId());
        });
    }

    /* package */ void updateTab(final Message message, final WebExtension extension) {
        final WebExtension.SessionTabDelegate delegate = message.session.getWebExtensionController()
                .getTabDelegate(extension);
        final EventCallback callback = message.callback;

        if (delegate == null) {
            callback.sendError(null);
            return;
        }

        delegate.onUpdateTab(extension, message.session,
            new WebExtension.UpdateTabDetails(message.bundle.getBundle("updateProperties")))
            .accept(value -> {
                if (value == AllowOrDeny.ALLOW) {
                    callback.sendSuccess(null);
                } else {
                    callback.sendError(null);
                }
            });
    }

    // TODO: remove Bug 1618987
    /* package */ void closeTab(final GeckoBundle bundle, final EventCallback callback,
                                final GeckoSession session,
                              final TabDelegate legacyDelegate) {
        final Message message = new Message("GeckoView:WebExtension:NewTab",
                bundle, callback, null);
        extensionFromBundle(bundle).accept(extension ->
                newTab(message, legacyDelegate, extension)
        );
    }

    /* package */ void closeTab(final Message message,
                                final WebExtension extension) {
        closeTab(message, extension, null);
    }

    // TODO: remove legacyDelegate Bug 1618987
    /* package */ void closeTab(final Message message,
                                final WebExtension extension,
                                final TabDelegate legacyDelegate) {
        final WebExtension.SessionTabDelegate delegate =
                message.session.getWebExtensionController().getTabDelegate(extension);

        final GeckoResult<AllowOrDeny> result;
        if (delegate != null) {
            result = delegate.onCloseTab(extension, message.session);
        } else if (legacyDelegate != null) {
            result = legacyDelegate.onCloseTab(extension, message.session);
        } else {
            result = GeckoResult.fromValue(AllowOrDeny.DENY);
        }

        result.accept(value -> {
            if (value == AllowOrDeny.ALLOW) {
                message.callback.sendSuccess(null);
            } else {
                message.callback.sendError(null);
            }
        });
    }

    /**
     * Notifies extensions about a active tab change over the `tabs.onActivated` event.
     *
     * @param session The {@link GeckoSession} of the newly selected session/tab.
     * @param active true if the tab became active, false if the tab became inactive.
     */
    @AnyThread
    public void setTabActive(@NonNull final GeckoSession session, final boolean active) {
        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putBoolean("active", active);
        session.getEventDispatcher().dispatch(
            "GeckoView:WebExtension:SetTabActive",
            bundle);
    }

    /* package */ void unregisterWebExtension(final WebExtension webExtension) {
        mExtensions.remove(webExtension.id);
        webExtension.setDelegateController(null);
        mListener.unregisterWebExtension(webExtension);

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
            delegate = sender.session.getWebExtensionController()
                    .getMessageDelegate(sender.webExtension, nativeApp);
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

    private void openPopup(final Message message, final WebExtension extension,
                           final @WebExtension.Action.ActionType int actionType) {
        if (extension == null) {
            return;
        }

        final WebExtension.Action action = new WebExtension.Action(
                actionType, message.bundle.getBundle("action"), extension);

        final WebExtension.ActionDelegate delegate = actionDelegateFor(extension, message.session);
        if (delegate == null) {
            return;
        }

        final GeckoResult<GeckoSession> popup = delegate.onOpenPopup(extension, action);
        action.openPopup(popup);
    }

    private WebExtension.ActionDelegate actionDelegateFor(final WebExtension extension,
                                                          final GeckoSession session) {
        if (session == null) {
            return mListener.getActionDelegate(extension);
        }

        return session.getWebExtensionController().getActionDelegate(extension);
    }

    private void actionUpdate(final Message message, final WebExtension extension,
                              final @WebExtension.Action.ActionType int actionType) {
        if (extension == null) {
            return;
        }

        final WebExtension.ActionDelegate delegate = actionDelegateFor(extension, message.session);
        if (delegate == null) {
            return;
        }

        final WebExtension.Action action = new WebExtension.Action(
                actionType, message.bundle.getBundle("action"), extension);
        if (actionType == WebExtension.Action.TYPE_BROWSER_ACTION) {
            delegate.onBrowserAction(extension, message.session, action);
        } else if (actionType == WebExtension.Action.TYPE_PAGE_ACTION) {
            delegate.onPageAction(extension, message.session, action);
        }
    }
}
