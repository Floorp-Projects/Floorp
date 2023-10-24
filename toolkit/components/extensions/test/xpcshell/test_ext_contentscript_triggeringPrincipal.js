/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

/**
 * Tests that various types of inline content elements initiate requests
 * with the triggering pringipal of the caller that requested the load,
 * and that the correct security policies are applied to the resulting
 * loads.
 */

// Make sure media pre-loading is enabled on Android so that our <audio> and
// <video> elements trigger the expected requests.
Services.prefs.setIntPref("media.autoplay.default", Ci.nsIAutoplay.ALLOWED);
Services.prefs.setIntPref("media.preload.default", 3);

// Increase the length of the code samples included in CSP reports so that we
// can correctly validate them.
Services.prefs.setIntPref(
  "security.csp.reporting.script-sample.max-length",
  4096
);

// Do not limit the number of CSP reports.
Services.prefs.setIntPref("security.csp.reporting.limit.count", 0);

// Do not trunacate the blocked-uri in CSP reports for frame navigations.
Services.prefs.setBoolPref(
  "security.csp.truncate_blocked_uri_for_frame_navigations",
  false
);

// ExtensionContent.jsm needs to know when it's running from xpcshell,
// to use the right timeout for content scripts executed at document_idle.
ExtensionTestUtils.mockAppInfo();

const server = createHttpServer({
  hosts: ["example.com", "csplog.example.net"],
});

server.registerDirectory("/data/", do_get_file("data"));

var gContentSecurityPolicy = null;

const BASE_URL = `http://example.com`;
const CSP_REPORT_PATH = "/csp-report.sjs";

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
    if (gContentSecurityPolicy) {
      response.setHeader("Content-Security-Policy", gContentSecurityPolicy);
    }
    response.write(content);
  });
}

/**
 * A set of tags which are automatically closed in HTML documents, and
 * do not require an explicit closing tag.
 */
const AUTOCLOSE_TAGS = new Set(["img", "input", "link", "source"]);

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
 *        may be one of "page", "contentScript", or "extension". The actual load
 *        of the URL will be tested against the computed origin strings for
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

  let { srcAttr, src } = test;

  // Absolutify the URL, so it passes sanity checks that ignore
  // triggering principals for relative URLs.
  src = new URL(
    src +
      `?origin=${encodeURIComponent(opts.origin)}&source=${encodeURIComponent(
        opts.source
      )}`,
    baseURL
  ).href;

  let haveSrc = false;
  function rec(element) {
    let [tagName, attrs, ...children] = element;

    if (children.length) {
      children = children.map(rec);
    } else if (!haveSrc) {
      attrs = Object.assign({ [srcAttr]: src }, attrs);
      haveSrc = true;
    }

    return { tagName, attrs, children };
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

  function rec({ tagName, attrs, children }) {
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

  return { elem, srcElem, src };
}

/**
 * Escapes any occurrences of &, ", < or > with XML entities.
 *
 * @param {string} str
 *        The string to escape.
 * @returns {string} The escaped string.
 */
function escapeXML(str) {
  let replacements = {
    "&": "&amp;",
    '"': "&quot;",
    "'": "&apos;",
    "<": "&lt;",
    ">": "&gt;",
  };
  return String(str).replace(/[&"''<>]/g, m => replacements[m]);
}

/**
 * A tagged template function which escapes any XML metacharacters in
 * interpolated values.
 *
 * @param {Array<string>} strings
 *        An array of literal strings extracted from the templates.
 * @param {Array} values
 *        An array of interpolated values extracted from the template.
 * @returns {string}
 *        The result of the escaped values interpolated with the literal
 *        strings.
 */
function escaped(strings, ...values) {
  let result = [];

  for (let [i, string] of strings.entries()) {
    result.push(string);
    if (i < values.length) {
      result.push(escapeXML(values[i]));
    }
  }

  return result.join("");
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
  function rec({ tagName, attrs, children }) {
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
 * Injects various permutations of inline CSS into a content page, from both
 * extension content script and content page contexts, and sends a "css-sources"
 * message to the test harness describing the injected content for verification.
 */
function testInlineCSS() {
  let urls = [];
  let sources = [];

  /**
   * Constructs the URL of an image to be loaded by the given origin, and
   * returns a CSS url() expression for it.
   *
   * The `name` parameter is an arbitrary name which should describe how the URL
   * is loaded. The `opts` object may contain arbitrary properties which
   * describe the load. Currently, only `inline` is recognized, and indicates
   * that the URL is being used in an inline stylesheet which may be blocked by
   * CSP.
   *
   * The URL and its parameters are recorded, and sent to the parent process for
   * verification.
   *
   * @param {string} origin
   * @param {string} name
   * @param {object} [opts]
   * @returns {string}
   */
  let i = 0;
  let url = (origin, name, opts = {}) => {
    let source = `${origin}-${name}`;

    let { href } = new URL(
      `css-${i++}.png?origin=${encodeURIComponent(
        origin
      )}&source=${encodeURIComponent(source)}`,
      location.href
    );

    urls.push(Object.assign({}, opts, { href, origin, source }));
    return `url("${href}")`;
  };

  /**
   * Registers the given inline CSS source as being loaded by the given origin,
   * and returns that CSS text.
   *
   * @param {string} origin
   * @param {string} css
   * @returns {string}
   */
  let source = (origin, css) => {
    sources.push({ origin, css });
    return css;
  };

  /**
   * Saves the given function to be run after a short delay, just before sending
   * the list of loaded sources to the parent process.
   */
  let laters = [];
  let later = fn => {
    laters.push(fn);
  };

  // Note: When accessing an element through `wrappedJSObject`, the operations
  // occur in the content page context, using the content subject principal.
  // When accessing it through X-ray wrappers, they happen in the content script
  // context, using its subject principal.

  {
    let li = document.createElement("li");
    li.setAttribute(
      "style",
      source(
        "contentScript",
        `background: ${url("contentScript", "li.style-first")}`
      )
    );
    li.style.wrappedJSObject.listStyleImage = url(
      "page",
      "li.style.listStyleImage-second"
    );
    document.body.appendChild(li);
  }

  {
    let li = document.createElement("li");
    li.wrappedJSObject.setAttribute(
      "style",
      source(
        "page",
        `background: ${url("page", "li.style-first", { inline: true })}`
      )
    );
    li.style.listStyleImage = url(
      "contentScript",
      "li.style.listStyleImage-second"
    );
    document.body.appendChild(li);
  }

  {
    let li = document.createElement("li");
    document.body.appendChild(li);
    li.setAttribute(
      "style",
      source(
        "contentScript",
        `background: ${url("contentScript", "li.style-first")}`
      )
    );
    later(() =>
      li.wrappedJSObject.setAttribute(
        "style",
        source(
          "page",
          `background: ${url("page", "li.style-second", { inline: true })}`
        )
      )
    );
  }

  {
    let li = document.createElement("li");
    document.body.appendChild(li);
    li.wrappedJSObject.setAttribute(
      "style",
      source(
        "page",
        `background: ${url("page", "li.style-first", { inline: true })}`
      )
    );
    later(() =>
      li.setAttribute(
        "style",
        source(
          "contentScript",
          `background: ${url("contentScript", "li.style-second")}`
        )
      )
    );
  }

  {
    let li = document.createElement("li");
    document.body.appendChild(li);
    li.style.cssText = source(
      "contentScript",
      `background: ${url("contentScript", "li.style.cssText-first")}`
    );

    // TODO: This inline style should be blocked, since our style-src does not
    // include 'unsafe-eval', but that is currently unimplemented.
    later(() => {
      li.style.wrappedJSObject.cssText = `background: ${url(
        "page",
        "li.style.cssText-second"
      )}`;
    });
  }

  // Creates a new element, inserts it into the page, and returns its CSS selector.
  let divNum = 0;
  function getSelector() {
    let div = document.createElement("div");
    div.id = `generated-div-${divNum++}`;
    document.body.appendChild(div);
    return `#${div.id}`;
  }

  for (let prop of ["textContent", "innerHTML"]) {
    // Test creating <style> element from the extension side and then replacing
    // its contents from the content side.
    {
      let sel = getSelector();
      let style = document.createElement("style");
      style[prop] = source(
        "extension",
        `${sel} { background: ${url("extension", `style-${prop}-first`)}; }`
      );
      document.head.appendChild(style);

      later(() => {
        style.wrappedJSObject[prop] = source(
          "page",
          `${sel} { background: ${url("page", `style-${prop}-second`, {
            inline: true,
          })}; }`
        );
      });
    }

    // Test creating <style> element from the extension side and then appending
    // a text node to it. Regardless of whether the append happens from the
    // content or extension side, this should cause the principal to be
    // forgotten.
    let testModifyAfterInject = (name, modifyFunc) => {
      let sel = getSelector();
      let style = document.createElement("style");
      style[prop] = source(
        "extension",
        `${sel} { background: ${url(
          "extension",
          `style-${name}-${prop}-first`
        )}; }`
      );
      document.head.appendChild(style);

      later(() => {
        modifyFunc(
          style,
          `${sel} { background: ${url("page", `style-${name}-${prop}-second`, {
            inline: true,
          })}; }`
        );
        source("page", style.textContent);
      });
    };

    testModifyAfterInject("appendChild", (style, css) => {
      style.appendChild(document.createTextNode(css));
    });

    // Test creating <style> element from the extension side and then appending
    // to it using insertAdjacentHTML, with the same rules as above.
    testModifyAfterInject("insertAdjacentHTML", (style, css) => {
      style.insertAdjacentHTML("beforeend", css);
    });

    // And again using insertAdjacentText.
    testModifyAfterInject("insertAdjacentText", (style, css) => {
      style.insertAdjacentText("beforeend", css);
    });

    // Test creating a style element and then accessing its CSSStyleSheet object.
    {
      let sel = getSelector();
      let style = document.createElement("style");
      style[prop] = source(
        "extension",
        `${sel} { background: ${url("extension", `style-${prop}-sheet`)}; }`
      );
      document.head.appendChild(style);

      browser.test.assertThrows(
        () => style.sheet.wrappedJSObject.cssRules,
        /Not allowed to access cross-origin stylesheet/,
        "Page content should not be able to access extension-generated CSS rules"
      );

      style.sheet.insertRule(
        source(
          "extension",
          `${sel} { border-image: ${url(
            "extension",
            `style-${prop}-sheet-insertRule`
          )}; }`
        )
      );
    }
  }

  setTimeout(() => {
    for (let fn of laters) {
      fn();
    }
    browser.test.sendMessage("css-sources", { urls, sources });
  });
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
  window.addEventListener(
    "load",
    () => {
      if (typeof browser === "object") {
        try {
          testInlineCSS();
        } catch (e) {
          browser.test.fail(`Error: ${e} :: ${e.stack}`);
        }
      }

      // Basic smoke test to check that SVG images do not try to create a document
      // with an expanded principal, which would cause a crash.
      let img = document.createElement("img");
      img.src = "data:image/svg+xml,%3Csvg%2F%3E";
      document.body.appendChild(img);

      let rand = Math.random();

      // Basic smoke test to check that we don't try to create stylesheets with an
      // expanded principal, which would cause a crash when loading font sets.
      let cssText = `
      @font-face {
          font-family: "DoesNotExist${rand}";
          src: url("fonts/DoesNotExist.${rand}.woff") format("woff");
          font-weight: normal;
          font-style: normal;
      }`;

      let link = document.createElement("link");
      link.rel = "stylesheet";
      link.href = "data:text/css;base64," + btoa(cssText);
      document.head.appendChild(link);

      let style = document.createElement("style");
      style.textContent = cssText;
      document.head.appendChild(style);

      let overrideOpts = opts => Object.assign({}, baseOpts, opts);
      let opts = baseOpts;

      // Build the full element with setAttr, then inject.
      for (let test of tests) {
        let { elem, srcElem, src } = createElement(test, opts);
        srcElem.setAttribute(test.srcAttr, src);
        document.body.appendChild(elem);
      }

      // Build the full element with a property setter.
      opts = overrideOpts({ source: `${baseOpts.source}-prop` });
      for (let test of tests) {
        let { elem, srcElem, src } = createElement(test, opts);
        srcElem[test.srcAttr] = src;
        document.body.appendChild(elem);
      }

      // Build the element without the source attribute, inject, then set
      // it.
      opts = overrideOpts({ source: `${baseOpts.source}-attr-after-inject` });
      for (let test of tests) {
        let { elem, srcElem, src } = createElement(test, opts);
        document.body.appendChild(elem);
        srcElem.setAttribute(test.srcAttr, src);
      }

      // Build the element without the source attribute, inject, then set
      // the corresponding property.
      opts = overrideOpts({ source: `${baseOpts.source}-prop-after-inject` });
      for (let test of tests) {
        let { elem, srcElem, src } = createElement(test, opts);
        document.body.appendChild(elem);
        srcElem[test.srcAttr] = src;
      }

      // Build the element with a relative, rather than absolute, URL, and
      // make sure it always has the page origin.
      opts = overrideOpts({
        source: `${baseOpts.source}-relative-url`,
        origin: "page",
      });
      for (let test of tests) {
        let { elem, srcElem, src } = createElement(test, opts);
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
        opts = overrideOpts({
          source: `${baseOpts.source}-content-attr-after-inject`,
          origin: "page",
        });

        for (let test of tests) {
          let { elem, srcElem, src } = createElement(test, opts);

          document.body.appendChild(elem);
          window.wrappedJSObject.elem = srcElem;
          window.wrappedJSObject.eval(
            `elem.setAttribute(${JSON.stringify(
              test.srcAttr
            )}, ${JSON.stringify(src)})`
          );
        }

        // Build the full element, then let content inject.
        opts = overrideOpts({
          source: `${baseOpts.source}-content-inject-after-attr`,
        });
        for (let test of tests) {
          let { elem, srcElem, src } = createElement(test, opts);
          srcElem.setAttribute(test.srcAttr, src);
          window.wrappedJSObject.elem = elem;
          window.wrappedJSObject.eval(`document.body.appendChild(elem)`);
        }

        // Build the element without the source attribute, let content set
        // it, then inject.
        opts = overrideOpts({
          source: `${baseOpts.source}-inject-after-content-attr`,
          origin: "page",
        });

        for (let test of tests) {
          let { elem, srcElem, src } = createElement(test, opts);
          window.wrappedJSObject.elem = srcElem;
          window.wrappedJSObject.eval(
            `elem.setAttribute(${JSON.stringify(
              test.srcAttr
            )}, ${JSON.stringify(src)})`
          );
          document.body.appendChild(elem);
        }

        // Build the element with a dummy source attribute, inject, then
        // let content change it.
        opts = overrideOpts({
          source: `${baseOpts.source}-content-change-after-inject`,
          origin: "page",
        });

        for (let test of tests) {
          let { elem, srcElem, src } = createElement(test, opts);
          srcElem.setAttribute(test.srcAttr, "meh.txt");
          document.body.appendChild(elem);
          window.wrappedJSObject.elem = srcElem;
          window.wrappedJSObject.eval(
            `elem.setAttribute(${JSON.stringify(
              test.srcAttr
            )}, ${JSON.stringify(src)})`
          );
        }
      }
    },
    { once: true }
  );
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
    ${testInlineCSS}
    (${injectElements})(${JSON.stringify(tests)},
                        ${JSON.stringify(opts)});
  `;
}

/**
 * Extracts the "origin" query parameter from the given URL, and returns it,
 * along with the URL sans origin parameter.
 *
 * @param {string} origURL
 * @returns {object}
 *        An object with `origin` and `baseURL` properties, containing the value
 *        or the URL's "origin" query parameter and the URL with that parameter
 *        removed, respectively.
 */
function getOriginBase(origURL) {
  let url = new URL(origURL);
  let origin = url.searchParams.get("origin");
  url.searchParams.delete("origin");

  return { origin, baseURL: url.href };
}

/**
 * An object containing sets of base URLs and CSS sources which are present in
 * the test page, sorted based on how they should be treated by CSP.
 *
 * @typedef {object} RequestedURLs
 * @property {Set<string>} expectedURLs
 *        A set of URLs which should be successfully requested by the content
 *        page.
 * @property {Set<string>} forbiddenURLs
 *        A set of URLs which are present in the content page, but should never
 *        generate requests.
 * @property {Set<string>} blockedURLs
 *        A set of URLs which are present in the content page, and should be
 *        blocked by CSP, and reported in a CSP report.
 * @property {Set<string>} blockedSources
 *        A set of inline CSS sources which should be blocked by CSP, and
 *        reported in a CSP report.
 */

/**
 * Computes a list of expected and forbidden base URLs for the given
 * sets of tests and sources. The base URL is the complete request URL
 * with the `origin` query parameter removed.
 *
 * @param {Array<ElementTestCase>} tests
 *        A list of tests, as understood by {@see getElementData}.
 * @param {Object<string, object>} expectedSources
 *        A set of sources for which each of the above tests is expected
 *        to generate one request, if each of the properties in the
 *        value object matches the value of the same property in the
 *        test object.
 * @param {Object<string, object>} [forbiddenSources = {}]
 *        A set of sources for which requests should never be sent. Any
 *        matching requests from these sources will cause the test to
 *        fail.
 * @returns {RequestedURLs}
 */
function computeBaseURLs(tests, expectedSources, forbiddenSources = {}) {
  let expectedURLs = new Set();
  let forbiddenURLs = new Set();

  function* iterSources(test, sources) {
    for (let [source, attrs] of Object.entries(sources)) {
      // if a source defines attributes (e.g. liveSrc in PAGE_SOURCES etc.) then all
      // attributes in the source must be matched by the test (see const TEST).
      if (Object.keys(attrs).every(attr => attrs[attr] === test[attr])) {
        yield `${BASE_URL}/${test.src}?source=${source}`;
      }
    }
  }

  for (let test of tests) {
    for (let urlPrefix of iterSources(test, expectedSources)) {
      expectedURLs.add(urlPrefix);
    }
    for (let urlPrefix of iterSources(test, forbiddenSources)) {
      forbiddenURLs.add(urlPrefix);
    }
  }

  return { expectedURLs, forbiddenURLs, blockedURLs: forbiddenURLs };
}

/**
 * @typedef InjectedUrl
 *        A URL present in styles injected by the content script.
 * @type {object}
 * @property {string} origin
 *        The origin of the URL, one of "page", "contentScript", or "extension".
 * @param {string} href
 *        The URL string.
 * @param {boolean} inline
 *        If true, the URL is present in an inline stylesheet, which may be
 *        blocked by CSP prior to parsing, depending on its origin.
 */

/**
 * @typedef InjectedSource
 *        An inline CSS source injected by the content script.
 * @type {object}
 * @param {string} origin
 *        The origin of the CSS, one of "page", "contentScript", or "extension".
 * @param {string} css
 *        The CSS source text.
 */

/**
 * Generates a set of expected and forbidden URLs and sources based on the CSS
 * injected by our content script.
 *
 * @param {object} message
 *        The "css-sources" message sent by the content script, containing lists
 *        of CSS sources injected into the page.
 * @param {Array<InjectedUrl>} message.urls
 *        A list of URLs present in styles injected by the content script.
 * @param {Array<InjectedSource>} message.sources
 *        A list of inline CSS sources injected by the content script.
 * @param {boolean} [cspEnabled = false]
 *        If true, a strict CSP is enabled for this page, and inline page
 *        sources should be blocked. URLs present in these sources will not be
 *        expected to generate a CSP report, the inline sources themselves will.
 * @param {boolean} [contentCspEnabled = false]
 * @returns {RequestedURLs}
 */
function computeExpectedForbiddenURLs(
  { urls, sources },
  cspEnabled = false,
  contentCspEnabled = false
) {
  let expectedURLs = new Set();
  let forbiddenURLs = new Set();
  let blockedURLs = new Set();
  let blockedSources = new Set();

  for (let { href, origin, inline } of urls) {
    let { baseURL } = getOriginBase(href);
    if (cspEnabled && origin === "page") {
      if (inline) {
        forbiddenURLs.add(baseURL);
      } else {
        blockedURLs.add(baseURL);
      }
    } else if (contentCspEnabled && origin === "contentScript") {
      if (inline) {
        forbiddenURLs.add(baseURL);
      }
    } else {
      expectedURLs.add(baseURL);
    }
  }

  if (cspEnabled) {
    for (let { origin, css } of sources) {
      if (origin === "page") {
        blockedSources.add(css);
      }
    }
  }

  return { expectedURLs, forbiddenURLs, blockedURLs, blockedSources };
}

/**
 * Awaits the content loads for each of the given expected base URLs,
 * and checks that their origin strings are as expected. Triggers a test
 * failure if any of the given forbidden URLs is requested.
 *
 * @param {Promise<object>} urlsPromise
 *        A promise which resolves to an object containing expected and
 *        forbidden URL sets, as returned by {@see computeBaseURLs}.
 * @param {Object<string, string>} origins
 *        A mapping of origin parameters as they appear in URL query
 *        strings to the origin strings returned by corresponding
 *        principals. These values are used to test requests against
 *        their expected origins.
 * @returns {Promise}
 *        A promise which resolves when all requests have been
 *        processed.
 */
function awaitLoads(urlsPromise, origins) {
  return new Promise(resolve => {
    let expectedURLs, forbiddenURLs;
    let queuedChannels = [];

    let observer;

    function checkChannel(channel) {
      let origURL = channel.URI.spec;
      let { baseURL, origin } = getOriginBase(origURL);

      if (forbiddenURLs.has(baseURL)) {
        ok(false, `Got unexpected request for forbidden URL ${origURL}`);
      }

      if (expectedURLs.has(baseURL)) {
        expectedURLs.delete(baseURL);

        equal(
          channel.loadInfo.triggeringPrincipal.origin,
          origins[origin],
          `Got expected origin for URL ${origURL}`
        );

        if (!expectedURLs.size) {
          Services.obs.removeObserver(observer, "http-on-modify-request");
          info("Got all expected requests");
          resolve();
        }
      }
    }

    urlsPromise.then(urls => {
      expectedURLs = new Set(urls.expectedURLs);
      forbiddenURLs = new Set([...urls.forbiddenURLs, ...urls.blockedURLs]);

      for (let channel of queuedChannels.splice(0)) {
        checkChannel(channel.QueryInterface(Ci.nsIChannel));
      }
    });

    observer = (channel, topic, data) => {
      if (expectedURLs) {
        checkChannel(channel.QueryInterface(Ci.nsIChannel));
      } else {
        queuedChannels.push(channel);
      }
    };
    Services.obs.addObserver(observer, "http-on-modify-request");
  });
}

function readUTF8InputStream(stream) {
  let buffer = NetUtil.readInputStream(stream, stream.available());
  return new TextDecoder().decode(buffer);
}

/**
 * Awaits CSP reports for each of the given forbidden base URLs.
 * Triggers a test failure if any of the given expected URLs triggers a
 * report.
 *
 * @param {Promise<object>} urlsPromise
 *        A promise which resolves to an object containing expected and
 *        forbidden URL sets, as returned by {@see computeBaseURLs}.
 * @returns {Promise}
 *        A promise which resolves when all requests have been
 *        processed.
 */
function awaitCSP(urlsPromise) {
  return new Promise(resolve => {
    let expectedURLs, blockedURLs, blockedSources;
    let queuedRequests = [];

    function checkRequest(request) {
      let body = JSON.parse(readUTF8InputStream(request.bodyInputStream));
      let report = body["csp-report"];

      let origURL = report["blocked-uri"];
      if (origURL !== "inline" && origURL !== "") {
        let { baseURL } = getOriginBase(origURL);

        if (expectedURLs.has(baseURL)) {
          ok(false, `Got unexpected CSP report for allowed URL ${origURL}`);
        }

        if (blockedURLs.has(baseURL)) {
          blockedURLs.delete(baseURL);

          ok(true, `Got CSP report for forbidden URL ${origURL}`);
        }
      }

      let source = report["script-sample"];
      if (source) {
        if (blockedSources.has(source)) {
          blockedSources.delete(source);

          ok(
            true,
            `Got CSP report for forbidden inline source ${JSON.stringify(
              source
            )}`
          );
        }
      }

      if (!blockedURLs.size && !blockedSources.size) {
        ok(true, "Got all expected CSP reports");
        resolve();
      }
    }

    urlsPromise.then(urls => {
      blockedURLs = new Set(urls.blockedURLs);
      blockedSources = new Set(urls.blockedSources);
      ({ expectedURLs } = urls);

      for (let request of queuedRequests.splice(0)) {
        checkRequest(request);
      }
    });

    server.registerPathHandler(CSP_REPORT_PATH, (request, response) => {
      response.setStatusLine(request.httpVersion, 204, "No Content");

      if (expectedURLs) {
        checkRequest(request);
      } else {
        queuedRequests.push(request);
      }
    });
  });
}

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
    // the blocked-uri for frame-navigations is the pre-path URI. For the
    // purpose of this test we do not strip the blocked-uri by setting the
    // preference 'truncate_blocked_uri_for_frame_navigations'
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
    element: ["input", { type: "image" }],
    src: "input.png",
  },
  {
    element: ["link", { rel: "stylesheet" }],
    src: "link.css",
    srcAttr: "href",
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

for (let test of TESTS) {
  if (!test.srcAttr) {
    test.srcAttr = "src";
  }
  if (!("liveSrc" in test)) {
    test.liveSrc = true;
  }
}

/**
 * A set of sources for which each of the above tests is expected to
 * generate one request, if each of the properties in the value object
 * matches the value of the same property in the test object.
 */
// Sources which load with the page context.
const PAGE_SOURCES = {
  "contentScript-content-attr-after-inject": { liveSrc: true },
  "contentScript-content-change-after-inject": { liveSrc: true },
  "contentScript-inject-after-content-attr": {},
  "contentScript-relative-url": {},
  pageHTML: {},
  pageScript: {},
  "pageScript-attr-after-inject": {},
  "pageScript-prop": {},
  "pageScript-prop-after-inject": {},
  "pageScript-relative-url": {},
};
// Sources which load with the extension context.
const EXTENSION_SOURCES = {
  contentScript: {},
  "contentScript-attr-after-inject": { liveSrc: true },
  "contentScript-content-inject-after-attr": {},
  "contentScript-prop": {},
  "contentScript-prop-after-inject": {},
};
// When our default content script CSP is applied, only
// liveSrc: true are loading.  IOW, the "script" test above
// will fail.
const EXTENSION_SOURCES_CONTENT_CSP = {
  contentScript: { liveSrc: true },
  "contentScript-attr-after-inject": { liveSrc: true },
  "contentScript-content-inject-after-attr": { liveSrc: true },
  "contentScript-prop": { liveSrc: true },
  "contentScript-prop-after-inject": { liveSrc: true },
};
// All sources.
const SOURCES = Object.assign({}, PAGE_SOURCES, EXTENSION_SOURCES);

registerStaticPage(
  "/page.html",
  `<!DOCTYPE html>
  <html lang="en">
  <head>
    <meta charset="UTF-8">
    <title></title>
    <script nonce="deadbeef">
      ${getInjectionScript(TESTS, { source: "pageScript", origin: "page" })}
    </script>
  </head>
  <body>
    ${TESTS.map(test =>
      toHTML(test, { source: "pageHTML", origin: "page" })
    ).join("\n  ")}
  </body>
  </html>`
);

function catchViolation() {
  // eslint-disable-next-line mozilla/balanced-listeners
  document.addEventListener("securitypolicyviolation", e => {
    browser.test.assertTrue(
      e.documentURI !== "moz-extension",
      `securitypolicyviolation: ${e.violatedDirective} ${e.documentURI}`
    );
  });
}

const EXTENSION_DATA = {
  manifest: {
    content_scripts: [
      {
        matches: ["http://*/page.html"],
        run_at: "document_start",
        js: ["violation.js", "content_script.js"],
      },
    ],
  },

  files: {
    "violation.js": catchViolation,
    "content_script.js": getInjectionScript(TESTS, {
      source: "contentScript",
      origin: "contentScript",
    }),
  },
};

const pageURL = `${BASE_URL}/page.html`;
const pageURI = Services.io.newURI(pageURL);

// Merges the sets of expected URL and source data returned by separate
// computedExpectedForbiddenURLs and computedBaseURLs calls.
function mergeSources(a, b) {
  return {
    expectedURLs: new Set([...a.expectedURLs, ...b.expectedURLs]),
    forbiddenURLs: new Set([...a.forbiddenURLs, ...b.forbiddenURLs]),
    blockedURLs: new Set([...a.blockedURLs, ...b.blockedURLs]),
    blockedSources: a.blockedSources || b.blockedSources,
  };
}

// Returns a set of origin strings for the given extension and content page, for
// use in verifying request triggering principals.
function getOrigins(extension) {
  return {
    page: Services.scriptSecurityManager.createContentPrincipal(pageURI, {})
      .origin,
    contentScript: Cu.getObjectPrincipal(
      Cu.Sandbox([pageURL, extension.principal])
    ).origin,
    extension: extension.principal.origin,
  };
}

/**
 * Tests that various types of inline content elements initiate requests
 * with the triggering pringipal of the caller that requested the load.
 */
add_task(async function test_contentscript_triggeringPrincipals() {
  let extension = ExtensionTestUtils.loadExtension(EXTENSION_DATA);
  await extension.startup();

  let urlsPromise = extension.awaitMessage("css-sources").then(msg => {
    return mergeSources(
      computeExpectedForbiddenURLs(msg),
      computeBaseURLs(TESTS, SOURCES)
    );
  });

  let origins = getOrigins(extension.extension);
  let finished = awaitLoads(urlsPromise, origins);

  let contentPage = await ExtensionTestUtils.loadContentPage(pageURL);

  await finished;

  await extension.unload();
  await contentPage.close();

  clearCache();
});

/**
 * Tests that the correct CSP is applied to loads of inline content
 * depending on whether the load was initiated by an extension or the
 * content page.
 */
add_task(async function test_contentscript_csp() {
  // TODO bug 1408193: We currently don't get the full set of CSP reports when
  // running in network scheduling chaos mode. It's not entirely clear why.
  let chaosMode = parseInt(Services.env.get("MOZ_CHAOSMODE"), 16);
  let checkCSPReports = !(chaosMode === 0 || chaosMode & 0x02);

  gContentSecurityPolicy = `default-src 'none' 'report-sample'; script-src 'nonce-deadbeef' 'unsafe-eval' 'report-sample'; report-uri ${CSP_REPORT_PATH};`;

  let extension = ExtensionTestUtils.loadExtension(EXTENSION_DATA);
  await extension.startup();

  let urlsPromise = extension.awaitMessage("css-sources").then(msg => {
    return mergeSources(
      computeExpectedForbiddenURLs(msg, true),
      computeBaseURLs(TESTS, EXTENSION_SOURCES, PAGE_SOURCES)
    );
  });

  let origins = getOrigins(extension.extension);

  let finished = Promise.all([
    awaitLoads(urlsPromise, origins),
    checkCSPReports && awaitCSP(urlsPromise),
  ]);

  let contentPage = await ExtensionTestUtils.loadContentPage(pageURL);

  await finished;

  await extension.unload();
  await contentPage.close();
});

/**
 * Tests that the correct CSP is applied to loads of inline content
 * depending on whether the load was initiated by an extension or the
 * content page.
 */
add_task(async function test_extension_contentscript_csp() {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);

  // TODO bug 1408193: We currently don't get the full set of CSP reports when
  // running in network scheduling chaos mode. It's not entirely clear why.
  let chaosMode = parseInt(Services.env.get("MOZ_CHAOSMODE"), 16);
  let checkCSPReports = !(chaosMode === 0 || chaosMode & 0x02);

  gContentSecurityPolicy = `default-src 'none' 'report-sample'; script-src 'nonce-deadbeef' 'unsafe-eval' 'report-sample'; report-uri ${CSP_REPORT_PATH};`;

  let data = {
    ...EXTENSION_DATA,
    manifest: {
      ...EXTENSION_DATA.manifest,
      manifest_version: 3,
      host_permissions: ["http://example.com/*"],
      granted_host_permissions: true,
    },
    temporarilyInstalled: true,
  };

  let extension = ExtensionTestUtils.loadExtension(data);
  await extension.startup();

  let urlsPromise = extension.awaitMessage("css-sources").then(msg => {
    return mergeSources(
      computeExpectedForbiddenURLs(msg, true, true),
      computeBaseURLs(TESTS, EXTENSION_SOURCES_CONTENT_CSP, PAGE_SOURCES)
    );
  });

  let origins = getOrigins(extension.extension);

  let finished = Promise.all([
    awaitLoads(urlsPromise, origins),
    checkCSPReports && awaitCSP(urlsPromise),
  ]);

  let contentPage = await ExtensionTestUtils.loadContentPage(pageURL);

  await finished;

  await extension.unload();
  await contentPage.close();
  Services.prefs.clearUserPref("extensions.manifestV3.enabled");
});
