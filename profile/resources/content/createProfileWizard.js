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
 * Contributor(s):
 *   Ben Goodger (30/09/99)
 */

// The WIZARD of GORE

// NOTICE! Wanting to add a panel to this wizard? Follow these instructions:
/* 1) Add your panel to the testMap Below. If you're adding your panel after
      the last one, remember to set the "next" property on that element to the
      idfier of your panel, and make the "next" property on your panel null.
      This is important because the state of the Next/Back buttons depends on
      correct filling of testMap.
   2) You must create GetFields and SetFields functions in the JS file
      associated with your panel XUL file. This is true even if your panel
      contains no fields, only text, as functions in this file will expect to
      find them. if you do not plan to include data fields, simply include empty
      functions

   PERSISTING & SAVING DATA:
   3) You are responsible for collecting data from your panel. You do this with
      the GetFields function in your panel JS file. However you do this, this
      function must return an array with the following format:
         [[identifier, value],[identifier,value],...]
      where identifier is some string identifier of the element (usually just
      the ID attribute), and the value is the value being saved.
      Make sure you don't choose an identifier that conflicts with one used
      for any other panel. It is recommended you choose something fairly unique.
   4) You are responsible for setting the contents of your panel. You do this
      with the SetFields function, which is called for each element you've saved
      when the panel loads. This function can set attributes, use DOM
      manipulation, whatever you deem necessary to populate the panel.

    You can find examples of usage of GetFields and SetFields in
    newProfile1_2.js

 */

var wizardMap = ["newProfile1_1.xul"];
var content;
var wizardHash = new Array;
var firstTime = true;

var profile = Components.classes["component://netscape/profile/manager"].createInstance();
profile = profile.QueryInterface(Components.interfaces.nsIProfile);

// Navigation Set for pages contained in wizard
var testMap = {
  newProfile1_1: { previous: null, next: "newProfile1_2" },
  newProfile1_2: { previous: "newProfile1_1", next: null},
}
var pagePrefix = "chrome://profile/content/";
var pagePostfix=".xul";
var currentPageTag;


var profName = "";
var profDir = "";

function wizardPageLoaded(tag)
{
  if (firstTime)
    Startup();
  currentPageTag = tag;
  SetButtons();
  populatePage();
}

var backButton = null;
var nextButton = null;
var finishButton = null;

function loadPage(thePage)
{
  if (!firstTime)
    saveData();
  displayPage(thePage);
  firstTime = false;
  return(true);
}

function SetButtons()
{
  if (!currentPageTag)
    return;
  if (!backButton)
    backButton = document.getElementById("back");
  if (!nextButton)
    nextButton = document.getElementById("next");

  nextTag = testMap[currentPageTag].next;
  if (nextTag) {
    var nextLabel = bundle.GetStringFromName("nextButtonLabel");
    nextButton.setAttribute("value",nextLabel);
    nextButton.setAttribute("onclick","onNext()");
  }
  else {
    var finishLabel = bundle.GetStringFromName("finishButtonLabel");
    nextButton.setAttribute("value",finishLabel);
    nextButton.setAttribute("onclick","onFinish(opener)");
  }

  prevTag = testMap[currentPageTag].previous;
  if (prevTag)
    backButton.setAttribute("disabled", "false");
  else
    backButton.setAttribute("disabled", "true");
}

function onNext()
{
  //dump("in onnext\n");
  if (nextButton.getAttribute("disabled") == "true") {
     return;
  }
  saveData();
  var nextPageTag = testMap[currentPageTag].next;
  var url = getUrlFromTag(nextPageTag);
  displayPage(url);
}

function onBack()
{
  //dump("in onback\n");
  if (backButton.getAttribute("disabled") == "true")
    return;

  saveData();
  previousPageTag = testMap[currentPageTag].previous;
  var url = getUrlFromTag(previousPageTag);
  displayPage(url);
}

function displayPage(content)
{
  if (content != "") {
    var contentFrame = document.getElementById("content");
    if (contentFrame)
      contentFrame.setAttribute("src", content);
  }
}


function populatePage()
{
  var contentWindow = window.frames["content"];
  var doc = contentWindow.document;
  for (var i in wizardHash)
    contentWindow.SetFields(i,wizardHash[i]);
}

function saveData()
{
  var contentWindow = window.frames["content"];
  var data = contentWindow.GetFields();
  if (data != undefined)  {
    for (var i = 0; i < data.length; i++)
      wizardHash[data[i][0]] = data[i][1];
  }
}

function onCancel()
{
  // we came from the profile manager window...
  if (top.window.opener) {
    //dump("just close\n");
    window.close();
  }
  else {
    //dump("exit\n");
    try {
      profile.forgetCurrentProfile();
    }
    catch (ex) {
      dump("failed to forget current profile.\n");
    }
    ExitApp();
  }
}

// utility functions
function getUrlFromTag(title)
{
  return pagePrefix + title + pagePostfix;
}

function Startup()
{
  //dump("Doing Startup...\n");
}

function onFinish(opener)
{
  // lets check if we're at final stage using null
  if (testMap[currentPageTag].next)
    return;

  try {
    saveData();
    proceed = processCreateProfileData();
    if (proceed) {
      if (opener) {
        opener.CreateProfile();
        window.close();
      }
      else {
        ExitApp();
      }
    }
  }
  catch (ex) {
    alert("Failed to create a profile.");
  }
}

function processCreateProfileData()
{
  //Process Create Profile Data
  var i;

  for (i in wizardHash) {
    if (i == "ProfileName") {
      profName = wizardHash[i];
    }
    if (i == "ProfileDir") {
      profDir = wizardHash[i];
    }
  }
  try {
    //dump("name,dir = " + profName + "," + profDir + "\n");
    if (profName == "") {
      alert("You need to enter a profile name.");
      return false;
    }
    if (profile.profileExists(profName)) {
      alert("That profile name already exists.");
      return false;
    }
    profile.createNewProfile(profName, profDir);
    profile.startApprunner(profName);
  }
  catch (ex) {
    alert("Failed to create a profile.");
    return false;
  }
  return true;
}

function ExitApp()
{
  // Need to call this to stop the event loop
  var appShell = Components.classes['component://netscape/appshell/appShellService'].getService();
  appShell = appShell.QueryInterface( Components.interfaces.nsIAppShellService);
  appShell.Quit();
}

var bundle = srGetStrBundle("chrome://profile/locale/createProfileWizard.properties");
