package org.mozilla.geckoview;

import android.graphics.Bitmap;
import android.graphics.Color;
import android.support.annotation.AnyThread;
import android.support.annotation.IntDef;
import android.support.annotation.LongDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.UUID;

/**
 * Represents a WebExtension that may be used by GeckoView.
 */
public class WebExtension {
    /**
     * <code>file:</code> or <code>resource:</code> URI that points to the
     * install location of this WebExtension. When the WebExtension is included
     * with the APK the file can be specified using the
     * <code>resource://android</code> alias. E.g.
     *
     * <pre><code>
     *      resource://android/assets/web_extensions/my_webextension/
     * </code></pre>
     *
     * Will point to folder
     * <code>/assets/web_extensions/my_webextension/</code> in the APK.
     */
    public final @NonNull String location;
    /**
     * Unique identifier for this WebExtension
     */
    public final @NonNull String id;
    /**
     * {@link Flags} for this WebExtension.
     */
    public final @WebExtensionFlags long flags;

    /** Provides information about this {@link WebExtension}. */
    // TODO: move to @NonNull when we remove registerWebExtension
    public final @Nullable MetaData metaData;

    // TODO: make public
    final boolean isBuiltIn;

    /** Called whenever a delegate is set or unset on this {@link WebExtension} instance.
    /* package */ interface DelegateController {
        void onMessageDelegate(final String nativeApp, final MessageDelegate delegate);
        void onActionDelegate(final ActionDelegate delegate);
        void onTabDelegate(final TabDelegate delegate);
        ActionDelegate getActionDelegate();
        TabDelegate getTabDelegate();
    }

    private DelegateController mDelegateController = null;

    /* package */ void setDelegateController(final DelegateController delegate) {
        mDelegateController = delegate;
    }

    @Override
    public String toString() {
        return "WebExtension {" +
                "location=" + location + ", " +
                "id=" + id + ", " +
                "flags=" + flags + "}";
    }

    private final static String LOGTAG = "WebExtension";

    public static class Flags {
        /*
         * Default flags for this WebExtension.
         */
        public static final long NONE = 0;
        /**
         * Set this flag if you want to enable content scripts messaging.
         * To listen to such messages you can use
         * {@link SessionController#setMessageDelegate}.
         */
        public static final long ALLOW_CONTENT_MESSAGING = 1 << 0;

        // Do not instantiate this class.
        protected Flags() {}
    }

    @Retention(RetentionPolicy.SOURCE)
    @LongDef(flag = true,
            value = { Flags.NONE, Flags.ALLOW_CONTENT_MESSAGING })
    /* package */ @interface WebExtensionFlags {}

    /* package */ WebExtension(final GeckoBundle bundle) {
        location = bundle.getString("locationURI");
        id = bundle.getString("webExtensionId");
        flags = bundle.getInt("webExtensionFlags", 0);
        isBuiltIn = bundle.getBoolean("isBuiltIn", false);
        if (bundle.containsKey("metaData")) {
            metaData = new MetaData(bundle.getBundle("metaData"));
        } else {
            metaData = null;
        }
    }

    /**
     * Builds a WebExtension instance that can be loaded in GeckoView using
     * {@link GeckoRuntime#registerWebExtension}
     *
     * @param location The WebExtension install location. It must be either a
     *                 <code>resource:</code> URI to a folder inside the APK or
     *                 a <code>file:</code> URL to a <code>.xpi</code> file.
     * @param id Unique identifier for this WebExtension. This identifier must
     *           either be a GUID or a string formatted like an email address.
     *           E.g. <pre><code>
     *              "extensionname@example.org"
     *              "{daf44bf7-a45e-4450-979c-91cf07434c3d}"
     *           </code></pre>
     *
     *           See also: <ul>
     *           <li><a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/browser_specific_settings">
     *                  WebExtensions/manifest.json/browser_specific_settings
     *               </a>
     *           <li><a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/WebExtensions_and_the_Add-on_ID#When_do_you_need_an_add-on_ID">
     *                  WebExtensions/WebExtensions_and_the_Add-on_ID
     *               </a>
     *           </ul>
     * @param flags {@link Flags} for this WebExtension.
     * @param controller the current {@link WebExtensionController} instance
     */
    public WebExtension(final @NonNull String location, final @NonNull String id,
                        final @WebExtensionFlags long flags,
                        final @NonNull WebExtensionController controller) {
        setDelegateController(controller.delegateFor(this));
        this.location = location;
        this.id = id;
        this.flags = flags;

        // TODO:
        this.isBuiltIn = false;
        this.metaData = null;
    }

    /**
     * Builds a WebExtension instance that can be loaded in GeckoView using
     * {@link GeckoRuntime#registerWebExtension}
     * The <code>id</code> for this web extension will be automatically
     * generated.
     *
     * All messaging from the web extension will be ignored.
     *
     * @param location The WebExtension install location. It must be either a
     *                 <code>resource:</code> URI to a folder inside the APK or
     *                 a <code>file:</code> URL to a <code>.xpi</code> file.
     * @param controller the current {@link WebExtensionController} instance
     */
    public WebExtension(final @NonNull String location,
                        final @NonNull WebExtensionController controller) {
        this(location, "{" + UUID.randomUUID().toString() + "}", Flags.NONE, controller);
    }

    /**
     * Defines the message delegate for a Native App.
     *
     * This message delegate will receive messages from the background script
     * for the native app specified in <code>nativeApp</code>.
     *
     * For messages from content scripts, set a session-specific message
     * delegate using {@link SessionController#setMessageDelegate}.
     *
     * See also
     *  <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging">
     *     WebExtensions/Native_messaging
     *  </a>
     *
     * @param messageDelegate handles messaging between the WebExtension and
     *      the app. To send a message from the WebExtension use the
     *      <code>runtime.sendNativeMessage</code> WebExtension API:
     *      E.g. <pre><code>
     *        browser.runtime.sendNativeMessage(nativeApp,
     *              {message: "Hello from WebExtension!"});
     *      </code></pre>
     *
     *      For bidirectional communication, use <code>runtime.connectNative</code>.
     *      E.g. in a content script: <pre><code>
     *          let port = browser.runtime.connectNative(nativeApp);
     *          port.onMessage.addListener(message =&gt; {
     *              console.log("Message received from app");
     *          });
     *          port.postMessage("Ping from WebExtension");
     *      </code></pre>
     *
     *      The code above will trigger a {@link MessageDelegate#onConnect}
     *      call that will contain the corresponding {@link Port} object that
     *      can be used to send messages to the WebExtension. Note: the
     *      <code>nativeApp</code> specified in the WebExtension needs to match
     *      the <code>nativeApp</code> parameter of this method.
     *
     *      You can unset the message delegate by setting a <code>null</code>
     *      messageDelegate.
     *
     * @param nativeApp which native app id this message delegate will handle
     *                  messaging for. Needs to match the
     *                  <code>application</code> parameter of
     *                  <code>runtime.sendNativeMessage</code> and
     *                  <code>runtime.connectNative</code>.
     *
     * @see SessionController#setMessageDelegate
     */
    @UiThread
    public void setMessageDelegate(final @Nullable MessageDelegate messageDelegate,
                                   final @NonNull String nativeApp) {
        if (mDelegateController != null) {
            mDelegateController.onMessageDelegate(nativeApp, messageDelegate);
        }
    }

    /**
     * Delegates that responds to messages sent from a WebExtension.
     */
    @UiThread
    public interface MessageDelegate {
        /**
         * Called whenever the WebExtension sends a message to an app using
         * <code>runtime.sendNativeMessage</code>.
         *
         * @param nativeApp The application identifier of the MessageDelegate that
         *                  sent this message.
         * @param message The message that was sent, either a primitive type or
         *                a {@link org.json.JSONObject}.
         * @param sender The {@link MessageSender} corresponding to the frame
         *               that originated the message.
         *
         *               Note: all messages are to be considered untrusted and
         *               should be checked carefully for validity.
         * @return A {@link GeckoResult} that resolves with a response to the
         *         message.
         */
        @Nullable
        default GeckoResult<Object> onMessage(final @NonNull String nativeApp,
                                              final @NonNull Object message,
                                              final @NonNull MessageSender sender) {
            return null;
        }

        /**
         * Called whenever the WebExtension connects to an app using
         * <code>runtime.connectNative</code>.
         *
         * @param port {@link Port} instance that can be used to send and
         *             receive messages from the WebExtension. Use {@link
         *             Port#sender} to verify the origin of this connection
         *             request.
         */
        @Nullable
        default void onConnect(final @NonNull Port port) {}
    }

    /**
     * Delegate that handles communication from a WebExtension on a specific
     * {@link Port} instance.
     */
    @UiThread
    public interface PortDelegate {
        /**
         * Called whenever a message is sent through the corresponding {@link
         * Port} instance.
         *
         * @param message The message that was sent, either a primitive type or
         *                a {@link org.json.JSONObject}.
         * @param port The {@link Port} instance that received this message.
         */
        default void onPortMessage(final @NonNull Object message, final @NonNull Port port) {}

        /**
         * Called whenever the corresponding {@link Port} instance is
         * disconnected or the corresponding {@link GeckoSession} is destroyed.
         * Any message sent from the port after this call will be ignored.
         *
         * @param port The {@link Port} instance that was disconnected.
         */
        @NonNull
        default void onDisconnect(final @NonNull Port port) {}
    }

    /**
     * Port object that can be used for bidirectional communication with a
     * WebExtension.
     *
     * See also: <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port">
     *  WebExtensions/API/runtime/Port
     * </a>.
     *
     * @see MessageDelegate#onConnect
     */
    @UiThread
    public static class Port {
        /* package */ final long id;
        /* package */ PortDelegate delegate;
        /* package */ boolean disconnected = false;
        /* package */ final WeakReference<Observer> observer;

        /** {@link MessageSender} corresponding to this port. */
        public @NonNull final MessageSender sender;

        /** The application identifier of the MessageDelegate that opened this port. */
        public @NonNull final String name;

        /* package */ interface Observer {
            void onDisconnectFromApp(Port port);
            void onDelegateAttached(Port port);
        }

        /** Override for tests. */
        protected Port() {
            this.id = -1;
            this.delegate = null;
            this.observer = null;
            this.sender = null;
            this.name = null;
        }

        /* package */ Port(final String name, final long id, final MessageSender sender,
                           final Observer observer) {
            this.id = id;
            this.delegate = null;
            this.observer = new WeakReference<>(observer);
            this.sender = sender;
            this.name = name;
        }

        /**
         * Post a message to the WebExtension connected to this {@link Port} instance.
         *
         * @param message {@link JSONObject} that will be sent to the WebExtension.
         */
        public void postMessage(final @NonNull JSONObject message) {
            GeckoBundle args = new GeckoBundle(2);
            args.putLong("portId", id);
            try {
                args.putBundle("message", GeckoBundle.fromJSONObject(message));
            } catch (JSONException ex) {
                throw new RuntimeException(ex);
            }

            EventDispatcher.getInstance()
                    .dispatch("GeckoView:WebExtension:PortMessageFromApp", args);
        }

        /**
         * Disconnects this port and notifies the other end.
         */
        public void disconnect() {
            if (this.disconnected) {
                return;
            }

            final Observer observer = this.observer.get();
            if (observer != null) {
                observer.onDisconnectFromApp(this);
            }

            GeckoBundle args = new GeckoBundle(1);
            args.putLong("portId", id);

            EventDispatcher.getInstance().dispatch("GeckoView:WebExtension:PortDisconnect", args);
            this.disconnected = true;
        }

        /**
         * Set a delegate for incoming messages through this {@link Port}.
         *
         * @param delegate Delegate that will receive messages sent through
         * this {@link Port}.
         */
        public void setDelegate(final @Nullable PortDelegate delegate) {
            this.delegate = delegate;
            final Observer observer = this.observer.get();
            if (observer != null) {
                observer.onDelegateAttached(this);
            }
        }
    }

    /**
     * This delegate is invoked whenever an extension uses the `tabs` WebExtension API to modify
     * the state of a tab. See also
     * <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs">WebExtensions/API/tabs</a>.
     */
    public interface SessionTabDelegate {
        /**
         * Called when tabs.remove is invoked, this method decides if WebExtension can close the
         * tab. In case WebExtension can close the tab, it should close passed GeckoSession and
         * return GeckoResult.ALLOW or GeckoResult.DENY in case tab cannot be closed.
         *
         * See also: <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs/remove">
         *     WebExtensions/API/tabs/remove</a>
         *
         * @param source An instance of {@link WebExtension} or null if extension was not registered
         *               with GeckoRuntime.registerWebextension
         * @param session An instance of {@link GeckoSession} to be closed.
         * @return GeckoResult.ALLOW if the tab will be closed, GeckoResult.DENY otherwise
         */
        @UiThread
        @NonNull
        default GeckoResult<AllowOrDeny> onCloseTab(@Nullable WebExtension source,
                                                    @NonNull GeckoSession session)  {
            return GeckoResult.DENY;
        }

        /**
         * Called when tabs.update is invoked. The uri is provided for informational purposes,
         * there's no need to call <code>loadURI</code> on it. The page will be loaded if
         * this method returns GeckoResult.ALLOW.
         *
         * See also: <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs/update">
         *     WebExtensions/API/tabs/update</a>
         *
         * @param extension The extension that requested to update the tab.
         * @param session The {@link GeckoSession} instance that needs to be updated.
         * @param details {@link UpdateTabDetails} instance that describes what
         *                needs to be updated for this tab.
         * @return <code>GeckoResult.ALLOW</code> if the tab will be updated,
         *         <code>GeckoResult.DENY</code> otherwise.
         */
        @UiThread
        @NonNull
        default GeckoResult<AllowOrDeny> onUpdateTab(final @NonNull WebExtension extension,
                                                     final @NonNull GeckoSession session,
                                                     final @NonNull UpdateTabDetails details) {
            return GeckoResult.DENY;
        }
    }

    /**
     * Provides details about upating a tab with <code>tabs.update</code>.
     *
     * Whenever a field is not passed in by the extension that value will be <code>null</code>.
     *
     * See also: <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs/update">
     *  WebExtensions/API/tabs/update
     * </a>.
     */
    public static class UpdateTabDetails {
        /**
         * Whether the tab should become active. If <code>true</code>,
         * non-active highlighted tabs should stop being highlighted. If
         * <code>false</code>, does nothing.
         */
        @Nullable
        public final Boolean active;
        /**
         * Whether the tab should be discarded automatically by the app when
         * resources are low.
         */
        @Nullable
        public final Boolean autoDiscardable;
        /**
         * If <code>true</code> and the tab is not highlighted, it should become
         * active by default.
         */
        @Nullable
        public final Boolean highlighted;
        /**
         * Whether the tab should be muted.
         */
        @Nullable
        public final Boolean muted;
        /**
         * Whether the tab should be pinned.
         */
        @Nullable
        public final Boolean pinned;
        /**
         * The url that the tab will be navigated to. This url is provided just
         * for informational purposes, there is no need to call
         * <code>loadUri</code> on it. The corresponding {@link GeckoSession} will be
         * navigated to the right URL after returning
         * <code>GeckoResult.ALLOW</code> from {@link SessionTabDelegate#onUpdateTab}
         */
        @Nullable
        public final String url;

        /** For testing. */
        protected UpdateTabDetails() {
            active = null;
            autoDiscardable = null;
            highlighted = null;
            muted = null;
            pinned = null;
            url = null;
        }

        /* package */ UpdateTabDetails(final GeckoBundle bundle) {
            active = bundle.getBooleanObject("active");
            autoDiscardable = bundle.getBooleanObject("autoDiscardable");
            highlighted = bundle.getBooleanObject("highlighted");
            muted = bundle.getBooleanObject("muted");
            pinned = bundle.getBooleanObject("pinned");
            url = bundle.getString("url");
        }
    }

    /**
     * Provides details about creating a tab with <code>tabs.create</code>.
     * See also: <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs/create">
     *  WebExtensions/API/tabs/create
     * </a>.
     *
     * Whenever a field is not passed in by the extension that value will be <code>null</code>.
     */
    public static class CreateTabDetails {
        /**
         * Whether the tab should become active. If <code>true</code>,
         * non-active highlighted tabs should stop being highlighted. If
         * <code>false</code>, does nothing.
         */
        @Nullable
        public final Boolean active;
        /**
         * Whether the tab is created and made visible in the tab bar
         * without any content loaded into memory, a state known as
         * discarded. The tab’s content should be loaded when the tab is
         * activated.
         */
        @Nullable
        public final Boolean discarded;
        /**
         * The position the tab should take in the window.
         */
        @Nullable
        public final Integer index;
        /**
         * If true, open this tab in Reader Mode.
         */
        @Nullable
        public final Boolean openInReaderMode;
        /**
         * Whether the tab should be pinned.
         */
        @Nullable
        public final Boolean pinned;
        /**
         * The url that the tab will be navigated to. This url is provided just
         * for informational purposes, there is no need to call
         * <code>loadUri</code> on it. The corresponding {@link GeckoSession} will be
         * navigated to the right URL after returning
         * <code>GeckoResult.ALLOW</code> from {@link TabDelegate#onNewTab}
         */
        @Nullable
        public final String url;

        /** For testing. */
        protected CreateTabDetails() {
            active = null;
            discarded = null;
            index = null;
            openInReaderMode = null;
            pinned = null;
            url = null;
        }

        /* package */ CreateTabDetails(final GeckoBundle bundle) {
            active = bundle.getBooleanObject("active");
            discarded = bundle.getBooleanObject("discarded");
            index = bundle.getInteger("index");
            openInReaderMode = bundle.getBooleanObject("openInReaderMode");
            pinned = bundle.getBooleanObject("pinned");
            url = bundle.getString("url");
        }
    }

    /**
     * This delegate is invoked whenever an extension uses the `tabs` WebExtension API and
     * the request is not specific to an existing tab, e.g. when creating a new tab. See also
     * <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs">WebExtensions/API/tabs</a>.
     */
    public interface TabDelegate {
        /**
         * Called when tabs.create is invoked, this method returns a *newly-created* session
         * that GeckoView will use to load the requested page on. If the returned value
         * is null the page will not be opened.
         *
         * @param source An instance of {@link WebExtension}
         * @param createDetails Information about this tab.
         * @return A {@link GeckoResult} which holds the returned GeckoSession. May be null, in
         *        which case the request for a new tab by the extension will fail.
         *        The implementation of onNewTab is responsible for maintaining a reference
         *        to the returned object, to prevent it from being garbage collected.
         */
        @UiThread
        @Nullable
        default GeckoResult<GeckoSession> onNewTab(@NonNull WebExtension source,
                                                   @NonNull CreateTabDetails createDetails) {
            return null;
        }
    }

    /**
     * Get the tab delegate for this extension.
     *
     * See also
     * <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs">WebExtensions/API/tabs</a>.
     *
     * @return The {@link TabDelegate} instance for this extension.
     */
    @UiThread
    @Nullable
    public WebExtension.TabDelegate getTabDelegate() {
        return mDelegateController.getTabDelegate();
    }

    /**
     * Set the tab delegate for this extension. This delegate will be invoked whenever
     * this extension tries to modify the tabs state using the `tabs` WebExtension API.
     *
     * See also
     * <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/tabs">WebExtensions/API/tabs</a>.
     *
     * @param delegate the {@link TabDelegate} instance for this extension.
     */
    @UiThread
    public void setTabDelegate(final @Nullable TabDelegate delegate) {
        if (mDelegateController != null) {
            mDelegateController.onTabDelegate(delegate);
        }
    }


    private static class Sender {
        public String webExtensionId;
        public String nativeApp;

        public Sender(final String webExtensionId, final String nativeApp) {
            this.webExtensionId = webExtensionId;
            this.nativeApp = nativeApp;
        }

        @Override
        public boolean equals(final Object other) {
            if (!(other instanceof Sender)) {
                return false;
            }

            Sender o = (Sender) other;
            return webExtensionId.equals(o.webExtensionId) &&
                    nativeApp.equals(o.nativeApp);
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + (webExtensionId != null ? webExtensionId.hashCode() : 0);
            result = 31 * result + (nativeApp != null ? nativeApp.hashCode() : 0);
            return result;
        }
    }

    // Public wrapper for Listener
    public static class SessionController {
        private final Listener<SessionTabDelegate> mListener;

        /* package */ void setRuntime(final GeckoRuntime runtime) {
            mListener.runtime = runtime;
        }

        /* package */ SessionController(final GeckoSession session) {
            mListener = new Listener<>(session);
        }

        /**
         * Defines a message delegate for a Native App.
         *
         * If a delegate is already present, this delegate will replace the
         * existing one.
         *
         * This message delegate will be responsible for handling messaging between
         * a WebExtension content script running on the {@link GeckoSession}.
         *
         * Note: To receive messages from content scripts, the WebExtension needs
         * to explicitely allow it in {@link WebExtension#WebExtension} by setting
         * {@link Flags#ALLOW_CONTENT_MESSAGING}.
         *
         * @param webExtension {@link WebExtension} that this delegate receives
         *                     messages from.
         *
         * @param delegate {@link MessageDelegate} that will receive messages from
         *                                         this session.
         * @param nativeApp which native app id this message delegate will handle
         *                  messaging for.
         * @see WebExtension#setMessageDelegate
         */
        @AnyThread
        public void setMessageDelegate(final @NonNull WebExtension webExtension,
                                       final @Nullable WebExtension.MessageDelegate delegate,
                                       final @NonNull String nativeApp) {
            mListener.setMessageDelegate(webExtension, delegate, nativeApp);
        }

        /**
         * Get the message delegate for <code>nativeApp</code>.
         *
         * @param extension {@link WebExtension} that this delegate receives messages from.
         * @param nativeApp identifier for the native app
         * @return The {@link MessageDelegate} attached to the
         *         <code>nativeApp</code>.  <code>null</code> if no delegate is
         *         present.
         */
        @AnyThread
        public @Nullable WebExtension.MessageDelegate getMessageDelegate(
                final @NonNull WebExtension extension,
                final @NonNull String nativeApp) {
            return mListener.getMessageDelegate(extension, nativeApp);
        }

        /**
         * Set the Action delegate for this session.
         *
         * This delegate will receive page and browser action overrides specific to
         * this session.  The default Action will be received by the delegate set
         * by {@link WebExtension#setActionDelegate}.
         *
         * @param extension the {@link WebExtension} object this delegate will
         *                     receive updates for
         * @param delegate the {@link ActionDelegate} that will receive updates.
         * @see WebExtension.Action
         */
        @AnyThread
        public void setActionDelegate(final @NonNull WebExtension extension,
                                      final @Nullable ActionDelegate delegate) {
            mListener.setActionDelegate(extension, delegate);
        }

        /**
         * Get the Action delegate for this session.
         *
         * @param extension {@link WebExtension} that this delegates receive
         *                     updates for.
         * @return {@link ActionDelegate} for this
         *         session
         */
        @AnyThread
        @Nullable
        public ActionDelegate getActionDelegate(
                final @NonNull WebExtension extension) {
            return mListener.getActionDelegate(extension);
        }

        /**
         * Set the TabDelegate for this session.
         *
         * This delegate will receive messages specific for this session coming from
         * the WebExtension <code>tabs</code> API.
         *
         * @param extension the {@link WebExtension} this delegate will receive updates
         *                  for
         * @param delegate the {@link TabDelegate} that will receive updates.
         * @see WebExtension#setTabDelegate
         */
        @AnyThread
        public void setTabDelegate(final @NonNull WebExtension extension,
                                   final @Nullable SessionTabDelegate delegate) {
            mListener.setTabDelegate(extension, delegate);
        }

        /**
         * Get the TabDelegate for the given extension.
         *
         * @param extension the {@link WebExtension} this delegate refers to.
         *
         * @return the current {@link SessionTabDelegate} instance
         */
        @AnyThread
        @Nullable
        public SessionTabDelegate getTabDelegate(final @NonNull WebExtension extension) {
            return mListener.getTabDelegate(extension);
        }
    }

    /* package */ final static class Listener<TabDelegate> implements BundleEventListener {
        final private HashMap<Sender, WebExtension.MessageDelegate> mMessageDelegates;
        final private HashMap<String, WebExtension.ActionDelegate> mActionDelegates;
        final private HashMap<String, TabDelegate> mTabDelegates;

        final private GeckoSession mSession;
        final private EventDispatcher mEventDispatcher;

        private boolean mActionDelegateRegistered = false;
        private boolean mMessageDelegateRegistered = false;
        private boolean mTabDelegateRegistered = false;

        // TODO: remove Bug 1618987
        private WebExtensionController.TabDelegate mLegacyTabDelegate;

        public GeckoRuntime runtime;

        public Listener(final GeckoRuntime runtime) {
            this(null, runtime);
        }

        public Listener(final GeckoSession session) {
            this(session, null);
            // TODO: Remove Bug 1618987
            // Close tab event is forwarded to the main listener so we need to listen
            // to it here.
            mEventDispatcher.registerUiThreadListener(
                    this,
                    "GeckoView:WebExtension:NewTab",
                    "GeckoView:WebExtension:UpdateTab",
                    "GeckoView:WebExtension:CloseTab"
            );
            mTabDelegateRegistered = true;
        }

        private Listener(final GeckoSession session, final GeckoRuntime runtime) {
            mMessageDelegates = new HashMap<>();
            mActionDelegates = new HashMap<>();
            mTabDelegates = new HashMap<>();
            mEventDispatcher = session != null
                    ? session.getEventDispatcher()
                    : EventDispatcher.getInstance();
            mSession = session;
            this.runtime = runtime;
        }

        // TODO: remove Bug 1618987
        @Deprecated
        public void setTabDelegate(final WebExtensionController.TabDelegate delegate) {
            if (!mTabDelegateRegistered && delegate != null) {
                mEventDispatcher.registerUiThreadListener(
                        this,
                        "GeckoView:WebExtension:NewTab",
                        "GeckoView:WebExtension:UpdateTab",
                        "GeckoView:WebExtension:CloseTab"
                );
                mTabDelegateRegistered = true;
            }

            mLegacyTabDelegate = delegate;
        }

        // TODO: remove Bug 1618987
        @Deprecated
        public WebExtensionController.TabDelegate getTabDelegate() {
            return mLegacyTabDelegate;
        }

        public void unregisterWebExtension(final WebExtension extension) {
            mMessageDelegates.remove(extension.id);
            mActionDelegates.remove(extension.id);
            mTabDelegates.remove(extension.id);
        }

        public void setTabDelegate(final WebExtension webExtension,
                                   final TabDelegate delegate) {
            if (!mTabDelegateRegistered && delegate != null) {
                mEventDispatcher.registerUiThreadListener(
                        this,
                        "GeckoView:WebExtension:NewTab",
                        "GeckoView:WebExtension:UpdateTab",
                        "GeckoView:WebExtension:CloseTab"
                );
                mTabDelegateRegistered = true;
            }

            mTabDelegates.put(webExtension.id, delegate);
        }

        public TabDelegate getTabDelegate(final WebExtension webExtension) {
            return mTabDelegates.get(webExtension.id);
        }

        public void setActionDelegate(final WebExtension webExtension,
                                      final WebExtension.ActionDelegate delegate) {
            if (!mActionDelegateRegistered && delegate != null) {
                mEventDispatcher.registerUiThreadListener(this,
                        "GeckoView:BrowserAction:Update",
                        "GeckoView:BrowserAction:OpenPopup",
                        "GeckoView:PageAction:Update",
                        "GeckoView:PageAction:OpenPopup");
                mActionDelegateRegistered = true;
            }

            mActionDelegates.put(webExtension.id, delegate);
        }

        public WebExtension.ActionDelegate getActionDelegate(final WebExtension webExtension) {
            return mActionDelegates.get(webExtension.id);
        }

        public void setMessageDelegate(final WebExtension webExtension,
                                       final WebExtension.MessageDelegate delegate,
                                       final String nativeApp) {
            if (!mMessageDelegateRegistered && delegate != null) {
                mEventDispatcher.registerUiThreadListener(this,
                        "GeckoView:WebExtension:Message",
                        "GeckoView:WebExtension:PortMessage",
                        "GeckoView:WebExtension:Connect",
                        "GeckoView:WebExtension:Disconnect");
                mMessageDelegateRegistered = true;
            }

            mMessageDelegates.put(new Sender(webExtension.id, nativeApp), delegate);
        }

        public WebExtension.MessageDelegate getMessageDelegate(final WebExtension webExtension,
                                                               final String nativeApp) {
            return mMessageDelegates.get(new Sender(webExtension.id, nativeApp));
        }

        @Override
        public void handleMessage(final String event, final GeckoBundle message,
                                  final EventCallback callback) {
            if (runtime == null) {
                return;
            }

            // TODO: remove Bug 1618987
            final WebExtensionController controller = runtime.getWebExtensionController();
            WebExtensionController.TabDelegate delegate = controller.getTabDelegate();
            if (delegate != null) {
                if ("GeckoView:WebExtension:CloseTab".equals(event)) {
                    controller.closeTab(message, callback, mSession, delegate);
                    return;
                } else if ("GeckoView:WebExtension:NewTab".equals(event)) {
                    controller.newTab(message, callback, delegate);
                    return;
                }
            }

            runtime.getWebExtensionController().handleMessage(event, message, callback, mSession);
        }
    }

    /**
     * Describes the sender of a message from a WebExtension.
     *
     * See also: <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/MessageSender">
     *     WebExtensions/API/runtime/MessageSender</a>
     */
    @UiThread
    public static class MessageSender {
        /** {@link WebExtension} that sent this message. */
        public final @NonNull WebExtension webExtension;

        /** {@link GeckoSession} that sent this message.
         *
         * <code>null</code> if coming from a background script. */
        public final @Nullable GeckoSession session;

        @Retention(RetentionPolicy.SOURCE)
        @IntDef({ENV_TYPE_UNKNOWN, ENV_TYPE_EXTENSION, ENV_TYPE_CONTENT_SCRIPT})
        /* package */ @interface EnvType {}
        /* package */ static final int ENV_TYPE_UNKNOWN = 0;
        /** This sender originated inside a privileged extension context like
         * a background script. */
        public static final int ENV_TYPE_EXTENSION = 1;

        /** This sender originated inside a content script. */
        public static final int ENV_TYPE_CONTENT_SCRIPT = 2;

        /**
         * Type of environment that sent this message, either
         *
         * <ul>
         *     <li>{@link MessageSender#ENV_TYPE_EXTENSION} if the message was sent from
         *         a background page </li>
         *     <li>{@link MessageSender#ENV_TYPE_CONTENT_SCRIPT} if the message was sent
         *         from a content script </li>
         * </ul>
         */
        // TODO: Bug 1534640 do we need ENV_TYPE_EXTENSION_PAGE ?
        public final @EnvType int environmentType;

        /** URL of the frame that sent this message.
         *
         *  Use this value together with {@link MessageSender#isTopLevel} to
         *  verify that the message is coming from the expected page. Only top
         *  level frames can be trusted.
         */
        public final @NonNull String url;

        /* package */ final boolean isTopLevel;

        /* package */ MessageSender(final @NonNull WebExtension webExtension,
                                    final @Nullable GeckoSession session,
                                    final @Nullable String url,
                                    final @EnvType int environmentType,
                                    final boolean isTopLevel) {
            this.webExtension = webExtension;
            this.session = session;
            this.isTopLevel = isTopLevel;
            this.url = url;
            this.environmentType = environmentType;
        }

        /** Override for testing. */
        protected MessageSender() {
            this.webExtension = null;
            this.session = null;
            this.isTopLevel = false;
            this.url = null;
            this.environmentType = ENV_TYPE_UNKNOWN;
        }

        /** Whether this MessageSender belongs to a top level frame.
         *
         * @return true if the MessageSender was sent from the top level frame,
         *         false otherwise.
         * */
        public boolean isTopLevel() {
            return this.isTopLevel;
        }
    }

    /**
     * Represents an icon, e.g. the browser action icon or the extension icon.
     */
    public static class Icon {
        private Map<Integer, String> mIconUris;

        /**
         * Get the best version of this icon for size <code>pixelSize</code>.
         *
         * Embedders are encouraged to cache the result of this method keyed with this instance.
         *
         * @param pixelSize pixel size at which this icon will be displayed at.
         *
         * @return A {@link GeckoResult} that resolves to the bitmap when ready.
         */
        @AnyThread
        @NonNull
        public GeckoResult<Bitmap> get(final int pixelSize) {
            int size;

            if (mIconUris.containsKey(pixelSize)) {
                // If this size matches exactly, return it
                size = pixelSize;
            } else {
                // Otherwise, find the smallest larger image (or the largest image if they are all
                // smaller)
                List<Integer> sizes = new ArrayList<>();
                sizes.addAll(mIconUris.keySet());
                Collections.sort(sizes, (a, b) -> Integer.compare(b - pixelSize, a - pixelSize));
                size = sizes.get(0);
            }

            final String uri = mIconUris.get(size);
            return ImageDecoder.instance().decode(uri, pixelSize);
        }

        /* package */ Icon(final GeckoBundle bundle) {
            mIconUris = new HashMap<>();

            for (final String key: bundle.keys()) {
                final Integer intKey = Integer.valueOf(key);
                if (intKey == null) {
                    Log.e(LOGTAG, "Non-integer icon key: " + intKey);
                    if (BuildConfig.DEBUG) {
                        throw new RuntimeException("Non-integer icon key: " + key);
                    }
                    continue;
                }

                final String value = getIconValue(bundle.get(key));
                if (value != null) {
                    mIconUris.put(intKey, value);
                }
            }
        }

        private String getIconValue(final Object value) {
            // The icon value can either be an object containing icons for each theme...
            if (value instanceof GeckoBundle) {
                // We don't support theme_icons yet, so let's just return the default value.
                final GeckoBundle themeIcons = (GeckoBundle) value;
                final Object defaultIcon = themeIcons.get("default");

                if (!(defaultIcon instanceof String)) {
                    if (BuildConfig.DEBUG) {
                        throw new RuntimeException("Unexpected themed_icon value.");
                    }
                    Log.e(LOGTAG, "Unexpected themed_icon value.");
                    return null;
                }

                return (String) defaultIcon;
            }

            // ... or just a URL
            if (value instanceof String) {
                return (String) value;
            }

            // We never expect it to be something else, so let's error out here
            if (BuildConfig.DEBUG) {
                throw new RuntimeException("Unexpected icon value: " + value);
            }

            Log.e(LOGTAG, "Unexpected icon value.");
            return null;
        }

        /** Override for tests. */
        protected Icon() {
            mIconUris = null;
        }

        @Override
        public boolean equals(final Object o) {
            if (o == this) {
                return true;
            }

            if (!(o instanceof Icon)) {
                return false;
            }

            return mIconUris.equals(((Icon) o).mIconUris);
        }

        @Override
        public int hashCode() {
            return mIconUris.hashCode();
        }
    }

    /**
     * Represents either a Browser Action or a Page Action from the
     * WebExtension API.
     *
     * Instances of this class may represent the default <code>Action</code>
     * which applies to all WebExtension tabs or a tab-specific override. To
     * reconstruct the full <code>Action</code> object, you can use
     * {@link Action#withDefault}.
     *
     * Tab specific overrides can be obtained by registering a delegate using
     * {@link SessionController#setActionDelegate}, while default values
     * can be obtained by registering a delegate using
     * {@link #setActionDelegate}.
     *
     * <br>
     * See also
     * <ul>
     *     <li><a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/browserAction">
     *         WebExtensions/API/browserAction
     *     </a></li>
     *     <li><a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/pageAction">
     *         WebExtensions/API/pageAction
     *     </a></li>
     * </ul>
     */
    @AnyThread
    public static class Action {
        /**
         * Title of this Action.
         *
         * See also:
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/pageAction/getTitle">
         *     pageAction/getTitle</a>,
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/browserAction/getTitle">
         *     browserAction/getTitle</a>
         */
        final public @Nullable String title;
        /**
         * Icon for this Action.
         *
         * See also:
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/pageAction/setIcon">
         *     pageAction/setIcon</a>,
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/browserAction/setIcon">
         *     browserAction/setIcon</a>
         */
        final public @Nullable Icon icon;
        /**
         * URI of the Popup to display when the user taps on the icon for this
         * Action.
         *
         * See also:
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/pageAction/getPopup">
         *     pageAction/getPopup</a>,
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/browserAction/getPopup">
         *     browserAction/getPopup</a>
         */
        final private @Nullable String mPopupUri;
        /**
         * Whether this action is enabled and should be visible.
         *
         * Note: for page action, this is <code>true</code> when the extension calls
         * <code>pageAction.show</code> and <code>false</code> when the extension
         * calls <code>pageAction.hide</code>.
         *
         * See also:
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/pageAction/show">
         *     pageAction/show</a>,
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/browserAction/enabled">
         *     browserAction/enabled</a>
         */
        final public @Nullable Boolean enabled;
        /**
         * Badge text for this action.
         *
         * See also:
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/browserAction/getBadgeText">
         *     browserAction/getBadgeText</a>
         */
        final public @Nullable String badgeText;
        /**
         * Background color for the badge for this Action.
         *
         * This method will return an Android color int that can be used in
         * {@link android.widget.TextView#setBackgroundColor(int)} and similar
         * methods.
         *
         * See also:
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/browserAction/getBadgeBackgroundColor">
         *     browserAction/getBadgeBackgroundColor</a>
         */
        final public @Nullable Integer badgeBackgroundColor;
        /**
         * Text color for the badge for this Action.
         *
         * This method will return an Android color int that can be used in
         * {@link android.widget.TextView#setTextColor(int)} and similar
         * methods.
         *
         * See also:
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/browserAction/getBadgeTextColor">
         *     browserAction/getBadgeTextColor</a>
         */
        final public @Nullable Integer badgeTextColor;

        final private WebExtension mExtension;

        /* package */ final static int TYPE_BROWSER_ACTION = 1;
        /* package */ final static int TYPE_PAGE_ACTION = 2;
        @Retention(RetentionPolicy.SOURCE)
        @IntDef({TYPE_BROWSER_ACTION, TYPE_PAGE_ACTION})
        /* package */ @interface ActionType {}

        /* package */ final @ActionType int type;

        /* package */ Action(final @ActionType int type,
                             final GeckoBundle bundle, final WebExtension extension) {
            mExtension = extension;
            mPopupUri = bundle.getString("popup");

            this.type = type;

            title = bundle.getString("title");
            badgeText = bundle.getString("badgeText");
            badgeBackgroundColor = colorFromRgbaArray(
                    bundle.getDoubleArray("badgeBackgroundColor"));
            badgeTextColor = colorFromRgbaArray(
                    bundle.getDoubleArray("badgeTextColor"));

            if (bundle.containsKey("icon")) {
                icon = new Icon(bundle.getBundle("icon"));
            } else {
                icon = null;
            }

            if (bundle.getBoolean("patternMatching", false)) {
                // This action was enabled by pattern matching
                enabled = true;
            } else if (bundle.containsKey("enabled")) {
                enabled = bundle.getBoolean("enabled");
            } else {
                enabled = null;
            }
        }

        private Integer colorFromRgbaArray(final double[] c) {
            if (c == null) {
                return null;
            }

            return Color.argb((int) c[3], (int) c[0], (int) c[1], (int) c[2]);
        }

        @Override
        public String toString() {
            return "Action {\n"
                    + "\ttitle: " + this.title + ",\n"
                    + "\ticon: " + this.icon + ",\n"
                    + "\tpopupUri: " + this.mPopupUri + ",\n"
                    + "\tenabled: " + this.enabled + ",\n"
                    + "\tbadgeText: " + this.badgeText + ",\n"
                    + "\tbadgeTextColor: " + this.badgeTextColor + ",\n"
                    + "\tbadgeBackgroundColor: " + this.badgeBackgroundColor + ",\n"
                    + "}";
        }

        // For testing
        protected Action() {
            type = TYPE_BROWSER_ACTION;
            mExtension = null;
            mPopupUri = null;
            title = null;
            icon = null;
            enabled = null;
            badgeText = null;
            badgeTextColor = null;
            badgeBackgroundColor = null;
        }

        /**
         * Merges values from this Action with the default Action.
         *
         * @param defaultValue the default Action as received from
         *                     {@link ActionDelegate#onBrowserAction}
         *                     or {@link ActionDelegate#onPageAction}.
         *
         * @return an {@link Action} where all <code>null</code> values from
         *         this instance are replaced with values from
         *         <code>defaultValue</code>.
         * @throws IllegalArgumentException if defaultValue is not of the same
         *         type, e.g. if this Action is a Page Action and default
         *         value is a Browser Action.
         */
        @NonNull
        public Action withDefault(final @NonNull Action defaultValue) {
            return new Action(this, defaultValue);
        }

        /** @see Action#withDefault */
        private Action(final Action source, final Action defaultValue) {
            if (source.type != defaultValue.type) {
                throw new IllegalArgumentException(
                        "defaultValue must be of the same type.");
            }

            type = source.type;
            mExtension = source.mExtension;

            title = source.title != null ? source.title : defaultValue.title;
            icon = source.icon != null ? source.icon : defaultValue.icon;
            mPopupUri = source.mPopupUri != null ? source.mPopupUri : defaultValue.mPopupUri;
            enabled = source.enabled != null  ? source.enabled : defaultValue.enabled;
            badgeText = source.badgeText != null ? source.badgeText : defaultValue.badgeText;
            badgeTextColor = source.badgeTextColor != null
                    ? source.badgeTextColor : defaultValue.badgeTextColor;
            badgeBackgroundColor = source.badgeBackgroundColor != null
                    ? source.badgeBackgroundColor : defaultValue.badgeBackgroundColor;
        }

        @UiThread
        public void click() {
            if (mPopupUri != null && !mPopupUri.isEmpty()) {
                final ActionDelegate delegate = mExtension.mDelegateController.getActionDelegate();
                if (delegate == null) {
                    return;
                }

                GeckoResult<GeckoSession> popup = delegate.onTogglePopup(mExtension, this);
                openPopup(popup);

                // When popupUri is specified, the extension doesn't get a callback
                return;
            }

            final GeckoBundle bundle = new GeckoBundle(1);
            bundle.putString("extensionId", mExtension.id);

            if (type == TYPE_BROWSER_ACTION) {
                EventDispatcher.getInstance().dispatch(
                        "GeckoView:BrowserAction:Click", bundle);
            } else if (type == TYPE_PAGE_ACTION) {
                EventDispatcher.getInstance().dispatch(
                        "GeckoView:PageAction:Click", bundle);
            } else {
                throw new IllegalStateException("Unknown Action type");
            }
        }

        /* package */ void openPopup(final GeckoResult<GeckoSession> popup) {
            if (popup == null) {
                return;
            }

            popup.accept(session -> {
                if (session == null) {
                    return;
                }

                session.getSettings().setIsPopup(true);
                session.loadUri(mPopupUri);
            });
        }
    }

    /**
     * Receives updates whenever a Browser action or a Page action has been
     * defined by an extension.
     *
     * This delegate will receive the default action when registered with
     * {@link WebExtension#setActionDelegate}. To receive
     * {@link GeckoSession}-specific overrides you can use
     * {@link SessionController#setActionDelegate}.
     */
    public interface ActionDelegate {
        /**
         * Called whenever a browser action is defined or updated.
         *
         * This method will be called whenever an extension that defines a
         * browser action is registered or the properties of the Action are
         * updated.
         *
         * See also <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/browserAction">
         *  WebExtensions/API/browserAction
         * </a>,
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/browser_action">
         *    WebExtensions/manifest.json/browser_action
         * </a>.
         *
         * @param extension The extension that defined this browser action.
         * @param session Either the {@link GeckoSession} corresponding to the
         *                tab to which this Action override applies.
         *                <code>null</code> if <code>action</code> is the new
         *                default value.
         * @param action {@link Action} containing the override values for this
         *               {@link GeckoSession} or the default value if
         *               <code>session</code> is <code>null</code>.
         */
        @UiThread
        default void onBrowserAction(final @NonNull WebExtension extension,
                                     final @Nullable GeckoSession session,
                                     final @NonNull Action action) {}
        /**
         * Called whenever a page action is defined or updated.
         *
         * This method will be called whenever an extension that defines a page
         * action is registered or the properties of the Action are updated.
         *
         * See also <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/pageAction">
         *  WebExtensions/API/pageAction
         * </a>,
         * <a target=_blank href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/page_action">
         *    WebExtensions/manifest.json/page_action
         * </a>.
         *
         * @param extension The extension that defined this page action.
         * @param session Either the {@link GeckoSession} corresponding to the
         *                tab to which this Action override applies.
         *                <code>null</code> if <code>action</code> is the new
         *                default value.
         * @param action {@link Action} containing the override values for this
         *               {@link GeckoSession} or the default value if
         *               <code>session</code> is <code>null</code>.
         */
        @UiThread
        default void onPageAction(final @NonNull WebExtension extension,
                                  final @Nullable GeckoSession session,
                                  final @NonNull Action action) {}

        /**
         * Called whenever the action wants to toggle a popup view.
         *
         * @param extension The extension that wants to display a popup
         * @param action The action where the popup is defined
         * @return A GeckoSession that will be used to display the pop-up,
         *         null if no popup will be displayed.
         */
        @UiThread
        @Nullable
        default GeckoResult<GeckoSession> onTogglePopup(final @NonNull WebExtension extension,
                                                        final @NonNull Action action) {
            return null;
        }

        /**
         * Called whenever the action wants to open a popup view.
         *
         * @param extension The extension that wants to display a popup
         * @param action The action where the popup is defined
         * @return A GeckoSession that will be used to display the pop-up,
         *         null if no popup will be displayed.
         */
        @UiThread
        @Nullable
        default GeckoResult<GeckoSession> onOpenPopup(final @NonNull WebExtension extension,
                                                      final @NonNull Action action) {
            return null;
        }
    }

    /** Extension thrown when an error occurs during extension installation. */
    public static class InstallException extends Exception {
        public static class ErrorCodes {
            /** The download failed due to network problems. */
            public static final int ERROR_NETWORK_FAILURE = -1;
            /** The downloaded file did not match the provided hash. */
            public static final int ERROR_INCORRECT_HASH = -2;
            /** The downloaded file seems to be corrupted in some way. */
            public static final int ERROR_CORRUPT_FILE = -3;
            /** An error occurred trying to write to the filesystem. */
            public static final int ERROR_FILE_ACCESS = -4;
            /** The extension must be signed and isn't. */
            public static final int ERROR_SIGNEDSTATE_REQUIRED = -5;
            /** The downloaded extension had a different type than expected. */
            public static final int ERROR_UNEXPECTED_ADDON_TYPE = -6;
            /** The extension did not have the expected ID. */
            public static final int ERROR_INCORRECT_ID = -7;
            /** The extension install was canceled. */
            public static final int ERROR_USER_CANCELED = -100;
            /** The extension install was postponed until restart. */
            public static final int ERROR_POSTPONED = -101;

            /** For testing. */
            protected ErrorCodes() {}
        }

        @Retention(RetentionPolicy.SOURCE)
        @IntDef(value = {
                ErrorCodes.ERROR_NETWORK_FAILURE,
                ErrorCodes.ERROR_INCORRECT_HASH,
                ErrorCodes.ERROR_CORRUPT_FILE,
                ErrorCodes.ERROR_FILE_ACCESS,
                ErrorCodes.ERROR_SIGNEDSTATE_REQUIRED,
                ErrorCodes.ERROR_UNEXPECTED_ADDON_TYPE,
                ErrorCodes.ERROR_INCORRECT_ID,
                ErrorCodes.ERROR_USER_CANCELED,
                ErrorCodes.ERROR_POSTPONED,
        })
        /* package */ @interface Codes {}

        /** One of {@link ErrorCodes} that provides more information about this exception. */
        public final @Codes int code;

        /** For testing */
        protected InstallException() {
            this.code = ErrorCodes.ERROR_NETWORK_FAILURE;
        }

        @Override
        public String toString() {
            return "InstallException: " + code;
        }

        /* package */ InstallException(final @Codes int code) {
            this.code = code;
        }
    }

    /**
     * Set the Action delegate for this WebExtension.
     *
     * This delegate will receive updates every time the default Action value
     * changes.
     *
     * To listen for {@link GeckoSession}-specific updates, use
     * {@link SessionController#setActionDelegate}
     *
     * @param delegate {@link ActionDelegate} that will receive updates.
     */
    @AnyThread
    public void setActionDelegate(final @Nullable ActionDelegate delegate) {
        if (mDelegateController != null) {
            mDelegateController.onActionDelegate(delegate);
        }

        final GeckoBundle bundle = new GeckoBundle(1);
        bundle.putString("extensionId", id);

        EventDispatcher.getInstance().dispatch(
                "GeckoView:ActionDelegate:Attached", bundle);
    }

    /** Describes the signed status for a {@link WebExtension}.
     *
     * See <a href="https://support.mozilla.org/en-US/kb/add-on-signing-in-firefox">
     *   Add-on signing in Firefox.
     * </a>
     */
    public static class SignedStateFlags {
        // Keep in sync with AddonManager.jsm
        /** This extension may be signed but by a certificate that doesn't
         * chain to our our trusted certificate. */
        public final static int UNKNOWN = -1;
        /** This extension is unsigned. */
        public final static int MISSING = 0;
        /** This extension has been preliminarily reviewed. */
        public final static int PRELIMINARY = 1;
        /** This extension has been fully reviewed. */
        public final static int SIGNED = 2;
        /** This extension is a system add-on. */
        public final static int SYSTEM = 3;
        /** This extension is signed with a "Mozilla Extensions" certificate. */
        public final static int PRIVILEGED = 4;

        /* package */ final static int LAST = PRIVILEGED;
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ SignedStateFlags.UNKNOWN, SignedStateFlags.MISSING, SignedStateFlags.PRELIMINARY,
        SignedStateFlags.SIGNED, SignedStateFlags.SYSTEM, SignedStateFlags.PRIVILEGED})
    @interface SignedState {}

    /** Describes the blocklist state for a {@link WebExtension}.
     *  See <a href="https://support.mozilla.org/en-US/kb/add-ons-cause-issues-are-on-blocklist">
     *      Add-ons that cause stability or security issues are put on a blocklist
     *  </a>.
     */
    public static class BlocklistStateFlags {
        // Keep in sync with nsIBlocklistService.idl
        /** This extension does not appear in the blocklist. */
        public final static int NOT_BLOCKED = 0;
        /** This extension is in the blocklist but the problem is not severe
         * enough to warant forcibly blocking. */
        public final static int SOFTBLOCKED = 1;
        /** This extension should be blocked and never used. */
        public final static int BLOCKED = 2;
        /** This extension is considered outdated, and there is a known update
         * available. */
        public final static int OUTDATED = 3;
        /** This extension is vulnerable and there is an update. */
        public final static int VULNERABLE_UPDATE_AVAILABLE = 4;
        /** This extension is vulnerable and there is no update. */
        public final static int VULNERABLE_NO_UPDATE = 5;
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ BlocklistStateFlags.NOT_BLOCKED, BlocklistStateFlags.SOFTBLOCKED,
            BlocklistStateFlags.BLOCKED, BlocklistStateFlags.OUTDATED,
            BlocklistStateFlags.VULNERABLE_UPDATE_AVAILABLE,
            BlocklistStateFlags.VULNERABLE_NO_UPDATE})
    @interface BlocklistState {}

    public static class DisabledFlags {
        /** The extension has been disabled by the user */
        public final static int USER = 1 << 1;

        /** The extension has been disabled by the blocklist. The details of why this extension
         * was blocked can be found in {@link MetaData#blocklistState}. */
        public final static int BLOCKLIST = 1 << 2;

        /** The extension has been disabled by the application. To enable the extension you can use
         * {@link WebExtensionController#enable} passing in
         * {@link WebExtensionController.EnableSource#APP} as <code>source</code>. */
        public final static int APP = 1 << 3;
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef(flag = true,
            value = { DisabledFlags.USER, DisabledFlags.BLOCKLIST,
                      DisabledFlags.APP })
    @interface EnabledFlags {}

    /** Provides information about a {@link WebExtension}. */
    public class MetaData {
        /** Main {@link Icon} branding for this {@link WebExtension}.
          * Can be used when displaying prompts. */
        public final @NonNull Icon icon;
        /** API permissions requested or granted to this extension.
          *
          * Permission identifiers match entries in the manifest, see
          * <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions#API_permissions">
          *   API permissions
          * </a>.
          */
        public final @NonNull String[] permissions;
        /** Host permissions requested or granted to this extension.
          *
          * See <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions#Host_permissions">
          *   Host permissions
          * </a>.
          */
        public final @NonNull String[] origins;
        /** Branding name for this extension.
          *
          * See <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/name">
          *   manifest.json/name
          * </a>
          */
        public final @Nullable String name;
        /** Branding description for this extension. This string will be
          * localized using the current GeckoView language setting.
          *
          * See <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/description">
          *   manifest.json/description
          * </a>
          */
        public final @Nullable String description;
        /** Version string for this extension.
          *
          * See <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/version">
          *   manifest.json/version
          * </a>
          */
        public final @NonNull String version;
        /** Creator name as provided in the manifest.
          *
          * See <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/developer">
          *   manifest.json/developer
          * </a>
          */
        public final @Nullable String creatorName;
        /** Creator url as provided in the manifest.
          *
          * See <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/developer">
          *   manifest.json/developer
          * </a>
          */
        public final @Nullable String creatorUrl;
        /** Homepage url as provided in the manifest.
          *
          * See <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/homepage_url">
          *   manifest.json/homepage_url
          * </a>
          */
        public final @Nullable String homepageUrl;
        /** Options page as provided in the manifest.
          *
          * See <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/options_ui">
          *   manifest.json/options_ui
          * </a>
          */
        public final @Nullable String optionsPageUrl;
        /** Whether the options page should be open in a Tab or not.
          *
          * See <a href="https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/options_ui#Syntax">
          *   manifest.json/options_ui#Syntax
          * </a>
          */
        public final boolean openOptionsPageInTab;
        /** Whether or not this is a recommended extension.
          *
          * See <a href="https://blog.mozilla.org/firefox/firefox-recommended-extensions/">
          *   Recommended Extensions program
          * </a>
          */
        public final boolean isRecommended;
        /** Blocklist status for this extension.
          *
          * See <a href="https://support.mozilla.org/en-US/kb/add-ons-cause-issues-are-on-blocklist">
          *     Add-ons that cause stability or security issues are put on a blocklist
          * </a>.
          */
        public final @BlocklistState int blocklistState;
        /** Signed status for this extension.
          *
          * See <a href="https://support.mozilla.org/en-US/kb/add-on-signing-in-firefox">
          *   Add-on signing in Firefox.
          * </a>.
          */
        public final @SignedState int signedState;

        /**
         * Disabled binary flags for this extension.
         *
         * This will be either equal to <code>0</code> if the extension
         * is enabled or will contain one or more flags from {@link DisabledFlags}.
         *
         * e.g. if the extension has been disabled by the user, the value in
         * {@link DisabledFlags#USER} will be equal to <code>1</code>:
         *
         * <pre><code>
         *     boolean isUserDisabled = metaData.disabledFlags
         *          &amp; DisabledFlags.USER &gt; 0;
         * </code></pre>
         */
        public final @EnabledFlags int disabledFlags;

        /**
         * Root URL for this extension's pages. Can be used to determine if a given URL
         * belongs to this extension.
         */
        public final @NonNull String baseUrl;

        /**
         * Whether this extension is allowed to run in private browsing or not.
         * To modify this value use {@link WebExtensionController#setAllowedInPrivateBrowsing}.
         */
        public final boolean allowedInPrivateBrowsing;

        /**
         * Whether this extension is enabled or not.
         */
        public final boolean enabled;

        /** Override for testing. */
        protected MetaData() {
            icon = null;
            permissions = null;
            origins = null;
            name = null;
            description = null;
            version = null;
            creatorName = null;
            creatorUrl = null;
            homepageUrl = null;
            optionsPageUrl = null;
            openOptionsPageInTab = false;
            isRecommended = false;
            blocklistState = BlocklistStateFlags.NOT_BLOCKED;
            signedState = SignedStateFlags.UNKNOWN;
            disabledFlags = 0;
            enabled = true;
            baseUrl = null;
            allowedInPrivateBrowsing = false;
        }

        /* package */ MetaData(final GeckoBundle bundle) {
            // We only expose permissions that the embedder should prompt for
            permissions = bundle.getStringArray("promptPermissions");
            origins = bundle.getStringArray("origins");
            description = bundle.getString("description");
            version = bundle.getString("version");
            creatorName = bundle.getString("creatorName");
            creatorUrl = bundle.getString("creatorURL");
            homepageUrl = bundle.getString("homepageURL");
            name = bundle.getString("name");
            optionsPageUrl = bundle.getString("optionsPageURL");
            openOptionsPageInTab = bundle.getBoolean("openOptionsPageInTab");
            isRecommended = bundle.getBoolean("isRecommended");
            blocklistState = bundle.getInt("blocklistState", BlocklistStateFlags.NOT_BLOCKED);
            enabled = bundle.getBoolean("enabled", false);
            baseUrl = bundle.getString("baseURL");
            allowedInPrivateBrowsing = bundle.getBoolean("privateBrowsingAllowed", false);

            int signedState = bundle.getInt("signedState", SignedStateFlags.UNKNOWN);
            if (signedState <= SignedStateFlags.LAST) {
                this.signedState = signedState;
            } else {
                Log.e(LOGTAG, "Unrecognized signed state: " + signedState);
                this.signedState = SignedStateFlags.UNKNOWN;
            }

            int disabledFlags = 0;
            final String[] disabledFlagsString = bundle.getStringArray("disabledFlags");

            for (final String flag : disabledFlagsString) {
                if (flag.equals("userDisabled")) {
                    disabledFlags |= DisabledFlags.USER;
                } else if (flag.equals("blocklistDisabled")) {
                    disabledFlags |= DisabledFlags.BLOCKLIST;
                } else if (flag.equals("appDisabled")) {
                    disabledFlags |= DisabledFlags.APP;
                } else {
                    Log.e(LOGTAG, "Unrecognized disabledFlag state: " + flag);
                }
            }
            this.disabledFlags = disabledFlags;

            if (bundle.containsKey("icons")) {
                icon = new Icon(bundle.getBundle("icons"));
            } else {
                icon = null;
            }
        }
    }
}
