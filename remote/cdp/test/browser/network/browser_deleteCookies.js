/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SJS_PATH = "/browser/remote/cdp/test/browser/network/sjs-cookies.sjs";

const DEFAULT_HOST = "http://example.org";
const DEFAULT_HOSTNAME = "example.org";
const ALT_HOST = "http://example.net";
const SECURE_HOST = "https://example.com";

const DEFAULT_URL = `${DEFAULT_HOST}${SJS_PATH}`;

// Bug 1617611: Fix all the tests broken by "cookies SameSite=lax by default"
Services.prefs.setBoolPref("network.cookie.sameSite.laxByDefault", false);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("network.cookie.sameSite.laxByDefault");
});

add_task(async function failureWithoutArguments({ client }) {
  const { Network } = client;

  await Assert.rejects(
    Network.deleteCookies(),
    err => err.message.includes("name: string value expected"),
    "Fails without any arguments"
  );
});

add_task(async function failureWithoutDomainOrURL({ client }) {
  const { Network } = client;

  await Assert.rejects(
    Network.deleteCookies({ name: "foo" }),
    err =>
      err.message.includes(
        "At least one of the url and domain needs to be specified"
      ),
    "Fails without domain or URL"
  );
});

add_task(async function failureWithInvalidProtocol({ client }) {
  const { Network } = client;

  const FTP_URL = `ftp://${DEFAULT_HOSTNAME}`;

  await Assert.rejects(
    Network.deleteCookies({ name: "foo", url: FTP_URL }),
    err => err.message.includes("An http or https url must be specified"),
    "Fails for invalid protocol in URL"
  );
});

add_task(async function pristineContext({ client }) {
  const { Network } = client;

  await loadURL(DEFAULT_URL);

  const { cookies } = await Network.getCookies();
  is(cookies.length, 0, "No cookies have been found");

  await Network.deleteCookies({ name: "foo", url: DEFAULT_URL });
});

add_task(async function fromHostWithPort({ client }) {
  const { Network } = client;

  const PORT_URL = `${DEFAULT_HOST}:8000${SJS_PATH}`;
  await loadURL(PORT_URL + "?name=id&value=1");

  const cookie = {
    name: "id",
    value: "1",
  };

  try {
    const { cookies: before } = await Network.getCookies();
    is(before.length, 1, "A cookie has been found");

    await Network.deleteCookies({ name: cookie.name, url: PORT_URL });

    const { cookies: after } = await Network.getCookies();
    is(after.length, 0, "No cookie has been found");
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function forSpecificDomain({ client }) {
  const { Network } = client;

  const ALT_URL = ALT_HOST + SJS_PATH;

  await loadURL(`${ALT_URL}?name=foo&value=bar`);
  await loadURL(`${DEFAULT_URL}?name=foo&value=bar`);

  const cookie = {
    name: "foo",
    value: "bar",
    domain: "example.net",
  };

  try {
    const { cookies: before } = await Network.getCookies();
    is(before.length, 1, "A cookie has been found");

    await Network.deleteCookies({
      name: cookie.name,
      domain: DEFAULT_HOSTNAME,
    });

    const { cookies: after } = await Network.getCookies();
    is(after.length, 0, "No cookie has been found");

    await loadURL(ALT_URL);

    const { cookies: other } = await Network.getCookies();
    is(other.length, 1, "A cookie has been found");
    assertCookie(other[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function forSpecificURL({ client }) {
  const { Network } = client;

  const ALT_URL = ALT_HOST + SJS_PATH;

  await loadURL(`${ALT_URL}?name=foo&value=bar`);
  await loadURL(`${DEFAULT_URL}?name=foo&value=bar`);

  const cookie = {
    name: "foo",
    value: "bar",
    domain: "example.net",
  };

  try {
    const { cookies: before } = await Network.getCookies();
    is(before.length, 1, "A cookie has been found");

    await Network.deleteCookies({ name: cookie.name, url: DEFAULT_URL });

    const { cookies: after } = await Network.getCookies();
    is(after.length, 0, "No cookie has been found");

    await loadURL(ALT_URL);

    const { cookies: other } = await Network.getCookies();
    is(other.length, 1, "A cookie has been found");
    assertCookie(other[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function forSecureURL({ client }) {
  const { Network } = client;

  const SECURE_URL = `${SECURE_HOST}${SJS_PATH}`;

  await loadURL(`${DEFAULT_URL}?name=foo&value=bar`);
  await loadURL(`${SECURE_URL}?name=foo&value=bar`);

  const cookie = {
    name: "foo",
    value: "bar",
    domain: "example.org",
  };

  try {
    const { cookies: before } = await Network.getCookies();
    is(before.length, 1, "A cookie has been found");

    await Network.deleteCookies({ name: cookie.name, url: SECURE_URL });

    const { cookies: after } = await Network.getCookies();
    is(after.length, 0, "No cookie has been found");

    await loadURL(DEFAULT_URL);

    const { cookies: other } = await Network.getCookies();
    is(other.length, 1, "A cookie has been found");
    assertCookie(other[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function forSpecificDomainAndURL({ client }) {
  const { Network } = client;

  const ALT_URL = ALT_HOST + SJS_PATH;

  await loadURL(`${ALT_URL}?name=foo&value=bar`);
  await loadURL(`${DEFAULT_URL}?name=foo&value=bar`);

  const cookie = {
    name: "foo",
    value: "bar",
    domain: "example.net",
  };

  try {
    const { cookies: before } = await Network.getCookies();
    is(before.length, 1, "A cookie has been found");

    // Domain has precedence before URL
    await Network.deleteCookies({
      name: cookie.name,
      domain: DEFAULT_HOSTNAME,
      url: ALT_URL,
    });

    const { cookies: after } = await Network.getCookies();
    is(after.length, 0, "No cookie has been found");

    await loadURL(ALT_URL);

    const { cookies: other } = await Network.getCookies();
    is(other.length, 1, "A cookie has been found");
    assertCookie(other[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function path({ client }) {
  const { Network } = client;

  const PATH = "/browser/remote/cdp/test/browser/";
  const PARENT_PATH = "/browser/remote/cdp/test/";
  const SUB_PATH = "/browser/remote/cdp/test/browser/network/";

  const cookie = {
    name: "foo",
    value: "bar",
    path: PATH,
  };

  try {
    console.log("Check exact path");
    await loadURL(`${DEFAULT_URL}?name=foo&value=bar&path=${PATH}`);
    let result = await Network.getCookies();
    is(result.cookies.length, 1, "A single cookie has been found");

    await Network.deleteCookies({
      name: cookie.name,
      path: PATH,
      url: DEFAULT_URL,
    });
    result = await Network.getCookies();
    is(result.cookies.length, 0, "No cookie has been found");

    console.log("Check sub path");
    await loadURL(`${DEFAULT_URL}?name=foo&value=bar&path=${PATH}`);
    result = await Network.getCookies();
    is(result.cookies.length, 1, "A single cookie has been found");

    await Network.deleteCookies({
      name: cookie.name,
      path: SUB_PATH,
      url: DEFAULT_URL,
    });
    result = await Network.getCookies();
    is(result.cookies.length, 1, "A single cookie has been found");

    console.log("Check parent path");
    await loadURL(`${DEFAULT_URL}?name=foo&value=bar&path=${PATH}`);
    result = await Network.getCookies();
    is(result.cookies.length, 1, "A single cookie has been found");

    await Network.deleteCookies({
      name: cookie.name,
      path: PARENT_PATH,
      url: DEFAULT_URL,
    });
    result = await Network.getCookies();
    is(result.cookies.length, 0, "No cookie has been found");

    console.log("Check non matching path");
    await loadURL(`${DEFAULT_URL}?name=foo&value=bar&path=${PATH}`);
    result = await Network.getCookies();
    is(result.cookies.length, 1, "A single cookie has been found");

    await Network.deleteCookies({
      name: cookie.name,
      path: "/foo/bar",
      url: DEFAULT_URL,
    });
    result = await Network.getCookies();
    is(result.cookies.length, 1, "A single cookie has been found");
  } finally {
    Services.cookies.removeAll();
  }
});
