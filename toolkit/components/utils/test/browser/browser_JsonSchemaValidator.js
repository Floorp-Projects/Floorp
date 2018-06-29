/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

ChromeUtils.import("resource://gre/modules/components-utils/JsonSchemaValidator.jsm", this);

add_task(async function test_boolean_values() {
  let schema = {
    type: "boolean"
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(true, schema);
  ok(valid && parsed === true, "Parsed boolean value correctly");

  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(false, schema);
  ok(valid && parsed === false, "Parsed boolean value correctly");

  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(0, schema);
  ok(valid && parsed === false, "0 parsed as false correctly");

  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(1, schema);
  ok(valid && parsed === true, "1 parsed as true correctly");

  // Invalid values:
  ok(!JsonSchemaValidator.validateAndParseParameters("0", schema)[0], "No type coercion");
  ok(!JsonSchemaValidator.validateAndParseParameters("true", schema)[0], "No type coercion");
  ok(!JsonSchemaValidator.validateAndParseParameters(2, schema)[0], "Other number values are not valid");
  ok(!JsonSchemaValidator.validateAndParseParameters(undefined, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters(null, schema)[0], "Invalid value");
});

add_task(async function test_number_values() {
  let schema = {
    type: "number"
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(1, schema);
  ok(valid && parsed === 1, "Parsed number value correctly");

  // Invalid values:
  ok(!JsonSchemaValidator.validateAndParseParameters("1", schema)[0], "No type coercion");
  ok(!JsonSchemaValidator.validateAndParseParameters(true, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters(null, schema)[0], "Invalid value");
});

add_task(async function test_integer_values() {
  // Integer is an alias for number
  let schema = {
    type: "integer"
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(1, schema);
  ok(valid && parsed == 1, "Parsed integer value correctly");

  // Invalid values:
  ok(!JsonSchemaValidator.validateAndParseParameters("1", schema)[0], "No type coercion");
  ok(!JsonSchemaValidator.validateAndParseParameters(true, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters(null, schema)[0], "Invalid value");
});

add_task(async function test_string_values() {
  let schema = {
    type: "string"
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters("foobar", schema);
  ok(valid && parsed == "foobar", "Parsed string value correctly");

  // Invalid values:
  ok(!JsonSchemaValidator.validateAndParseParameters(1, schema)[0], "No type coercion");
  ok(!JsonSchemaValidator.validateAndParseParameters(true, schema)[0], "No type coercion");
  ok(!JsonSchemaValidator.validateAndParseParameters(undefined, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters(null, schema)[0], "Invalid value");
});

add_task(async function test_URL_values() {
  let schema = {
    type: "URL"
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters("https://www.example.com/foo#bar", schema);
  ok(valid, "URL is valid");
  ok(parsed instanceof URL, "parsed is a URL");
  is(parsed.origin, "https://www.example.com", "origin is correct");
  is(parsed.pathname + parsed.hash, "/foo#bar", "pathname is correct");

  // Invalid values:
  ok(!JsonSchemaValidator.validateAndParseParameters("", schema)[0], "Empty string is not accepted for URL");
  ok(!JsonSchemaValidator.validateAndParseParameters("www.example.com", schema)[0], "Scheme is required for URL");
  ok(!JsonSchemaValidator.validateAndParseParameters("https://:!$%", schema)[0], "Invalid URL");
  ok(!JsonSchemaValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
});

add_task(async function test_URLorEmpty_values() {
  let schema = {
    type: "URLorEmpty"
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters("https://www.example.com/foo#bar", schema);
  ok(valid, "URL is valid");
  ok(parsed instanceof URL, "parsed is a nsIURI");
  is(parsed.origin, "https://www.example.com", "origin is correct");
  is(parsed.pathname + parsed.hash, "/foo#bar", "pathname is correct");

  // Test that this type also accept empty strings
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters("", schema);
  ok(valid, "URLorEmpty is valid");
  ok(!parsed, "parsed value is falsy");
  is(typeof(parsed), "string", "parsed is a string");
  is(parsed, "", "parsed is an empty string");

  // Invalid values:
  ok(!JsonSchemaValidator.validateAndParseParameters(" ", schema)[0], "Non-empty string is not accepted");
  ok(!JsonSchemaValidator.validateAndParseParameters("www.example.com", schema)[0], "Scheme is required for URL");
  ok(!JsonSchemaValidator.validateAndParseParameters("https://:!$%", schema)[0], "Invalid URL");
  ok(!JsonSchemaValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
});


add_task(async function test_origin_values() {
  // Origin is a URL that doesn't contain a path/query string (i.e., it's only scheme + host + port)
  let schema = {
    type: "origin"
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters("https://www.example.com", schema);
  ok(valid, "Origin is valid");
  ok(parsed instanceof URL, "parsed is a nsIURI");
  is(parsed.origin, "https://www.example.com", "origin is correct");
  is(parsed.pathname + parsed.hash, "/", "pathname is corect");

  // Invalid values:
  ok(!JsonSchemaValidator.validateAndParseParameters("https://www.example.com/foobar", schema)[0], "Origin cannot contain a path part");
  ok(!JsonSchemaValidator.validateAndParseParameters("https://:!$%", schema)[0], "Invalid origin");
  ok(!JsonSchemaValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
});

add_task(async function test_array_values() {
  // The types inside an array object must all be the same
  let schema = {
    type: "array",
    items: {
      type: "number"
    }
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters([1, 2, 3], schema);
  ok(valid, "Array is valid");
  ok(Array.isArray(parsed), "parsed is an array");
  is(parsed.length, 3, "array is correct");

  // An empty array is also valid
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters([], schema);
  ok(valid, "Array is valid");
  ok(Array.isArray(parsed), "parsed is an array");
  is(parsed.length, 0, "array is correct");

  // Invalid values:
  ok(!JsonSchemaValidator.validateAndParseParameters([1, true, 3], schema)[0], "Mixed types");
  ok(!JsonSchemaValidator.validateAndParseParameters(2, schema)[0], "Type is correct but not in an array");
  ok(!JsonSchemaValidator.validateAndParseParameters({}, schema)[0], "Object is not an array");
});

add_task(async function test_non_strict_arrays() {
  // Non-srict arrays ignores invalid values (don't include
  // them in the parsed output), instead of failing the validation.
  // Note: invalid values might still report errors to the console.
  let schema = {
    type: "array",
    strict: false,
    items: {
      type: "string"
    }
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(
    ["valid1", "valid2", false, 3, "valid3"], schema);
  ok(valid, "Array is valid");
  ok(Array.isArray(parsed, "parsed is an array"));
  is(parsed.length, 3, "Only valid values were included in the parsed array");
  Assert.deepEqual(parsed, ["valid1", "valid2", "valid3"], "Results were expected");

  // Checks that strict defaults to true;
  delete schema.strict;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(
    ["valid1", "valid2", false, 3, "valid3"], schema);
  ok(!valid, "Same verification was invalid without strict=false");
});

add_task(async function test_object_values() {
  let schema = {
    type: "object",
    properties: {
      url: {
        type: "URL"
      },
      title: {
        type: "string"
      }
    }
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(
    {
      url: "https://www.example.com/foo#bar",
      title: "Foo",
      alias: "Bar"
    },
    schema);

  ok(valid, "Object is valid");
  ok(typeof(parsed) == "object", "parsed in an object");
  ok(parsed.url instanceof URL, "types inside the object are also parsed");
  is(parsed.url.href, "https://www.example.com/foo#bar", "URL was correctly parsed");
  is(parsed.title, "Foo", "title was correctly parsed");
  is(parsed.alias, undefined, "property not described in the schema is not present in the parsed object");

  // Invalid values:
  ok(!JsonSchemaValidator.validateAndParseParameters(
    {
      url: "https://www.example.com/foo#bar",
      title: 3,
    },
    schema)[0], "Mismatched type for title");

  ok(!JsonSchemaValidator.validateAndParseParameters(
    {
      url: "www.example.com",
      title: 3,
    },
    schema)[0], "Invalid URL inside the object");
});

add_task(async function test_array_of_objects() {
  // This schema is used, for example, for bookmarks
  let schema = {
    type: "array",
    items: {
      type: "object",
      properties: {
        url: {
          type: "URL",
        },
        title: {
          type: "string"
        }
      }
    }
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(
    [{
      url: "https://www.example.com/bookmark1",
      title: "Foo",
    },
    {
      url: "https://www.example.com/bookmark2",
      title: "Bar",
    }],
    schema);

  ok(valid, "Array is valid");
  is(parsed.length, 2, "Correct number of items");

  ok(typeof(parsed[0]) == "object" && typeof(parsed[1]) == "object", "Correct objects inside array");

  is(parsed[0].url.href, "https://www.example.com/bookmark1", "Correct URL for bookmark 1");
  is(parsed[1].url.href, "https://www.example.com/bookmark2", "Correct URL for bookmark 2");

  is(parsed[0].title, "Foo", "Correct title for bookmark 1");
  is(parsed[1].title, "Bar", "Correct title for bookmark 2");
});

add_task(async function test_missing_arrays_inside_objects() {
  let schema = {
    type: "object",
    properties: {
      allow: {
        type: "array",
        items: {
          type: "boolean"
        }
      },
      block: {
        type: "array",
        items: {
          type: "boolean"
        }
      }

    }
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters({
    allow: [true, true, true]
  }, schema);

  ok(valid, "Object is valid");
  is(parsed.allow.length, 3, "Allow array is correct.");
  is(parsed.block, undefined, "Block array is undefined, as expected.");
});

add_task(async function test_required_vs_nonrequired_properties() {
  let schema = {
    type: "object",
    properties: {
      "non-required-property": {
        type: "number"
      },

      "required-property": {
        type: "number"
      }
    },
    required: ["required-property"]
  };

  let valid, parsed;
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters({
    "required-property": 5
  }, schema);

  ok(valid, "Object is valid since required property is present");
  is(parsed["required-property"], 5, "required property is correct");
  is(parsed["non-required-property"], undefined, "non-required property is undefined, as expected");

  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters({
    "non-required-property": 5
  }, schema);

  ok(!valid, "Object is not valid since the required property is missing");
  is(parsed, null, "Nothing was returned as parsed");
});

add_task(async function test_number_or_string_values() {
  let schema = {
    type: ["number", "string"],
  };

  let valid, parsed;
  // valid values
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(1, schema);
  ok(valid && parsed === 1, "Parsed number value correctly");
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters("foobar", schema);
  ok(valid && parsed === "foobar", "Parsed string value correctly");
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters("1", schema);
  ok(valid && parsed === "1", "Did not coerce string to number");

  // Invalid values:
  ok(!JsonSchemaValidator.validateAndParseParameters(true, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters(null, schema)[0], "Invalid value");
});

add_task(async function test_number_or_array_values() {
  let schema = {
    type: ["number", "array"],
    items: {
      type: "number",
    }
  };

  let valid, parsed;
  // valid values
  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters(1, schema);
  ok(valid, "Number is valid");
  is(parsed, 1, "Parsed correctly");
  ok(valid && parsed === 1, "Parsed number value correctly");

  [valid, parsed] = JsonSchemaValidator.validateAndParseParameters([1, 2, 3], schema);
  ok(valid, "Array is valid");
  Assert.deepEqual(parsed, [1, 2, 3], "Parsed correctly");

  // Invalid values:
  ok(!JsonSchemaValidator.validateAndParseParameters(true, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters({}, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters(null, schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters(["a", "b"], schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters([[]], schema)[0], "Invalid value");
  ok(!JsonSchemaValidator.validateAndParseParameters([0, 1, [2, 3]], schema)[0], "Invalid value");
});
