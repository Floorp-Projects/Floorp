/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["Runtime"];

const {t} = ChromeUtils.import("chrome://remote/content/Protocol.jsm");

var Runtime = {
  StackTrace: {schema: t.String},

  RemoteObject: {
    schema: t.Either(
        {
          type: t.Enum([
            "object",
            "function",
            "undefined",
            "string",
            "number",
            "boolean",
            "symbol",
            "bigint",
          ]),
          subtype: t.Optional(t.Enum([
            "array",
            "date",
            "error",
            "map",
            "node",
            "null",
            "promise",
            "proxy",
            "regexp",
            "set",
            "typedarray",
            "weakmap",
            "weakset",
          ])),
          objectId: t.String,
      },
      {
        unserializableValue: t.Enum(["Infinity", "-Infinity", "-0", "NaN"]),
      },
      {
        value: t.Any,
      }),
  },
};
