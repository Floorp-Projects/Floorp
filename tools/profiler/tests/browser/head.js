
const { BrowserTestUtils } = ChromeUtils.import("resource://testing-common/BrowserTestUtils.jsm");

const BASE_URL = "http://example.com/browser/tools/profiler/tests/browser/";

function startProfiler() {
  const settings = {
    entries: 1000000, // 9MB
    interval: 1, // ms
    features: ["threads"],
    threads: ["GeckoMain"],
  };
  Services.profiler.StartProfiler(
    settings.entries,
    settings.interval,
    settings.features,
    settings.features.length,
    settings.threads,
    settings.threads.length,
    settings.duration
  );
}
