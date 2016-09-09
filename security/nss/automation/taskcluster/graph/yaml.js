/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var fs = require("fs");
var yaml = require("js-yaml");

// Register custom YAML types.
var YAML_SCHEMA = yaml.Schema.create([
  // Point in time at $now + x hours.
  new yaml.Type('!from_now', {
    kind: "scalar",

    resolve: function (data) {
      return true;
    },

    construct: function (data) {
      var d = new Date();
      d.setHours(d.getHours() + (data|0));
      return d.toJSON();
    }
  }),

  // Environment variables.
  new yaml.Type('!env', {
    kind: "scalar",

    resolve: function (data) {
      return true;
    },

    construct: function (data) {
      return process.env[data] || "{{" + data.toLowerCase() + "}}";
    }
  })
]);

// Parse a given YAML file.
function parse(file, fallback) {
  // Return fallback if the file doesn't exist.
  if (!fs.existsSync(file) && fallback) {
    return fallback;
  }

  // Otherwise, read the file or fail.
  var source = fs.readFileSync(file, "utf-8");
  return yaml.load(source, {schema: YAML_SCHEMA});
}

module.exports.parse = parse;
