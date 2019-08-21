#!/usr/bin/env node
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

/**
 * This script exports the schemas from this package in JSON format, for use by
 * other tools. It is run as a part of the publishing process to NPM.
 */

const fs = require("fs");
const schemas = require("./index.js");

fs.writeFile("./schemas.json", JSON.stringify(schemas), err => {
  if (err) {
    console.error("error", err);
  }
});
