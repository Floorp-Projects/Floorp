

// The WIZARD of GORE

var wizardMap = ["newProfile1_1.xul"];
var content;
var wizardHash = new Array;
var firstTime = true;

var profile = Components.classes["component://netscape/profile/manager"].createInstance();
profile = profile.QueryInterface(Components.interfaces.nsIProfile); 

var testMap = {
    newProfile1_1: { previous: null, next: "newProfile1_2" },
    newProfile1_2: { previous: "newProfile1_1", next: null},
}

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
    nextButton.setAttribute("onclick","Finish(opener)");
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
	if (content != "")
	{
		var	contentFrame = document.getElementById("content");
		if (contentFrame)
			contentFrame.setAttribute("src", content);
	}
}


function populatePage() 
{
	var contentWindow = window.frames["content"];
	var doc = contentWindow.document;
  for (var i in wizardHash) 
  {
    var element = doc.getElementById(i);
    if (element)
      contentWindow.SetFields(element,wizardHash[i]);
  }
}

function saveData()
{
	var contentWindow = window.frames["content"];
  var data = contentWindow.GetFields();
  if(data != undefined)  {
    for(var i = 0; i < data.length; i++) 
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
    return title + pagePostfix;
}

function Startup()
{
	//dump("Doing Startup...\n");
}

function Finish(opener)
{
  // lets check if we're at final stage using null
  if(testMap[currentPageTag].next)
    return null;
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
		return;
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
		profile.createNewProfile(profName, profDir);
		profile.startCommunicator(profName);
		return true;
	}
	catch (ex) {
		alert("Failed to create a profile.");
		return false;
	}	
	return false;
}

function ExitApp()
{
  // Need to call this to stop the event loop
  var appShell = Components.classes['component://netscape/appshell/appShellService'].getService();
  appShell = appShell.QueryInterface( Components.interfaces.nsIAppShellService);
  appShell.Quit();
}

var bundle = srGetStrBundle("chrome://profile/locale/createProfileWizard.properties");
