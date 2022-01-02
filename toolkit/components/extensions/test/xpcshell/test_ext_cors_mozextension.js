"use strict";

const server = createHttpServer({
  hosts: ["example.com", "x.example.com"],
});
server.registerPathHandler("/dummy", (req, res) => {
  res.write("dummy");
});
server.registerPathHandler("/redir", (req, res) => {
  res.setStatusLine(req.httpVersion, 302, "Found");
  res.setHeader("Access-Control-Allow-Origin", "http://example.com");
  res.setHeader("Access-Control-Allow-Credentials", "true");
  res.setHeader("Location", new URLSearchParams(req.queryString).get("url"));
});

add_task(async function load_moz_extension_with_and_without_cors() {
  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      web_accessible_resources: ["ok.js"],
    },
    files: {
      "ok.js": "window.status = 'loaded';",
      "deny.js": "window.status = 'unexpected load'",
    },
  });
  await extension.startup();
  const EXT_BASE_URL = `moz-extension://${extension.uuid}`;
  let contentPage = await ExtensionTestUtils.loadContentPage(
    "http://example.com/dummy"
  );
  await contentPage.spawn(EXT_BASE_URL, async EXT_BASE_URL => {
    const { document, window } = this.content;
    async function checkScriptLoad({ setupScript, expectLoad, description }) {
      const scriptElem = document.createElement("script");
      setupScript(scriptElem);
      return new Promise(resolve => {
        window.status = "initial";
        scriptElem.onload = () => {
          Assert.equal(window.status, "loaded", "Script executed upon load");
          Assert.ok(expectLoad, `Script loaded - ${description}`);
          resolve();
        };
        scriptElem.onerror = () => {
          Assert.equal(window.status, "initial", "not executed upon error");
          Assert.ok(!expectLoad, `Script not loaded - ${description}`);
          resolve();
        };
        document.head.append(scriptElem);
      });
    }

    function sameOriginRedirectUrl(url) {
      return `http://example.com/redir?url=` + encodeURIComponent(url);
    }
    function crossOriginRedirectUrl(url) {
      return `http://x.example.com/redir?url=` + encodeURIComponent(url);
    }

    // Direct load of web-accessible extension script.
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = `${EXT_BASE_URL}/ok.js`;
      },
      expectLoad: true,
      description: "web-accessible script, plain load",
    });
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = `${EXT_BASE_URL}/ok.js`;
        scriptElem.crossOrigin = "anonymous";
      },
      expectLoad: true,
      description: "web-accessible script, cors",
    });
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = `${EXT_BASE_URL}/ok.js`;
        scriptElem.crossOrigin = "use-credentials";
      },
      expectLoad: true,
      description: "web-accessible script, cors+credentials",
    });

    // Load of web-accessible extension scripts, after same-origin redirect.
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = sameOriginRedirectUrl(`${EXT_BASE_URL}/ok.js`);
      },
      expectLoad: true,
      description: "same-origin redirect to web-accessible script, plain load",
    });
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = sameOriginRedirectUrl(`${EXT_BASE_URL}/ok.js`);
        scriptElem.crossOrigin = "anonymous";
      },
      expectLoad: true,
      description: "same-origin redirect to web-accessible script, cors",
    });
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = sameOriginRedirectUrl(`${EXT_BASE_URL}/ok.js`);
        scriptElem.crossOrigin = "use-credentials";
      },
      expectLoad: true,
      description:
        "same-origin redirect to web-accessible script, cors+credentials",
    });

    // Load of web-accessible extension scripts, after cross-origin redirect.
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = crossOriginRedirectUrl(`${EXT_BASE_URL}/ok.js`);
      },
      expectLoad: true,
      description: "cross-origin redirect to web-accessible script, plain load",
    });
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = crossOriginRedirectUrl(`${EXT_BASE_URL}/ok.js`);
        scriptElem.crossOrigin = "anonymous";
      },
      expectLoad: true,
      description: "cross-origin redirect to web-accessible script, cors",
    });
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = crossOriginRedirectUrl(`${EXT_BASE_URL}/ok.js`);
        scriptElem.crossOrigin = "use-credentials";
      },
      expectLoad: true,
      description:
        "cross-origin redirect to web-accessible script, cors+credentials",
    });

    // Various loads of non-web-accessible extension script.
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = `${EXT_BASE_URL}/deny.js`;
      },
      expectLoad: false,
      description: "non-accessible script, plain load",
    });
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = `${EXT_BASE_URL}/deny.js`;
        scriptElem.crossOrigin = "anonymous";
      },
      expectLoad: false,
      description: "non-accessible script, cors",
    });
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = sameOriginRedirectUrl(`${EXT_BASE_URL}/deny.js`);
        scriptElem.crossOrigin = "anonymous";
      },
      expectLoad: false,
      description: "same-origin redirect to non-accessible script, cors",
    });
    await checkScriptLoad({
      setupScript(scriptElem) {
        scriptElem.src = crossOriginRedirectUrl(`${EXT_BASE_URL}/deny.js`);
        scriptElem.crossOrigin = "anonymous";
      },
      expectLoad: false,
      description: "cross-origin redirect to non-accessible script, cors",
    });

    // Sub-resource integrity usually requires CORS. Verify that web-accessible
    // extension resources are still subjected to SRI.
    const sriHashOkJs = // SRI hash for "window.status = 'loaded';" (=ok.js).
      "sha384-EAofaAZpgy6JshegITJJHeE3ROzn9ngGw1GAuuzjSJV1c/YS9PLvHMt9oh4RovrI";

    async function testSRI({ integrityMatches }) {
      const integrity = integrityMatches ? sriHashOkJs : "sha384-bad-sri-hash";
      const sriDescription = integrityMatches
        ? "web-accessible script, good sri, "
        : "web-accessible script, sri not matching, ";
      await checkScriptLoad({
        setupScript(scriptElem) {
          scriptElem.src = `${EXT_BASE_URL}/ok.js`;
          scriptElem.integrity = integrity;
        },
        expectLoad: integrityMatches,
        description: `${sriDescription} no cors, plain load`,
      });
      await checkScriptLoad({
        setupScript(scriptElem) {
          scriptElem.src = `${EXT_BASE_URL}/ok.js`;
          scriptElem.crossOrigin = "anonymous";
          scriptElem.integrity = integrity;
        },
        expectLoad: integrityMatches,
        description: `${sriDescription} cors, plain load`,
      });
      await checkScriptLoad({
        setupScript(scriptElem) {
          scriptElem.src = sameOriginRedirectUrl(`${EXT_BASE_URL}/ok.js`);
          scriptElem.crossOrigin = "anonymous";
          scriptElem.integrity = integrity;
        },
        expectLoad: integrityMatches,
        description: `${sriDescription} cors, same-origin redirect`,
      });
      await checkScriptLoad({
        setupScript(scriptElem) {
          scriptElem.src = crossOriginRedirectUrl(`${EXT_BASE_URL}/ok.js`);
          scriptElem.crossOrigin = "anonymous";
          scriptElem.integrity = integrity;
        },
        expectLoad: integrityMatches,
        description: `${sriDescription} cors, cross-origin redirect`,
      });
    }
    await testSRI({ integrityMatches: true });
    await testSRI({ integrityMatches: false });
  });
  await contentPage.close();
  await extension.unload();
});
