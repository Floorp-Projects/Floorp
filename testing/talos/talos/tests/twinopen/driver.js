/* eslint-env webextensions */
addEventListener("load", () => {
  browser.twinopen.startTest().then(result => {
    window.tpRecordTime(result);
  });
});
