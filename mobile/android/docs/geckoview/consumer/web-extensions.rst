.. -*- Mode: rst; fill-column: 80; -*-

============================
Interacting with Web content
============================

Interacting with Web content and WebExtensions
==============================================

GeckoView allows embedder applications to register and run
`WebExtensions <https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions>`_
in a GeckoView instance. WebExtensions are the preferred way to interact
with Web content.

.. contents:: :local:

Running WebExtensions in GeckoView
----------------------------------

WebExtensions bundled with applications can be provided either in
compressed ``.xpi`` files or in regular folders. Like ordinary
WebExtensions, every WebExtension requires a
`manifest.json <https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json>`_
file.

To run a WebExtension in GeckoView, simply create a
`WebExtension`_
object and register it in your
`GeckoRuntime`_
instance.

.. code:: java

   WebExtension extension = new WebExtension(
       // The location where the web extension is installed, if the location is a
       // folder make sure the path ends with a "/" character.
       "resource://android/assets/messaging/",
       // This is the id and must be unique for every web extension
       "myextension@example.com",
       // Extra flags can be specified here
       WebExtension.Flags.NONE);

   // Run the WebExtension
   runtime.registerWebExtension(extension);

Note that the lifetime of the WebExtension is tied with the lifetime of
the
`GeckoRuntime`_
instance. The WebExtension will need to be registered every time the
runtime is created and will not persist once the runtime is closed.

To locate files bundled with the APK, GeckoView provides a shorthand
``resource://android/`` that points to the root of the APK.

E.g. ``resource://android/assets/messaging/`` will point to the
``/assets/messaging/`` folder present in the APK.

Communicating with Web Content
------------------------------

GeckoView allows bidirectional communication with Web pages through
WebExtensions.

When using GeckoView, `native
messaging <https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Native_messaging#Exchanging_messages>`_
can be used for communicating to and from the browser. 

- `runtime.sendNativeMessage`_ for one-off messages. 
- `runtime.connectNative <https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/connectNative>`_ for connection-based messaging.

Note: these APIs are only available when the ``geckoViewAddons``
`permission <https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/manifest.json/permissions>`_
is present in the ``manifest.json`` file of the WebExtension.

One-off messages
~~~~~~~~~~~~~~~~

The easiest way to send messages from a `content
script <https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Content_scripts>`_
or a `background
script <https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/Anatomy_of_a_WebExtension#Background_scripts>`_
is using
`runtime.sendNativeMessage`_.
The app will set up a message delegate on the same ``nativeApp`` that
the WebExtension is using to send messages. In our example we will use
the ``"browser"`` native app identifier.

To receive messages from the background script, call
`setMessageDelegate <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html#setMessageDelegate-org.mozilla.geckoview.WebExtension.MessageDelegate-java.lang.String->`_
on the
`WebExtension`_
object.

`GeckoSession.setMessageDelegate <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoSession.html#setMessageDelegate-org.mozilla.geckoview.WebExtension.MessageDelegate-java.lang.String->`_
allows the app to receive messages from content scripts.

Note: WebExtensions can only send messages from content scripts if
explicitly authorized by the app setting
`WebExtension.Flags.ALLOW_CONTENT_MESSAGING <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.Flags.html#ALLOW_CONTENT_MESSAGING>`_
in the
`constructor <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html#WebExtension-java.lang.String-java.lang.String-long->`_.

Example
~~~~~~~

Let’s set up an activity that registers a WebExtension located in the
``/assets/messaging/`` folder of the APK. This activity will set up a
`MessageDelegate <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.MessageDelegate.html>`_
that will be used to communicate with Web Content.

You can find the full example here:
`MessagingExample <https://searchfox.org/mozilla-central/source/mobile/android/examples/extensions/messaging_example/>`_.

Activity.java
^^^^^^^^^^^^^

.. code:: java

   WebExtension.MessageDelegate messageDelegate = new WebExtension.MessageDelegate() {
       @Nullable
       public GeckoResult<Object> onMessage(final @NonNull Object message,
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

   WebExtension extension = new WebExtension(
       "resource://android/assets/messaging/",
       "myextension@example.com",
       WebExtension.Flags.ALLOW_CONTENT_MESSAGING);

   // Run the WebExtension
   runtime.registerWebExtension(extension);

   // Set the delegate that will receive messages coming from this WebExtension.
   session.setMessageDelegate(messageDelegate, "browser");

Now add the ``geckoViewAddons`` and ``nativeMessaging`` permissions to
your ``manifest.json`` file.

/assets/messaging/manifest.json
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: json

   {
     "manifest_version": 2,
     "name": "messaging",
     "version": "1.0",
     "description": "Example messaging web extension.",
     "content_scripts": [
       {
         "matches": ["*://*.twitter.com/*"],
         "js": ["messaging.js"]
       }
     ],
     "permissions": [
       "nativeMessaging",
       "geckoViewAddons"
     ]
   }

And finally, write a content script that will send a message to the app
when a certain event occurs. For example, you could send a message
whenever a `WPA
manifest <https://developer.mozilla.org/en-US/docs/Web/Manifest>`_ is
found on the page. Note that our ``nativeApp`` identifier used for
``sendNativeMessage`` is the same as the one used in the
``setMessageDelegate`` call in `Activity.java <#activityjava>`_.

/assets/messaging/messaging.js
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code:: javascript

   let manifest = document.querySelector("head > link[rel=manifest]");
   if (manifest) {
        fetch(manifest.href)
           .then(response => response.json())
           .then(json => {
                let message = {type: "WPAManifest", manifest: json};
                browser.runtime.sendNativeMessage("browser", message);
           });
   }

You can handle this message in the ``onMessage`` method in the
``messageDelegate`` `above <#activityjava>`_.

.. code:: java

   @Nullable
   public GeckoResult<Object> onMessage(final @NonNull Object message,
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

Note that, in the case of content scripts, ``sender.session`` will be a
reference to the ``GeckoSession`` instance from which the message
originated. For background scripts, ``sender.session`` will always be
``null``.

Also note that the type of ``message`` will depend on what was sent from
the WebExtension.

The type of ``message`` will be ``JSONObject`` when the WebExtension
sends a javascript object, but could also be a primitive type if the
WebExtension sends one, e.g. for

.. code:: javascript

   runtime.browser.sendNativeMessage("browser", "Hello World!");

the type of ``message`` will be ``java.util.String``.

Connection-based messaging
--------------------------

For more complex scenarios or for when you want to send messages *from*
the app to the WebExtension,
`runtime.connectNative <https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/connectNative>`_
is the appropriate API to use.

``connectNative`` returns a
`runtime.Port <https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/Port>`_
that can be used to send messages to the app. On the app side,
implementing
`MessageDelegate#onConnect <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.MessageDelegate.html#onConnect-org.mozilla.geckoview.WebExtension.Port->`_
will allow you to receive a
`Port <https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.Port.html>`_
object that can be used to receive and send messages to the
WebExtension.

The following example can be found
`here <https://searchfox.org/mozilla-central/source/mobile/android/examples/extensions/port_messaging_example/>`_.

For this example, the WebExtension side will do the following: 

- open a port on the background script using ``connectNative`` 
- listen on the port and log to console every message received 
- send a message immediately after opening the port.

/assets/messaging/background.js
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. code:: javascript

   // Establish connection with app
   let port = browser.runtime.connectNative("browser");
   port.onMessage.addListener(response => {
       // Let's just echo the message back
       port.postMessage(`Received: ${JSON.stringify(response)}`);
   });
   port.postMessage("Hello from WebExtension!");

On the app side, following the `above <#activityjava>`_ example,
``onConnect`` will be storing the ``Port`` object in a member variable
and then using it when needed.

.. code:: java

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
               // WebExtension through this port. For now, let's just log a
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
       // WebExtension
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

       WebExtension extension = new WebExtension(
               "resource://android/assets/messaging/");

       // Register message delegate for the background script
       extension.setMessageDelegate(messageDelegate, "browser");

       // ... other
   }

For example, let’s send a message to the WebExtension every time the
user long presses on a key on the virtual keyboard, e.g. on the back
button.

.. code:: java

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

This allows bidirectional communication between the app and the
WebExtension.

.. _GeckoRuntime: https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/GeckoRuntime.html
.. _runtime.sendNativeMessage: https://developer.mozilla.org/en-US/docs/Mozilla/Add-ons/WebExtensions/API/runtime/sendNativeMessage
.. _WebExtension: https://mozilla.github.io/geckoview/javadoc/mozilla-central/org/mozilla/geckoview/WebExtension.html
