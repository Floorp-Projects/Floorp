/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const ALT_HOST = "foo.example.org";
const DEFAULT_HOST = "example.org";

add_task(async function failureWithoutArguments({ client }) {
  const { Network } = client;

  let errorThrown = false;
  try {
    await Network.setCookies();
  } catch (e) {
    errorThrown = true;
  }
  ok(errorThrown, "Fails without any arguments");
});

add_task(async function setCookies({ client }) {
  const { Network } = client;

  const expected_cookies = [
    {
      name: "foo",
      value: "bar",
      domain: DEFAULT_HOST,
    },
    {
      name: "user",
      value: "password",
      domain: ALT_HOST,
    },
  ];

  try {
    await Network.setCookies({ cookies: expected_cookies });

    const cookies = getCookies();
    cookies.sort((a, b) => a.name.localeCompare(b.name));
    is(cookies.length, expected_cookies.length, "Correct number of cookies");
    assertCookie(cookies[0], expected_cookies[0]);
    assertCookie(cookies[1], expected_cookies[1]);
  } finally {
    Services.cookies.removeAll();
  }
});

add_task(async function setCookiesWithInvalidField({ client }) {
  const { Network } = client;

  const cookies = [
    {
      name: "foo",
      value: "bar",
      domain: "",
    },
  ];

  let errorThrown = false;
  try {
    await Network.setCookies({ cookies });
  } catch (e) {
    errorThrown = true;
  } finally {
    Services.cookies.removeAll();
  }
  ok(errorThrown, "Fails with an invalid field");
});
