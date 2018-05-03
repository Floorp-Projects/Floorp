'use strict';

cookie_test(async t => {
  let eventPromise = observeNextCookieChangeEvent();
  await setCookieStringHttp('HTTP-🍪=🔵; path=/');
  assert_equals(
      await getCookieString(),
      'HTTP-🍪=🔵',
      'Cookie we wrote using HTTP in cookie jar');
  assert_equals(
      await getCookieStringHttp(),
      'HTTP-🍪=🔵',
      'Cookie we wrote using HTTP in HTTP cookie jar');
  await verifyCookieChangeEvent(
    eventPromise, {changed: [{name: 'HTTP-🍪', value: '🔵'}]},
    'Cookie we wrote using HTTP is observed');

  eventPromise = observeNextCookieChangeEvent();
  await setCookieStringHttp('HTTP-🍪=🔵🔵; path=/');
  assert_equals(
      await getCookieString(),
      'HTTP-🍪=🔵🔵',
      'Cookie we overwrote using HTTP in cookie jar');
  assert_equals(
      await getCookieStringHttp(),
      'HTTP-🍪=🔵🔵',
      'Cookie we overwrote using HTTP in HTTP cookie jar');
  await verifyCookieChangeEvent(
    eventPromise, {changed: [{name: 'HTTP-🍪', value: '🔵🔵'}]},
    'Cookie we overwrote using HTTP is observed');

  eventPromise = observeNextCookieChangeEvent();
  await setCookieStringHttp('HTTP-🍪=DELETED; path=/; max-age=0');
  assert_equals(
      await getCookieString(),
      undefined,
      'Empty cookie jar after HTTP cookie-clearing using max-age=0');
  assert_equals(
      await getCookieStringHttp(),
      undefined,
      'Empty HTTP cookie jar after HTTP cookie-clearing using max-age=0');
  await verifyCookieChangeEvent(
    eventPromise, {deleted: [{name: 'HTTP-🍪'}]},
    'Deletion observed after HTTP cookie-clearing using max-age=0');
  await cookieStore.delete('HTTP-🍪');
}, 'HTTP set/overwrite/delete observed in CookieStore');


cookie_test(async t => {
  let eventPromise = observeNextCookieChangeEvent();
  await cookieStore.set('TEST', 'value0');
  assert_equals(
    await getCookieString(),
    'TEST=value0',
    'Cookie jar contains only cookie we set');
  assert_equals(
    await getCookieStringHttp(),
    'TEST=value0',
    'HTTP cookie jar contains only cookie we set');
  await verifyCookieChangeEvent(
    eventPromise,
    {changed: [{name: 'TEST', value: 'value0'}]},
    'Observed value that was set');

  eventPromise = observeNextCookieChangeEvent();
  await cookieStore.set('TEST', 'value');
  assert_equals(
    await getCookieString(),
    'TEST=value',
    'Cookie jar contains only cookie we set');
  assert_equals(
    await getCookieStringHttp(),
    'TEST=value',
    'HTTP cookie jar contains only cookie we set');
  await verifyCookieChangeEvent(
    eventPromise,
    {changed: [{name: 'TEST', value: 'value'}]},
    'Observed value that was overwritten');

  eventPromise = observeNextCookieChangeEvent();
  await cookieStore.delete('TEST');
  assert_equals(
    await getCookieString(),
    undefined,
    'Cookie jar does not contain cookie we deleted');
  assert_equals(
    await getCookieStringHttp(),
    undefined,
    'HTTP cookie jar does not contain cookie we deleted');
  await verifyCookieChangeEvent(
    eventPromise,
    {deleted: [{name: 'TEST'}]},
    'Observed cookie that was deleted');
}, 'CookieStore set/overwrite/delete observed in HTTP headers');


cookie_test(async t => {
  // Non-UTF-8 byte sequences cause the Set-Cookie to be dropped.
  let eventPromise = observeNextCookieChangeEvent();
  await setCookieBinaryHttp(
      unescape(encodeURIComponent('HTTP-🍪=🔵')) + '\xef\xbf\xbd; path=/');
  assert_equals(
      await getCookieString(),
      'HTTP-🍪=🔵\ufffd',
      'Binary cookie we wrote using HTTP in cookie jar');
  assert_equals(
      await getCookieStringHttp(),
      'HTTP-🍪=🔵\ufffd',
      'Binary cookie we wrote using HTTP in HTTP cookie jar');
  assert_equals(
      decodeURIComponent(escape(await getCookieBinaryHttp())),
      'HTTP-🍪=🔵\ufffd',
      'Binary cookie we wrote in binary HTTP cookie jar');
  assert_equals(
      await getCookieBinaryHttp(),
      unescape(encodeURIComponent('HTTP-🍪=🔵')) + '\xef\xbf\xbd',
      'Binary cookie we wrote in binary HTTP cookie jar');
  await verifyCookieChangeEvent(
    eventPromise, {changed: [{name: 'HTTP-🍪', value: '🔵\ufffd'}]},
    'Binary cookie we wrote using HTTP is observed');

  eventPromise = observeNextCookieChangeEvent();
  await setCookieBinaryHttp(
      unescape(encodeURIComponent('HTTP-🍪=🔵🔵')) + '\xef\xbf\xbd; path=/');
  assert_equals(
      await getCookieString(),
      'HTTP-🍪=🔵🔵\ufffd',
      'Binary cookie we overwrote using HTTP in cookie jar');
  assert_equals(
      await getCookieStringHttp(),
      'HTTP-🍪=🔵🔵\ufffd',
      'Binary cookie we overwrote using HTTP in HTTP cookie jar');
  assert_equals(
      decodeURIComponent(escape(await getCookieBinaryHttp())),
      'HTTP-🍪=🔵🔵\ufffd',
      'Binary cookie we overwrote in binary HTTP cookie jar');
  assert_equals(
      await getCookieBinaryHttp(),
      unescape(encodeURIComponent('HTTP-🍪=🔵🔵')) + '\xef\xbf\xbd',
      'Binary cookie we overwrote in binary HTTP cookie jar');
  await verifyCookieChangeEvent(
    eventPromise, {changed: [{name: 'HTTP-🍪', value: '🔵🔵\ufffd'}]},
    'Binary cookie we overwrote using HTTP is observed');

  eventPromise = observeNextCookieChangeEvent();
  await setCookieBinaryHttp(
      unescape(encodeURIComponent('HTTP-🍪=DELETED; path=/; max-age=0')));
  assert_equals(
      await getCookieString(),
      undefined,
      'Empty cookie jar after binary HTTP cookie-clearing using max-age=0');
  assert_equals(
      await getCookieStringHttp(),
      undefined,
      'Empty HTTP cookie jar after' +
        ' binary HTTP cookie-clearing using max-age=0');
  assert_equals(
      await getCookieBinaryHttp(),
      undefined,
      'Empty binary HTTP cookie jar after' +
        ' binary HTTP cookie-clearing using max-age=0');
  await verifyCookieChangeEvent(
    eventPromise, {deleted: [{name: 'HTTP-🍪'}]},
    'Deletion observed after binary HTTP cookie-clearing using max-age=0');
}, 'Binary HTTP set/overwrite/delete observed in CookieStore');
