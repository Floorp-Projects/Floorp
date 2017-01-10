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

// This is currently pointless - it *looks* like it is trying to check that
// one of the entries in formdata1 has been removed, but (a) the delete code
// isn't active (see comments below), and (b) the way the verification works
// means it would never do the right thing - it only checks all the entries
// here exist, but not that they are the only entries in the DB.
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

var formdata_new = [
  { fieldname: "new-field",
    value: "new-value"
  }
]
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
// [Formdata.verifyNot, formdata_delete],
  [Formdata.verify, formdata2],
  // add new data after the first Sync, ensuring the tracker works.
  [Formdata.add, formdata_new],
  [Sync],
]);

Phase('phase4', [
  [Sync],
  [Formdata.verify, formdata2],
  [Formdata.verify, formdata_new],
//[Formdata.verifyNot, formdata_delete]
]);


