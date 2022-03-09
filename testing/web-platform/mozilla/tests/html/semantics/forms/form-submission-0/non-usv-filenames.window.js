// META: script=/html/semantics/forms/form-submission-0/enctypes-helper.js

// This test is built on the same infrastructure as the WPT tests
// urlencoded2.window.js, multipart-formdata.window.js and text-plain.window.js,
// except modified because this file only tests the serialization of filenames.
// See the enctypes-helper.js file in the regular WPT test suite for more info.

// The `urlencoded`, `multipart` and `textPlain` functions take a `file`
// property rather than `name` and `value` properties, and the value of
// `expected` is the serialization of the filename in the given encoding.

function formSubmissionTemplate2(enctype, expectedBuilder) {
  const formTestFn = formSubmissionTemplate(enctype, expectedBuilder);
  return ({ file, formEncoding, expected, description }) =>
    formTestFn({ name: "a", value: file, formEncoding, expected, description });
}

const urlencoded = formSubmissionTemplate2(
  "application/x-www-form-urlencoded",
  filename => `a=${filename}`
);
const multipart = formSubmissionTemplate2(
  "multipart/form-data",
  (filename, serialized) => {
    const boundary = serialized.split("\r\n")[0];
    return [
      boundary,
      `Content-Disposition: form-data; name="a"; filename="${filename}"`,
      "Content-Type: text/plain",
      "",
      "", // File contents
      `${boundary}--`,
      "",
    ].join("\r\n");
  }
);
const textPlain = formSubmissionTemplate2(
  "text/plain",
  filename => `a=${filename}\r\n`
);

// -----------------------------------------------------------------------------

(async () => {
  // This creates an empty filesystem file with an arbitrary name and returns it
  // as a File object with name "a\uD800b".
  const file = SpecialPowers.unwrap(
    await SpecialPowers.createFiles(
      [{ data: "", options: { name: "a\uD800b", type: "text/plain" } }],
      files => files[0]
    )
  );

  urlencoded({
    file,
    formEncoding: "UTF-8",
    expected: "a%EF%BF%BDb",
    description: "lone surrogate in filename, UTF-8",
  });

  urlencoded({
    file,
    formEncoding: "windows-1252",
    expected: "a%26%2365533%3Bb",
    description: "lone surrogate in filename, windows-1252",
  });

  multipart({
    file,
    formEncoding: "UTF-8",
    expected: "a\xEF\xBF\xBDb",
    description: "lone surrogate in filename, UTF-8",
  });

  multipart({
    file,
    formEncoding: "windows-1252",
    expected: "a&#65533;b",
    description: "lone surrogate in filename, windows-1252",
  });

  textPlain({
    file,
    formEncoding: "UTF-8",
    expected: "a\xEF\xBF\xBDb",
    description: "lone surrogate in filename, UTF-8",
  });

  textPlain({
    file,
    formEncoding: "windows-1252",
    expected: "a&#65533;b",
    description: "lone surrogate in filename, windows-1252",
  });
})();
