var wizardMap = ["newProfile1_1.xul"];
var content;
var wizardHash = new Array;
var firstTime = true;

var testMap = {
    newProfile1_1: { previous: null, next: "newProfile1_2" },
    newProfile1_2: { previous: "newProfile1_1", next: null},
}

var pagePostfix=".xul";
var currentPageTag;


var profName = "";
var profDir = "";

function wizardPageLoaded(tag) {
	//dump("**********wizardPageLoaded\n");

	if (firstTime) {
		Startup();
	}

	currentPageTag = tag;
	//dump("currentPageTag: "+currentPageTag+"\n");
	SetButtons();
	populatePage();
}

var backButton = null;
var nextButton = null;
var finishButton = null;

function loadPage(thePage)
{
	if (!firstTime) {
		saveData();
	}

	//dump("**********loadPage\n");
	//dump("thePage: "+thePage+"\n");
	displayPage(thePage);

	firstTime = false;
	return(true);
}

function SetButtons()
{
	if (!currentPageTag) return;

	if (!backButton) {
		backButton = document.getElementById("back"); 
	}
	if (!nextButton) {
		nextButton = document.getElementById("next"); 
	}
	if (!finishButton) {
		finishButton = document.getElementById("finish"); 
	}

	//dump("currentPageTag == " + currentPageTag + "\n");
	nextTag = testMap[currentPageTag].next;
	//dump("nextTag == " + nextTag + "\n");
	if (nextTag) {
		//dump("next on, finish off\n");
		nextButton.setAttribute("disabled", "false");
		finishButton.setAttribute("disabled", "true");
	}
	else {
		//dump("next off, finish on\n");
		nextButton.setAttribute("disabled", "true");
		finishButton.setAttribute("disabled", "false");
	}

	prevTag = testMap[currentPageTag].previous;
	//dump("prevTag == " + prevTag + "\n");
	if (prevTag) {
		//dump("back on\n");
		backButton.setAttribute("disabled", "false");
	}
	else {
		//dump("back off\n");
		backButton.setAttribute("disabled", "true"); 
	}
}

function onNext()
{
	if (nextButton.getAttribute("disabled") == "true") {
		 return;
	}

	//dump("***********onNext\n");

	if (currentPageTag != "newProfile1_2") {
		saveData();
		var nextPageTag = testMap[currentPageTag].next;
		var url = getUrlFromTag(nextPageTag);
		displayPage(url);
	}
}

function onBack()
{
	if (backButton.getAttribute("disabled") == "true")  {
		return;
	}

	if (currentPageTag != "newProfile1_2") {
		saveData();
		previousPageTag = testMap[currentPageTag].previous;
		var url = grtUrlFromTag(previousPageTag);
		displayPage(url);
	}
}

function displayPage(content)
{
	//dump(content + "\n");
	//dump("********INSIDE DISPLAYPAGE\n\n");

	if (content != "")
	{
		var	contentFrame = document.getElementById("content");
		if (contentFrame)
		{
			contentFrame.setAttribute("src", content);
		}
               //hack for onLoadHandler problem bug #15458
               var tmpUrl = content.split(".");
               var tag = tmpUrl[0];
               wizardPageLoaded(tag);	}
}


function populatePage() 
{
    //dump("************initializePage\n");
	var contentWindow = window.frames["content"];
	var doc = contentWindow.document;

    for (var i in wizardHash) {
        var formElement=doc.getElementById(i);
		//dump("formElement: "+formElement+"\n");

        if (formElement) {
            formElement.value = wizardHash[i];
			//dump("wizardHash["+"i]: "+wizardHash[i]+"\n");
        }
    }
}

function saveData()
{
	//dump("************ SAVE DATA\n");

	var contentWindow = window.frames["content"];
	var doc = contentWindow.document;

	var inputs = doc.getElementsByTagName("FORM")[0].elements;

	//dump("There are " + inputs.length + " input tags\n");
	for (var i=0 ; i<inputs.length; i++) {
        //dump("Saving: ");
	//dump("ID=" + inputs[i].id + " Value=" + inputs[i].value + "..");
		wizardHash[inputs[i].id] = inputs[i].value;
	}
    //dump("done.\n");
}

function onCancel()
{
	//dump("************** ON CANCEL\n");
	saveData();
	var i;
    for (i in wizardHash) {
		//dump("element: "+i+"\n");
        //dump("value: "+wizardHash[i]+"\n");
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
	if (finishButton.getAttribute("disabled") == "true") {
                 return;
        }
  
	//dump("******FINISH ROUTINE\n");

	try {
		saveData();
		processCreateProfileData();

		if (opener) {
			opener.CreateProfile();
		}
	}
	catch (ex) {
		alert("Failed to create a profile.");
	}
}

function processCreateProfileData()
{
	//Process Create Profile Data

	//dump("******processCreateProfileData ROUTINE\n");

	var i;
	var data = "";

    for (i in wizardHash) {
		//dump("element: "+i+"\n");
        //dump("value: "+wizardHash[i]+"\n");
		
		if (i == "ProfileName") {
			profName = wizardHash[i];
			data = data+i+"="+profName+"%";
		}

		if (i == "ProfileDir") {
			profDir = wizardHash[i];
			data = data+i+"="+profDir+"%";
		}
    }

	//dump("data: "+data+"\n");
	var profile = Components.classes["component://netscape/profile/manager"].createInstance();
	profile = profile.QueryInterface(Components.interfaces.nsIProfile);
	//dump("profile = " + profile + "\n");  
	//dump("********DATA: "+data+"\n\n");
	try {
		profile.createNewProfile(data);
		profile.startCommunicator(profName);
	}
	catch (ex) {
		alert("Failed to create a profile.");
		return;
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

function cancelApp()
{
	dump("fix this.\n");
}
