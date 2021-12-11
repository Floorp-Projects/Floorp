/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const cacheURL =
  "http://example.org/browser/browser/components/originattributes/test/browser/file_cache.html";

function countMatchingCacheEntries(cacheEntries, domain, fileSuffix) {
  return cacheEntries
    .map(entry => entry.uri.asciiSpec)
    .filter(spec => spec.includes(domain))
    .filter(spec => spec.includes("file_thirdPartyChild." + fileSuffix)).length;
}

async function checkCache(suffixes, originAttributes) {
  const loadContextInfo = Services.loadContextInfo.custom(
    false,
    originAttributes
  );

  const data = await new Promise(resolve => {
    let cacheEntries = [];
    let cacheVisitor = {
      onCacheStorageInfo(num, consumption) {},
      onCacheEntryInfo(uri, idEnhance) {
        cacheEntries.push({ uri, idEnhance });
      },
      onCacheEntryVisitCompleted() {
        resolve(cacheEntries);
      },
      QueryInterface: ChromeUtils.generateQI(["nsICacheStorageVisitor"]),
    };
    // Visiting the disk cache also visits memory storage so we do not
    // need to use Services.cache2.memoryCacheStorage() here.
    let storage = Services.cache2.diskCacheStorage(loadContextInfo);
    storage.asyncVisitStorage(cacheVisitor, true);
  });

  for (let suffix of suffixes) {
    let foundEntryCount = countMatchingCacheEntries(
      data,
      "example.net",
      suffix
    );
    ok(
      foundEntryCount > 0,
      `Cache entries expected for ${suffix} and OA=${JSON.stringify(
        originAttributes
      )}`
    );
  }
}

add_task(async function() {
  info("Disable predictor and accept all");
  await SpecialPowers.pushPrefEnv({
    set: [
      ["network.predictor.enabled", false],
      ["network.predictor.enable-prefetch", false],
      ["network.cookie.cookieBehavior", 0],
    ],
  });

  const tests = [
    {
      prefValue: true,
      originAttributes: { partitionKey: "(http,example.org)" },
    },
    {
      prefValue: false,
      originAttributes: {},
    },
  ];

  for (let test of tests) {
    info("Clear image and network caches");
    let tools = SpecialPowers.Cc["@mozilla.org/image/tools;1"].getService(
      SpecialPowers.Ci.imgITools
    );
    let imageCache = tools.getImgCacheForDocument(window.document);
    imageCache.clearCache(true); // true=chrome
    imageCache.clearCache(false); // false=content
    Services.cache2.clear();

    info("Enabling network state partitioning");
    await SpecialPowers.pushPrefEnv({
      set: [["privacy.partition.network_state", test.prefValue]],
    });

    info("Let's load a page to populate some entries");
    let tab = (gBrowser.selectedTab = BrowserTestUtils.addTab(gBrowser));
    BrowserTestUtils.loadURI(tab.linkedBrowser, cacheURL);
    await BrowserTestUtils.browserLoaded(tab.linkedBrowser, false, cacheURL);

    let argObj = {
      randomSuffix: Math.random(),
      urlPrefix:
        "http://example.net/browser/browser/components/originattributes/test/browser/",
    };

    await SpecialPowers.spawn(tab.linkedBrowser, [argObj], async function(arg) {
      // The CSS cache needs to be cleared in-process.
      content.windowUtils.clearSharedStyleSheetCache();

      let videoURL = arg.urlPrefix + "file_thirdPartyChild.video.ogv";
      let audioURL = arg.urlPrefix + "file_thirdPartyChild.audio.ogg";
      let URLSuffix = "?r=" + arg.randomSuffix;

      // Create the audio and video elements.
      let audio = content.document.createElement("audio");
      let video = content.document.createElement("video");
      let audioSource = content.document.createElement("source");

      // Append the audio element into the body, and wait until they're finished.
      await new content.Promise(resolve => {
        let audioLoaded = false;

        let audioListener = () => {
          Assert.ok(true, `Audio suspended: ${audioURL + URLSuffix}`);
          audio.removeEventListener("suspend", audioListener);

          audioLoaded = true;
          if (audioLoaded) {
            resolve();
          }
        };

        Assert.ok(true, `Loading audio: ${audioURL + URLSuffix}`);

        // Add the event listeners before everything in case we lose events.
        audio.addEventListener("suspend", audioListener);

        // Assign attributes for the audio element.
        audioSource.setAttribute("src", audioURL + URLSuffix);
        audioSource.setAttribute("type", "audio/ogg");

        audio.appendChild(audioSource);
        audio.autoplay = true;

        content.document.body.appendChild(audio);
      });

      // Append the video element into the body, and wait until it's finished.
      await new content.Promise(resolve => {
        let listener = () => {
          Assert.ok(true, `Video suspended: ${videoURL + URLSuffix}`);
          video.removeEventListener("suspend", listener);
          resolve();
        };

        Assert.ok(true, `Loading video: ${videoURL + URLSuffix}`);

        // Add the event listener before everything in case we lose the event.
        video.addEventListener("suspend", listener);

        // Assign attributes for the video element.
        video.setAttribute("src", videoURL + URLSuffix);
        video.setAttribute("type", "video/ogg");

        content.document.body.appendChild(video);
      });
    });

    let maybePartitionedSuffixes = [
      "iframe.html",
      "link.css",
      "script.js",
      "img.png",
      "favicon.png",
      "object.png",
      "embed.png",
      "xhr.html",
      "worker.xhr.html",
      "audio.ogg",
      "video.ogv",
      "fetch.html",
      "worker.fetch.html",
      "request.html",
      "worker.request.html",
      "import.js",
      "worker.js",
      "sharedworker.js",
    ];

    info("Query the cache (maybe) partitioned cache");
    await checkCache(maybePartitionedSuffixes, test.originAttributes);

    gBrowser.removeCurrentTab();
  }
});
