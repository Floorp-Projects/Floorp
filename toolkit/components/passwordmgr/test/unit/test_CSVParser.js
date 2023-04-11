/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { CSV } = ChromeUtils.importESModule(
  "resource://gre/modules/CSV.sys.mjs"
);

const TEST_CASES = [
  {
    description:
      "string with fields with no special characters gets parsed correctly",
    csvString: `
url,username,password
https://example.com/,testusername,testpassword
`,
    expectedHeaderLine: ["url", "username", "password"],
    expectedParsedLines: [
      {
        url: "https://example.com/",
        username: "testusername",
        password: "testpassword",
      },
    ],
    delimiter: ",",
    throwsError: false,
  },
  {
    description:
      "string with fields enclosed in quotes with no special characters gets parsed correctly",
    csvString: `
"url","username","password"
"https://example.com/","testusername","testpassword"
`,
    expectedHeaderLine: ["url", "username", "password"],
    expectedParsedLines: [
      {
        url: "https://example.com/",
        username: "testusername",
        password: "testpassword",
      },
    ],
    delimiter: ",",
    throwsError: false,
  },
  {
    description: "empty fields gets parsed correctly",
    csvString: `
"url","username","password"
"https://example.com/","",""
`,
    expectedHeaderLine: ["url", "username", "password"],
    expectedParsedLines: [
      {
        url: "https://example.com/",
        username: "",
        password: "",
      },
    ],
    delimiter: ",",
    throwsError: false,
  },
  {
    description: "string with commas in fields gets parsed correctly",
    csvString: `
url,username,password
https://example.com/,"test,usern,ame","tes,,tpassword"
`,
    expectedHeaderLine: ["url", "username", "password"],
    expectedParsedLines: [
      {
        url: "https://example.com/",
        username: "test,usern,ame",
        password: "tes,,tpassword",
      },
    ],
    delimiter: ",",
    throwsError: false,
  },
  {
    description: "string with line break in fields gets parsed correctly",
    csvString: `
url,username,password
https://example.com/,"test\nusername","\ntestpass\n\nword"
`,
    expectedHeaderLine: ["url", "username", "password"],
    expectedParsedLines: [
      {
        url: "https://example.com/",
        username: "test\nusername",
        password: "\ntestpass\n\nword",
      },
    ],
    delimiter: ",",
    throwsError: false,
  },
  {
    description: "string with quotation mark in fields gets parsed correctly",
    csvString: `
url,username,password
https://example.com/,"testusern""ame","test""""pass""word"
`,
    expectedHeaderLine: ["url", "username", "password"],
    expectedParsedLines: [
      {
        url: "https://example.com/",
        username: 'testusern"ame',
        password: 'test""pass"word',
      },
    ],
    delimiter: ",",
    throwsError: false,
  },
  {
    description: "tsv string with tab as delimiter gets parsed correctly",
    csvString: `
url\tusername\tpassword
https://example.com/\ttestusername\ttestpassword
`,
    expectedHeaderLine: ["url", "username", "password"],
    expectedParsedLines: [
      {
        url: "https://example.com/",
        username: "testusername",
        password: "testpassword",
      },
    ],
    delimiter: "\t",
    throwsError: false,
  },
  {
    description: "string with CR LF as line breaks gets parsed correctly",
    csvString:
      "url,username,password\r\nhttps://example.com/,testusername,testpassword\r\n",
    expectedHeaderLine: ["url", "username", "password"],
    expectedParsedLines: [
      {
        url: "https://example.com/",
        username: "testusername",
        password: "testpassword",
      },
    ],
    delimiter: ",",
    throwsError: false,
  },
  {
    description:
      "string without line break at the end of the file gets parsed correctly",
    csvString: `
url,username,password
https://example.com/,testusername,testpassword`,
    expectedHeaderLine: ["url", "username", "password"],
    expectedParsedLines: [
      {
        url: "https://example.com/",
        username: "testusername",
        password: "testpassword",
      },
    ],
    delimiter: ",",
    throwsError: false,
  },
  {
    description:
      "multiple line breaks at the beginning, in the middle or at the end of a string are trimmed and not parsed as empty rows",
    csvString: `
\r\r
url,username,password
\n\n
https://example.com/,testusername,testpassword
\n\r
`,
    expectedHeaderLine: ["url", "username", "password"],
    expectedParsedLines: [
      {
        url: "https://example.com/",
        username: "testusername",
        password: "testpassword",
      },
    ],
    delimiter: ",",
    throwsError: false,
  },
  {
    description:
      "throws error when after a field, that is enclosed in quotes, follow any invalid characters (doesn't follow csv standard - RFC 4180)",
    csvString: `
  url,username,password
  https://example.com/,"testusername"outside,testpassword
  `,
    delimiter: ",",
    throwsError: true,
  },
  {
    description:
      "throws error when the closing quotation mark for a field is missing (doesn't follow csv standard - RFC 4180)",
    csvString: `
url,"username,password
https://example.com/,testusername,testpassword
`,
    delimiter: ",",
    throwsError: true,
  },
  {
    description:
      "parsing empty csv file results in empty header line and empty parsedLines",
    csvString: "",
    expectedHeaderLine: [],
    expectedParsedLines: [],
    delimiter: ",",
    throwsError: false,
  },
  {
    description:
      "parsing csv file with only header line results in empty parsedLines",
    csvString: "url,username,password\n",
    expectedHeaderLine: ["url", "username", "password"],
    expectedParsedLines: [],
    delimiter: ",",
    throwsError: false,
  },
];

async function parseCSVStringAndValidateResult(test) {
  info(`Test case: ${test.description}`);
  if (test.throwsError) {
    Assert.throws(
      () => CSV.parse(test.csvString, test.delimiter),
      /Stopped parsing because of wrong csv format/
    );
  } else {
    let [resultHeaderLine, resultParsedLines] = CSV.parse(
      test.csvString,
      test.delimiter
    );
    Assert.deepEqual(
      resultHeaderLine,
      test.expectedHeaderLine,
      "Header line check"
    );
    Assert.deepEqual(
      resultParsedLines,
      test.expectedParsedLines,
      "Parsed lines check"
    );
  }
}

add_task(function test_csv_parsing_results() {
  TEST_CASES.forEach(testCase => {
    parseCSVStringAndValidateResult(testCase);
  });
});
