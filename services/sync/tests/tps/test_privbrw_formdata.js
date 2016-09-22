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
 * Form data
 */

// the form data to add to the browser
var formdata1 = [
   { fieldname: "name",
     value: "xyz",
     date: -1
   },
   { fieldname: "email",
     value: "abc@gmail.com",
     date: -2
   },
   { fieldname: "username",
     value: "joe"
   }
];

// the form data to add in private browsing mode
var formdata2 = [
   { fieldname: "password",
     value: "secret",
     date: -1
   },
   { fieldname: "city",
     value: "mtview"
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
  [Formdata.verify, formdata1]
]);

Phase('phase3', [
  [Sync],
  [Windows.add, { private: true }],
  [Formdata.add, formdata2],
  [Formdata.verify, formdata2],
  [Sync],
]);

Phase('phase4', [
  [Sync],
  [Formdata.verify, formdata1],
  [Formdata.verifyNot, formdata2]
]);
