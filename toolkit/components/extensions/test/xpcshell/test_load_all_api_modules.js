"use strict";

const { ExtensionCommon } = ChromeUtils.import(
  "resource://gre/modules/ExtensionCommon.jsm"
);
const { Schemas } = ChromeUtils.import("resource://gre/modules/Schemas.jsm");

const BASE_SCHEMA = "chrome://extensions/content/schemas/manifest.json";

const CATEGORY_EXTENSION_MODULES = "webextension-modules";
const CATEGORY_EXTENSION_SCHEMAS = "webextension-schemas";
const CATEGORY_EXTENSION_SCRIPTS = "webextension-scripts";

const CATEGORY_EXTENSION_SCRIPTS_ADDON = "webextension-scripts-addon";
const CATEGORY_EXTENSION_SCRIPTS_CONTENT = "webextension-scripts-content";
const CATEGORY_EXTENSION_SCRIPTS_DEVTOOLS = "webextension-scripts-devtools";

let schemaURLs = new Set();
schemaURLs.add("chrome://extensions/content/schemas/experiments.json");

// Helper class used to load the API modules similarly to the apiManager
// defined in ExtensionParent.jsm.
class FakeAPIManager extends ExtensionCommon.SchemaAPIManager {
  constructor(processType = "main") {
    super(processType, Schemas);
    this.initialized = false;
  }

  getModuleJSONURLs() {
    return Array.from(
      Services.catMan.enumerateCategory(CATEGORY_EXTENSION_MODULES),
      ({ value }) => value
    );
  }

  async lazyInit() {
    if (this.initialized) {
      return;
    }

    this.initialized = true;

    let modulesPromise = this.loadModuleJSON(this.getModuleJSONURLs());

    let scriptURLs = [];
    for (let { value } of Services.catMan.enumerateCategory(
      CATEGORY_EXTENSION_SCRIPTS
    )) {
      scriptURLs.push(value);
    }

    let scripts = await Promise.all(
      scriptURLs.map(url => ChromeUtils.compileScript(url))
    );

    this.initModuleData(await modulesPromise);

    this.initGlobal();
    for (let script of scripts) {
      script.executeInGlobal(this.global);
    }

    // Load order matters here. The base manifest defines types which are
    // extended by other schemas, so needs to be loaded first.
    await Schemas.load(BASE_SCHEMA).then(() => {
      let promises = [];
      for (let { value } of Services.catMan.enumerateCategory(
        CATEGORY_EXTENSION_SCHEMAS
      )) {
        promises.push(Schemas.load(value));
      }
      for (let [url, { content }] of this.schemaURLs) {
        promises.push(Schemas.load(url, content));
      }
      for (let url of schemaURLs) {
        promises.push(Schemas.load(url));
      }
      return Promise.all(promises).then(() => {
        Schemas.updateSharedSchemas();
      });
    });
  }

  async loadAllModules(reverseOrder = false) {
    await this.lazyInit();

    let apiModuleNames = Array.from(this.modules.keys())
      .filter(moduleName => {
        let moduleDesc = this.modules.get(moduleName);
        return moduleDesc && !!moduleDesc.url;
      })
      .sort();

    apiModuleNames = reverseOrder ? apiModuleNames.reverse() : apiModuleNames;

    for (let apiModule of apiModuleNames) {
      info(
        `Loading apiModule ${apiModule}: ${this.modules.get(apiModule).url}`
      );
      await this.asyncLoadModule(apiModule);
    }
  }
}

// Specialized helper class used to test loading "child process" modules (similarly to the
// SchemaAPIManagers sub-classes defined in ExtensionPageChild.jsm and ExtensionContent.jsm).
class FakeChildProcessAPIManager extends FakeAPIManager {
  constructor({ processType, categoryScripts }) {
    super(processType, Schemas);

    this.categoryScripts = categoryScripts;
  }

  async lazyInit() {
    if (!this.initialized) {
      this.initialized = true;
      this.initGlobal();
      for (let { value } of Services.catMan.enumerateCategory(
        this.categoryScripts
      )) {
        await this.loadScript(value);
      }
    }
  }
}

async function test_loading_api_modules(createAPIManager) {
  let fakeAPIManager;

  info("Load API modules in alphabetic order");

  fakeAPIManager = createAPIManager();
  await fakeAPIManager.loadAllModules();

  info("Load API modules in reverse order");

  fakeAPIManager = createAPIManager();
  await fakeAPIManager.loadAllModules(true);
}

add_task(function test_loading_main_process_api_modules() {
  return test_loading_api_modules(() => {
    return new FakeAPIManager();
  });
});

add_task(function test_loading_extension_process_modules() {
  return test_loading_api_modules(() => {
    return new FakeChildProcessAPIManager({
      processType: "addon",
      categoryScripts: CATEGORY_EXTENSION_SCRIPTS_ADDON,
    });
  });
});

add_task(function test_loading_devtools_modules() {
  return test_loading_api_modules(() => {
    return new FakeChildProcessAPIManager({
      processType: "devtools",
      categoryScripts: CATEGORY_EXTENSION_SCRIPTS_DEVTOOLS,
    });
  });
});

add_task(async function test_loading_content_process_modules() {
  return test_loading_api_modules(() => {
    return new FakeChildProcessAPIManager({
      processType: "content",
      categoryScripts: CATEGORY_EXTENSION_SCRIPTS_CONTENT,
    });
  });
});
