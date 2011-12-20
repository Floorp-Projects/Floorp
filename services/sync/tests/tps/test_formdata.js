/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * The list of phases mapped to their corresponding profiles.  The object
 * here must be in strict JSON format, as it will get parsed by the Python
 * testrunner (no single quotes, extra comma's, etc).
 */
EnableEngines(["forms"]);

var phases = { "phase1": "profile1",
               "phase2": "profile2",
               "phase3": "profile1",
               "phase4": "profile2" };

/*
 * Form data asset lists: these define form values that are used in the tests.
 */

var formdata1 = [
  { fieldname: "testing",
    value: "success",
    date: -1
  },
  { fieldname: "testing",
    value: "failure",
    date: -2
  },
  { fieldname: "username",
    value: "joe"
  }
];

var formdata2 = [
  { fieldname: "testing",
    value: "success",
    date: -1
  },
  { fieldname: "username",
    value: "joe"
  }
];

var formdata_delete = [
  { fieldname: "testing",
    value: "failure"
  }
];

/*
 * Test phases
 */

Phase('phase1', [
  [Formdata.add, formdata1],
  [Formdata.verify, formdata1],
  [Sync]
]);

Phase('phase2', [
  [Sync],
  [Formdata.verify, formdata1],
]);

/*
 * Note: Weave does not support syncing deleted form data, so those
 * tests are disabled below.  See bug 568363.
 */

Phase('phase3', [
  [Sync],
  [Formdata.delete, formdata_delete],
//[Formdata.verifyNot, formdata_delete],
  [Formdata.verify, formdata2],
  [Sync],
]);

Phase('phase4', [
  [Sync],
  [Formdata.verify, formdata2],
//[Formdata.verifyNot, formdata_delete]
]);


