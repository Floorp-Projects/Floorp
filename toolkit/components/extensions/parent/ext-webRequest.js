/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  WebRequest: "resource://gre/modules/WebRequest.sys.mjs",
});

var { parseMatchPatterns } = ExtensionUtils;

// The guts of a WebRequest event handler.  Takes care of converting
// |details| parameter when invoking listeners.
function registerEvent(
  extension,
  eventName,
  fire,
  filter,
  info,
  remoteTab = null
) {
  let listener = async data => {
    let event = data.serialize(eventName);
    if (data.registerTraceableChannel) {
      // If this is a primed listener, no tabParent was passed in here,
      // but the convert() callback later in this function will be called
      // when the background page is started.  Force that to happen here
      // after which we'll have a valid tabParent.
      if (fire.wakeup) {
        await fire.wakeup();
      }
      data.registerTraceableChannel(extension.policy, remoteTab);
    }

    return fire.sync(event);
  };

  let filter2 = {};
  if (filter.urls) {
    let perms = new MatchPatternSet([
      ...extension.allowedOrigins.patterns,
      ...extension.optionalOrigins.patterns,
    ]);

    filter2.urls = parseMatchPatterns(filter.urls);

    if (!perms.overlapsAll(filter2.urls)) {
      Cu.reportError(
        "The webRequest.addListener filter doesn't overlap with host permissions."
      );
    }
  }
  if (filter.types) {
    filter2.types = filter.types;
  }
  if (filter.tabId !== undefined) {
    filter2.tabId = filter.tabId;
  }
  if (filter.windowId !== undefined) {
    filter2.windowId = filter.windowId;
  }
  if (filter.incognito !== undefined) {
    filter2.incognito = filter.incognito;
  }

  let blockingAllowed = extension.hasPermission("webRequestBlocking");

  let info2 = [];
  if (info) {
    for (let desc of info) {
      if (desc == "blocking" && !blockingAllowed) {
        // This is usually checked in the child process (based on the API schemas, where these options
        // should be checked with the "webRequestBlockingPermissionRequired" postprocess property),
        // but it is worth to also check it here just in case a new webRequest has been added and
        // it has not yet using the expected postprocess property).
        Cu.reportError(
          "Using webRequest.addListener with the blocking option " +
            "requires the 'webRequestBlocking' permission."
        );
      } else {
        info2.push(desc);
      }
    }
  }

  let listenerDetails = {
    addonId: extension.id,
    policy: extension.policy,
    blockingAllowed,
  };
  WebRequest[eventName].addListener(listener, filter2, info2, listenerDetails);

  return {
    unregister: () => {
      WebRequest[eventName].removeListener(listener);
    },
    convert(_fire, context) {
      fire = _fire;
      remoteTab = context.xulBrowser.frameLoader.remoteTab;
    },
  };
}

function makeWebRequestEventAPI(context, event, extensionApi) {
  return new EventManager({
    context,
    module: "webRequest",
    event,
    extensionApi,
  }).api();
}

function makeWebRequestEventRegistrar(event) {
  return function ({ fire, context }, params) {
    // ExtensionAPIPersistent makes sure this function will be bound
    // to the ExtensionAPIPersistent instance.
    const { extension } = this;

    const [filter, info] = params;

    // When we are registering the real listener coming from the extension context,
    // we should get the additional remoteTab parameter value from the extension context
    // (which is then used by the registerTraceableChannel helper to register stream
    // filters to the channel and associate them to the extension context that has
    // created it and will be handling the filter onstart/ondata/onend events).
    let remoteTab;
    if (context) {
      remoteTab = context.xulBrowser.frameLoader.remoteTab;
    }

    return registerEvent(extension, event, fire, filter, info, remoteTab);
  };
}

this.webRequest = class extends ExtensionAPIPersistent {
  primeListener(event, fire, params, isInStartup) {
    // During early startup if the listener does not use blocking we do not prime it.
    if (!isInStartup || params[1]?.includes("blocking")) {
      return super.primeListener(event, fire, params, isInStartup);
    }
  }

  PERSISTENT_EVENTS = {
    onBeforeRequest: makeWebRequestEventRegistrar("onBeforeRequest"),
    onBeforeSendHeaders: makeWebRequestEventRegistrar("onBeforeSendHeaders"),
    onSendHeaders: makeWebRequestEventRegistrar("onSendHeaders"),
    onHeadersReceived: makeWebRequestEventRegistrar("onHeadersReceived"),
    onAuthRequired: makeWebRequestEventRegistrar("onAuthRequired"),
    onBeforeRedirect: makeWebRequestEventRegistrar("onBeforeRedirect"),
    onResponseStarted: makeWebRequestEventRegistrar("onResponseStarted"),
    onErrorOccurred: makeWebRequestEventRegistrar("onErrorOccurred"),
    onCompleted: makeWebRequestEventRegistrar("onCompleted"),
  };

  getAPI(context) {
    return {
      webRequest: {
        onBeforeRequest: makeWebRequestEventAPI(
          context,
          "onBeforeRequest",
          this
        ),
        onBeforeSendHeaders: makeWebRequestEventAPI(
          context,
          "onBeforeSendHeaders",
          this
        ),
        onSendHeaders: makeWebRequestEventAPI(context, "onSendHeaders", this),
        onHeadersReceived: makeWebRequestEventAPI(
          context,
          "onHeadersReceived",
          this
        ),
        onAuthRequired: makeWebRequestEventAPI(context, "onAuthRequired", this),
        onBeforeRedirect: makeWebRequestEventAPI(
          context,
          "onBeforeRedirect",
          this
        ),
        onResponseStarted: makeWebRequestEventAPI(
          context,
          "onResponseStarted",
          this
        ),
        onErrorOccurred: makeWebRequestEventAPI(
          context,
          "onErrorOccurred",
          this
        ),
        onCompleted: makeWebRequestEventAPI(context, "onCompleted", this),
        getSecurityInfo: function (requestId, options = {}) {
          return WebRequest.getSecurityInfo({
            id: requestId,
            policy: context.extension.policy,
            remoteTab: context.xulBrowser.frameLoader.remoteTab,
            options,
          });
        },
        handlerBehaviorChanged: function () {
          // TODO: Flush all caches.
        },
      },
    };
  }
};
