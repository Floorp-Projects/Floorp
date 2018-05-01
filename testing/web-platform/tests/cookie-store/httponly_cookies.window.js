// META: script=resources/cookie-test-helpers.js

'use strict';

cookie_test(async t => {
  let eventPromise = observeNextCookieChangeEvent();
  await setCookieStringHttp('HTTPONLY-🍪=🔵; path=/; httponly');
  assert_equals(
      await getCookieString(),
      undefined,
      'HttpOnly cookie we wrote using HTTP in cookie jar' +
        ' is invisible to script');
  assert_equals(
      await getCookieStringHttp(),
      'HTTPONLY-🍪=🔵',
    'HttpOnly cookie we wrote using HTTP in HTTP cookie jar');

  eventPromise = observeNextCookieChangeEvent();
  await setCookieStringHttp(
      'HTTPONLY-🍪=DELETED; path=/; max-age=0; httponly');
  assert_equals(
      await getCookieString(),
      undefined,
      'Empty cookie jar after HTTP cookie-clearing using max-age=0');
  assert_equals(
      await getCookieStringHttp(),
      undefined,
      'Empty HTTP cookie jar after HTTP cookie-clearing using max-age=0');

  // HTTPONLY cookie changes should not have been observed; perform
  // a dummy change to verify that nothing else was queued up.
  await cookieStore.set('TEST', 'dummy');
  await verifyCookieChangeEvent(
    eventPromise, {changed: [{name: 'TEST', value: 'dummy'}]},
    'HttpOnly cookie deletion was not observed');
}, 'HttpOnly cookies are not observed');
