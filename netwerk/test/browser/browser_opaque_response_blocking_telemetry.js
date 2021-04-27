/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const DIRPATH = getRootDirectory(gTestPath).replace(
  "chrome://mochitests/content/",
  ""
);

const ORIGIN1 = "https://example.com";
const URL = `${ORIGIN1}/${DIRPATH}dummy.html`;

// Cross origin urls.
const ORIGIN2 = "https://example.org";
const SAFE_LISTED_URL = `${ORIGIN2}/${DIRPATH}res.css`;
const BLOCKED_LISTED_NEVER_SNIFFED_URL = `${ORIGIN2}/${DIRPATH}res.csv`;
const BLOCKED_LISTED_206_URL = `${ORIGIN2}/${DIRPATH}res_206.html`;
const BLOCKED_LISTED_NOSNIFF_URL = `${ORIGIN2}/${DIRPATH}res_nosniff.html`;
const IMAGE_URL = `${ORIGIN2}/${DIRPATH}res_img.png`;
const NOSNIFF_URL = `${ORIGIN2}/${DIRPATH}res_nosniff2.html`;
const NOT_OK_URL = `${ORIGIN2}/${DIRPATH}res_not_ok.html`;
const UNKNOWN_URL = `${ORIGIN2}/${DIRPATH}res.unknown`;
const IMAGE_UNKNOWN_URL = `${ORIGIN2}/${DIRPATH}res_img_unknown.png`;
const MEDIA_URL = `${ORIGIN2}/${DIRPATH}res.mp3`;
const MEDIA_206_URL = `${ORIGIN2}/${DIRPATH}res_206.mp3`;
const MEDIA_INVALID_PARTIAL_URL = `${ORIGIN2}/${DIRPATH}res_invalid_partial.mp3`;
const MEDIA_NOT_200OR206_URL = `${ORIGIN2}/${DIRPATH}res_not_200or206.mp3`;

add_task(async function() {
  await SpecialPowers.pushPrefEnv({
    set: [["browser.opaqueResponseBlocking", true]],
  });
  let histogram = Services.telemetry.getKeyedHistogramById(
    "OPAQUE_RESPONSE_BLOCKING"
  );
  const testcases = [
    {
      url: SAFE_LISTED_URL,
      key: "Allowed_SafeListed",
    },
    {
      url: BLOCKED_LISTED_NEVER_SNIFFED_URL,
      key: "Blocked_BlockListedNeverSniffed",
    },
    {
      url: BLOCKED_LISTED_206_URL,
      key: "Blocked_206AndBlockListed",
    },
    {
      url: BLOCKED_LISTED_NOSNIFF_URL,
      key: "Blocked_NosniffAndEitherBlockListedOrTextPlain",
    },
    {
      url: IMAGE_URL,
      key: "Allowed_SniffAsImageOrAudioOrVideo",
    },
    {
      url: NOSNIFF_URL,
      key: "Blocked_NoSniffHeaderAfterSniff",
    },
    {
      url: NOT_OK_URL,
      key: "Blocked_ResponseNotOk",
    },
    {
      url: UNKNOWN_URL,
      key: "Allowed_FailtoGetMIMEType",
    },
    {
      url: IMAGE_UNKNOWN_URL,
      key: "Blocked_ContentTypeBeginsWithImageOrVideoOrAudio",
    },
    {
      url: MEDIA_URL,
      key: "Allowed_SniffAsImageOrAudioOrVideo",
      media: true,
    },
    {
      url: MEDIA_206_URL,
      key: "Allowed_SniffAsImageOrAudioOrVideo",
      media: true,
    },
    {
      url: MEDIA_INVALID_PARTIAL_URL,
      key: "Blocked_InvaliidPartialResponse",
      media: true,
    },
    {
      url: MEDIA_NOT_200OR206_URL,
      key: "Blocked_Not200Or206",
      media: true,
    },
  ];

  for (let testcase of testcases) {
    histogram.clear();

    await BrowserTestUtils.withNewTab(URL, async function(browser) {
      BrowserTestUtils.loadURI(browser, URL);
      await BrowserTestUtils.browserLoaded(browser);

      info(`Fetching cross-origin url: ${testcase.url}.`);

      await SpecialPowers.spawn(
        browser,
        [testcase.url, testcase.media],
        async (url, media) => {
          try {
            if (media) {
              const audio = content.document.createElement("audio");
              audio.src = url;
              content.document.body.appendChild(audio);
              audio.load();
              await new Promise(res => {
                audio.onloadedmetadata = () => {
                  audio.src = "";
                  audio.onloadedmetadata = undefined;
                  audio.load();
                  res();
                };
              });
            } else {
              await content.window.fetch(url);
            }
          } catch (e) {
            /* Ignore result */
          }
        }
      );
    });

    info(`Validating if the telemetry probe has been reported.`);

    let snapshot = histogram.snapshot();
    let keys = testcase.keys;
    if (!keys) {
      keys = [testcase.key];
    }
    for (let key of keys) {
      ok(snapshot.hasOwnProperty(key), `Should have recorded key ${key}`);
    }
  }
});
