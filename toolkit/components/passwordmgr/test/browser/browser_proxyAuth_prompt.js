/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use-strict";

let proxyChannel;

function initProxy() {
  return new Promise(resolve => {
    let proxyChannel;

    let proxyCallback = {
      QueryInterface: ChromeUtils.generateQI(["nsIProtocolProxyCallback"]),

      onProxyAvailable(req, uri, pi, status) {
        class ProxyChannelListener {
          onStartRequest(request) {
            resolve(proxyChannel);
          }
          onStopRequest(request, status) {}
        }
        // I'm cheating a bit here... We should probably do some magic foo to get
        // something implementing nsIProxiedProtocolHandler and then call
        // NewProxiedChannel(), so we have something that's definately a proxied
        // channel. But Mochitests use a proxy for a number of hosts, so just
        // requesting a normal channel will give us a channel that's proxied.
        // The proxyChannel needs to move to at least on-modify-request to
        // have valid ProxyInfo, but we use OnStartRequest during startup()
        // for simplicity.
        proxyChannel = Services.io.newChannel(
          "http://mochi.test:8888",
          null,
          null,
          null, // aLoadingNode
          Services.scriptSecurityManager.getSystemPrincipal(),
          null, // aTriggeringPrincipal
          Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
          Ci.nsIContentPolicy.TYPE_OTHER
        );
        proxyChannel.asyncOpen(new ProxyChannelListener());
      },
    };

    // Need to allow for arbitrary network servers defined in PAC instead of a hardcoded moz-proxy.
    let pps = Cc["@mozilla.org/network/protocol-proxy-service;1"].getService();

    let channel = Services.io.newChannel(
      "http://example.com",
      null,
      null,
      null, // aLoadingNode
      Services.scriptSecurityManager.getSystemPrincipal(),
      null, // aTriggeringPrincipal
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      Ci.nsIContentPolicy.TYPE_OTHER
    );
    pps.asyncResolve(channel, 0, proxyCallback);
  });
}

add_task(async function setup() {
  await SpecialPowers.pushPrefEnv({
    // This test relies on tab auth prompts.
    set: [["prompts.modalType.httpAuth", Services.prompt.MODAL_TYPE_TAB]],
  });
  proxyChannel = await initProxy();
});

/**
 * Create an object for consuming an nsIAuthPromptCallback.
 * @returns result
 * @returns {nsIAuthPromptCallback} result.callback - Callback to be passed into
 * asyncPromptAuth.
 * @returns {Promise} result.promise - Promise which resolves with authInfo once
 * the callback has been called.
 */
function getAuthPromptCallback() {
  let callbackResolver;
  let promise = new Promise(resolve => {
    callbackResolver = resolve;
  });
  let callback = {
    onAuthAvailable(context, authInfo) {
      callbackResolver(authInfo);
    },
  };
  return { callback, promise };
}

/**
 * Tests that if a window proxy auth prompt is open, subsequent auth calls with
 * matching realm will be merged into the existing prompt. This should work even
 * if the follwing auth call has browser parent.
 */
add_task(async function testProxyAuthPromptMerge() {
  let tabA = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.com"
  );
  let tabB = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "https://example.org"
  );

  const promptFac = Cc[
    "@mozilla.org/passwordmanager/authpromptfactory;1"
  ].getService(Ci.nsIPromptFactory);
  let prompter = promptFac.getPrompt(window, Ci.nsIAuthPrompt2);

  let level = Ci.nsIAuthPrompt2.LEVEL_NONE;
  let proxyAuthinfo = {
    username: "",
    password: "",
    domain: "",
    flags: Ci.nsIAuthInformation.AUTH_PROXY,
    authenticationScheme: "basic",
    realm: "",
  };

  // The next prompt call will result in window prompt, because the prompt does
  // not have a browser set yet. The browser is used as a parent for tab
  // prompts.
  let promptOpened = PromptTestUtils.waitForPrompt(null, {
    modalType: Services.prompt.MODAL_TYPE_WINDOW,
  });
  let cbWinPrompt = getAuthPromptCallback();
  info("asyncPromptAuth no parent");
  prompter.asyncPromptAuth(
    proxyChannel,
    cbWinPrompt.callback,
    null,
    level,
    proxyAuthinfo
  );
  let prompt = await promptOpened;

  // Set a browser so the next prompt would open as tab prompt.
  prompter.QueryInterface(Ci.nsILoginManagerAuthPrompter).browser =
    tabA.linkedBrowser;

  // Since we already have an open window prompts, subsequent calls with
  // matching realm should be merged into this prompt and no additional prompts
  // must be spawned.
  let cbNoPrompt = getAuthPromptCallback();
  info("asyncPromptAuth tabA parent");
  prompter.asyncPromptAuth(
    proxyChannel,
    cbNoPrompt.callback,
    null,
    level,
    proxyAuthinfo
  );

  // Switch to the next tabs browser.
  prompter.QueryInterface(Ci.nsILoginManagerAuthPrompter).browser =
    tabB.linkedBrowser;

  let cbNoPrompt2 = getAuthPromptCallback();
  info("asyncPromptAuth tabB parent");
  prompter.asyncPromptAuth(
    proxyChannel,
    cbNoPrompt2.callback,
    null,
    level,
    proxyAuthinfo
  );

  // Accept the prompt.
  PromptTestUtils.handlePrompt(prompt, {});

  // Accepting the window prompts should complete all auth requests.
  let authInfo1 = await cbWinPrompt.promise;
  ok(authInfo1, "Received callback from first proxy auth call.");
  let authInfo2 = await cbNoPrompt.promise;
  ok(authInfo2, "Received callback from second proxy auth call.");
  let authInfo3 = await cbNoPrompt2.promise;
  ok(authInfo3, "Received callback from third proxy auth call.");

  BrowserTestUtils.removeTab(tabA);
  BrowserTestUtils.removeTab(tabB);
});
