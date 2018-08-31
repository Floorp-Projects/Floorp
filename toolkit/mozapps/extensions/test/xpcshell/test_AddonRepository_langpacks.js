ChromeUtils.import("resource://gre/modules/addons/AddonRepository.jsm");

const PREF_GET_LANGPACKS = "extensions.getAddons.langpacks.url";

let server = AddonTestUtils.createHttpServer({hosts: ["example.com"]});

add_task(async function setup() {
  Services.prefs.setStringPref(PREF_GET_LANGPACKS, "http://example.com/langpacks.json");

  function setData(data) {
    if (typeof data != "string") {
      data = JSON.stringify(data);
    }

    server.registerPathHandler("/langpacks.json", (request, response) => {
      response.setHeader("content-type", "application/json");
      response.write(data);
    });
  }

  const EXPECTED = [
    {
      target_locale: "kl",
      url: "http://example.com/langpack1.xpi",
      hash: "sha256:0123456789abcdef",
    },
    {
      target_locale: "fo",
      url: "http://example.com/langpack2.xpi",
      hash: "sha256:fedcba9876543210",
    },
  ];

  setData({
    results: [
      // A simple entry
      {
        target_locale: EXPECTED[0].target_locale,
        current_compatible_version: {
          files: [
            {
              platform: "all",
              url: EXPECTED[0].url,
              hash: EXPECTED[0].hash,
            },
          ],
        },
      },

      // An entry with multiple supported platforms
      {
        target_locale: EXPECTED[1].target_locale,
        current_compatible_version: {
          files: [
            {
              platform: "somethingelse",
              url: "http://example.com/bogus.xpi",
              hash: "sha256:abcd",
            },
            {
              platform: Services.appinfo.OS.toLowerCase(),
              url: EXPECTED[1].url,
              hash: EXPECTED[1].hash,
            },
          ],
        },
      },

      // An entry with no matching platform
      {
        target_locale: "bla",
        current_compatible_version: {
          files: [
            {
              platform: "unsupportedplatform",
              url: "http://example.com/bogus2.xpi",
              hash: "sha256:1234",
            },
          ],
        },
      },
    ],
  });

  let result = await AddonRepository.getAvailableLangpacks();
  equal(result.length, 2, "Got 2 results");

  deepEqual(result[0], EXPECTED[0], "Got expected result for simple entry");
  deepEqual(result[1], EXPECTED[1], "Got expected result for multi-platform entry");

  setData("not valid json");
  await Assert.rejects(AddonRepository.getAvailableLangpacks(),
                       /SyntaxError/, "Got parse error on invalid JSON");

});
