/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/**
 * Tests that various types of inline content elements initiate requests
 * with the triggering pringipal of the caller that requested the load.
 */

const {escaped} = Cu.import("resource://testing-common/AddonTestUtils.jsm", {});

Cu.importGlobalProperties(["URL"]);

// Make sure media pre-loading is enabled on Android so that our <audio> and
// <video> elements trigger the expected requests.
Services.prefs.setBoolPref("media.autoplay.enabled", true);
Services.prefs.setIntPref("media.preload.default", 3);

// ExtensionContent.jsm needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

const server = createHttpServer();
server.registerDirectory("/data/", do_get_file("data"));

/**
 * Registers a static HTML document with the given content at the given
 * path in our test HTTP server.
 *
 * @param {string} path
 * @param {string} content
 */
function registerStaticPage(path, content) {
  server.registerPathHandler(path, (request, response) => {
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/html");
    response.write(content);
  });
}

const BASE_URL = `http://localhost:${server.identity.primaryPort}`;

/**
 * A set of tags which are automatically closed in HTML documents, and
 * do not require an explicit closing tag.
 */
const AUTOCLOSE_TAGS = new Set(["img", "input", "source"]);

/**
 * An object describing the elements to create for a specific test.
 *
 * @typedef {object} ElementTestCase
 * @property {Array} element
 *        A recursive array, describing the element to create, in the
 *        following format:
 *
 *          ["tagname", {attr: "attrValue"},
 *            ["child-tagname", {attr: "value"}],
 *            ...]
 *
 *        For each test, a DOM tree will be created with this structure.
 *        A source attribute, with the name `test.srcAttr` and a value
 *        based on the values of `test.src` and `opts`, will be added to
 *        the first leaf node encountered.
 * @property {string} src
 *        The relative URL to use as the source of the element. Each
 *        load of this URL will have a separate set of query parameters
 *        appended to it, based on the values in `opts`.
 * @property {string} [srcAttr = "src"]
 *        The attribute in which to store the element's source URL.
 * @property {string} [srcAttr = "src"]
 *        The attribute in which to store the element's source URL.
 * @property {boolean} [liveSrc = false]
 *        If true, changing the source attribute after the element has
 *        been inserted into the document is expected to trigger a new
 *        load, and that configuration will be tested.
 */

/**
 * Options for this specific configuration of an element test.
 *
 * @typedef {object} ElementTestOptions
 * @property {string} origin
 *        The origin with which the content is expected to load. This
 *        may be either "page" or "extension". The actual load of the
 *        URL will be tested against the computed origin strings for
 *        those two contexts.
 * @property {string} source
 *        An arbitrary string which uniquely identifies the source of
 *        the load. For instance, each of these should have separate
 *        origin strings:
 *
 *         - An element present in the initial page HTML.
 *         - An element injected by a page script belonging to web
 *           content.
 *         - An element injected by an extension content script.
 */

/**
 * Data describing a test element, which can be used to create a
 * corresponding DOM tree.
 *
 * @typedef {object} ElementData
 * @property {string} tagName
 *        The tag name for the element.
 * @property {object} attrs
 *        A property containing key-value pairs for each of the
 *        attribute's elements.
 * @property {Array<ElementData>} children
 *        A possibly empty array of element data for child elements.
 */

/**
 * Returns data necessary to create test elements for the given test,
 * with the given options.
 *
 * @param {ElementTestCase} test
 *        An object describing the elements to create for a specific
 *        test. This element will be created under various
 *        circumstances, as described by `opts`.
 * @param {ElementTestOptions} opts
 *        Options for this specific configuration of the test.
 * @returns {ElementData}
 */
function getElementData(test, opts) {
  let baseURL = typeof BASE_URL !== "undefined" ? BASE_URL : location.href;

  let {srcAttr, src} = test;

  // Absolutify the URL, so it passes sanity checks that ignore
  // triggering principals for relative URLs.
  src = new URL(src + `?origin=${encodeURIComponent(opts.origin)}&source=${encodeURIComponent(opts.source)}`,
                baseURL).href;

  let haveSrc = false;
  function rec(element) {
    let [tagName, attrs, ...children] = element;

    if (children.length) {
      children = children.map(rec);
    } else if (!haveSrc) {
      attrs = Object.assign({[srcAttr]: src}, attrs);
      haveSrc = true;
    }

    return {tagName, attrs, children};
  }
  return rec(test.element);
}

/**
 * The result type of the {@see createElement} function.
 *
 * @typedef {object} CreateElementResult
 * @property {Element} elem
 *        The root element of the created DOM tree.
 * @property {Element} srcElem
 *        The element in the tree to which the source attribute must be
 *        added.
 * @property {string} src
 *        The value of the source element.
 */

/**
 * Creates a DOM tree for a given test, in a given configuration, as
 * understood by {@see getElementData}, but without the `test.srcAttr`
 * attribute having been set. The caller must set the value of that
 * attribute to the returned `src` value.
 *
 * There are many different ways most source values can be set
 * (DOM attribute, DOM property, ...) and many different contexts
 * (content script verses page script). Each test should be run with as
 * many variants of these as possible.
 *
 * @param {ElementTestCase} test
 *        A test object, as passed to {@see getElementData}.
 * @param {ElementTestOptions} opts
 *        An options object, as passed to {@see getElementData}.
 * @returns {CreateElementResult}
 */
function createElement(test, opts) {
  let srcElem;
  let src;

  function rec({tagName, attrs, children}) {
    let elem = document.createElement(tagName);

    for (let [key, val] of Object.entries(attrs)) {
      if (key === test.srcAttr) {
        srcElem = elem;
        src = val;
      } else {
        elem.setAttribute(key, val);
      }
    }
    for (let child of children) {
      elem.appendChild(rec(child));
    }
    return elem;
  }
  let elem = rec(getElementData(test, opts));

  return {elem, srcElem, src};
}

/**
 * Converts the given test data, as accepted by {@see getElementData},
 * to an HTML representation.
 *
 * @param {ElementTestCase} test
 *        A test object, as passed to {@see getElementData}.
 * @param {ElementTestOptions} opts
 *        An options object, as passed to {@see getElementData}.
 * @returns {string}
 */
function toHTML(test, opts) {
  function rec({tagName, attrs, children}) {
    let html = [`<${tagName}`];
    for (let [key, val] of Object.entries(attrs)) {
      html.push(escaped` ${key}="${val}"`);
    }

    html.push(">");
    if (!AUTOCLOSE_TAGS.has(tagName)) {
      for (let child of children) {
        html.push(rec(child));
      }

      html.push(`</${tagName}>`);
    }
    return html.join("");
  }
  return rec(getElementData(test, opts));
}

/**
 * A function which will be stringified, and run both as a page script
 * and an extension content script, to test element injection under
 * various configurations.
 *
 * @param {Array<ElementTestCase>} tests
 *        A list of test objects, as understood by {@see getElementData}.
 * @param {ElementTestOptions} baseOpts
 *        A base options object, as understood by {@see getElementData},
 *        which represents the default values for injections under this
 *        context.
 */
function injectElements(tests, baseOpts) {
  window.addEventListener("load", () => {
    let overrideOpts = opts => Object.assign({}, baseOpts, opts);
    let opts = baseOpts;

    // Build the full element with setAttr, then inject.
    for (let test of tests) {
      let {elem, srcElem, src} = createElement(test, opts);
      srcElem.setAttribute(test.srcAttr, src);
      document.body.appendChild(elem);
    }

    // Build the full element with a property setter.
    opts = overrideOpts({source: `${baseOpts.source}-prop`});
    for (let test of tests) {
      let {elem, srcElem, src} = createElement(test, opts);
      srcElem[test.srcAttr] = src;
      document.body.appendChild(elem);
    }

    // Build the element without the source attribute, inject, then set
    // it.
    opts = overrideOpts({source: `${baseOpts.source}-attr-after-inject`});
    for (let test of tests) {
      let {elem, srcElem, src} = createElement(test, opts);
      document.body.appendChild(elem);
      srcElem.setAttribute(test.srcAttr, src);
    }

    // Build the element without the source attribute, inject, then set
    // the corresponding property.
    opts = overrideOpts({source: `${baseOpts.source}-prop-after-inject`});
    for (let test of tests) {
      let {elem, srcElem, src} = createElement(test, opts);
      document.body.appendChild(elem);
      srcElem[test.srcAttr] = src;
    }

    // Build the element with a relative, rather than absolute, URL, and
    // make sure it always has the page origin.
    opts = overrideOpts({source: `${baseOpts.source}-relative-url`,
                         origin: "page"});
    for (let test of tests) {
      let {elem, srcElem, src} = createElement(test, opts);
      // Note: This assumes that the content page and the src URL are
      // always at the server root. If that changes, the test will
      // timeout waiting for matching requests.
      src = src.replace(/.*\//, "");
      srcElem.setAttribute(test.srcAttr, src);
      document.body.appendChild(elem);
    }

    // If we're in an extension content script, do some additional checks.
    if (typeof browser !== "undefined") {
      // Build the element without the source attribute, inject, then
      // have content set it.
      opts = overrideOpts({source: `${baseOpts.source}-content-attr-after-inject`,
                           origin: "page"});

      for (let test of tests) {
        let {elem, srcElem, src} = createElement(test, opts);
        document.body.appendChild(elem);
        window.wrappedJSObject.elem = srcElem;
        window.wrappedJSObject.eval(`elem.setAttribute(${uneval(test.srcAttr)}, ${uneval(src)})`);
      }

      // Build the full element, then let content inject.
      opts = overrideOpts({source: `${baseOpts.source}-content-inject-after-attr`});
      for (let test of tests) {
        let {elem, srcElem, src} = createElement(test, opts);
        srcElem.setAttribute(test.srcAttr, src);
        window.wrappedJSObject.elem = elem;
        window.wrappedJSObject.eval(`document.body.appendChild(elem)`);
      }

      // Build the element without the source attribute, let content set
      // it, then inject.
      opts = overrideOpts({source: `${baseOpts.source}-inject-after-content-attr`,
                           origin: "page"});

      for (let test of tests) {
        let {elem, srcElem, src} = createElement(test, opts);
        window.wrappedJSObject.elem = srcElem;
        window.wrappedJSObject.eval(`elem.setAttribute(${uneval(test.srcAttr)}, ${uneval(src)})`);
        document.body.appendChild(elem);
      }

      // Build the element with a dummy source attribute, inject, then
      // let content change it.
      opts = overrideOpts({source: `${baseOpts.source}-content-change-after-inject`,
                           origin: "page"});

      for (let test of tests) {
        let {elem, srcElem, src} = createElement(test, opts);
        srcElem.setAttribute(test.srcAttr, "meh.txt");
        document.body.appendChild(elem);
        window.wrappedJSObject.elem = srcElem;
        window.wrappedJSObject.eval(`elem.setAttribute(${uneval(test.srcAttr)}, ${uneval(src)})`);
      }
    }
  }, {once: true});
}

/**
 * Stringifies the {@see injectElements} function for use as a page or
 * content script.
 *
 * @param {Array<ElementTestCase>} tests
 *        A list of test objects, as understood by {@see getElementData}.
 * @param {ElementTestOptions} opts
 *        A base options object, as understood by {@see getElementData},
 *        which represents the default values for injections under this
 *        context.
 * @returns {string}
 */
function getInjectionScript(tests, opts) {
  return `
    ${getElementData}
    ${createElement}
    (${injectElements})(${JSON.stringify(tests)},
                        ${JSON.stringify(opts)});
  `;
}

/**
 * Awaits the content loads for each of the given tests, with each of
 * the given sources, and checks that their origin strings are as
 * expected.
 *
 * @param {Array<ElementTestCase>} tests
 *        A list of tests, as understood by {@see getElementData}.
 * @param {Object<string, object>} sources
 *        A set of sources for which each of the above tests is expected
 *        to generate one request, if each of the properties in the
 *        value object matches the value of the same property in the
 *        test object.
 * @param {object<string, string>} origins
 *        A mapping of origin parameters as they appear in URL query
 *        strings to the origin strings returned by corresponding
 *        principals. These values are used to test requests against
 *        their expected origins.
 * @returns {Promise}
 *        A promise which resolves when all requests have been
 *        processed.
 */
function awaitLoads(tests, sources, origins) {
  let expectedURLs = new Set();

  for (let test of tests) {
    for (let [source, attrs] of Object.entries(sources)) {
      if (Object.keys(attrs).every(attr => attrs[attr] === test[attr])) {
        let urlPrefix = `${BASE_URL}/${test.src}?source=${source}`;
        expectedURLs.add(urlPrefix);
      }
    }
  }

  return new Promise(resolve => {
    let observer = (channel, topic, data) => {
      channel.QueryInterface(Ci.nsIChannel);

      let url = new URL(channel.URI.spec);
      let origin = url.searchParams.get("origin");
      url.searchParams.delete("origin");

      if (expectedURLs.has(url.href)) {
        expectedURLs.delete(url.href);

        equal(channel.loadInfo.triggeringPrincipal.origin,
              origins[origin],
              `Got expected origin for URL ${channel.URI.spec}`);

        if (!expectedURLs.size) {
          Services.obs.removeObserver(observer, "http-on-modify-request");
          resolve();
        }
      }
    };
    Services.obs.addObserver(observer, "http-on-modify-request");
  });
}

add_task(async function test_contentscript_triggeringPrincipals() {
  /**
   * A list of tests to run in each context, as understood by
   * {@see getElementData}.
   */
  const TESTS = [
    {
      element: ["audio", {}],
      src: "audio.webm",
    },
    {
      element: ["audio", {}, ["source", {}]],
      src: "audio-source.webm",
    },
    // TODO: <frame> element, which requires a frameset document.
    {
      element: ["iframe", {}],
      src: "iframe.html",
    },
    {
      element: ["img", {}],
      src: "img.png",
    },
    {
      element: ["img", {}],
      src: "imgset.png",
      srcAttr: "srcset",
    },
    {
      element: ["input", {type: "image"}],
      src: "input.png",
    },
    {
      element: ["picture", {}, ["source", {}], ["img", {}]],
      src: "picture.png",
      srcAttr: "srcset",
    },
    {
      element: ["script", {}],
      src: "script.js",
      liveSrc: false,
    },
    {
      element: ["video", {}],
      src: "video.webm",
    },
    {
      element: ["video", {}, ["source", {}]],
      src: "video-source.webm",
    },
  ];

  /**
   * A set of sources for which each of the above tests is expected to
   * generate one request, if each of the properties in the value object
   * matches the value of the same property in the test object.
   */
  const SOURCES = {
    "contentScript": {},
    "contentScript-attr-after-inject": {liveSrc: true},
    "contentScript-content-attr-after-inject": {liveSrc: true},
    "contentScript-content-change-after-inject": {liveSrc: true},
    "contentScript-content-inject-after-attr": {},
    "contentScript-inject-after-content-attr": {},
    "contentScript-prop": {},
    "contentScript-prop-after-inject": {},
    "contentScript-relative-url": {},
    "pageHTML": {},
    "pageScript": {},
    "pageScript-attr-after-inject": {},
    "pageScript-prop": {},
    "pageScript-prop-after-inject": {},
    "pageScript-relative-url": {},
  };

  for (let test of TESTS) {
    if (!test.srcAttr) {
      test.srcAttr = "src";
    }
    if (!("liveSrc" in test)) {
      test.liveSrc = true;
    }
  }


  registerStaticPage("/page.html", `<!DOCTYPE html>
    <html lang="en">
    <head>
      <meta charset="UTF-8">
      <title></title>
      <script>
        ${getInjectionScript(TESTS, {source: "pageScript", origin: "page"})}
      </script>
    </head>
    <body>
      ${TESTS.map(test => toHTML(test, {source: "pageHTML", origin: "page"})).join("\n  ")}
    </body>
    </html>`);


  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [{
        "matches": ["http://*/page.html"],
        "run_at": "document_start",
        "js": ["content_script.js"],
      }],
    },

    files: {
      "content_script.js": getInjectionScript(TESTS, {source: "contentScript", origin: "extension"}),
    },
  });

  await extension.startup();

  const pageURL = `${BASE_URL}/page.html`;
  const pageURI = Services.io.newURI(pageURL);

  let origins = {
    page: Services.scriptSecurityManager.createCodebasePrincipal(pageURI, {}).origin,
    extension: Cu.getObjectPrincipal(Cu.Sandbox([extension.extension.principal, pageURL])).origin,
  };
  let finished = awaitLoads(TESTS, SOURCES, origins);

  let contentPage = await ExtensionTestUtils.loadContentPage(pageURL);

  await finished;

  await extension.unload();
  await contentPage.close();
});
