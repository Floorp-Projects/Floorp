ChromeUtils.import("resource://gre/modules/Services.jsm");

AntiTracking.runTest("Image cache - should load the image twice.",
  // blocking callback
  async _ => {
    // Let's load the image twice here.
    let img = document.createElement("img");
    document.body.appendChild(img);
    img.src = "https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/image.sjs",
    await new Promise(resolve => { img.onload = resolve; });
    ok(true, "Image 1 loaded");

    img = document.createElement("img");
    document.body.appendChild(img);
    img.src = "https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/image.sjs",
    await new Promise(resolve => { img.onload = resolve; });
    ok(true, "Image 2 loaded");
  },

  // non-blocking callback
  async _ => {
    // Let's load the image twice here as well.
    let img = document.createElement("img");
    document.body.appendChild(img);
    img.src = "https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/image.sjs",
    await new Promise(resolve => { img.onload = resolve; });
    ok(true, "Image 3 loaded");

    img = document.createElement("img");
    document.body.appendChild(img);
    img.src = "https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/image.sjs",
    await new Promise(resolve => { img.onload = resolve; });
    ok(true, "Image 4 loaded");
  },
  null, // cleanup function
  null, // no extra prefs
  false, // no window open test
  false // no user-interaction test
);

// We still want to see just 2 requests.
add_task(async _ => {
  await fetch("https://tracking.example.com/browser/toolkit/components/antitracking/test/browser/image.sjs?result")
    .then(r => r.text())
    .then(text => {
      is(text, 2, "The image should be loaded correctly.");
    });

  await new Promise(resolve => {
    Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, value => resolve());
  });
});
