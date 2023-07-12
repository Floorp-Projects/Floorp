/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  Log: "chrome://remote/content/shared/Log.sys.mjs",
  HTTP_404: "chrome://remote/content/server/httpd.sys.mjs",
  HTTP_405: "chrome://remote/content/server/httpd.sys.mjs",
  HTTP_500: "chrome://remote/content/server/httpd.sys.mjs",
  Protocol: "chrome://remote/content/cdp/Protocol.sys.mjs",
  RemoteAgentError: "chrome://remote/content/cdp/Error.sys.mjs",
  TabManager: "chrome://remote/content/shared/TabManager.sys.mjs",
});

export class JSONHandler {
  constructor(cdp) {
    this.cdp = cdp;
    this.routes = {
      "/json/version": {
        handler: this.getVersion.bind(this),
      },

      "/json/protocol": {
        handler: this.getProtocol.bind(this),
      },

      "/json/list": {
        handler: this.getTargetList.bind(this),
      },

      "/json": {
        handler: this.getTargetList.bind(this),
      },

      // PUT only - /json/new?{url}
      "/json/new": {
        handler: this.newTarget.bind(this),
        method: "PUT",
      },

      // /json/activate/{targetId}
      "/json/activate": {
        handler: this.activateTarget.bind(this),
        parameter: true,
      },

      // /json/close/{targetId}
      "/json/close": {
        handler: this.closeTarget.bind(this),
        parameter: true,
      },
    };
  }

  getVersion() {
    const mainProcessTarget = this.cdp.targetList.getMainProcessTarget();

    const { userAgent } = Cc[
      "@mozilla.org/network/protocol;1?name=http"
    ].getService(Ci.nsIHttpProtocolHandler);

    return {
      body: {
        Browser: `${Services.appinfo.name}/${Services.appinfo.version}`,
        "Protocol-Version": "1.3",
        "User-Agent": userAgent,
        "V8-Version": "1.0",
        "WebKit-Version": "1.0",
        webSocketDebuggerUrl: mainProcessTarget.toJSON().webSocketDebuggerUrl,
      },
    };
  }

  getProtocol() {
    return { body: lazy.Protocol.Description };
  }

  getTargetList() {
    return { body: [...this.cdp.targetList].filter(x => x.type !== "browser") };
  }

  /** HTTP copy of Target.createTarget() */
  async newTarget(url) {
    const onTarget = this.cdp.targetList.once("target-created");

    // Open new tab
    const tab = await lazy.TabManager.addTab({
      focus: true,
    });

    // Get the newly created target
    const target = await onTarget;
    if (tab.linkedBrowser != target.browser) {
      throw new Error(
        "Unexpected tab opened: " + tab.linkedBrowser.currentURI.spec
      );
    }

    const returnJson = target.toJSON();

    // Load URL if given, otherwise stay on about:blank
    if (url) {
      let validURL;
      try {
        validURL = Services.io.newURI(url);
      } catch {
        // If we failed to parse given URL, return now since we already loaded about:blank
        return { body: returnJson };
      }

      target.browsingContext.loadURI(validURL, {
        triggeringPrincipal:
          Services.scriptSecurityManager.getSystemPrincipal(),
      });

      // Force the URL in the returned target JSON to match given
      // even if loading/will fail (matches Chromium behavior)
      returnJson.url = url;
    }

    return { body: returnJson };
  }

  /** HTTP copy of Target.activateTarget() */
  async activateTarget(targetId) {
    // Try to get the target from given id
    const target = this.cdp.targetList.getById(targetId);

    if (!target) {
      return {
        status: lazy.HTTP_404,
        body: `No such target id: ${targetId}`,
        json: false,
      };
    }

    // Select the tab (this endpoint does not show the window)
    await lazy.TabManager.selectTab(target.tab);

    return { body: "Target activated", json: false };
  }

  /** HTTP copy of Target.closeTarget() */
  async closeTarget(targetId) {
    // Try to get the target from given id
    const target = this.cdp.targetList.getById(targetId);

    if (!target) {
      return {
        status: lazy.HTTP_404,
        body: `No such target id: ${targetId}`,
        json: false,
      };
    }

    // Remove the tab
    await lazy.TabManager.removeTab(target.tab);

    return { body: "Target is closing", json: false };
  }

  // nsIHttpRequestHandler

  async handle(request, response) {
    // Mark request as async so we can execute async routes and return values
    response.processAsync();

    // Run a provided route (function) with an argument
    const runRoute = async (route, data) => {
      try {
        // Run the route to get data to return
        const {
          status = { code: 200, description: "OK" },
          json = true,
          body,
        } = await route(data);

        // Stringify into returnable JSON if wanted
        const payload = json
          ? JSON.stringify(body, null, lazy.Log.verbose ? "\t" : null)
          : body;

        // Handle HTTP response
        response.setStatusLine(
          request.httpVersion,
          status.code,
          status.description
        );
        response.setHeader("Content-Type", "application/json");
        response.setHeader("Content-Security-Policy", "frame-ancestors 'none'");
        response.write(payload);
      } catch (e) {
        new lazy.RemoteAgentError(e).notify();

        // Mark as 500 as an error has occured internally
        response.setStatusLine(
          request.httpVersion,
          lazy.HTTP_500.code,
          lazy.HTTP_500.description
        );
      }
    };

    // Trim trailing slashes to conform with expected routes
    const path = request.path.replace(/\/+$/, "");

    let route;
    for (const _route in this.routes) {
      // Prefixed/parameter route (/path/{parameter})
      if (path.startsWith(_route + "/") && this.routes[_route].parameter) {
        route = _route;
        break;
      }

      // Regular route (/path/example)
      if (path === _route) {
        route = _route;
        break;
      }
    }

    if (!route) {
      // Route does not exist
      response.setStatusLine(
        request.httpVersion,
        lazy.HTTP_404.code,
        lazy.HTTP_404.description
      );
      response.write("Unknown command: " + path.replace("/json/", ""));

      return response.finish();
    }

    const { handler, method, parameter } = this.routes[route];

    // If only one valid method for route, check method matches
    if (method && request.method !== method) {
      response.setStatusLine(
        request.httpVersion,
        lazy.HTTP_405.code,
        lazy.HTTP_405.description
      );
      response.write(
        `Using unsafe HTTP verb ${request.method} to invoke ${route}. This action supports only PUT verb.`
      );
      return response.finish();
    }

    if (parameter) {
      await runRoute(handler, path.split("/").pop());
    } else {
      await runRoute(handler, request.queryString);
    }

    // Send response
    return response.finish();
  }

  // XPCOM

  get QueryInterface() {
    return ChromeUtils.generateQI(["nsIHttpRequestHandler"]);
  }
}
