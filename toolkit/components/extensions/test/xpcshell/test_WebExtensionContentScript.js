/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { newURI } = Services.io;

const server = createHttpServer({ hosts: ["example.com"] });
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

    includeGlobs: ["*flerg*", "*.com/bar", "*/quux"].map(
      glob => new MatchGlob(glob)
    ),

    excludeGlobs: ["*glorg*"].map(glob => new MatchGlob(glob)),
  });

  ok(
    contentScript.matchesURI(newURI("http://foo.com/bar")),
    "Simple matches include should match"
  );

  ok(
    contentScript.matchesURI(newURI("https://bar.com/baz/xflergx")),
    "Simple matches include should match"
  );

  ok(
    !contentScript.matchesURI(newURI("https://bar.com/baz/xx")),
    "Failed includeGlobs match pattern should not match"
  );

  ok(
    !contentScript.matchesURI(newURI("https://bar.com/baz/quux")),
    "Excluded match pattern should not match"
  );

  ok(
    !contentScript.matchesURI(newURI("https://bar.com/baz/xflergxglorgx")),
    "Excluded match glob should not match"
  );
});

async function loadURL(url) {
  let requests = new Map();

  function requestObserver(request) {
    request.QueryInterface(Ci.nsIChannel);
    if (request.isDocument) {
      requests.set(request.name, request);
    }
  }

  Services.obs.addObserver(requestObserver, "http-on-examine-response");

  let contentPage = await ExtensionTestUtils.loadContentPage(url);

  Services.obs.removeObserver(requestObserver, "http-on-examine-response");

  return { contentPage, requests };
}

add_task(async function test_WebExtensinonContentScript_frame_matching() {
  if (AppConstants.platform == "linux") {
    // The windowless browser currently does not load correctly on Linux on
    // infra.
    return;
  }

  let baseURL = `http://example.com/data`;
  let urls = {
    topLevel: `${baseURL}/file_toplevel.html`,
    iframe: `${baseURL}/file_iframe.html`,
    srcdoc: "about:srcdoc",
    aboutBlank: "about:blank",
  };

  let { contentPage, requests } = await loadURL(urls.topLevel);

  let tests = [
    {
      matches: ["http://example.com/data/*"],
      contentScript: {},
      topLevel: true,
      iframe: false,
      aboutBlank: false,
      srcdoc: false,
    },

    {
      matches: ["http://example.com/data/*"],
      contentScript: {
        frameID: 0,
      },
      topLevel: true,
      iframe: false,
      aboutBlank: false,
      srcdoc: false,
    },

    {
      matches: ["http://example.com/data/*"],
      contentScript: {
        allFrames: true,
      },
      topLevel: true,
      iframe: true,
      aboutBlank: false,
      srcdoc: false,
    },

    {
      matches: ["http://example.com/data/*"],
      contentScript: {
        allFrames: true,
        matchAboutBlank: true,
      },
      topLevel: true,
      iframe: true,
      aboutBlank: true,
      srcdoc: true,
    },

    {
      matches: ["http://foo.com/data/*"],
      contentScript: {
        allFrames: true,
        matchAboutBlank: true,
      },
      topLevel: false,
      iframe: false,
      aboutBlank: false,
      srcdoc: false,
    },
  ];

  // matchesWindowGlobal tests against content frames
  await contentPage.spawn({ tests, urls }, args => {
    this.windows = new Map();
    this.windows.set(this.content.location.href, this.content);
    for (let c of Array.from(this.content.frames)) {
      this.windows.set(c.location.href, c);
    }
    this.policy = new WebExtensionPolicy({
      id: "foo@bar.baz",
      mozExtensionHostname: "88fb51cd-159f-4859-83db-7065485bc9b2",
      baseURL: "file:///foo",

      allowedOrigins: new MatchPatternSet([]),
      localizeCallback() {},
    });

    let tests = args.tests.map(t => {
      t.contentScript.matches = new MatchPatternSet(t.matches);
      t.script = new WebExtensionContentScript(this.policy, t.contentScript);
      return t;
    });
    for (let [i, test] of tests.entries()) {
      for (let [frame, url] of Object.entries(args.urls)) {
        let should = test[frame] ? "should" : "should not";
        let wgc = this.windows.get(url).windowGlobalChild;
        Assert.equal(
          test.script.matchesWindowGlobal(wgc),
          test[frame],
          `Script ${i} ${should} match the ${frame} frame`
        );
      }
    }
  });

  // Parent tests against loadInfo
  tests = tests.map(t => {
    t.contentScript.matches = new MatchPatternSet(t.matches);
    t.script = new WebExtensionContentScript(policy, t.contentScript);
    return t;
  });

  for (let [i, test] of tests.entries()) {
    for (let [frame, url] of Object.entries(urls)) {
      let should = test[frame] ? "should" : "should not";

      if (url.startsWith("http")) {
        let request = requests.get(url);

        equal(
          test.script.matchesLoadInfo(request.URI, request.loadInfo),
          test[frame],
          `Script ${i} ${should} match the request LoadInfo for ${frame} frame`
        );
      }
    }
  }

  await contentPage.close();
});
