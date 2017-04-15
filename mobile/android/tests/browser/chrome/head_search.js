// Bits and pieces copied from toolkit/components/search/tests/xpcshell/head_search.js

var { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");

/**
 * Adds test engines and returns a promise resolved when they are installed.
 *
 * The engines are added in the given order.
 *
 * @param aItems
 *        Array of objects with the following properties:
 *        {
 *          name: Engine name, used to wait for it to be loaded.
 *          details: Array containing the parameters of addEngineWithDetails,
 *                   except for the engine name.  Alternative to xmlFileName.
 *        }
 */
var addTestEngines = Task.async(function* (aItems) {
  let engines = [];

  for (let item of aItems) {
    yield new Promise((resolve, reject) => {
      Services.obs.addObserver(function obs(subject, topic, data) {
        try {
          let engine = subject.QueryInterface(Ci.nsISearchEngine);
          if (data != "engine-added" || engine.name != item.name) {
            return;
          }

          Services.obs.removeObserver(obs, "browser-search-engine-modified");
          engines.push(engine);
          resolve();
        } catch (ex) {
          reject(ex);
        }
      }, "browser-search-engine-modified");

      Services.search.addEngineWithDetails(item.name, ...item.details);
    });
  }

  return engines;
});
