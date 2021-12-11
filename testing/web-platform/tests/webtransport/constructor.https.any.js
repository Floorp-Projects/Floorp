// META: global=window,worker
// META: script=/common/get-host-info.sub.js
// META: script=resources/webtransport-test-helpers.sub.js

const BAD_URLS = [
  null,
  '',
  'no-scheme',
  'http://example.com/' /* scheme is wrong */,
  'quic-transport://example.com/' /* scheme is wrong */,
  'https:///' /* no host  specified */,
  'https://example.com/#failing' /* has fragment */,
  `https://${HOST}:999999/` /* invalid port */,
];

for (const url of BAD_URLS) {
  test(() => {
    assert_throws_dom('SyntaxError', () => new WebTransport(url),
                      'constructor should throw');
  }, `WebTransport constructor should reject URL '${url}'`);
}

promise_test(async t => {
  const wt = new WebTransport(`https://${HOST}:0/`);

  // Sadly we cannot use promise_rejects_dom as the error constructor is
  // WebTransportError rather than DOMException.
  // We get a possible error, and then make sure wt.ready is rejected with it.
  const e = await wt.ready.catch(e => e);

  await promise_rejects_exactly(t, e, wt.ready, 'ready should be rejected');
  await promise_rejects_exactly(t, e, wt.closed, 'closed should be rejected');
  assert_true(e instanceof WebTransportError);
  assert_equals(e.source, 'session', 'source');
  assert_equals(e.streamErrorCode, null, 'streamErrorCode');
}, 'Connection to port 0 should fail');
