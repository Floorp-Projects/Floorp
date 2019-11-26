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
    /**
     * Delegates that handle messaging between this WebExtension and the app.
     */
    /* package */ final @NonNull Map<String, MessageDelegate> messageDelegates;

    /* package */ @NonNull ActionDelegate actionDelegate;

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
         * {@link GeckoSession#setMessageDelegate}.
         */
        public static final long ALLOW_CONTENT_MESSAGING = 1 << 0;

        // Do not instantiate this class.
        protected Flags() {}
    }

    @Retention(RetentionPolicy.SOURCE)
    @LongDef(flag = true,
            value = { Flags.NONE, Flags.ALLOW_CONTENT_MESSAGING })
    /* package */ @interface WebExtensionFlags {}

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
     */

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
     */
    public WebExtension(final @NonNull String location, final @NonNull String id,
                        final @WebExtensionFlags long flags) {
        this.location = location;
        this.id = id;
        this.flags = flags;
        this.messageDelegates = new HashMap<>();
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
     */
    public WebExtension(final @NonNull String location) {
        this(location, "{" + UUID.randomUUID().toString() + "}", Flags.NONE);
    }

    /**
     * Defines the message delegate for a Native App.
     *
     * This message delegate will receive messages from the background script
     * for the native app specified in <code>nativeApp</code>.
     *
     * For messages from content scripts, set a session-specific message
     * delegate using {@link GeckoSession#setMessageDelegate}.
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
     * @see GeckoSession#setMessageDelegate
     */
    @UiThread
    public void setMessageDelegate(final @Nullable MessageDelegate messageDelegate,
                                   final @NonNull String nativeApp) {
        if (messageDelegate == null) {
            messageDelegates.remove(nativeApp);
            return;
        }
        messageDelegates.put(nativeApp, messageDelegate);
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
        /* package */ final WeakReference<DisconnectDelegate> disconnectDelegate;

        /** {@link MessageSender} corresponding to this port. */
        public @NonNull final MessageSender sender;

        /** The application identifier of the MessageDelegate that opened this port. */
        public @NonNull final String name;

        /* package */ interface DisconnectDelegate {
            void onDisconnectFromApp(Port port);
        }

        /** Override for tests. */
        protected Port() {
            this.id = -1;
            this.delegate = null;
            this.disconnectDelegate = null;
            this.sender = null;
            this.name = null;
        }

        /* package */ Port(final String name, final long id, final MessageSender sender,
                           final DisconnectDelegate disconnectDelegate) {
            this.id = id;
            this.delegate = NULL_PORT_DELEGATE;
            this.disconnectDelegate = new WeakReference<>(disconnectDelegate);
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

            DisconnectDelegate disconnectDelegate = this.disconnectDelegate.get();
            if (disconnectDelegate != null) {
                disconnectDelegate.onDisconnectFromApp(this);
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
            if (delegate != null) {
                this.delegate = delegate;
            } else {
                this.delegate = NULL_PORT_DELEGATE;
            }
        }
    }

    /* package */ static final WebExtension.PortDelegate NULL_PORT_DELEGATE = new WebExtension.PortDelegate() {
        @Override
        public void onPortMessage(final @NonNull Object message,
                                  final @NonNull Port port) {
            Log.d(LOGTAG, "Unhandled message from " + port.sender.webExtension.id
                    + ": " + message.toString());
        }

        @NonNull
        @Override
        public void onDisconnect(final @NonNull Port port) {
            Log.d(LOGTAG, "Unhandled disconnect from " + port.sender.webExtension.id);
        }
    };

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

    /* package */ final static class Listener implements BundleEventListener {
        final private HashMap<Sender, WebExtension.MessageDelegate> mMessageDelegates;
        final private HashMap<String, WebExtension.ActionDelegate> mActionDelegates;
        final private GeckoSession mSession;
        public GeckoRuntime runtime;

        public Listener(final GeckoSession session) {
            mMessageDelegates = new HashMap<>();
            mActionDelegates = new HashMap<>();
            mSession = session;
        }

        /* package */ void registerListeners() {
            mSession.getEventDispatcher().registerUiThreadListener(this,
                    "GeckoView:WebExtension:Message",
                    "GeckoView:WebExtension:PortMessage",
                    "GeckoView:WebExtension:Connect",
                    "GeckoView:WebExtension:CloseTab",

                    // Browser and Page Actions
                    "GeckoView:BrowserAction:Update",
                    "GeckoView:BrowserAction:OpenPopup",
                    "GeckoView:PageAction:Update",
                    "GeckoView:PageAction:OpenPopup");
        }

        public void setActionDelegate(final WebExtension webExtension,
                                      final WebExtension.ActionDelegate delegate) {
            mActionDelegates.put(webExtension.id, delegate);
        }

        public WebExtension.ActionDelegate getActionDelegate(final WebExtension webExtension) {
            return mActionDelegates.get(webExtension.id);
        }

        public void setMessageDelegate(final WebExtension webExtension,
                                       final WebExtension.MessageDelegate delegate,
                                       final String nativeApp) {
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

            if ("GeckoView:WebExtension:Message".equals(event)
                    || "GeckoView:WebExtension:PortMessage".equals(event)
                    || "GeckoView:WebExtension:Connect".equals(event)
                    || "GeckoView:PageAction:Update".equals(event)
                    || "GeckoView:PageAction:OpenPopup".equals(event)
                    || "GeckoView:BrowserAction:Update".equals(event)
                    || "GeckoView:BrowserAction:OpenPopup".equals(event)) {
                runtime.getWebExtensionController()
                        .handleMessage(event, message, callback, mSession);
                return;
            } else if ("GeckoView:WebExtension:CloseTab".equals(event)) {
                runtime.getWebExtensionController().closeTab(message, callback, mSession);
                return;
            }
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
                mIconUris.put(intKey, bundle.getString(key));
            }
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
     * {@link GeckoSession#setWebExtensionActionDelegate}, while default values
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
                if (mExtension.actionDelegate == null) {
                    return;
                }

                GeckoResult<GeckoSession> popup =
                        mExtension.actionDelegate.onTogglePopup(mExtension, this);
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
     * {@link GeckoSession#setWebExtensionActionDelegate}.
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

    /**
     * Set the Action delegate for this WebExtension.
     *
     * This delegate will receive updates every time the default Action value
     * changes.
     *
     * To listen for {@link GeckoSession}-specific updates, use
     * {@link GeckoSession#setWebExtensionActionDelegate}
     *
     * @param delegate {@link ActionDelegate} that will receive updates.
     */
    @AnyThread
    public void setActionDelegate(final @Nullable ActionDelegate delegate) {
        actionDelegate = delegate;
    }
}
