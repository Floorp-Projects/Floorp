# Simple Example

As a tutorial, let's create a very simple example, with a couple of modules:

* a root (parent process) module to retrieve the current version of the browser

* a windowglobal (content process) module to retrieve the location of a given tab

Some concepts used here will not be explained in details. More documentation should follow to clarify those.

We will not use events in this example, only commands.

## Create a root `version` module

First let's create the root module.

```javascript
import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class VersionModule extends Module {
  destroy() {}

  getVersion() {
    return Services.appinfo.platformVersion;
  }
}

export const version = VersionModule;
```

All modules should extend Module.sys.mjs and must define a destroy method.
Each public method of a Module class will be exposed as a command for this module.
The name used to export the module class will be the public name of the module, used to call commands on it.

## Create a windowglobal `location` module

Let's create the second module.

```javascript
import { Module } from "chrome://remote/content/shared/messagehandler/Module.sys.mjs";

class LocationModule extends Module {
  #window;

  constructor(messageHandler) {
    super(messageHandler);

    // LocationModule will be a windowglobal module, so `messageHandler` will
    // be a WindowGlobalMessageHandler which comes with a few helpful getters
    // such as a `window` getter.
    this.#window = messageHandler.window;
  }

  destroy() {
    this.#window = null;
  }

  getLocation() {
    return this.#window.location.href;
  }
}

export const location = LocationModule;
```

We could simplify the module and simply write `getLocation` to return `this.messageHandler.window.location.href`, but this gives us the occasion to get a glimpse at the module constructor.

## Register the modules as Firefox modules

Before we register those modules for the MessageHandler framework, we need to register them as Firefox modules first. For the sake of simplicity, we can assume they are added under a new folder `remote/example`:

* `remote/example/modules/root/version.sys.mjs`

* `remote/example/modules/windowglobal/location.sys.mjs`

Register them in the jar.mn so that they can be loaded as any other Firefox module.

The paths contain the corresponding layer (root, windowglobal) only for clarity. We don't rely on this as a naming convention to actually load the modules so you could decide to organize your folders differently. However the name used to export the module's class (eg `location`) will be the official name of the module, used in commands and events, so pay attention and use the correct export name.

## Define a ModuleRegistry

We do need to instruct the framework where each module should be loaded however.

This is done via a ModuleRegistry. Without getting into too much details, each "set of modules" intended to work with the MessageHandler framework needs to provide a ModuleRegistry module which exports a single `getModuleClass` helper. This method will be called by the framework to know which modules are available. For now let's just define the simplest registry possible for us under `remote/example/modules/root/ModuleRegistry.sys.mjs`

```javascript
export const getModuleClass = function(moduleName, moduleFolder) {
  if (moduleName === "version" && moduleFolder === "root") {
    return ChromeUtils.importESModule(
      "chrome://remote/content/example/modules/root/version.sys.mjs"
    ).version;
  }
  if (moduleName === "location" && moduleFolder === "windowglobal") {
    return ChromeUtils.importESModule(
      "chrome://remote/content/example/modules/windowglobal/location.sys.mjs"
    ).location;
  }
  return null;
};
```

Note that this can (and should) be improved by defining some naming conventions or patterns, but for now each set of modules is really free to implement this logic as needed.

Add this module to jar.mn as well so that it becomes a valid Firefox module.

### Temporary workaround to use the custom ModuleRegistry

With this we have a set of modules which is almost ready to use. Except that for now MessageHandler is hardcoded to use WebDriver BiDi modules only. Once [Bug 1722464](https://bugzilla.mozilla.org/show_bug.cgi?id=1722464) is fixed we will be able to specify other protocols, but at the moment, the only way to instruct the MessageHandler framework to use non-bidi modules is to update the [following line](https://searchfox.org/mozilla-central/rev/08f7e9ef03dd2a83118fba6768d1143d809f5ebe/remote/shared/messagehandler/ModuleCache.sys.mjs#25) to point to `remote/example/modules/ModuleRegistry.sys.mjs`.

Now with this, you should be able to create a MessageHandler network and use your modules.

## Try it out

For instance, you can open the Browser Console and run the following snippet:

```javascript
(async function() {
  const { RootMessageHandlerRegistry } = ChromeUtils.importESModule(
    "chrome://remote/content/shared/messagehandler/RootMessageHandlerRegistry.sys.mjs"
  );
  const messageHandler = RootMessageHandlerRegistry.getOrCreateMessageHandler("test-session");
  const version = await messageHandler.handleCommand({
    moduleName: "version",
    commandName: "getVersion",
    params: {},
    destination: {
      type: "ROOT",
    },
  });
  console.log({ version });

  const location = await messageHandler.handleCommand({
    moduleName: "location",
    commandName: "getLocation",
    params: {},
    destination: {
      type: "WINDOW_GLOBAL",
      id: gBrowser.selectedBrowser.browsingContext.id,
    },
  });
  console.log({ location });
})();
```

This should print a version number `{ version: "109.0a1" }` and a location `{ location: "https://www.mozilla.org/en-US/" }` (actual values should of course be different for you).

We are voluntarily skipping detailed explanations about the various parameters passed to `handleCommand`, as well as about the `RootMessageHandlerRegistry`, but this should give you some idea already of how you can start creating modules and using them.
