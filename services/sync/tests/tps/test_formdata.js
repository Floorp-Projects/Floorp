/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

EnableEngines(["forms"]);

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in JSON format as it will get parsed by the Python
 * testrunner. It is parsed by the YAML package, so it relatively flexible.
 */
var phases = {
  phase1: "profile1",
  phase2: "profile2",
  phase3: "profile1",
  phase4: "profile2",
};

/*
 * Form data asset lists: these define form values that are used in the tests.
 */

var formdata1 = [
  { fieldname: "testing", value: "success", date: -1 },
  { fieldname: "testing", value: "failure", date: -2 },
  { fieldname: "username", value: "joe" },
];

var formdata2 = [
  { fieldname: "testing", value: "success", date: -1 },
  { fieldname: "username", value: "joe" },
];

var formdata_delete = [{ fieldname: "testing", value: "failure" }];

var formdata_new = [{ fieldname: "new-field", value: "new-value" }];
/*
 * Test phases
 */

Phase("phase1", [
  [Formdata.add, formdata1],
  [Formdata.verify, formdata1],
  [Sync],
]);

Phase("phase2", [[Sync], [Formdata.verify, formdata1]]);

Phase("phase3", [
  [Sync],
  [Formdata.delete, formdata_delete],
  [Formdata.verifyNot, formdata_delete],
  [Formdata.verify, formdata2],
  // add new data after the first Sync, ensuring the tracker works.
  [Formdata.add, formdata_new],
  [Sync],
]);

Phase("phase4", [
  [Sync],
  [Formdata.verify, formdata2],
  [Formdata.verify, formdata_new],
  [Formdata.verifyNot, formdata_delete],
]);
