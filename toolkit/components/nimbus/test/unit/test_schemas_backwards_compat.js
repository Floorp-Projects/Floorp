/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

const { FeatureManifest } = ChromeUtils.import(
  "resource://nimbus/FeatureManifest.js"
);

Cu.importGlobalProperties(["fetch"]);

let SCHEMAS = {};

add_setup(async function setup() {
  function fetchSchema(uri) {
    return fetch(uri, { credentials: "omit" }).then(rsp => rsp.json());
  }

  const entries = await Promise.all(
    Object.entries(FeatureManifest)
      .filter(([, feature]) => typeof feature?.schema?.uri !== "undefined")
      .map(([featureId, feature]) =>
        fetchSchema(feature.schema.uri).then(schema => [featureId, schema])
      )
  );

  for (const [featureId, schema] of entries) {
    SCHEMAS[featureId] = schema;
  }
});

/**
 * Walk a JSON schema object, calling a function on each node corresponding to a
 * definition (that is, each node that contains a "type" key).
 *
 * @param {object}   schema The parsed schema object.
 * @param {Function} fn     The function to call on each definition.
 * @param {any}      node   The current definition being evaluated.
 * @param {Array}    path   The path to the current definition in JSON Schema
 *                          notation.
 */
function walkSchema(schema, fn, node = schema, path = []) {
  function formatPath(segments) {
    return `#/${segments.join("/")}`;
  }

  node = node ?? schema;

  if (typeof node === "object" && typeof node.$ref !== "undefined") {
    const subPath = path
      .substring(1)
      .split("/")
      .slice(1);

    let referent = schema;
    for (const segment of path) {
      referent = referent[segment];
    }

    walkSchema(schema, fn, referent, subPath);
  } else {
    fn(node, formatPath(path));

    if (node.type === "object" && typeof node.properties !== "undefined") {
      for (const [name, prop] of Object.entries(node.properties)) {
        walkSchema(schema, fn, prop, [...path, "properties", name]);
      }
    } else if (node.type === "array" && typeof node.items !== "undefined") {
      walkSchema(schema, fn, node.items, [...path, "items"]);
    }
  }
}

add_task(function test_additionalProperties() {
  for (const [featureId, schema] of Object.entries(SCHEMAS)) {
    info(`Checking schema for feature ID ${featureId}`);
    walkSchema(schema, (node, path) => {
      if (
        node.type === "object" &&
        Object.hasOwn(node, "additionalProperties")
      ) {
        if (node.additionalProperties === false) {
          Assert.ok(
            false,
            `Schema ${featureId} path ${path} has additionalProperties: false`
          );
        }
      }
    });
  }
});
