#!/usr/bin/env node
// SPDX-License-Identifier: MPL-2.0
import { ChromePipe } from "./chrome-cdp-pipe.mjs";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
const index = process.argv.indexOf("--server");
if (index < 0) throw Error("--server /path/to/server.json is required");
const file = resolve(process.argv[index + 1]);
const output = dirname(file);
/** @type {import("./sidepanel-test-types.ts").VaultInfo} */
const server = JSON.parse(readFileSync(file).toString("utf8"));
if (
  new URL(server.url).hostname !== "localhost" ||
  !server.url.startsWith("https://")
) throw Error("Only the genuine local test vault is allowed");
/** @type {import("./sidepanel-test-types.ts").VaultCredentials} */
const credentials = JSON.parse(
  readFileSync(join(output, "private-credentials.json")).toString("utf8"),
);
const run = join(output, "account-signup");
mkdirSync(run, { recursive: true });
const browser = new ChromePipe(join(output, "chrome-local-trust"), run, [
  "--no-proxy-server",
], { headed: true });
/** @param {number} ms */
const pause = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
/** @type {string|undefined} */
let session;
/** @template T @param {string} expression @returns {Promise<T>} */
const evaluate = async (expression) => {
  const result = await browser.call("Runtime.evaluate", {
    expression,
    returnByValue: true,
  }, session);
  if (result.exceptionDetails) throw Error(result.exceptionDetails.text);
  return /** @type {T} */ (result.result.value);
};
/** @param {string} expression */
const wait = async (expression) => {
  for (let n = 0; n < 200; n++) {
    if (await evaluate(expression)) return;
    await pause(100);
  }
  throw Error("Official Web Vault UI did not reach the expected signup step");
};
/** @param {string} selector */
const click = async (selector) => {
  /** @type {import("./sidepanel-test-types.ts").Point} */
  const point = await evaluate(
    `(()=>{const e=document.querySelector(${
      JSON.stringify(selector)
    });e.scrollIntoView({block:'center'});const r=e.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2};})()`,
  );
  for (const type of ["mousePressed", "mouseReleased"]) {
    await browser.call("Input.dispatchMouseEvent", {
      type,
      ...point,
      button: "left",
      clickCount: 1,
    }, session);
  }
};
/** @param {string} selector @param {string} text */
const type = async (selector, text) => {
  await evaluate(`document.querySelector(${JSON.stringify(selector)}).focus()`);
  await browser.call("Input.insertText", { text }, session);
};
try {
  const { targetId } = await browser.call("Target.createTarget", {
    url: server.url + "/#/signup",
  });
  session =
    (await browser.call("Target.attachToTarget", { targetId, flatten: true }))
      .sessionId;
  await wait("!!document.querySelector('#register-start_form_input_email')");
  await type("#register-start_form_input_email", credentials.email);
  await type("#register-start_form_input_name", "Floorp authored test account");
  await click("button[type=submit]");
  await wait("!!document.querySelector('#input-password-form_new-password')");
  await type("#input-password-form_new-password", credentials.masterPassword);
  await type(
    "#input-password-form_new-password-confirm",
    credentials.masterPassword,
  );
  if (
    await evaluate(
      "document.querySelector('#input-password-form_check-for-breaches')?.checked",
    )
  ) await click("#input-password-form_check-for-breaches");
  await click("button[type=submit]");
  await wait(
    "document.body.innerText.includes('Your new account has been created!')",
  );
  const proof = {
    passed: true,
    server: { image: server.image, url: server.url },
    runtime: await browser.call("Browser.getVersion"),
    accountCreatedThrough: "official Web Vault signup UI",
    ownEphemeralAccount: true,
    breachLookupDisabledInSignupUI: true,
  };
  writeFileSync(join(run, "report.json"), JSON.stringify(proof, null, 2));
  server.accountCreatedThrough = proof.accountCreatedThrough;
  server.accountCreationProof = "account-signup/report.json";
  writeFileSync(file, JSON.stringify(server, null, 2));
  console.log(
    JSON.stringify({ passed: true, report: join(run, "report.json") }),
  );
} finally {
  await browser.close();
}
