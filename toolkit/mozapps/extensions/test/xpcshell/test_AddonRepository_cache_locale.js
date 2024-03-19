"user strict";

const PREF_GETADDONS_CACHE_ENABLED = "extensions.getAddons.cache.enabled";
const PREF_METADATA_LASTUPDATE = "extensions.getAddons.cache.lastUpdate";
Services.prefs.setBoolPref(PREF_GETADDONS_CACHE_ENABLED, true);

AddonTestUtils.init(this);
AddonTestUtils.overrideCertDB();
AddonTestUtils.createAppInfo(
  "xpcshell@tests.mozilla.org",
  "XPCShell",
  "42",
  "42"
);

let server = AddonTestUtils.createHttpServer({ hosts: ["example.com"] });
server.registerDirectory("/data/", do_get_file("data"));

// Use %LOCALE% as the default pref does. It is set from appLocaleAsBCP47.
Services.prefs.setStringPref(
  PREF_GETADDONS_BYIDS,
  "http://example.com/addons.json?guids=%IDS%&locale=%LOCALE%"
);

const TEST_ADDON_ID = "test_AddonRepository_1@tests.mozilla.org";

const repositoryAddons = {
  "test_AddonRepository_1@tests.mozilla.org": {
    name: "Repo Add-on 1",
    type: "extension",
    guid: TEST_ADDON_ID,
    current_version: {
      version: "2.1",
      files: [
        {
          platform: "all",
          size: 9,
          url: "http://example.com/repo/1/install.xpi",
        },
      ],
    },
  },
  "langpack-und@test.mozilla.org": {
    // included only to avoid exceptions in AddonRepository
    name: "und langpack",
    type: "language",
    guid: "langpack-und@test.mozilla.org",
    current_version: {
      version: "1.1",
      files: [
        {
          platform: "all",
          size: 9,
          url: "http://example.com/repo/1/langpack.xpi",
        },
      ],
    },
  },
};

server.registerPathHandler("/addons.json", (request, response) => {
  let search = new URLSearchParams(request.queryString);
  let IDs = search.get("guids").split(",");
  let locale = search.get("locale");

  let repositoryData = {
    page_size: 25,
    page_count: 1,
    count: 0,
    next: null,
    previous: null,
    results: [],
  };
  for (let id of IDs) {
    let data = JSON.parse(JSON.stringify(repositoryAddons[id]));
    data.summary = `This is an ${locale} addon data object`;
    data.description = `Full Description ${locale}`;
    repositoryData.results.push(data);
  }
  repositoryData.count = repositoryData.results.length;

  // The request contains the IDs to retreive, but we're just handling the
  // two test addons so it's static data.
  response.setHeader("content-type", "application/json");
  response.write(JSON.stringify(repositoryData));
});

const ADDONS = [
  {
    manifest: {
      name: "XPI Add-on 1",
      version: "1.1",

      description: "XPI Add-on 1 - Description",
      developer: {
        name: "XPI Add-on 1 - Author",
      },

      homepage_url: "http://example.com/xpi/1/homepage.html",
      icons: {
        32: "icon.png",
      },

      options_ui: {
        page: "options.html",
      },

      browser_specific_settings: {
        gecko: { id: TEST_ADDON_ID },
      },
    },
  },
  {
    // Necessary to provide the "und" locale
    manifest: {
      name: "und Language Pack",
      version: "1.0",
      manifest_version: 2,
      browser_specific_settings: {
        gecko: {
          id: "langpack-und@test.mozilla.org",
        },
      },
      sources: {
        browser: {
          base_path: "browser/",
        },
      },
      langpack_id: "und",
      languages: {
        und: {
          chrome_resources: {
            global: "chrome/und/locale/und/global/",
          },
          version: "20171001190118",
        },
      },
      author: "Mozilla Localization Task Force",
      description: "Language pack for Testy for und",
    },
  },
];
const ADDON_FILES = ADDONS.map(addon =>
  AddonTestUtils.createTempWebExtensionFile(addon)
);

const REQ_LOC_CHANGE_EVENT = "intl:requested-locales-changed";

function promiseLocaleChanged(requestedLocale) {
  if (Services.locale.appLocaleAsBCP47 == requestedLocale) {
    return Promise.resolve();
  }
  return new Promise(resolve => {
    let localeObserver = {
      observe(aSubject, aTopic) {
        switch (aTopic) {
          case REQ_LOC_CHANGE_EVENT:
            let reqLocs = Services.locale.requestedLocales;
            equal(reqLocs[0], requestedLocale);
            Services.obs.removeObserver(localeObserver, REQ_LOC_CHANGE_EVENT);
            resolve();
        }
      },
    };
    Services.obs.addObserver(localeObserver, REQ_LOC_CHANGE_EVENT);
    Services.locale.requestedLocales = [requestedLocale];
  });
}

function promiseMetaDataUpdate() {
  return new Promise(resolve => {
    let listener = () => {
      Services.prefs.removeObserver(PREF_METADATA_LASTUPDATE, listener);
      resolve();
    };

    Services.prefs.addObserver(PREF_METADATA_LASTUPDATE, listener);
  });
}

function promiseLocale(locale) {
  return Promise.all([promiseLocaleChanged(locale), promiseMetaDataUpdate()]);
}

add_task(async function setup() {
  await promiseStartupManager();
  for (let xpi of ADDON_FILES) {
    await promiseInstallFile(xpi);
  }
});

add_task(async function test_locale_change() {
  await promiseLocale("en-US");
  let addon = await AddonRepository.getCachedAddonByID(TEST_ADDON_ID);
  Assert.ok(addon.description.includes("en-US"), "description is en-us");
  Assert.ok(
    addon.fullDescription.includes("en-US"),
    "fullDescription is en-us"
  );

  // This pref is a 1s resolution, set it to zero so the
  // next test can wait on it being updated again.
  Services.prefs.setIntPref(PREF_METADATA_LASTUPDATE, 0);
  // Wait for the last update timestamp to be updated.
  await promiseLocale("und");

  addon = await AddonRepository.getCachedAddonByID(TEST_ADDON_ID);

  Assert.ok(
    addon.description.includes("und"),
    `description is ${addon.description}`
  );
  Assert.ok(
    addon.fullDescription.includes("und"),
    `fullDescription is ${addon.fullDescription}`
  );
});
