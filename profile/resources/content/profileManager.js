/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 */

var selected    = null;
var currProfile = "";
var profile = Components.classes["component://netscape/profile/manager"].createInstance();
profile = profile.QueryInterface(Components.interfaces.nsIProfile);
//dump("profile = " + profile + "\n");

function openCreateProfile()
{
  // Need to call CreateNewProfile xuls
  window.openDialog('chrome://profile/content/createProfileWizard.xul', 'CPW', 'chrome');
}

function CreateProfile()
{
  this.location.href = "chrome://profile/content/profileManager.xul";
}

function RenameProfile(w)
{
  renameButton = document.getElementById("rename");
  if (renameButton.getAttribute("disabled") == "true") {
    return;
  }
  if (!selected) {
    alert("Select a profile to rename.");
    return;
  }

  var newName = w.document.getElementById("NewName").value;
  var oldName = selected.getAttribute("rowName");
  var migrate = selected.getAttribute("rowMigrate");

  if (migrate == "true") {
    alert("Migrate this profile before renaming it.");
    return;
  }

  //dump("RenameProfile : " + oldName + " to " + newName + "\n");
  try {
    profile.renameProfile(oldName, newName);
  }
  catch (ex) {
    alert("Sorry, failed to rename profile.");
  }
  //this.location.replace(this.location);
  this.location.href = "chrome://profile/content/profileManager.xul";
}

function DeleteProfile(deleteFiles)
{
  deleteButton = document.getElementById("delete");
  if (deleteButton.getAttribute("disabled") == "true") {
    return;
  }
  if (!selected) {
    alert("Select a profile to delete.");
    return;
  }

  var migrate = selected.getAttribute("rowMigrate");

  var name = selected.getAttribute("rowName");
  //dump("Delete '" + name + "'\n");
  try {
    profile.deleteProfile(name, deleteFiles);
  }
  catch (ex) {
    alert("Sorry, failed to delete profile.");
  }
  //this.location.replace(this.location);
  //this.location.href = this.location;
  this.location.href = "chrome://profile/content/profileManager.xul";
}

function onStart()
{
  //dump("************Inside Start Communicator prof\n");
  startButton = document.getElementById("start");
  if (startButton.getAttribute("disabled") == "true") {
    return;
  }
  if (!selected) {
    alert("Select a profile to start.");
    return;
  }

  var migrate = selected.getAttribute("rowMigrate");
  var name = selected.getAttribute("rowName");

  if (migrate == "true") {
    // pass true for show progress as modal window
    profile.migrateProfile(name, true);
  }

  //dump("************name: "+name+"\n");
  try {
    profile.startApprunner(name);
    ExitApp();
  }
  catch (ex) {
    alert("Failed to start communicator with the " + name + " profile.");
  }
}

function onExit()
{
  try {
    profile.forgetCurrentProfile();
  }
  catch (ex) {
    //dump("Failed to forget current profile.\n");
  }
  ExitApp();
}

function ExitApp()
{
  // Need to call this to stop the event loop
  var appShell = Components.classes['component://netscape/appshell/appShellService'].getService();
  appShell = appShell.QueryInterface( Components.interfaces.nsIAppShellService);
  appShell.Quit();
}


function setButtonsDisabledState(state)
{
  startButton = document.getElementById("start");
  deleteButton = document.getElementById("delete");
  renameButton = document.getElementById("rename");

  deleteButton.setAttribute("disabled", state);
  renameButton.setAttribute("disabled", state);
  startButton.setAttribute("disabled", state);
}

function showSelection(node)
{
  //dump("************** In showSelection routine!!!!!!!!! \n");
  // (see tree's onclick definition)
  // Tree events originate in the smallest clickable object which is the cell.  The object
  // originating the event is available as event.target.  We want the cell's row, so we go
  // one further and get event.target.parentNode.
  selected = node;
  var num = node.getAttribute("rowNum");
  //dump("num: "+num+"\n");

  var name = node.getAttribute("rowName");
  //dump("name: "+name+"\n");

  //dump("Selected " + num + " : " + name + "\n");

  // turn on all the buttons that should be on
  // when something is selected.
  setButtonsDisabledState("false");
}

function addTreeItem(num, name, migrate)
{
  //dump("Adding element " + num + " : " + name + "\n");
  var body = document.getElementById("theTreeBody");

  var newitem = document.createElement('treeitem');
  var newrow = document.createElement('treerow');
  newrow.setAttribute("rowNum", num);
  newrow.setAttribute("rowName", name);
  newrow.setAttribute("rowMigrate", migrate);

  var elem = document.createElement('treecell');
  
  // Hack in a differentation for migration
  if (migrate) {
    elem.setAttribute("value", "Migrate");
  }
  
  newrow.appendChild(elem);

  var elem = document.createElement('treecell');
  elem.setAttribute("value", name);
  newrow.appendChild(elem);

  newitem.appendChild(newrow);
  body.appendChild(newitem);
}

function loadElements()
{
  //dump("****************hacked onload handler adds elements to tree widget\n");
  var profileList = "";

  profileList = profile.getProfileList();

  //dump("Got profile list of '" + profileList + "'\n");
  profileList = profileList.split(",");
  try {
    currProfile = profile.currentProfile;
  }
  catch (ex) {
    if (profileList != "") {
      currProfile = profileList;
    }
  }

  for (var i=0; i < profileList.length; i++) {
    var pvals = profileList[i].split(" - ");
    addTreeItem(i, pvals[0], (pvals[1] == "migrate"));
  }
}

function openRename()
{
  renameButton = document.getElementById("rename");
  if (renameButton.getAttribute("disabled") == "true") {
    return;
  }
  if (!selected) {
    alert("Select a profile to rename.");
  }
  else {
    var migrate = selected.getAttribute("rowMigrate");
    if (migrate == "true") {
      alert("Migrate the profile before renaming it.");
    }
    else
      window.openDialog('chrome://profile/content/renameProfile.xul', 'Renamer', 'chrome');
  }
}


function ConfirmDelete()
{
  deleteButton = document.getElementById("delete");
  if (deleteButton.getAttribute("disabled") == "true") {
    return;
  }
  if (!selected) {
    alert("Select a profile to delete.");
    return;
  }

  var migrate = selected.getAttribute("rowMigrate");
  var name = selected.getAttribute("rowName");

  if (migrate == "true") {
    alert("Migrate this profile before deleting it.");
    return;
  }

  var win = window.openDialog('chrome://profile/content/deleteProfile.xul', 'Deleter', 'chrome');
  return win;
}


function ConfirmMigrateAll()
{
  var win = window.openDialog('chrome://profile/content/profileManagerMigrateAll.xul', 'MigrateAll', 'chrome');
  return win;
}
