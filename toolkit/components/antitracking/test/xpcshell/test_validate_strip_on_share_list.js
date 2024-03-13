/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { JsonSchema } = ChromeUtils.importESModule(
  "resource://gre/modules/JsonSchema.sys.mjs"
);

let stripOnShareMergedList, stripOnShareLGPLParams, stripOnShareList;

async function fetchAndParseList(fileName) {
  let response = await fetch("chrome://global/content/" + fileName);
  if (!response.ok) {
    throw new Error(
      "Error fetching strip-on-share strip list" + response.status
    );
  }
  return await response.json();
}

async function validateSchema(paramList, nameOfList) {
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

  let validator = new JsonSchema.Validator(schema);
  let { valid, errors } = validator.validate(paramList);

  if (!valid) {
    info(
      nameOfList +
        " JSON contains these validation errors: " +
        JSON.stringify(errors, null, 2)
    );
  }

  Assert.ok(valid, nameOfList + " JSON is valid");
}

// Fetching strip on share list
add_setup(async function () {
  /* globals fetch */

  [stripOnShareList, stripOnShareLGPLParams] = await Promise.all([
    fetchAndParseList("antitracking/StripOnShare.json"),
    fetchAndParseList("antitracking/StripOnShareLGPL.json"),
  ]);

  stripOnShareMergedList = stripOnShareList;

  // Combines the mozilla licensed strip on share param
  // list and the LGPL licensed strip on share param list
  for (const key in stripOnShareLGPLParams) {
    if (Object.hasOwn(stripOnShareMergedList, key)) {
      stripOnShareMergedList[key].queryParams.push(
        ...stripOnShareLGPLParams[key].queryParams
      );
    } else {
      stripOnShareMergedList[key] = stripOnShareLGPLParams[key];
    }
  }
});

// Check if the Strip on Share list contains any duplicate params
add_task(async function test_check_duplicates() {
  for (const domain in stripOnShareMergedList) {
    // Creates a set of query parameters for a given domain to check
    // for duplicates
    let setOfParams = new Set(stripOnShareMergedList[domain].queryParams);

    // If there are duplicates which is known because the sizes of the set
    // and array don't match, then we check which parameters are duplciates
    if (setOfParams.size != stripOnShareMergedList[domain].queryParams.length) {
      let setToCheckDupes = new Set();
      let dupeList = new Set();
      for (let param in stripOnShareMergedList[domain].queryParams) {
        let tempParam = stripOnShareMergedList[domain].queryParams[param];

        if (setToCheckDupes.has(tempParam)) {
          dupeList.add(tempParam);
        } else {
          setToCheckDupes.add(tempParam);
        }
      }

      Assert.equal(
        setOfParams.size,
        stripOnShareMergedList[domain].queryParams.length,
        "There are duplicates rules in " +
          domain +
          ". The duplicate rules are " +
          [...dupeList]
      );
    }

    Assert.equal(
      setOfParams.size,
      stripOnShareMergedList[domain].queryParams.length,
      "There are no duplicates rules."
    );
  }
});

// Validate the format of Strip on Share list with Schema
add_task(async function test_check_schema() {
  validateSchema(stripOnShareLGPLParams, "Strip On Share LGPL");
  validateSchema(stripOnShareList, "Strip On Share");
});
