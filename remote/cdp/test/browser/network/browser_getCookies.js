/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const DEFAULT_HOST = "http://example.org";
const ALT_HOST = "http://example.net";
const SECURE_HOST = "https://example.com";

const BASE_PATH = "/browser/remote/cdp/test/browser/network";
const SJS_PATH = `${BASE_PATH}/sjs-cookies.sjs`;

const DEFAULT_URL = `${DEFAULT_HOST}${SJS_PATH}`;

add_task(async function noCookiesWhenNoneAreSet({ client }) {
  const { Network } = client;
  const { cookies } = await Network.getCookies({ urls: [DEFAULT_HOST] });
  is(cookies.length, 0, "No cookies have been found");
});

add_task(async function noCookiesForPristineContext({ client }) {
  const { Network } = client;
  await loadURL(DEFAULT_URL);

  try {
    const { cookies } = await Network.getCookies();
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
    const { cookies } = await Network.getCookies();
    is(cookies.length, 1, "All cookies have been found");
    assertCookie(cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function allCookiesFromCurrentURL({ client }) {
  const { Network } = client;
  await loadURL(`${ALT_HOST}${SJS_PATH}?name=user&value=password`);
  await loadURL(`${DEFAULT_URL}?name=foo&value=bar`);
  await loadURL(`${DEFAULT_URL}?name=user&value=password`);

  const cookie1 = { name: "foo", value: "bar", domain: "example.org" };
  const cookie2 = { name: "user", value: "password", domain: "example.org" };

  try {
    const { cookies } = await Network.getCookies();
    cookies.sort((a, b) => a.name.localeCompare(b.name));
    is(cookies.length, 2, "All cookies have been found");
    assertCookie(cookies[0], cookie1);
    assertCookie(cookies[1], cookie2);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function allCookiesIncludingSubFrames({ client }) {
  const GET_COOKIES_PAGE_URL = `${DEFAULT_HOST}${BASE_PATH}/doc_get_cookies_page.html`;

  const { Network } = client;
  await loadURL(GET_COOKIES_PAGE_URL);

  const cookie_page = { name: "page", value: "mainpage", path: BASE_PATH };
  const cookie_frame = { name: "frame", value: "subframe", path: BASE_PATH };

  try {
    const { cookies } = await Network.getCookies();
    cookies.sort((a, b) => a.name.localeCompare(b.name));
    is(cookies.length, 2, "All cookies have been found including subframe");
    assertCookie(cookies[0], cookie_frame);
    assertCookie(cookies[1], cookie_page);
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
    let result = await Network.getCookies();
    is(result.cookies.length, 1, "The secure cookie has been found");
    assertCookie(result.cookies[0], cookie);

    // For unsecure protocols no secure cookies are returned
    await loadURL(DEFAULT_URL);
    result = await Network.getCookies();
    is(result.cookies.length, 0, "No secure cookies have been found");
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
    const { cookies } = await Network.getCookies();
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
    const { cookies } = await Network.getCookies();
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
    let result = await Network.getCookies();
    is(result.cookies.length, 1, "A single cookie has been found");
    assertCookie(result.cookies[0], cookie);

    console.log("Check sub path");
    await loadURL(`${DEFAULT_HOST}${SJS_PATH}`);
    result = await Network.getCookies();
    is(result.cookies.length, 1, "A single cookie has been found");
    assertCookie(result.cookies[0], cookie);

    console.log("Check parent path");
    await loadURL(`${DEFAULT_HOST}${PARENT_PATH}`);
    result = await Network.getCookies();
    is(result.cookies.length, 0, "No cookies have been found");

    console.log("Check non matching path");
    await loadURL(`${DEFAULT_HOST}/foo/bar`);
    result = await Network.getCookies();
    is(result.cookies.length, 0, "No cookies have been found");
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
    const { cookies } = await Network.getCookies();
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
      const { cookies } = await Network.getCookies();
      is(cookies.length, 1, "A single cookie has been found");
      assertCookie(cookies[0], cookie);
    } finally {
      Services.cookies.removeAll();
    }
  }
});
