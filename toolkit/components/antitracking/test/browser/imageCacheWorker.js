/* import-globals-from head.js */
/* import-globals-from browser_imageCache1.js */
ChromeUtils.import("resource://gre/modules/Services.jsm");

AntiTracking.runTest("Image cache - should load the image three times.",
  // blocking callback
  async _ => {
    // Let's load the image twice here.
    let img = document.createElement("img");
    document.body.appendChild(img);
    img.src = "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/image.sjs";
    await new Promise(resolve => { img.onload = resolve; });
    ok(true, "Image 1 loaded");

    img = document.createElement("img");
    document.body.appendChild(img);
    img.src = "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/image.sjs";
    await new Promise(resolve => { img.onload = resolve; });
    ok(true, "Image 2 loaded");
  },

  // non-blocking callback
  {
    runExtraTests: false,
    cookieBehavior,
    blockingByContentBlocking,
    blockingByContentBlockingUI,
    blockingByContentBlockingRTUI,
    blockingByAllowList,
    callback: async _ => {
      // Let's load the image twice here as well.
      let img = document.createElement("img");
      document.body.appendChild(img);
      img.src = "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/image.sjs";
      await new Promise(resolve => { img.onload = resolve; });
      ok(true, "Image 3 loaded");

      img = document.createElement("img");
      document.body.appendChild(img);
      img.src = "https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/image.sjs";
      await new Promise(resolve => { img.onload = resolve; });
      ok(true, "Image 4 loaded");
    },
  },
  null, // cleanup function
  null, // no extra prefs
  false, // no window open test
  false, // no user-interaction test
  expectedBlockingNotifications
);

// If we didn't run the non-blocking test, only expect to have seen two images.
// Otherwise, expect to have seen three images.
let expected = (blockingByContentBlocking && blockingByContentBlockingUI) ? 2 : 3;

// We still want to see just expected requests.
add_task(async _ => {
  await fetch("https://tracking.example.org/browser/toolkit/components/antitracking/test/browser/image.sjs?result")
    .then(r => r.text())
    .then(text => {
      is(text, expected, "The image should be loaded correctly.");
    });

  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });
});
