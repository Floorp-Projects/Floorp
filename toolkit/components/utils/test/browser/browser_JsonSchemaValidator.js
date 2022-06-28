/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { JsonSchemaValidator } = ChromeUtils.import(
  "resource://gre/modules/components-utils/JsonSchemaValidator.jsm"
);

add_task(async function test_boolean_values() {
  let schema = {
    type: "boolean",
  };

  // valid values
  validate({
    value: true,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: true,
    },
  });
  validate({
    value: false,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: false,
    },
  });
  validate({
    value: 0,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: false,
    },
  });
  validate({
    value: 1,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: true,
    },
  });

  // Invalid values:
  validate({
    value: "0",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "0",
      error: {
        invalidValue: "0",
        invalidPropertyNameComponents: [],
        message: `The value '"0"' does not match the expected type 'boolean'`,
      },
    },
  });
  validate({
    value: "true",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "true",
      error: {
        invalidValue: "true",
        invalidPropertyNameComponents: [],
        message: `The value '"true"' does not match the expected type 'boolean'`,
      },
    },
  });
  validate({
    value: 2,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: 2,
      error: {
        invalidValue: 2,
        invalidPropertyNameComponents: [],
        message: `The value '2' does not match the expected type 'boolean'`,
      },
    },
  });
  validate({
    value: undefined,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: undefined,
        invalidPropertyNameComponents: [],
        message: `The value 'undefined' does not match the expected type 'boolean'`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: {},
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match the expected type 'boolean'`,
      },
    },
  });
  validate({
    value: null,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: null,
      error: {
        invalidValue: null,
        invalidPropertyNameComponents: [],
        message: `The value 'null' does not match the expected type 'boolean'`,
      },
    },
  });
});

add_task(async function test_number_values() {
  let schema = {
    type: "number",
  };

  validate({
    value: 1,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: 1,
    },
  });

  // Invalid values:
  validate({
    value: "1",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "1",
      error: {
        invalidValue: "1",
        invalidPropertyNameComponents: [],
        message: `The value '"1"' does not match the expected type 'number'`,
      },
    },
  });
  validate({
    value: true,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: true,
      error: {
        invalidValue: true,
        invalidPropertyNameComponents: [],
        message: `The value 'true' does not match the expected type 'number'`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: {},
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match the expected type 'number'`,
      },
    },
  });
  validate({
    value: null,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: null,
      error: {
        invalidValue: null,
        invalidPropertyNameComponents: [],
        message: `The value 'null' does not match the expected type 'number'`,
      },
    },
  });
});

add_task(async function test_integer_values() {
  // Integer is an alias for number
  let schema = {
    type: "integer",
  };

  validate({
    value: 1,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: 1,
    },
  });

  // Invalid values:
  validate({
    value: "1",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "1",
      error: {
        invalidValue: "1",
        invalidPropertyNameComponents: [],
        message: `The value '"1"' does not match the expected type 'integer'`,
      },
    },
  });
  validate({
    value: true,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: true,
      error: {
        invalidValue: true,
        invalidPropertyNameComponents: [],
        message: `The value 'true' does not match the expected type 'integer'`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: {},
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match the expected type 'integer'`,
      },
    },
  });
  validate({
    value: null,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: null,
      error: {
        invalidValue: null,
        invalidPropertyNameComponents: [],
        message: `The value 'null' does not match the expected type 'integer'`,
      },
    },
  });
});

add_task(async function test_null_values() {
  let schema = {
    type: "null",
  };

  validate({
    value: null,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: null,
    },
  });

  // Invalid values:
  validate({
    value: 1,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: 1,
      error: {
        invalidValue: 1,
        invalidPropertyNameComponents: [],
        message: `The value '1' does not match the expected type 'null'`,
      },
    },
  });
  validate({
    value: "1",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "1",
      error: {
        invalidValue: "1",
        invalidPropertyNameComponents: [],
        message: `The value '"1"' does not match the expected type 'null'`,
      },
    },
  });
  validate({
    value: true,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: true,
      error: {
        invalidValue: true,
        invalidPropertyNameComponents: [],
        message: `The value 'true' does not match the expected type 'null'`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: {},
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match the expected type 'null'`,
      },
    },
  });
  validate({
    value: [],
    schema,
    expectedResult: {
      valid: false,
      parsedValue: [],
      error: {
        invalidValue: [],
        invalidPropertyNameComponents: [],
        message: `The value '[]' does not match the expected type 'null'`,
      },
    },
  });
});

add_task(async function test_string_values() {
  let schema = {
    type: "string",
  };

  validate({
    value: "foobar",
    schema,
    expectedResult: {
      valid: true,
      parsedValue: "foobar",
    },
  });

  // Invalid values:
  validate({
    value: 1,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: 1,
      error: {
        invalidValue: 1,
        invalidPropertyNameComponents: [],
        message: `The value '1' does not match the expected type 'string'`,
      },
    },
  });
  validate({
    value: true,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: true,
      error: {
        invalidValue: true,
        invalidPropertyNameComponents: [],
        message: `The value 'true' does not match the expected type 'string'`,
      },
    },
  });
  validate({
    value: undefined,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: undefined,
        invalidPropertyNameComponents: [],
        message: `The value 'undefined' does not match the expected type 'string'`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: {},
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match the expected type 'string'`,
      },
    },
  });
  validate({
    value: null,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: null,
      error: {
        invalidValue: null,
        invalidPropertyNameComponents: [],
        message: `The value 'null' does not match the expected type 'string'`,
      },
    },
  });
});

add_task(async function test_URL_values() {
  let schema = {
    type: "URL",
  };

  let result = validate({
    value: "https://www.example.com/foo#bar",
    schema,
    expectedResult: {
      valid: true,
      parsedValue: new URL("https://www.example.com/foo#bar"),
    },
  });
  Assert.ok(URL.isInstance(result.parsedValue), "parsedValue is a URL");
  Assert.equal(
    result.parsedValue.origin,
    "https://www.example.com",
    "origin is correct"
  );
  Assert.equal(
    result.parsedValue.pathname + result.parsedValue.hash,
    "/foo#bar",
    "pathname is correct"
  );

  // Invalid values:
  validate({
    value: "",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "",
      error: {
        invalidValue: "",
        invalidPropertyNameComponents: [],
        message: `The value '""' does not match the expected type 'URL'`,
      },
    },
  });
  validate({
    value: "www.example.com",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "www.example.com",
      error: {
        invalidValue: "www.example.com",
        invalidPropertyNameComponents: [],
        message:
          `The value '"www.example.com"' does not match the expected ` +
          `type 'URL'`,
      },
    },
  });
  validate({
    value: "https://:!$%",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "https://:!$%",
      error: {
        invalidValue: "https://:!$%",
        invalidPropertyNameComponents: [],
        message: `The value '"https://:!$%"' does not match the expected type 'URL'`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: {},
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match the expected type 'URL'`,
      },
    },
  });
});

add_task(async function test_URLorEmpty_values() {
  let schema = {
    type: "URLorEmpty",
  };

  let result = validate({
    value: "https://www.example.com/foo#bar",
    schema,
    expectedResult: {
      valid: true,
      parsedValue: new URL("https://www.example.com/foo#bar"),
    },
  });
  Assert.ok(URL.isInstance(result.parsedValue), "parsedValue is a URL");
  Assert.equal(
    result.parsedValue.origin,
    "https://www.example.com",
    "origin is correct"
  );
  Assert.equal(
    result.parsedValue.pathname + result.parsedValue.hash,
    "/foo#bar",
    "pathname is correct"
  );

  // Test that this type also accept empty strings
  result = validate({
    value: "",
    schema,
    expectedResult: {
      valid: true,
      parsedValue: "",
    },
  });
  Assert.equal(typeof result.parsedValue, "string", "parsedValue is a string");

  // Invalid values:
  validate({
    value: " ",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: " ",
      error: {
        invalidValue: " ",
        invalidPropertyNameComponents: [],
        message: `The value '" "' does not match the expected type 'URLorEmpty'`,
      },
    },
  });
  validate({
    value: "www.example.com",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "www.example.com",
      error: {
        invalidValue: "www.example.com",
        invalidPropertyNameComponents: [],
        message:
          `The value '"www.example.com"' does not match the expected ` +
          `type 'URLorEmpty'`,
      },
    },
  });
  validate({
    value: "https://:!$%",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "https://:!$%",
      error: {
        invalidValue: "https://:!$%",
        invalidPropertyNameComponents: [],
        message:
          `The value '"https://:!$%"' does not match the expected ` +
          `type 'URLorEmpty'`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: {},
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match the expected type 'URLorEmpty'`,
      },
    },
  });
});

add_task(async function test_origin_values() {
  // Origin is a URL that doesn't contain a path/query string (i.e., it's only scheme + host + port)
  let schema = {
    type: "origin",
  };

  let result = validate({
    value: "https://www.example.com",
    schema,
    expectedResult: {
      valid: true,
      parsedValue: new URL("https://www.example.com/"),
    },
  });
  Assert.ok(URL.isInstance(result.parsedValue), "parsedValue is a URL");
  Assert.equal(
    result.parsedValue.origin,
    "https://www.example.com",
    "origin is correct"
  );
  Assert.equal(
    result.parsedValue.pathname + result.parsedValue.hash,
    "/",
    "pathname is correct"
  );

  // Invalid values:
  validate({
    value: "https://www.example.com/foobar",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: new URL("https://www.example.com/foobar"),
      error: {
        invalidValue: "https://www.example.com/foobar",
        invalidPropertyNameComponents: [],
        message:
          `The value '"https://www.example.com/foobar"' does not match the ` +
          `expected type 'origin'`,
      },
    },
  });
  validate({
    value: "https://:!$%",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "https://:!$%",
      error: {
        invalidValue: "https://:!$%",
        invalidPropertyNameComponents: [],
        message:
          `The value '"https://:!$%"' does not match the expected ` +
          `type 'origin'`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: {},
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match the expected type 'origin'`,
      },
    },
  });
});

add_task(async function test_origin_file_values() {
  // File URLs can also be origins
  let schema = {
    type: "origin",
  };

  let result = validate({
    value: "file:///foo/bar",
    schema,
    expectedResult: {
      valid: true,
      parsedValue: new URL("file:///foo/bar"),
    },
  });
  Assert.ok(URL.isInstance(result.parsedValue), "parsedValue is a URL");
  Assert.equal(
    result.parsedValue.href,
    "file:///foo/bar",
    "Should get what we passed in"
  );
});

add_task(async function test_origin_file_values() {
  // File URLs can also be origins
  let schema = {
    type: "origin",
  };

  let result = validate({
    value: "file:///foo/bar/foobar.html",
    schema,
    expectedResult: {
      valid: true,
      parsedValue: new URL("file:///foo/bar/foobar.html"),
    },
  });
  Assert.ok(URL.isInstance(result.parsedValue), "parsedValue is a URL");
  Assert.equal(
    result.parsedValue.href,
    "file:///foo/bar/foobar.html",
    "Should get what we passed in"
  );
});

add_task(async function test_array_values() {
  // The types inside an array object must all be the same
  let schema = {
    type: "array",
    items: {
      type: "number",
    },
  };

  validate({
    value: [1, 2, 3],
    schema,
    expectedResult: {
      valid: true,
      parsedValue: [1, 2, 3],
    },
  });

  // An empty array is also valid
  validate({
    value: [],
    schema,
    expectedResult: {
      valid: true,
      parsedValue: [],
    },
  });

  // Invalid values:
  validate({
    value: [1, true, 3],
    schema,
    expectedResult: {
      valid: false,
      parsedValue: true,
      error: {
        invalidValue: true,
        invalidPropertyNameComponents: [1],
        message:
          `The value 'true' does not match the expected type 'number'. The ` +
          `invalid value is property '1' in [1,true,3]`,
      },
    },
  });
  validate({
    value: 2,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: 2,
        invalidPropertyNameComponents: [],
        message: `The value '2' does not match the expected type 'array'`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match the expected type 'array'`,
      },
    },
  });
});

add_task(async function test_non_strict_arrays() {
  // Non-strict arrays ignores invalid values (don't include
  // them in the parsed output), instead of failing the validation.
  // Note: invalid values might still report errors to the console.
  let schema = {
    type: "array",
    strict: false,
    items: {
      type: "string",
    },
  };

  validate({
    value: ["valid1", "valid2", false, 3, "valid3"],
    schema,
    expectedResult: {
      valid: true,
      parsedValue: ["valid1", "valid2", "valid3"],
    },
  });

  // Checks that strict defaults to true;
  delete schema.strict;
  validate({
    value: ["valid1", "valid2", false, 3, "valid3"],
    schema,
    expectedResult: {
      valid: false,
      parsedValue: false,
      error: {
        invalidValue: false,
        invalidPropertyNameComponents: [2],
        message:
          `The value 'false' does not match the expected type 'string'. The ` +
          `invalid value is property '2' in ` +
          `["valid1","valid2",false,3,"valid3"]`,
      },
    },
  });

  // Pass allowArrayNonMatchingItems, should be valid
  validate({
    value: ["valid1", "valid2", false, 3, "valid3"],
    schema,
    options: {
      allowArrayNonMatchingItems: true,
    },
    expectedResult: {
      valid: true,
      parsedValue: ["valid1", "valid2", "valid3"],
    },
  });
});

add_task(async function test_object_values() {
  // valid values below

  validate({
    value: {
      foo: "hello",
      bar: 123,
    },
    schema: {
      type: "object",
      properties: {
        foo: {
          type: "string",
        },
        bar: {
          type: "number",
        },
      },
    },
    expectedResult: {
      valid: true,
      parsedValue: {
        foo: "hello",
        bar: 123,
      },
    },
  });

  validate({
    value: {
      foo: "hello",
      bar: {
        baz: 123,
      },
    },
    schema: {
      type: "object",
      properties: {
        foo: {
          type: "string",
        },
        bar: {
          type: "object",
          properties: {
            baz: {
              type: "number",
            },
          },
        },
      },
    },
    expectedResult: {
      valid: true,
      parsedValue: {
        foo: "hello",
        bar: {
          baz: 123,
        },
      },
    },
  });

  // allowExtraProperties
  let result = validate({
    value: {
      url: "https://www.example.com/foo#bar",
      title: "Foo",
      alias: "Bar",
    },
    schema: {
      type: "object",
      properties: {
        url: {
          type: "URL",
        },
        title: {
          type: "string",
        },
      },
    },
    options: {
      allowExtraProperties: true,
    },
    expectedResult: {
      valid: true,
      parsedValue: {
        url: new URL("https://www.example.com/foo#bar"),
        title: "Foo",
      },
    },
  });
  Assert.ok(
    URL.isInstance(result.parsedValue.url),
    "types inside the object are also parsed"
  );
  Assert.equal(
    result.parsedValue.url.href,
    "https://www.example.com/foo#bar",
    "URL was correctly parsed"
  );

  // allowExplicitUndefinedProperties
  validate({
    value: {
      foo: undefined,
    },
    schema: {
      type: "object",
      properties: {
        foo: {
          type: "string",
        },
      },
    },
    options: {
      allowExplicitUndefinedProperties: true,
    },
    expectedResult: {
      valid: true,
      parsedValue: {},
    },
  });

  // allowNullAsUndefinedProperties
  validate({
    value: {
      foo: null,
    },
    schema: {
      type: "object",
      properties: {
        foo: {
          type: "string",
        },
      },
    },
    options: {
      allowNullAsUndefinedProperties: true,
    },
    expectedResult: {
      valid: true,
      parsedValue: {},
    },
  });

  // invalid values below

  validate({
    value: null,
    schema: {
      type: "object",
    },
    expectedResult: {
      valid: false,
      parsedValue: null,
      error: {
        invalidValue: null,
        invalidPropertyNameComponents: [],
        message: `The value 'null' does not match the expected type 'object'`,
      },
    },
  });

  validate({
    value: {
      url: "not a URL",
    },
    schema: {
      type: "object",
      properties: {
        url: {
          type: "URL",
        },
      },
    },
    expectedResult: {
      valid: false,
      parsedValue: "not a URL",
      error: {
        invalidValue: "not a URL",
        invalidPropertyNameComponents: ["url"],
        message:
          `The value '"not a URL"' does not match the expected type 'URL'. ` +
          `The invalid value is property 'url' in {"url":"not a URL"}`,
      },
    },
  });

  validate({
    value: "test",
    schema: {
      type: "object",
      properties: {
        foo: {
          type: "string",
        },
      },
    },
    expectedResult: {
      valid: false,
      error: {
        invalidValue: "test",
        invalidPropertyNameComponents: [],
        message: `The value '"test"' does not match the expected type 'object'`,
      },
    },
  });

  validate({
    value: {
      foo: 123,
    },
    schema: {
      type: "object",
      properties: {
        foo: {
          type: "string",
        },
      },
    },
    expectedResult: {
      valid: false,
      parsedValue: 123,
      error: {
        invalidValue: 123,
        invalidPropertyNameComponents: ["foo"],
        message:
          `The value '123' does not match the expected type 'string'. ` +
          `The invalid value is property 'foo' in {"foo":123}`,
      },
    },
  });

  validate({
    value: {
      foo: {
        bar: 456,
      },
    },
    schema: {
      type: "object",
      properties: {
        foo: {
          type: "object",
          properties: {
            bar: {
              type: "string",
            },
          },
        },
      },
    },
    expectedResult: {
      valid: false,
      parsedValue: 456,
      error: {
        invalidValue: 456,
        invalidPropertyNameComponents: ["foo", "bar"],
        message:
          `The value '456' does not match the expected type 'string'. ` +
          `The invalid value is property 'foo.bar' in {"foo":{"bar":456}}`,
      },
    },
  });

  // null non-required property with strict=true: invalid
  validate({
    value: {
      foo: null,
    },
    schema: {
      type: "object",
      properties: {
        foo: {
          type: "string",
        },
      },
    },
    expectedResult: {
      valid: false,
      parsedValue: null,
      error: {
        invalidValue: null,
        invalidPropertyNameComponents: ["foo"],
        message:
          `The value 'null' does not match the expected type 'string'. ` +
          `The invalid value is property 'foo' in {"foo":null}`,
      },
    },
  });
  validate({
    value: {
      foo: null,
    },
    schema: {
      type: "object",
      strict: true,
      properties: {
        foo: {
          type: "string",
        },
      },
    },
    options: {
      allowNullAsUndefinedProperties: true,
    },
    expectedResult: {
      valid: false,
      parsedValue: null,
      error: {
        invalidValue: null,
        invalidPropertyNameComponents: ["foo"],
        message:
          `The value 'null' does not match the expected type 'string'. ` +
          `The invalid value is property 'foo' in {"foo":null}`,
      },
    },
  });

  // non-null falsey non-required property with strict=false: invalid
  validate({
    value: {
      foo: false,
    },
    schema: {
      type: "object",
      properties: {
        foo: {
          type: "string",
        },
      },
    },
    options: {
      allowExplicitUndefinedProperties: true,
      allowNullAsUndefinedProperties: true,
    },
    expectedResult: {
      valid: false,
      parsedValue: false,
      error: {
        invalidValue: false,
        invalidPropertyNameComponents: ["foo"],
        message:
          `The value 'false' does not match the expected type 'string'. ` +
          `The invalid value is property 'foo' in {"foo":false}`,
      },
    },
  });
  validate({
    value: {
      foo: false,
    },
    schema: {
      type: "object",
      strict: false,
      properties: {
        foo: {
          type: "string",
        },
      },
    },
    expectedResult: {
      valid: false,
      parsedValue: false,
      error: {
        invalidValue: false,
        invalidPropertyNameComponents: ["foo"],
        message:
          `The value 'false' does not match the expected type 'string'. ` +
          `The invalid value is property 'foo' in {"foo":false}`,
      },
    },
  });

  validate({
    value: {
      bogus: "test",
    },
    schema: {
      type: "object",
      properties: {
        foo: {
          type: "string",
        },
      },
    },
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: { bogus: "test" },
        invalidPropertyNameComponents: [],
        message: `Object has unexpected property 'bogus'`,
      },
    },
  });

  validate({
    value: {
      foo: {
        bogus: "test",
      },
    },
    schema: {
      type: "object",
      properties: {
        foo: {
          type: "object",
          properties: {
            bar: {
              type: "string",
            },
          },
        },
      },
    },
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: { bogus: "test" },
        invalidPropertyNameComponents: ["foo"],
        message:
          `Object has unexpected property 'bogus'. The invalid value is ` +
          `property 'foo' in {"foo":{"bogus":"test"}}`,
      },
    },
  });
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
          type: "string",
        },
      },
    },
  };

  validate({
    value: [
      {
        url: "https://www.example.com/bookmark1",
        title: "Foo",
      },
      {
        url: "https://www.example.com/bookmark2",
        title: "Bar",
      },
    ],
    schema,
    expectedResult: {
      valid: true,
      parsedValue: [
        {
          url: new URL("https://www.example.com/bookmark1"),
          title: "Foo",
        },
        {
          url: new URL("https://www.example.com/bookmark2"),
          title: "Bar",
        },
      ],
    },
  });
});

add_task(async function test_missing_arrays_inside_objects() {
  let schema = {
    type: "object",
    properties: {
      allow: {
        type: "array",
        items: {
          type: "boolean",
        },
      },
      block: {
        type: "array",
        items: {
          type: "boolean",
        },
      },
    },
  };

  validate({
    value: {
      allow: [true, true, true],
    },
    schema,
    expectedResult: {
      valid: true,
      parsedValue: {
        allow: [true, true, true],
      },
    },
  });
});

add_task(async function test_required_vs_nonrequired_properties() {
  let schema = {
    type: "object",
    properties: {
      "non-required-property": {
        type: "number",
      },

      "required-property": {
        type: "number",
      },
    },
    required: ["required-property"],
  };

  validate({
    value: {
      "required-property": 5,
      "non-required-property": undefined,
    },
    schema,
    options: {
      allowExplicitUndefinedProperties: true,
    },
    expectedResult: {
      valid: true,
      parsedValue: {
        "required-property": 5,
      },
    },
  });

  validate({
    value: {
      "non-required-property": 5,
    },
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: {
          "non-required-property": 5,
        },
        invalidPropertyNameComponents: [],
        message: `Object is missing required property 'required-property'`,
      },
    },
  });
});

add_task(async function test_number_or_string_values() {
  let schema = {
    type: ["number", "string"],
  };

  validate({
    value: 1,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: 1,
    },
  });
  validate({
    value: "foobar",
    schema,
    expectedResult: {
      valid: true,
      parsedValue: "foobar",
    },
  });
  validate({
    value: "1",
    schema,
    expectedResult: {
      valid: true,
      parsedValue: "1",
    },
  });

  // Invalid values:
  validate({
    value: true,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: true,
        invalidPropertyNameComponents: [],
        message: `The value 'true' does not match any type in ["number","string"]`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match any type in ["number","string"]`,
      },
    },
  });
  validate({
    value: null,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: null,
        invalidPropertyNameComponents: [],
        message: `The value 'null' does not match any type in ["number","string"]`,
      },
    },
  });
});

add_task(async function test_number_or_array_values() {
  let schema = {
    type: ["number", "array"],
    items: {
      type: "number",
    },
  };

  validate({
    value: 1,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: 1,
    },
  });
  validate({
    value: [1, 2, 3],
    schema,
    expectedResult: {
      valid: true,
      parsedValue: [1, 2, 3],
    },
  });

  // Invalid values:
  validate({
    value: true,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: true,
        invalidPropertyNameComponents: [],
        message: `The value 'true' does not match any type in ["number","array"]`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match any type in ["number","array"]`,
      },
    },
  });
  validate({
    value: null,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: null,
        invalidPropertyNameComponents: [],
        message: `The value 'null' does not match any type in ["number","array"]`,
      },
    },
  });
  validate({
    value: ["a", "b"],
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: ["a", "b"],
        invalidPropertyNameComponents: [],
        message: `The value '["a","b"]' does not match any type in ["number","array"]`,
      },
    },
  });
  validate({
    value: [[]],
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: [[]],
        invalidPropertyNameComponents: [],
        message: `The value '[[]]' does not match any type in ["number","array"]`,
      },
    },
  });
  validate({
    value: [0, 1, [2, 3]],
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: [0, 1, [2, 3]],
        invalidPropertyNameComponents: [],
        message:
          `The value '[0,1,[2,3]]' does not match any type in ` +
          `["number","array"]`,
      },
    },
  });
});

add_task(function test_number_or_null_Values() {
  let schema = {
    type: ["number", "null"],
  };

  validate({
    value: 1,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: 1,
    },
  });
  validate({
    value: null,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: null,
    },
  });

  // Invalid values:
  validate({
    value: true,
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: true,
        invalidPropertyNameComponents: [],
        message: `The value 'true' does not match any type in ["number","null"]`,
      },
    },
  });
  validate({
    value: "string",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: "string",
        invalidPropertyNameComponents: [],
        message: `The value '"string"' does not match any type in ["number","null"]`,
      },
    },
  });
  validate({
    value: {},
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: {},
        invalidPropertyNameComponents: [],
        message: `The value '{}' does not match any type in ["number","null"]`,
      },
    },
  });
  validate({
    value: ["a", "b"],
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: ["a", "b"],
        invalidPropertyNameComponents: [],
        message: `The value '["a","b"]' does not match any type in ["number","null"]`,
      },
    },
  });
});

add_task(async function test_patternProperties() {
  let schema = {
    type: "object",
    properties: {
      "S-bool-property": { type: "boolean" },
    },
    patternProperties: {
      "^S-": { type: "string" },
      "^N-": { type: "number" },
      "^B-": { type: "boolean" },
    },
  };

  validate({
    value: {
      "S-string": "test",
      "N-number": 5,
      "B-boolean": true,
      "S-bool-property": false,
    },
    schema,
    expectedResult: {
      valid: true,
      parsedValue: {
        "S-string": "test",
        "N-number": 5,
        "B-boolean": true,
        "S-bool-property": false,
      },
    },
  });

  validate({
    value: {
      "N-string": "test",
    },
    schema,
    expectedResult: {
      valid: false,
      parsedValue: "test",
      error: {
        invalidValue: "test",
        invalidPropertyNameComponents: ["N-string"],
        message:
          `The value '"test"' does not match the expected type 'number'. ` +
          `The invalid value is property 'N-string' in {"N-string":"test"}`,
      },
    },
  });

  validate({
    value: {
      "S-number": 5,
    },
    schema,
    expectedResult: {
      valid: false,
      parsedValue: 5,
      error: {
        invalidValue: 5,
        invalidPropertyNameComponents: ["S-number"],
        message:
          `The value '5' does not match the expected type 'string'. ` +
          `The invalid value is property 'S-number' in {"S-number":5}`,
      },
    },
  });

  schema = {
    type: "object",
    patternProperties: {
      "[": { " type": "string" },
    },
  };

  Assert.throws(
    () => JsonSchemaValidator.validate({}, schema),
    /Invalid property pattern/,
    "Checking that invalid property patterns throw"
  );
});

add_task(async function test_JSON_type() {
  let schema = {
    type: "JSON",
  };

  validate({
    value: {
      a: "b",
    },
    schema,
    expectedResult: {
      valid: true,
      parsedValue: {
        a: "b",
      },
    },
  });
  validate({
    value: '{"a": "b"}',
    schema,
    expectedResult: {
      valid: true,
      parsedValue: {
        a: "b",
      },
    },
  });

  validate({
    value: "{This{is{not{JSON}}}}",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: "{This{is{not{JSON}}}}",
        invalidPropertyNameComponents: [],
        message: `JSON string could not be parsed: "{This{is{not{JSON}}}}"`,
      },
    },
  });
  validate({
    value: "0",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: "0",
        invalidPropertyNameComponents: [],
        message: `JSON was not an object: "0"`,
      },
    },
  });
  validate({
    value: "true",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: "true",
        invalidPropertyNameComponents: [],
        message: `JSON was not an object: "true"`,
      },
    },
  });
});

add_task(async function test_enum() {
  let schema = {
    type: "string",
    enum: ["one", "two"],
  };

  validate({
    value: "one",
    schema,
    expectedResult: {
      valid: true,
      parsedValue: "one",
    },
  });

  validate({
    value: "three",
    schema,
    expectedResult: {
      valid: false,
      parsedValue: undefined,
      error: {
        invalidValue: "three",
        invalidPropertyNameComponents: [],
        message:
          `The value '"three"' is not one of the enumerated values ` +
          `["one","two"]`,
      },
    },
  });
});

add_task(async function test_bool_enum() {
  let schema = {
    type: "boolean",
    enum: ["one", "two"],
  };

  // `enum` is ignored because `type` is boolean.
  validate({
    value: true,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: true,
    },
  });
});

add_task(async function test_boolint_enum() {
  let schema = {
    type: "boolean",
    enum: ["one", "two"],
  };

  // `enum` is ignored because `type` is boolean and the integer value was
  // coerced to boolean.
  validate({
    value: 1,
    schema,
    expectedResult: {
      valid: true,
      parsedValue: true,
    },
  });
});

/**
 * Validates a value against a schema and asserts that the result is as
 * expected.
 *
 * @param {*} value
 *   The value to validate.
 * @param {object} schema
 *   The schema to validate against.
 * @param {object} expectedResult
 *   The expected result.  See JsonSchemaValidator.validate for what this object
 *   should look like.  If the expected result is invalid, then this object
 *   should have an `error` property with all the properties of validation
 *   errors, including `message`, except that `rootValue` and `rootSchema` are
 *   unnecessary because this function will add them for you.
 * @param {object} options
 *   Options to pass to JsonSchemaValidator.validate.
 * @return {object} The return value of JsonSchemaValidator.validate, which is
 *   a result.
 */
function validate({ value, schema, expectedResult, options = undefined }) {
  let result = JsonSchemaValidator.validate(value, schema, options);

  checkObject(
    result,
    expectedResult,
    {
      valid: false,
      parsedValue: true,
    },
    "Checking result property: "
  );

  Assert.equal("error" in result, "error" in expectedResult, "result.error");
  if (result.error && expectedResult.error) {
    expectedResult.error = Object.assign(expectedResult.error, {
      rootValue: value,
      rootSchema: schema,
    });
    checkObject(
      result.error,
      expectedResult.error,
      {
        rootValue: true,
        rootSchema: false,
        invalidPropertyNameComponents: false,
        invalidValue: true,
        message: false,
      },
      "Checking result.error property: "
    );
  }

  return result;
}

/**
 * Asserts that an object is the same as an expected object.
 *
 * @param {*} actual
 *   The actual object.
 * @param {*} expected
 *   The expected object.
 * @param {object} properties
 *   The properties to compare in the two objects.  This value should be an
 *   object.  The keys are the names of properties in the two objects.  The
 *   values are booleans: true means that the property should be compared using
 *   strict equality and false means deep equality.  Deep equality is used if
 *   the property is an object.
 */
function checkObject(actual, expected, properties, message) {
  for (let [name, strict] of Object.entries(properties)) {
    let assertFunc =
      !strict || typeof expected[name] == "object"
        ? "deepEqual"
        : "strictEqual";
    Assert[assertFunc](actual[name], expected[name], message + name);
  }
}
