/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const SJS_PATH = "/browser/remote/cdp/test/browser/network/sjs-cookies.sjs";

const DEFAULT_HOST = "example.org";
const ALT_HOST = "foo.example.org";
const SECURE_HOST = "example.com";

add_task(async function failureWithoutArguments({ client }) {
  const { Network } = client;

  await Assert.rejects(
    Network.setCookie(),
    err => err.message.includes("name: string value expected"),
    "Fails without any arguments"
  );
});

add_task(async function failureWithMissingNameAndValue({ client }) {
  const { Network } = client;

  await Assert.rejects(
    Network.setCookie({
      value: "bar",
      domain: "example.org",
    }),
    err => err.message.includes("name: string value expected"),
    "Fails without name specified"
  );

  await Assert.rejects(
    Network.setCookie({
      name: "foo",
      domain: "example.org",
    }),
    err => err.message.includes("value: string value expected"),
    "Fails without value specified"
  );
});

add_task(async function failureWithMissingDomainAndURL({ client }) {
  const { Network } = client;

  await Assert.rejects(
    Network.setCookie({ name: "foo", value: "bar" }),
    err =>
      err.message.includes(
        "At least one of the url and domain needs to be specified"
      ),
    "Fails without domain and URL specified"
  );
});

add_task(async function setCookieWithDomain({ client }) {
  const { Network } = client;

  const cookie = {
    name: "foo",
    value: "bar",
    domain: ALT_HOST,
  };

  try {
    const { success } = await Network.setCookie(cookie);
    ok(success, "Cookie has been set");

    const cookies = getCookies();
    is(cookies.length, 1, "A single cookie has been found");
    assertCookie(cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function setCookieWithEmptyDomain({ client }) {
  const { Network } = client;

  try {
    const { success } = await Network.setCookie({
      name: "foo",
      value: "bar",
      url: "",
    });
    ok(!success, "Cookie has not been set");

    const cookies = getCookies();
    is(cookies.length, 0, "No cookie has been found");
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function setCookieWithURL({ client }) {
  const { Network } = client;

  const cookie = {
    name: "foo",
    value: "bar",
    domain: ALT_HOST,
  };

  try {
    const { success } = await Network.setCookie({
      name: cookie.name,
      value: cookie.value,
      url: `http://${ALT_HOST}`,
    });
    ok(success, "Cookie has been set");

    const cookies = getCookies();
    is(cookies.length, 1, "A single cookie has been found");
    assertCookie(cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function setCookieWithEmptyURL({ client }) {
  const { Network } = client;

  try {
    const { success } = await Network.setCookie({
      name: "foo",
      value: "bar",
      url: "",
    });
    ok(!success, "No cookie has been set");

    const cookies = getCookies();
    is(cookies.length, 0, "No cookie has been found");
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function setCookieWithDomainAndURL({ client }) {
  const { Network } = client;

  const cookie = {
    name: "foo",
    value: "bar",
    domain: ALT_HOST,
  };

  try {
    const { success } = await Network.setCookie({
      name: cookie.name,
      value: cookie.value,
      domain: cookie.domain,
      url: `http://${DEFAULT_HOST}`,
    });
    ok(success, "Cookie has been set");

    const cookies = getCookies();
    is(cookies.length, 1, "A single cookie has been found");
    assertCookie(cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function setCookieWithHttpOnly({ client }) {
  const { Network } = client;

  const cookie = {
    name: "foo",
    value: "bar",
    domain: DEFAULT_HOST,
    httpOnly: true,
  };

  try {
    const { success } = await Network.setCookie(cookie);
    ok(success, "Cookie has been set");

    const cookies = getCookies();
    is(cookies.length, 1, "A single cookie has been found");
    assertCookie(cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function setCookieWithExpiry({ client }) {
  const { Network } = client;

  const tomorrow = Math.floor(Date.now() / 1000) + 60 * 60 * 24;

  const cookie = {
    name: "foo",
    value: "bar",
    domain: DEFAULT_HOST,
    expires: tomorrow,
    session: false,
  };

  try {
    const { success } = await Network.setCookie(cookie);
    ok(success, "Cookie has been set");

    const cookies = getCookies();
    is(cookies.length, 1, "A single cookie has been found");
    assertCookie(cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function setCookieWithPath({ client }) {
  const { Network } = client;

  const cookie = {
    name: "foo",
    value: "bar",
    domain: ALT_HOST,
    path: SJS_PATH,
  };

  try {
    const { success } = await Network.setCookie(cookie);
    ok(success, "Cookie has been set");

    const cookies = getCookies();
    is(cookies.length, 1, "A single cookie has been found");
    assertCookie(cookies[0], cookie);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function testAddSameSiteCookie({ client }) {
  const { Network } = client;

  for (const sameSite of ["None", "Lax", "Strict"]) {
    console.log(`Check same site value: ${sameSite}`);
    const cookie = {
      name: "foo",
      value: "bar",
      domain: DEFAULT_HOST,
    };
    if (sameSite != "None") {
      cookie.sameSite = sameSite;
    }

    try {
      const { success } = await Network.setCookie({
        name: cookie.name,
        value: cookie.value,
        domain: cookie.domain,
        sameSite,
      });
      ok(success, "Cookie has been set");

      const cookies = getCookies();
      is(cookies.length, 1, "A single cookie has been found");
      assertCookie(cookies[0], cookie);
    } finally {
      Services.cookies.removeAll();
    }
  }
});

add_task(async function testAddSecureCookie({ client }) {
  const { Network } = client;

  const cookie = {
    name: "foo",
    value: "bar",
    domain: "example.com",
    secure: true,
  };

  try {
    const { success } = await Network.setCookie({
      name: cookie.name,
      value: cookie.value,
      url: `https://${SECURE_HOST}`,
    });
    ok(success, "Cookie has been set");

    const cookies = getCookies();
    is(cookies.length, 1, "A single cookie has been found");
    assertCookie(cookies[0], cookie);
    ok(cookies[0].secure, `Cookie for HTTPS is secure`);
  } finally {
    Services.cookies.removeAll();
  }
});
