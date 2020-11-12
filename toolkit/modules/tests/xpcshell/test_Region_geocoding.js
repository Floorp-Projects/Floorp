"use strict";

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { Region } = ChromeUtils.import("resource://gre/modules/Region.jsm");
const { sinon } = ChromeUtils.import("resource://testing-common/Sinon.jsm");

async function readFile(file) {
  let decoder = new TextDecoder();
  let data = await OS.File.read(file.path);
  return decoder.decode(data);
}

function setLocation(location) {
  Services.prefs.setCharPref(
    "geo.provider.network.url",
    `data:application/json,${JSON.stringify({ location })}`
  );
}

async function stubMap(path, fun) {
  let map = await readFile(do_get_file(path));
  sinon.stub(Region, fun).resolves(JSON.parse(map));
}

add_task(async function test_setup() {
  Services.prefs.setBoolPref("browser.region.log", true);
  await stubMap("regions/world.geojson", "_getPlainMap");
  await stubMap("regions/world-buffered.geojson", "_getBufferedMap");
});

const LOCATIONS = [
  { lat: 55.867005, lng: -4.271078, expectedRegion: "GB" },
  // Small cove in Italy surrounded by another region.
  { lat: 45.6523148, lng: 13.7486427, expectedRegion: "IT" },
  // In Bosnia and Herzegovina but within a lot of borders.
  { lat: 42.557079, lng: 18.4370373, expectedRegion: "HR" },
  // In the sea bordering Italy and a few other regions.
  { lat: 45.608696, lng: 13.4667903, expectedRegion: "IT" },
  // In the middle of the Atlantic.
  { lat: 35.4411368, lng: -41.5372973, expectedRegion: null },
];

add_task(async function test_mls_results() {
  setLocation({ lat: -5.066019, lng: 39.1026251 });
  let expectedRegion = "TZ";
  let region = await Region._getRegionLocally();
  Assert.equal(region, expectedRegion);
});

/*add_task(async function test_basic() {
  for (const { lat, lng, expectedRegion } of LOCATIONS) {
    setLocation({ lat, lng });
    let region = await Region._getRegionLocally();
    Assert.equal(
      region,
      expectedRegion,
      `Got the expected region at ${lat},${lng}`
    );
  }
});*/

add_task(async function test_mls_results() {
  let data = await readFile(do_get_file("regions/mls-lookup-results.csv"));
  for (const row of data.split("\n")) {
    let [lat, lng, expectedRegion] = row.split(",");
    setLocation({ lng: parseFloat(lng), lat: parseFloat(lat) });
    let region = await Region._getRegionLocally();
    Assert.equal(
      region,
      expectedRegion,
      `Expected ${expectedRegion} at ${lat},${lng} got ${region}`
    );
  }
});
