function openWindow(url) {
  return new Promise(resolve => {
      let win = window.open(url, '_blank');
      add_completion_callback(() => win.close());
      window.onmessage = e => {
        assert_equals(e.data, 'LOADED');
        resolve(win);
      };
    });
}

// Run a referrer policy test with the given settings.
//
// Example:
// settings = {
//   workletType: 'paint',
//   referrerPolicy: 'no-referrer',
//   isCrossOrigin: false
// };
function runReferrerTest(settings) {
  const kWindowURL =
      'resources/referrer-window.html' +
      `?pipe=header(Referrer-Policy, ${settings.referrerPolicy})`;
  return openWindow(kWindowURL).then(win => {
    const promise = new Promise(resolve => window.onmessage = resolve);
    win.postMessage(settings, '*');
    return promise;
  }).then(msg_event => assert_equals(msg_event.data, 'RESOLVED'));
}

// Runs a series of tests related to the referrer policy on a worklet.
//
// Usage:
// runReferrerTests("paint");
function runReferrerTests(workletType) {
  const worklet = get_worklet(workletType);

  promise_test(() => {
      return runReferrerTest({ workletType: workletType,
                               referrerPolicy: 'no-referrer',
                               isCrossOrigin: false });
  }, 'Importing a same-origin script from a page that has "no-referrer" ' +
     'referrer policy should not send referrer.');

  promise_test(() => {
      return runReferrerTest({ workletType: workletType,
                               referrerPolicy: 'no-referrer',
                               isCrossOrigin: true });
  }, 'Importing a remote-origin script from a page that has "no-referrer" ' +
     'referrer policy should not send referrer.');

  promise_test(() => {
      return runReferrerTest({ workletType: workletType,
                               referrerPolicy: 'origin',
                               isCrossOrigin: false });
  }, 'Importing a same-origin script from a page that has "origin" ' +
     'referrer policy should send only an origin as referrer.');

  promise_test(() => {
      return runReferrerTest({ workletType: workletType,
                               referrerPolicy: 'origin',
                               isCrossOrigin: true });
  }, 'Importing a remote-origin script from a page that has "origin" ' +
     'referrer policy should send only an origin as referrer.');

  promise_test(() => {
      return runReferrerTest({ workletType: workletType,
                               referrerPolicy: 'same-origin',
                               isCrossOrigin: false });
  }, 'Importing a same-origin script from a page that has "same-origin" ' +
     'referrer policy should send referrer.');

  promise_test(() => {
      return runReferrerTest({ workletType: workletType,
                               referrerPolicy: 'same-origin',
                               isCrossOrigin: true });
  }, 'Importing a remote-origin script from a page that has "same-origin" ' +
     'referrer policy should not send referrer.');
}
