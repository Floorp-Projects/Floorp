/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var updateFunction;
var handleCancel;
var handleOK;
var sTestrunsWrapper; // an array of things that are kind of like testruns,
                      // but w/o important fields.
                      //returned by "test_runs_by_branch_product_name="

var sTestrun;   // actual testrun
var sTestgroup;
function handleLoad() {

  if (window.arguments.length > 0) {
    updateFunction = window.arguments[0];   // parent window passes in a function to update itself with data
    handleCancel = window.arguments[1];     // parent window passes in a function to restore state if dialog canceled
    handleOK = window.arguments[2];         // you get the idea
  }
  litmus.getTestruns(populateTestRuns);
}

function handleRunSelect() {
  var id = $("qa-st-testrun").selectedItem.getAttribute("value");
  if (id == "") return;
    // oddly, this check doesn't seem necessary in the other handlers...
  litmus.getTestrun(id, populateTestGroups);
}

function handleTestgroupSelect() {
  var id = document.getElementById("qa-st-testgroup").selectedItem.value;
  litmus.getTestgroup(id, populateSubgroups);
}

function handleSubgroupSelect() {
  var id = document.getElementById("qa-st-subgroup").selectedItem.value;
  updateCaller(sTestrun.name, sTestgroup.name, id, 0);
}

function populateTestRuns(testrunsWrapper) {
  var menu = document.getElementById("qa-st-testrun");
  testrunsWrapper = qaTools.arrayify(testrunsWrapper);
  sTestrunsWrapper = testrunsWrapper;

  while (menu.firstChild) {  // clear menu
    menu.removeChild(menu.firstChild);
  }

  for (var i = 0; i < testrunsWrapper.length; i++) {
    if (testrunsWrapper[i].enabled == 0) continue;
    var item = menu.appendItem(testrunsWrapper[i].name,
                              testrunsWrapper[i].test_run_id);
  }
  menu.selectedIndex = 0;
  handleRunSelect();
}

function populateTestGroups(testrun) {
  sTestrun = testrun;
  var menu = document.getElementById("qa-st-testgroup");
  while (menu.firstChild) {  // clear menu
    menu.removeChild(menu.firstChild);
  }

  var testgroups = qaTools.arrayify(testrun.testgroups);
  for (var i = 0; i < testgroups.length; i++) {
    if (testgroups[i].enabled == 0) continue;
    menu.appendItem(testgroups[i].name, testgroups[i].testgroup_id);
  }
  menu.selectedIndex = 0;
}

function populateSubgroups(testgroup) {
  sTestgroup = testgroup;
  var menu = document.getElementById("qa-st-subgroup");
  while (menu.firstChild) {  // clear menu
    menu.removeChild(menu.firstChild);
  }
  var subgroups = qaTools.arrayify(testgroup.subgroups);

  for (var i = 0; i < subgroups.length; i++) {
    if (subgroups[i].enabled == 0) continue;
    menu.appendItem(subgroups[i].name, subgroups[i].subgroup_id);
  }
  menu.selectedIndex = 0;
}

function OK() {
  handleOK();
  return true;
}

function updateCaller(testrunSummary, testgroupSummary, subgroupID, index) {
  litmus.writeStateToPref(testrunSummary, testgroupSummary, subgroupID, index);
  updateFunction();
}

function Cancel() {
  handleCancel();
  return true;
}
