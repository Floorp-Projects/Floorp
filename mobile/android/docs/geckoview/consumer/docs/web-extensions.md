---
layout: default
title: Interacting with Web content
summary: How to interact with Web content and register WebExtensions in GeckoView.
nav_exclude: true
exclude: true
---
# Interacting with Web content and WebExtensions

GeckoView allows embedder applications to register and run [WebExtensions][8]
in a GeckoView instance. Extensions are the preferred way to interact with
Web content.

## Running extensions in GeckoView

Extensions bundled with applications can be provided in a folder in the
`/assets` section of the APK. Like ordinary extensions, every extension bundled
with GeckoView requires a [`manifest.json`][23] file.

To locate files bundled with the APK, GeckoView provides a shorthand
`resource://android/` that points to the root of the APK.

E.g. `resource://android/assets/messaging/` will point to the
`/assets/messaging/` folder present in the APK.

Note: Every installed extension will need an [`id`][25] and [`version`][26]
specified in the `manifest.json` file.

To install a bundled extension in GeckoView, simply call
[`WebExtensionController.installBuilt`][3].

```java
runtime.getWebExtensionController()
  .installBuiltIn("resource://android/assets/messaging/")
```

Note that the lifetime of the extension is not tied with the lifetime of the
[`GeckoRuntime`][24] instance. The extension persist even when your app is
restarted. Installing at every start up is fine, but it could be slow. To avoid
installing multiple times you can query the list of installed extensions using
`WebExtensionRuntime.list`:

```java
runtime.getWebExtensionController().list().then(extensionList -> {
    for (WebExtension extension : extensionList) {
        if (extension.id.equals("messaging@example.com")
                && extension.metaData.version.equals("1.0")) {
            // Extension already installed, no need to install it again
            return GeckoResult.fromValue(extension);
        }
    }

    // Install if it's not already installed.
    return runtime.getWebExtensionController()
        .installBuiltIn("resource://android/assets/messaging/");
}).accept(
        extension -> Log.i("MessageDelegate", "Extension installed: " + extension),
        e -> Log.e("MessageDelegate", "Error registering WebExtension", e)
);
```

## Communicating with Web Content
GeckoView allows bidirectional communication with Web pages through
extensions.

When using GeckoView, [native messaging][11] can be used for communicating to
and from the browser.
* [`runtime.sendNativeMessage`][12] for one-off messages.
* [`runtime.connectNative`][13] for connection-based messaging.

Note: these APIs are only available when the `geckoViewAddons` [permission][17]
is present in the `manifest.json` file of the extension.

### One-off messages

The easiest way to send messages from a [content script][9] or a [background
script][10] is using [`runtime.sendNativeMessage`][12]. The app will set up a
message delegate on the same `nativeApp` that the extension is using to send
messages. In our example we will use the `"browser"` native app identifier.

To receive messages from the background script, call [`setMessageDelegate`][14]
on the [`WebExtension`][3] object.

```java
extension.setMessageDelegate(messageDelegate, "browser");
```

[`SessionController.setMessageDelegate`][16] allows the app to
receive messages from content scripts.

```java
session.getWebExtensionController()
    .setMessageDelegate(extension, messageDelegate, "browser");
```

Note: extension can only send messages from content scripts if explicitly
authorized by the app by adding `nativeMessagingFromContent` in the
manifest.json file, e.g.

```json
  "permissions": [
    "nativeMessaging",
    "nativeMessagingFromContent",
    "geckoViewAddons"
  ]
```

### Example

Let's set up an activity that registers an extension located in the
`/assets/messaging/` folder of the APK. This activity will set up a
[`MessageDelegate`][18] that will be used to communicate with Web Content.

You can find the full example here: [MessagingExample][20].

##### Activity.java
```java
WebExtension.MessageDelegate messageDelegate = new WebExtension.MessageDelegate() {
    @Nullable
    public GeckoResult<Object> onMessage(final @NonNull String nativeApp,
                                         final @NonNull Object message,
                                         final @NonNull WebExtension.MessageSender sender) {
        // The sender object contains information about the session that
        // originated this message and can be used to validate that the message
        // has been sent from the expected location.

        // Be careful when handling the type of message as it depends on what
        // type of object was sent from the WebExtension script.
        if (message instanceof JSONObject) {
            // Do something with message
        }
        return null;
    }
};

// Let's check if the extension is already installed first
runtime.getWebExtensionController().list().then(extensionList -> {
    for (WebExtension extension : extensionList) {
        if (extension.id.equals(EXTENSION_ID)
                && extension.metaData.version.equals(EXTENSION_VERSION)) {
            // Extension already installed, no need to install it again
            return GeckoResult.fromValue(extension);
        }
    }

    // Install if it's not already installed.
    return runtime.getWebExtensionController().installBuiltIn(EXTENSION_LOCATION);
}).accept(
        // Set the delegate that will receive messages coming from this WebExtension.
        extension -> session.getWebExtensionController()
                .setMessageDelegate(extension, messageDelegate, "browser"),
        // Something bad happened, let's log an error
        e -> Log.e("MessageDelegate", "Error registering WebExtension", e)
);
```

Now add the `geckoViewAddons`, `nativeMessaging` and
`nativeMessagingFromContent` permissions to your `manifest.json` file.

##### /assets/messaging/manifest.json
```json
{
  "manifest_version": 2,
  "name": "messaging",
  "version": "1.0",
  "description": "Example messaging web extension.",
  "browser_specific_settings": {
    "gecko": {
      "id": "messaging@example.com"
    }
  },
  "content_scripts": [
    {
      "matches": ["*://*.twitter.com/*"],
      "js": ["messaging.js"]
    }
  ],
  "permissions": [
    "nativeMessaging",
    "nativeMessagingFromContent",
    "geckoViewAddons"
  ]
}
```

And finally, write a content script that will send a message to the app when a
certain event occurs. For example, you could send a message whenever a [WPA
manifest][19] is found on the page. Note that our `nativeApp` identifier used
for `sendNativeMessage` is the same as the one used in the `setMessageDelegate`
call in [`Activity.java`](#activityjava).

##### /assets/messaging/messaging.js
```javascript
let manifest = document.querySelector("head > link[rel=manifest]");
if (manifest) {
     fetch(manifest.href)
        .then(response => response.json())
        .then(json => {
             let message = {type: "WPAManifest", manifest: json};
             browser.runtime.sendNativeMessage("browser", message);
        });
}
```

You can handle this message in the `onMessage` method in the `messageDelegate`
[above](#activityjava).

```java
@Nullable
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
```

Note that, in the case of content scripts, `sender.session` will be a reference
to the `GeckoSession` instance from which the message originated. For
background scripts, `sender.session` will always be `null`.

Also note that the type of `message` will depend on what was sent from the
extension.

The type of `message` will be `JSONObject` when the extension sends a
javascript object, but could also be a primitive type if the extension sends
one, e.g. for

```javascript
runtime.browser.sendNativeMessage("browser", "Hello World!");
```

the type of `message` will be `java.util.String`.

## Connection-based messaging

For more complex scenarios or for when you want to send messages _from_ the app
to the extension, [`runtime.connectNative`][13] is the appropriate API to
use.

`connectNative` returns a [`runtime.Port`][6] that can be used to send messages
to the app. On the app side, implementing [`MessageDelegate#onConnect`][1] will
allow you to receive a [`Port`][7] object that can be used to receive and send
messages to the extension.

The following example can be found [here][21].

For this example, the extension side will do the following:
  * open a port on the background script using `connectNative`
  * listen on the port and log to console every message received
  * send a message immediately after opening the port.

##### /assets/messaging/background.js
```javascript
// Establish connection with app
let port = browser.runtime.connectNative("browser");
port.onMessage.addListener(response => {
    // Let's just echo the message back
    port.postMessage(`Received: ${JSON.stringify(response)}`);
});
port.postMessage("Hello from WebExtension!");
```

On the app side, following the [above](#activityjava) example, `onConnect` will
be storing the `Port` object in a member variable and then using it when
needed.

```java
private WebExtension.Port mPort;

@Override
protected void onCreate(Bundle savedInstanceState) {
    // ... initialize GeckoView

    // This delegate will handle all communications from and to a specific Port
    // object
    WebExtension.PortDelegate portDelegate = new WebExtension.PortDelegate() {
        public WebExtension.Port port = null;

        public void onPortMessage(final @NonNull Object message,
                                  final @NonNull WebExtension.Port port) {
            // This method will be called every time a message is sent from the
            // extension through this port. For now, let's just log a
            // message.
            Log.d("PortDelegate", "Received message from WebExtension: "
                    + message);
        }

        public void onDisconnect(final @NonNull WebExtension.Port port) {
            // After this method is called, this port is not usable anymore.
            if (port == mPort) {
                mPort = null;
            }
        }
    };

    // This delegate will handle requests to open a port coming from the
    // extension
    WebExtension.MessageDelegate messageDelegate = new WebExtension.MessageDelegate() {
        @Nullable
        public void onConnect(final @NonNull WebExtension.Port port) {
            // Let's store the Port object in a member variable so it can be
            // used later to exchange messages with the WebExtension.
            mPort = port;

            // Registering the delegate will allow us to receive messages sent
            // through this port.
            mPort.setDelegate(portDelegate);
        }
    };

    runtime.getWebExtensionController()
        .installBuiltIn("resource://android/assets/messaging/")
        .accept(
            // Register message delegate for background script
            extension -> extension.setMessageDelegate(messageDelegate, "browser"),
            e -> Log.e("MessageDelegate", "Error registering WebExtension", e)
        );

    // ... other
}
```

For example, let's send a message to the extension every time the user long
presses on a key on the virtual keyboard, e.g. on the back button.

```java
@Override
public boolean onKeyLongPress(int keyCode, KeyEvent event) {
    if (mPort == null) {
        // No extension registered yet, let's ignore this message
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
```

This allows bidirectional communication between the app and the extension.

[1]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.MessageDelegate.html#onConnect-org.mozilla.geckoview.WebExtension.Port-
[3]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html
[6]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port
[7]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.Port.html
[8]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions
[9]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Content_scripts
[10]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Anatomy_of_a_WebExtension#Background_scripts
[11]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging#Exchanging_messages
[12]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/sendNativeMessage
[13]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/connectNative
[14]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html#setMessageDelegate-org.mozilla.geckoview.WebExtension.MessageDelegate-java.lang.String-
[15]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html#WebExtension-java.lang.String-java.lang.String-long-
[16]: ../javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.html#setMessageDelegate-org.mozilla.geckoview.WebExtension.MessageDelegate-java.lang.String-
[17]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions
[18]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.MessageDelegate.html
[19]: https://developer.mozilla.org/en-US/docs/Web/Manifest
[20]: https://searchfox.org/mozilla-central/source/mobile/android/examples/extensions/messaging_example/
[21]: https://searchfox.org/mozilla-central/source/mobile/android/examples/extensions/port_messaging_example/
[22]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.Flags.html#ALLOW_CONTENT_MESSAGING
[23]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json
[24]: {{ site.url }}{{ site.baseurl }}/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html
[25]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/browser_specific_settings
[26]: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/version
