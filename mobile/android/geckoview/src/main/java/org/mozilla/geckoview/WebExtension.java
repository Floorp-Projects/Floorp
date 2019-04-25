package org.mozilla.geckoview;

import android.support.annotation.IntDef;
import android.support.annotation.LongDef;
import android.support.annotation.NonNull;
import android.support.annotation.Nullable;
import android.support.annotation.UiThread;
import android.util.Log;

import org.json.JSONException;
import org.json.JSONObject;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.GeckoBundle;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.HashMap;
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

    private final static String LOGTAG = "WebExtension";

    public static class Flags {
        /*
         * Default flags for this WebExtension.
         */
        public static final long NONE = 0;
        /**
         * Set this flag if you want to enable content scripts messaging.
         * To listen to such messages you can use
         * {@link WebExtension#setMessageDelegate}.
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
        default GeckoResult<Object> onMessage(final @NonNull Object message,
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

    private static final MessageDelegate NULL_MESSAGE_DELEGATE = new MessageDelegate() {
        @Override
        public GeckoResult<Object> onMessage(final @NonNull Object message,
                                             final @NonNull MessageSender sender) {
            Log.d(LOGTAG, "Unhandled message from " +
                    sender.webExtension.id + ": " + message.toString());
            return null;
        }

        @Override
        public void onConnect(final @NonNull Port port) {
            Log.d(LOGTAG, "Unhandled connect request from " +
                    port.sender.webExtension.id);
        }
    };
}
