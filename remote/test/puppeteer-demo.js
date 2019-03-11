/**
 * To run the server:
 * $ ./mach run --remote-debugging-port=9000 --headless
 *   (this requires `ac_add_options --enable-cdp` to be set in your mozconfig)
 *
 * To run the test script:
 * $ npm install puppeteer
 * $ DEBUG="puppeteer:protocol" node puppeteer-demo.js
 */

/* global require */

const puppeteer = require("puppeteer");

console.log("Calling puppeteer.connect");
puppeteer.connect({ browserURL: "http://localhost:9000"}).then(async browser => {
  console.log("Connect success!");

  const page = await browser.newPage();
  console.log("page", !!page);
  await page.goto("https://www.mozilla.org/");

  return browser.close();
});
