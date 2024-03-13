/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { JsonSchema } = ChromeUtils.importESModule(
  "resource://gre/modules/JsonSchema.sys.mjs"
);

let stripOnShareList;

// Fetching strip on share list
add_setup(async function () {
  /* globals fetch */
  let response = await fetch(
    "chrome://global/content/antitracking/StripOnShare.json"
  );
  if (!response.ok) {
    throw new Error(
      "Error fetching strip-on-share strip list" + response.status
    );
  }
  stripOnShareList = await response.json();
});

// Check if the Strip on Share list contains any duplicate params
add_task(async function test_check_duplicates() {
  let stripOnShareParams = stripOnShareList;

  const allQueryParams = [];

  for (const domain in stripOnShareParams) {
    for (let param in stripOnShareParams[domain].queryParams) {
      allQueryParams.push(stripOnShareParams[domain].queryParams[param]);
    }
  }

  let setOfParams = new Set(allQueryParams);

  if (setOfParams.size != allQueryParams.length) {
    let setToCheckDupes = new Set();
    let dupeList = new Set();
    for (const domain in stripOnShareParams) {
      for (let param in stripOnShareParams[domain].queryParams) {
        let tempParam = stripOnShareParams[domain].queryParams[param];

        if (setToCheckDupes.has(tempParam)) {
          dupeList.add(tempParam);
        } else {
          setToCheckDupes.add(tempParam);
        }
      }
    }

    Assert.equal(
      setOfParams.size,
      allQueryParams.length,
      "There are duplicates rules. The duplicate rules are " + [...dupeList]
    );
  }

  Assert.equal(
    setOfParams.size,
    allQueryParams.length,
    "There are no duplicates rules."
  );
});

// Validate the format of Strip on Share list with Schema
add_task(async function test_check_schema() {
  let schema = {
    $schema: "http://json-schema.org/draft-07/schema#",
    type: "object",
    properties: {
      type: "object",
      properties: {
        queryParams: {
          type: "array",
          items: { type: "string" },
        },
        topLevelSites: {
          type: "array",
          items: { type: "string" },
        },
      },
      required: ["queryParams", "topLevelSites"],
    },
    required: ["global"],
  };

  let stripOnShareParams = stripOnShareList;
  let validator = new JsonSchema.Validator(schema);
  let { valid, errors } = validator.validate(stripOnShareParams);

  if (!valid) {
    info("validation errors: " + JSON.stringify(errors, null, 2));
  }

  Assert.ok(valid, "Strip on share JSON is valid");
});
