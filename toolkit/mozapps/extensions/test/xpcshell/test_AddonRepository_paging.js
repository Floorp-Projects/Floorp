// Test that AMO api results that are returned in muliple pages are
// properly handled.
add_task(async function test_paged_api() {
  const MAX_ADDON = 3;

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "2");

  let testserver = createHttpServer();
  const PORT = testserver.identity.primaryPort;

  const EMPTY_RESPONSE = {
    next: null,
    results: [],
  };

  function name(n) {
    return `Addon ${n}`;
  }
  function id(n) {
    return `test${n}@tests.mozilla.org`;
  }
  function summary(n) {
    return `Summary for addon ${n}`;
  }
  function description(n) {
    return `Description for addon ${n}`;
  }

  testserver.registerPathHandler("/empty", (request, response) => {
    response.setHeader("content-type", "application/json");
    response.write(JSON.stringify(EMPTY_RESPONSE));
  });

  testserver.registerPrefixHandler("/addons/", (request, response) => {
    let [page] = /\d+/.exec(request.path);
    page = page ? parseInt(page, 10) : 0;
    page = Math.min(page, MAX_ADDON);

    let result = {
      next:
        page == MAX_ADDON
          ? null
          : `http://localhost:${PORT}/addons/${page + 1}`,
      results: [
        {
          name: name(page),
          type: "extension",
          guid: id(page),
          summary: summary(page),
          description: description(page),
        },
      ],
    };

    response.setHeader("content-type", "application/json");
    response.write(JSON.stringify(result));
  });

  Services.prefs.setCharPref(
    PREF_GETADDONS_BYIDS,
    `http://localhost:${PORT}/addons/0`
  );

  await promiseStartupManager();

  for (let i = 0; i <= MAX_ADDON; i++) {
    await promiseInstallWebExtension({
      manifest: {
        applications: { gecko: { id: id(i) } },
      },
    });
  }

  await AddonManagerInternal.backgroundUpdateCheck();

  let ids = [];
  for (let i = 0; i <= MAX_ADDON; i++) {
    ids.push(id(i));
  }
  let addons = await AddonRepository.getAddonsByIDs(ids);

  equal(addons.length, MAX_ADDON + 1);
  for (let i = 0; i <= MAX_ADDON; i++) {
    equal(addons[i].name, name(i));
    equal(addons[i].id, id(i));
    equal(addons[i].description, summary(i));
    equal(addons[i].fullDescription, description(i));
  }

  await promiseShutdownManager();
});
