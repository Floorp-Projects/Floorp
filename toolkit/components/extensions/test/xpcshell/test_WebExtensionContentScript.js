/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const {newURI} = Services.io;

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

let policy = new WebExtensionPolicy({
  id: "foo@bar.baz",
  mozExtensionHostname: "88fb51cd-159f-4859-83db-7065485bc9b2",
  baseURL: "file:///foo",

  allowedOrigins: new MatchPatternSet([]),
  localizeCallback() {},
});

add_task(async function test_WebExtensinonContentScript_url_matching() {
  let contentScript = new WebExtensionContentScript(policy, {
    matches: new MatchPatternSet(["http://foo.com/bar", "*://bar.com/baz/*"]),

    excludeMatches: new MatchPatternSet(["*://bar.com/baz/quux"]),

    includeGlobs: ["*flerg*", "*.com/bar", "*/quux"].map(glob => new MatchGlob(glob)),

    excludeGlobs: ["*glorg*"].map(glob => new MatchGlob(glob)),
  });

  ok(contentScript.matchesURI(newURI("http://foo.com/bar")),
     "Simple matches include should match");

  ok(contentScript.matchesURI(newURI("https://bar.com/baz/xflergx")),
     "Simple matches include should match");

  ok(!contentScript.matchesURI(newURI("https://bar.com/baz/xx")),
     "Failed includeGlobs match pattern should not match");

  ok(!contentScript.matchesURI(newURI("https://bar.com/baz/quux")),
     "Excluded match pattern should not match");

  ok(!contentScript.matchesURI(newURI("https://bar.com/baz/xflergxglorgx")),
     "Excluded match glob should not match");
});

async function loadURL(url, {frameCount}) {
  let windows = new Map();
  let requests = new Map();

  let resolveLoad;
  let loadPromise = new Promise(resolve => { resolveLoad = resolve; });

  function requestObserver(request) {
    request.QueryInterface(Ci.nsIChannel);
    if (request.isDocument) {
      requests.set(request.name, request);
    }
  }
  function loadObserver(window) {
    windows.set(window.location.href, window);
    if (windows.size == frameCount) {
      resolveLoad();
    }
  }

  Services.obs.addObserver(requestObserver, "http-on-examine-response");
  Services.obs.addObserver(loadObserver, "content-document-global-created");

  let webNav = Services.appShell.createWindowlessBrowser(false);
  webNav.loadURI(url, 0, null, null, null);

  await loadPromise;

  Services.obs.removeObserver(requestObserver, "http-on-examine-response");
  Services.obs.removeObserver(loadObserver, "content-document-global-created");

  return {webNav, windows, requests};
}

add_task(async function test_WebExtensinonContentScript_frame_matching() {
  if (AppConstants.platform == "linux") {
    // The windowless browser currently does not load correctly on Linux on
    // infra.
    return;
  }

  let baseURL = `http://localhost:${server.identity.primaryPort}/data`;
  let urls = {
    topLevel: `${baseURL}/file_toplevel.html`,
    iframe: `${baseURL}/file_iframe.html`,
    srcdoc: "about:srcdoc",
    aboutBlank: "about:blank",
  };

  let {webNav, windows, requests} = await loadURL(urls.topLevel, {frameCount: 4});

  let tests = [
    {
      contentScript: {
        matches: new MatchPatternSet(["http://localhost/data/*"]),
      },
      topLevel: true,
      iframe: false,
      aboutBlank: false,
      srcdoc: false,
    },

    {
      contentScript: {
        matches: new MatchPatternSet(["http://localhost/data/*"]),
        frameID: 0,
      },
      topLevel: true,
      iframe: false,
      aboutBlank: false,
      srcdoc: false,
    },

    {
      contentScript: {
        matches: new MatchPatternSet(["http://localhost/data/*"]),
        allFrames: true,
      },
      topLevel: true,
      iframe: true,
      aboutBlank: false,
      srcdoc: false,
    },

    {
      contentScript: {
        matches: new MatchPatternSet(["http://localhost/data/*"]),
        allFrames: true,
        matchAboutBlank: true,
      },
      topLevel: true,
      iframe: true,
      aboutBlank: true,
      srcdoc: true,
    },

    {
      contentScript: {
        matches: new MatchPatternSet(["http://foo.com/data/*"]),
        allFrames: true,
        matchAboutBlank: true,
      },
      topLevel: false,
      iframe: false,
      aboutBlank: false,
      srcdoc: false,
    },
  ];

  for (let [i, test] of tests.entries()) {
    let contentScript = new WebExtensionContentScript(policy, test.contentScript);

    for (let [frame, url] of Object.entries(urls)) {
      let should = test[frame] ? "should" : "should not";

      equal(contentScript.matchesWindow(windows.get(url)),
            test[frame],
            `Script ${i} ${should} match the ${frame} frame`);

      if (url.startsWith("http")) {
        let request = requests.get(url);

        equal(contentScript.matchesLoadInfo(request.URI, request.loadInfo),
              test[frame],
              `Script ${i} ${should} match the request LoadInfo for ${frame} frame`);
      }
    }
  }

  webNav.close();
});
