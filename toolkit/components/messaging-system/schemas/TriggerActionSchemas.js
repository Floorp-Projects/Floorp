/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["TriggerActionSchemas"];

const TriggerActionSchemas = {
  openURL: {
    description:
      "Happens every time the user loads a new URL that matches the provided `hosts` or `patterns`",
    properties: {
      id: {
        type: "string",
        enum: ["openURL"],
      },
      params: {
        type: "array",
        items: {
          type: "string",
        },
        description: "List of urls we should match against",
      },
      patterns: {
        type: "array",
        items: {
          type: "string",
        },
        description: "List of Match pattern compatible strings to match agaist",
      },
    },
    type: "object",
    anyOf: [{ required: ["id", "params"] }, { required: ["id", "patterns"] }],
  },
  openArticleURL: {
    description:
      "Happens every time the user loads a document that is Reader Mode compatible.",
    properties: {
      id: {
        type: "string",
        enum: ["openArticleURL"],
      },
      params: {
        type: "array",
        items: {
          type: "string",
        },
        description: "List of urls we should match against",
      },
      patterns: {
        type: "array",
        items: {
          type: "string",
        },
        description: "List of Match pattern compatible strings to match agaist",
      },
    },
    type: "object",
    anyOf: [{ required: ["id", "params"] }, { required: ["id", "patterns"] }],
  },
  openBookmarkedURL: {
    description:
      "Happens every time the user adds a bookmark from the URL bar star icon",
    properties: {
      id: {
        type: "string",
        enum: ["openBookmarkedURL"],
      },
    },
    type: "object",
  },
  frequentVisits: {
    description:
      "Happens every time a user navigates (or switches tab to) to any of the `hosts` or `patterns` arguments but additionally provides information about the number of accesses to the matched domain.",
    properties: {
      id: {
        type: "string",
        enum: ["frequentVisits"],
      },
      params: {
        type: "array",
        items: {
          type: "string",
        },
        description: "List of urls we should match against",
      },
      patterns: {
        type: "array",
        items: {
          type: "string",
        },
        description: "List of Match pattern compatible strings to match agaist",
      },
    },
    type: "object",
    anyOf: [{ required: ["id", "params"] }, { required: ["id", "patterns"] }],
  },
  newSavedLogin: {
    description: "Happens every time the user adds or updates a login.",
    properties: {
      id: {
        type: "string",
        enum: ["newSavedLogin"],
      },
    },
    type: "object",
  },
  contentBlocking: {
    description:
      "Happens every time Firefox blocks the loading of a page script/asset/resource that matches the one of the tracking behaviours specifid through params",
    type: "object",
    properties: {
      id: {
        type: "string",
        enum: ["contentBlocking"],
      },
      params: {
        type: "array",
        items: {
          type: "number",
        },
        minItems: 1,
      },
    },
    required: ["id", "params"],
  },
};
