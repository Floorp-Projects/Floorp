/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env node, jest */
"use strict";

const fs = require("fs");
const path = require("path");
const { ts_link } = require("../build_xpcom.js");

const test_dts = path.join(__dirname, "../fixtures/xpctest.d.ts");
const dir = path.join(__dirname, "../../../xpcom/idl-parser/xpidl/fixtures/");
const files = ["xpctest.d.json"];

test("xpctest.d.json produces expected baseline d.ts", () => {
  let dts = ts_link(dir, files).join("\n");
  expect(dts).toEqual(fs.readFileSync(test_dts, "utf8"));
});
