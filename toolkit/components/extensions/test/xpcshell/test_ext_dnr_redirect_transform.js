"use strict";

// The validate_action_redirect_transform task of test_ext_dnr_session_rules.js
// confirms that redirect transform rules meet some minimum bar of validation.
// Despite passing validation, there are still interesting cases to explore,
// ranging from verifying that special characters appear as expected, to
// verifying that an invalid URL (e.g. too long after the transform) is handled
// reasonably well.

add_setup(() => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);

  // Allow navigation to URLs with embedded credentials, without prompt.
  Services.prefs.setBoolPref("network.auth.confirmAuth.enabled", false);
});

const server = createHttpServer({
  hosts: ["from", "dest", "127.0.0.127", "[::1]", "xn--stra-yna.de", "fqdn."],
});
server.identity.add("http", "dest", 443); // test_redirect_transform_port
server.identity.add("http", "dest", 700); // test_redirect_transform_port
server.identity.add("http", "dest", 777); // Dummy port in test cases.

server.registerPrefixHandler("/", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.write("GOOD_RESPONSE");
});

// This function is serialized and called in the context of the test extension's
// background page. dnrTestUtils is passed to the background function.
function makeDnrTestUtils() {
  const dnrTestUtils = {};
  const dnr = browser.declarativeNetRequest;
  function makeRedirectTransformRule(transform) {
    return {
      id: 1,
      condition: { requestDomains: ["from"] },
      action: {
        type: "redirect",
        // redirect to "dest" by default, different from "from", to avoid an
        // infinite redirect loop.
        redirect: { transform: { host: "dest", ...transform } },
      },
    };
  }
  async function setRedirectTransform(transform) {
    await dnr.updateSessionRules({
      removeRuleIds: [1],
      addRules: [makeRedirectTransformRule(transform)],
    });
  }
  // testFetch is simple/fast, but cannot always be used:
  // - when the request URL contains embedded credentials.
  // - when the final URL is supposed to contain a reference fragment.
  async function testFetch(from, to, description) {
    let res = await fetch(from);
    browser.test.assertEq(to, res.url, description);
    browser.test.assertEq("GOOD_RESPONSE", await res.text(), "expected body");
  }
  // testNavigate is the slower, complex version of testFetch. It should be
  // used in tests where the username, password or fragment components of a URL
  // are significant.
  async function testNavigate(from, to, description) {
    let resultPromise = new Promise(resolve => {
      browser.test.onMessage.addListener(function listener(msg, result) {
        if (msg === "test_navigate_result") {
          browser.test.onMessage.removeListener(listener);
          // resolve only resolves on the first call, which is ideal because
          // browser.test.onMessage.removeListener does not work (bug 1428213).
          resolve(result);
        }
      });
    });
    browser.test.sendMessage("test_navigate", from);
    browser.test.assertDeepEq({ from, to }, await resultPromise, description);
  }
  Object.assign(dnrTestUtils, {
    makeRedirectTransformRule,
    setRedirectTransform,
    testFetch,
    testNavigate,
  });
  return dnrTestUtils;
}

async function runAsDNRExtension({ background, manifest }) {
  let extension = ExtensionTestUtils.loadExtension({
    background: `(${background})((${makeDnrTestUtils})())`,
    allowInsecureRequests: true,
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest"],
      host_permissions: ["<all_urls>"],
      granted_host_permissions: true,
      web_accessible_resources: [
        { resources: ["war.txt"], matches: ["http://from/*"] },
      ],
      ...manifest,
    },
    temporarilyInstalled: true, // <-- for granted_host_permissions
    files: {
      "war.txt": "GOOD_RESPONSE",
      "nowar.txt": "nowar.txt is not in web_accessible_resources",
    },
  });
  extension.onMessage("test_navigate", async url => {
    // The DNR rule does not redirect the main frame.
    let contentPage = await ExtensionTestUtils.loadContentPage("http://from/");
    info(`Loading ${url}`);
    await contentPage.spawn([url], async url => {
      let { document } = this.content;
      let frame = document.createElement("iframe");
      frame.src = url;
      await new Promise(resolve => {
        frame.onload = resolve;
        document.body.appendChild(frame);
      });
    });
    let finalURL = contentPage.browsingContext.children[0].currentURI.spec;
    await contentPage.close();
    extension.sendMessage("test_navigate_result", { from: url, to: finalURL });
  });
  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
}

add_task(async function test_redirect_transform_all_at_once() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { setRedirectTransform, testFetch, testNavigate } = dnrTestUtils;

      await setRedirectTransform({
        scheme: "http",
        username: "a",
        password: "b",
        host: "dest",
        port: "777",
        path: "/d",
        query: "?e",
        queryTransform: null,
        fragment: "#f",
      });
      await testFetch(
        "https://from",
        "http://a:b@dest:777/d?e", // note: fetch cannot see '#f'.
        "Adds components to minimal URL (fetch)"
      );
      await testNavigate(
        "https://from",
        "http://a:b@dest:777/d?e#f",
        "Adds components to minimal URL (navigation)"
      );

      await browser.test.assertRejects(
        testFetch("https://user:pass@from:777/path?query#ref"),
        "Window.fetch: https://user:pass@from:777/path?query#ref is an url with embedded credentials.",
        "fetch does not work with embedded credentials"
      );
      await testNavigate(
        "https://user:pass@from:777/path?query#ref",
        "http://a:b@dest:777/d?e#f",
        "Replaces all components in existing URL (navigation)"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function test_redirect_transform_scheme() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { setRedirectTransform, testFetch, testNavigate } = dnrTestUtils;

      await setRedirectTransform({ scheme: "http" });
      await testFetch("https://from/", "http://dest/", "scheme change");
      await testNavigate(
        "https://user:pass@from:777/path?query#ref",
        "http://user:pass@dest:777/path?query#ref",
        "scheme change in complex URL with embedded credentials"
      );

      await setRedirectTransform({
        scheme: "moz-extension",
        host: location.hostname,
      });
      await testFetch(
        "http://from/war.txt",
        browser.runtime.getURL("war.txt"),
        "Scheme change to moz-extension:-URL"
      );
      await testNavigate(
        "http://from/war.txt",
        browser.runtime.getURL("war.txt"),
        "Scheme change to moz-extension:-URL (navigation)"
      );
      // While the initiator (extension) would be allowed to read the resource
      // due to it being same-origin, the pre-redirect URL (http://from) is not
      // matching web_accessible_resources[].matches, so the load is rejected.
      // This scenario is also tested in test_ext_dnr_without_webrequest.js, at
      // the redirect_request_with_dnr_to_extensionPath task.
      await browser.test.assertRejects(
        testFetch("http://from/nowar.txt"),
        "NetworkError when attempting to fetch resource.",
        "Cannot load redirect to moz-extension: not in web_accessible_resources"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function test_redirect_transform_username() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { setRedirectTransform, testFetch, testNavigate } = dnrTestUtils;

      await setRedirectTransform({ username: "" });
      await testNavigate(
        "http://user:pass@from:777/path?query#ref",
        "http://:pass@dest:777/path?query#ref",
        "username cleared"
      );

      await setRedirectTransform({ username: "new" });
      // Cannot pass credentials to fetch, but can read from response.url:
      await testFetch("http://from/", "http://new@dest/", "username added");
      await testNavigate("http://from/", "http://new@dest/", "username added");
      await testNavigate(
        "http://user:pass@from:777/path?query#ref",
        "http://new:pass@dest:777/path?query#ref",
        "username changed"
      );

      await setRedirectTransform({ username: "new User:name@%%20/" });
      await testNavigate(
        "http://user:pass@from:777/path?query#ref",
        "http://new%20User%3Aname%40%%20%2F:pass@dest:777/path?query#ref",
        "username changed to complex value"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function test_redirect_transform_password() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { setRedirectTransform, testFetch, testNavigate } = dnrTestUtils;

      await setRedirectTransform({ password: "" });
      await testNavigate(
        "http://user:pass@from:777/path?query#ref",
        "http://user@dest:777/path?query#ref",
        "password cleared"
      );

      await setRedirectTransform({ password: "new" });
      // Cannot pass credentials to fetch, but can read from response.url:
      await testFetch("http://from/", "http://:new@dest/", "password added");
      await testNavigate("http://from/", "http://:new@dest/", "password added");
      await testNavigate(
        "http://user:pass@from:777/path?query#ref",
        "http://user:new@dest:777/path?query#ref",
        "password changed"
      );

      await setRedirectTransform({ password: "new Pass:@%%20/" });
      await testNavigate(
        "http://user:pass@from:777/path?query#ref",
        "http://user:new%20Pass%3A%40%%20%2F@dest:777/path?query#ref",
        "password changed to complex value"
      );
      browser.test.notifyPass();
    },
  });
});

add_task(async function test_redirect_transform_host() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { setRedirectTransform, testFetch, testNavigate } = dnrTestUtils;

      await setRedirectTransform({ host: "dest" });
      await testFetch(
        "http://from:777/path?query",
        "http://dest:777/path?query",
        "host changed"
      );
      await testNavigate(
        "http://user:pass@from:777/path?query#ref",
        "http://user:pass@dest:777/path?query#ref",
        "host changed without affecting embedded credentials"
      );

      await setRedirectTransform({ host: "DEST" });
      await testFetch(
        "http://from/",
        "http://dest/",
        "host changed (non-canonical, upper case)"
      );

      await setRedirectTransform({ host: "%44%65%73%54" }); // "DesT", escaped.
      await testFetch(
        "http://from:777/",
        "http://dest:777/",
        "host changed (non-canonical, percent-escaped)"
      );

      await setRedirectTransform({ host: "127.0.0.127" });
      await testFetch(
        "http://from/",
        "http://127.0.0.127/",
        "host change to IPv4"
      );

      await setRedirectTransform({ host: "[::1]" });
      await testFetch("http://from/", "http://[::1]/", "host change to IPv6");

      await setRedirectTransform({ host: "xn--stra-yna.de" });
      await testFetch(
        "http://from/",
        "http://xn--stra-yna.de/",
        "host change to IDN (internationalized domain name, in punycode)"
      );

      await setRedirectTransform({ host: "straß.de" });
      await testFetch(
        "http://from/",
        "http://xn--stra-yna.de/",
        "host change to IDN (not punycode-encoded)"
      );

      await setRedirectTransform({ host: "fqdn." });
      await testFetch(
        "http://from/",
        "http://fqdn./",
        "host change to FQDN (fully-qualified domain name)"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function test_redirect_transform_port() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { setRedirectTransform, testFetch, testNavigate } = dnrTestUtils;

      await setRedirectTransform({ port: "" });
      await testFetch("http://from:777/", "http://dest/", "port cleared");
      await testNavigate(
        "http://user:pass@from:777/path?query#ref",
        "http://user:pass@dest/path?query#ref",
        "port cleared from URL with embedded credentials"
      );

      await setRedirectTransform({ port: "700" });
      await testFetch("http://from/", "http://dest:700/", "port added");
      await testFetch("http://from:777/", "http://dest:700/", "port changed");

      // 0-padded should not be misinterpreted as an octal number.
      await setRedirectTransform({ port: "0700" });
      await testFetch(
        "http://from:777/",
        "http://dest:700/",
        "port changed (non-canonical, 0-padded port)"
      );

      await setRedirectTransform({ port: "80" });
      await testFetch(
        "http://from:777/",
        "http://dest/",
        "port cleared if default protocol"
      );

      await setRedirectTransform({ scheme: "http", port: "443" });
      await testFetch(
        "https://from/",
        "http://dest:443/",
        "port added if new port is not default port of new protocol"
      );

      await setRedirectTransform({ scheme: "http", port: "80" });
      await testFetch(
        "https://from:777/",
        "http://dest/",
        "port cleared if new port is default port of new protocol"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function test_redirect_transform_path() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { setRedirectTransform, testFetch, testNavigate } = dnrTestUtils;

      await setRedirectTransform({ path: "" });
      await testFetch("http://from/path", "http://dest/", "path cleared");
      await testNavigate(
        "http://user:pass@from:777/path?query#ref",
        "http://user:pass@dest:777/?query#ref",
        "path cleared from URL with embedded credentials"
      );

      await setRedirectTransform({ path: "/new" });
      await testFetch("http://from/", "http://dest/new", "path added");
      await testFetch("http://from/path", "http://dest/new", "path changed");

      await setRedirectTransform({ path: "///" });
      await testFetch("http://from/", "http://dest///", "path added (///)");

      await setRedirectTransform({ path: "path" });
      await testFetch(
        "http://from/",
        "http://dest/path",
        "path added (non-canonical, missing slash)"
      );

      // " " -> "%20" (space)
      // "\x00" -> "%00" (null byte)
      // "<>" -> "%3C%3E" (URL encoding of angle brackets)
      // "%", "%20", "%3A", "%3a" -> not changed (%-encoding kept as-is).
      await setRedirectTransform({ path: "/Path_%_ _%20_?_#_\x00_<>_%3A%3a" });
      await testFetch(
        "http://from/",
        "http://dest/Path_%_%20_%20_%3F_%23_%00_%3C%3E_%3A%3a",
        "path added (non-canonical, partial percent encoding)"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function test_redirect_transform_query() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { setRedirectTransform, testFetch, testNavigate } = dnrTestUtils;

      await setRedirectTransform({ query: "" });
      await testFetch("http://from/?query", "http://dest/", "query cleared");
      await testNavigate(
        "http://user:pass@from:777/path?query#ref",
        "http://user:pass@dest:777/path#ref",
        "query cleared from URL with embedded credentials"
      );

      await setRedirectTransform({ query: "?new" });
      await testFetch("http://from/", "http://dest/?new", "query added");
      await testFetch(
        "http://from/?query",
        "http://dest/?new",
        "query changed"
      );

      await setRedirectTransform({ query: "?" });
      await testFetch("http://from/", "http://dest/?", "query set to just '?'");

      await setRedirectTransform({ query: "?Query_#_ _%20_%3a%3A_<>_\x00" });
      await testFetch(
        "http://from/",
        "http://dest/?Query_%23_%20_%20_%3a%3A_%3C%3E_%00",
        "query added (non-canonical, partial percent encoding)"
      );

      // Now rule.action.redirect.transform.queryTransform:
      await setRedirectTransform({
        queryTransform: {
          removeParams: ["query"],
        },
      });
      await testFetch(
        "http://from/?query",
        "http://dest/",
        "queryTransform removed query"
      );
      await testFetch(
        "http://from/?prefix&query&suffix",
        "http://dest/?prefix&suffix",
        "queryTransform removed part of query"
      );
      await testFetch(
        "http://from/?query&aquery&queryb&query=withvalue&not=query&QUERY&",
        "http://dest/?aquery&queryb&not=query&QUERY&",
        "queryTransform removed all occurrences of 'query' key"
      );
      await testFetch(
        "http://from/??query",
        "http://dest/??query",
        "queryTransform does not match param when it starts with '??'"
      );

      await setRedirectTransform({
        queryTransform: {
          removeParams: ["query"],
          addOrReplaceParams: [{ key: "query", value: "newvalue" }],
        },
      });
      await testFetch(
        "http://from/",
        "http://dest/?query=newvalue",
        "queryTransform appended query despite new param being in removeParams"
      );
      await testFetch(
        "http://from/?prefix&query&suffix",
        "http://dest/?prefix&suffix&query=newvalue",
        "queryTransform removed query, and appended new value"
      );
      await testFetch(
        "http://from/??query",
        "http://dest/??query&query=newvalue",
        "queryTransform ignores existing param starting with '??', and appends"
      );

      await setRedirectTransform({
        queryTransform: {
          addOrReplaceParams: [{ key: "query", value: "newvalue" }],
        },
      });
      await testFetch(
        "http://from/",
        "http://dest/?query=newvalue",
        "queryTransform appended query"
      );
      await testFetch(
        "http://from/?prefix&query=oldvalue&query=2&query=3",
        "http://dest/?prefix&query=newvalue&query=2&query=3",
        "queryTransform replaced the first occurrence and kept the others"
      );

      await setRedirectTransform({
        queryTransform: {
          addOrReplaceParams: [
            { key: "r", value: "default" }, // default:false
            { key: "r", value: "false", replaceOnly: false },
            { key: "r", value: "true", replaceOnly: true },
            { key: "r", value: "false2", replaceOnly: false },
            { key: "r", value: "true2", replaceOnly: true },
          ],
        },
      });
      // r=true and r=true2 are missing because there are no matching "r".
      await testFetch(
        "http://from/",
        "http://dest/?r=default&r=false&r=false2",
        "queryTransform appends all except replaceOnly=true"
      );
      // r=true2 should be missing because there is no matching "r".
      await testFetch(
        "http://from/?r=1&r=2&r=3&___",
        "http://dest/?r=default&r=false&r=true&___&r=false2",
        "queryTransform replaced in order and ignores last replaceOnly=true"
      );

      await setRedirectTransform({
        queryTransform: {
          addOrReplaceParams: [
            { key: "a", value: "appenda" },
            { key: "b", value: "b1" },
            { key: "c", value: "c1" },
            { key: "c", value: "c2" },
            { key: "c", value: "appendc" },
            { key: "d", value: "d1" },
          ],
        },
      });
      // Test case has:         b  c  c       d.
      // Rule only has: appenda b1 c2 appendc d1.
      // Expected out :         b1 c2         d1 appenda appendc.
      await testFetch(
        "http://from/?b=01&c=02&c=03&d=06",
        "http://dest/?b=b1&c=c1&c=c2&d=d1&a=appenda&c=appendc",
        "queryTransform replaces matched queries and appends the rest, in order"
      );

      await setRedirectTransform({
        queryTransform: {
          addOrReplaceParams: [{ key: "query", value: " _+_%00_#" }],
        },
      });
      await testFetch(
        "http://from/",
        "http://dest/?query=+_%2B_%2500_%23",
        "queryTransform urlencodes values"
      );

      // This part tests how param names with non-alphanumeric characters can be
      // (and not be) matched and replaced. This follows Chrome's behavior, see
      // https://bugzilla.mozilla.org/show_bug.cgi?id=1801870#c1
      await setRedirectTransform({
        queryTransform: {
          removeParams: ["?x", "%3Fx", "&x", "%26x"],
          addOrReplaceParams: [
            // Internally interpreted as: %3Fp:
            { key: "?p", value: "rawq", replaceOnly: true },
            // Internally interpreted as: %253Fp:
            { key: "%3Fp", value: "escape_upper_q", replaceOnly: true },
            // Internally interpreted as: %253fp:
            { key: "%3fp", value: "escape_lower_q", replaceOnly: true },
            // Internally interpreted as: %26p:
            { key: "&p", value: "rawa", replaceOnly: true },
            // Internally interpreted as: %2526p:
            { key: "%26p", value: "escape_a", replaceOnly: true },
          ],
        },
      });
      await testFetch(
        "http://from/?x&x&?x",
        "http://dest/?x&x&?x",
        "queryTransform does not match the '?' or '&' separators"
      );
      await testFetch(
        "http://from/??p&&p&?p",
        "http://dest/??p&&p&?p",
        "queryTransform cannot match literal '?p' because it is not urlencoded"
      );
      await testFetch(
        "http://from/?%3Fp",
        "http://dest/?%3Fp=rawq",
        "queryTransform matches already-urlencoded '%3Fp' with raw '?p'"
      );
      await testFetch(
        "http://from/?%3fp",
        "http://dest/?%3fp",
        "queryTransform cannot match non-canonical percent encoding (lowercase)"
      );
      await testFetch(
        "http://from/?%253fp&%253Fp",
        "http://dest/?%253fp=escape_lower_q&%253Fp=escape_upper_q",
        "queryTransform matches double-urlencoded '?p' with single-encoded '?p'"
      );
      await testFetch(
        "http://from/?%26p",
        "http://dest/?%26p=rawa",
        "queryTransform matches already-urlencoded '%26p' with raw '&p'"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function test_redirect_transform_fragment() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      // Note: not using testFetch because it cannot see fragment changes.
      const { setRedirectTransform, testNavigate } = dnrTestUtils;

      await setRedirectTransform({ fragment: "" });
      await testNavigate(
        "http://user:pass@from:777/path?query#ref",
        "http://user:pass@dest:777/path?query",
        "fragment cleared from URL with embedded credentials"
      );

      await setRedirectTransform({ fragment: "#new" });
      await testNavigate("http://from/", "http://dest/#new", "fragment added");
      await testNavigate(
        "http://from/#ref",
        "http://dest/#new",
        "fragment changed"
      );
      browser.test.notifyPass();
    },
  });
});

add_task(async function test_redirect_transform_failed_at_runtime() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { setRedirectTransform } = dnrTestUtils;

      // Maximum length of a UTL is 1048576 (network.standard-url.max-length).
      const network_standard_url_max_length = 1048576;
      // updateSessionRules does some validation on the limit (as seen by
      // validate_action_redirect_transform in test_ext_dnr_session_rules.js),
      // but it is still possible to pass validation and fail in practice when
      // the existing URL + new component exceeds the limit.
      const VERY_LONG_STRING = "x".repeat(network_standard_url_max_length - 20);

      // Like testFetch, except truncates URLs in log messages to avoid logspam.
      async function testFetchPossiblyLongUrl(from, to, body, description) {
        let res = await fetch(from);
        const shortx = s => s.replace(/x{10,}/g, xxx => `x{${xxx.length}}`);
        // VERY_LONG_STRING consists of many 'X'. Shorten to avoid logspam.
        browser.test.assertEq(shortx(to), shortx(res.url), description);
        browser.test.assertEq(body, await res.text(), "expected body");
      }

      await setRedirectTransform({ query: "?" + VERY_LONG_STRING });
      await testFetchPossiblyLongUrl(
        "http://from/short",
        `http://dest/short?${VERY_LONG_STRING}`,
        // Somehow the httpd server raises NS_ERROR_MALFORMED_URI when it tries
        // to use newURI to parse the received URL. But the server responding
        // with that implies that the redirect was successful, so for the
        // purpose of this test, that response is acceptable.
        "Bad request\n",
        "Can redirect to URL near (but not over) url max-length"
      );

      // This check confirms that not only does the request not redirect to
      // an invalid URL, but also that the request does not somehow end up in
      // an infinite redirect loop.
      await testFetchPossiblyLongUrl(
        "http://from/1234567890_1234567890",
        "http://from/1234567890_1234567890",
        "GOOD_RESPONSE",
        "Redirect to URL over max length is ignored; request continues"
      );

      browser.test.notifyPass();
    },
  });
});
