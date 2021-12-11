/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SJS_PATH = "/browser/remote/cdp/test/browser/network/sjs-cookies.sjs";

const DEFAULT_HOST = "http://example.org";
const ALT_HOST = "http://example.net";
const SECURE_HOST = "https://example.com";

const DEFAULT_URL = `${DEFAULT_HOST}${SJS_PATH}`;

add_task(async function noCookiesWhenNoneAreSet({ client }) {
  const { Network } = client;
  const { cookies } = await Network.getAllCookies();
  is(cookies.length, 0, "No cookies have been found");
});

add_task(async function noCookiesForPristineContext({ client }) {
  const { Network } = client;
  await loadURL(DEFAULT_URL);

  try {
    const { cookies } = await Network.getAllCookies();
    is(cookies.length, 0, "No cookies have been found");
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function allCookiesFromHostWithPort({ client }) {
  const { Network } = client;
  const PORT_URL = `${DEFAULT_HOST}:8000${SJS_PATH}?name=id&value=1`;
  await loadURL(PORT_URL);

  const cookie = {
    name: "id",
    value: "1",
  };

  try {
    const { cookies } = await Network.getAllCookies();
    is(cookies.length, 1, "All cookies have been found");
    assertCookie(cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function allCookiesFromMultipleOrigins({ client }) {
  const { Network } = client;
  await loadURL(`${ALT_HOST}${SJS_PATH}?name=users&value=password`);
  await loadURL(`${SECURE_HOST}${SJS_PATH}?name=secure&value=password`);
  await loadURL(`${DEFAULT_URL}?name=foo&value=bar`);

  const cookie1 = { name: "foo", value: "bar", domain: "example.org" };
  const cookie2 = { name: "secure", value: "password", domain: "example.com" };
  const cookie3 = { name: "users", value: "password", domain: "example.net" };

  try {
    const { cookies } = await Network.getAllCookies();
    cookies.sort((a, b) => a.name.localeCompare(b.name));
    is(cookies.length, 3, "All cookies have been found");
    assertCookie(cookies[0], cookie1);
    assertCookie(cookies[1], cookie2);
    assertCookie(cookies[2], cookie3);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function secure({ client }) {
  const { Network } = client;
  await loadURL(`${SECURE_HOST}${SJS_PATH}?name=foo&value=bar&secure`);

  const cookie = {
    name: "foo",
    value: "bar",
    domain: "example.com",
    secure: true,
  };

  try {
    // Cookie returned for secure protocols
    let result = await Network.getAllCookies();
    is(result.cookies.length, 1, "The secure cookie has been found");
    assertCookie(result.cookies[0], cookie);

    // For unsecure protocols the secure cookies are also returned
    await loadURL(DEFAULT_URL);
    result = await Network.getAllCookies();
    is(result.cookies.length, 1, "The secure cookie has been found");
    assertCookie(result.cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function expiry({ client }) {
  const { Network } = client;
  const date = new Date();
  date.setDate(date.getDate() + 3);

  const encodedDate = encodeURI(date.toUTCString());
  await loadURL(`${DEFAULT_URL}?name=foo&value=bar&expiry=${encodedDate}`);

  const cookie = {
    name: "foo",
    value: "bar",
    expires: Math.floor(date.getTime() / 1000),
    session: false,
  };

  try {
    const { cookies } = await Network.getAllCookies();
    is(cookies.length, 1, "A single cookie has been found");
    assertCookie(cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function session({ client }) {
  const { Network } = client;
  await loadURL(`${DEFAULT_URL}?name=foo&value=bar`);

  const cookie = {
    name: "foo",
    value: "bar",
    expiry: -1,
    session: true,
  };

  try {
    const { cookies } = await Network.getAllCookies();
    is(cookies.length, 1, "A single cookie has been found");
    assertCookie(cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function path({ client }) {
  const { Network } = client;
  const PATH = "/browser/remote/cdp/test/browser/";
  const PARENT_PATH = "/browser/remote/cdp/test/";

  await loadURL(`${DEFAULT_URL}?name=foo&value=bar&path=${PATH}`);

  const cookie = {
    name: "foo",
    value: "bar",
    path: PATH,
  };

  try {
    console.log("Check exact path");
    await loadURL(`${DEFAULT_HOST}${PATH}`);
    let result = await Network.getAllCookies();
    is(result.cookies.length, 1, "A single cookie has been found");
    assertCookie(result.cookies[0], cookie);

    console.log("Check sub path");
    await loadURL(`${DEFAULT_HOST}${SJS_PATH}`);
    result = await Network.getAllCookies();
    is(result.cookies.length, 1, "A single cookie has been found");
    assertCookie(result.cookies[0], cookie);

    console.log("Check parent path");
    await loadURL(`${DEFAULT_HOST}${PARENT_PATH}`);
    result = await Network.getAllCookies();
    is(result.cookies.length, 1, "A single cookie has been found");
    assertCookie(result.cookies[0], cookie);

    console.log("Check non matching path");
    await loadURL(`${DEFAULT_HOST}/foo/bar`);
    result = await Network.getAllCookies();
    is(result.cookies.length, 1, "A single cookie has been found");
    assertCookie(result.cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function httpOnly({ client }) {
  const { Network } = client;
  await loadURL(`${DEFAULT_URL}?name=foo&value=bar&httpOnly`);

  const cookie = {
    name: "foo",
    value: "bar",
    httpOnly: true,
  };

  try {
    const { cookies } = await Network.getAllCookies();
    is(cookies.length, 1, "A single cookie has been found");
    assertCookie(cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function sameSite({ client }) {
  const { Network } = client;
  for (const value of ["Lax", "Strict"]) {
    console.log(`Test cookie with SameSite=${value}`);
    await loadURL(`${DEFAULT_URL}?name=foo&value=bar&sameSite=${value}`);

    const cookie = {
      name: "foo",
      value: "bar",
      sameSite: value,
    };

    try {
      const { cookies } = await Network.getAllCookies();
      is(cookies.length, 1, "A single cookie has been found");
      assertCookie(cookies[0], cookie);
    } finally {
      Services.cookies.removeAll();
    }
  }
});
