var wizardMap = new Array("test-content1_1.xul");
var content;
var wizardHash = new Array;
var firstTime = true;

var testMap = {
    content1_1: { next: "content1_2" },
    content1_2: { previous: "content1_1" },
}

var pagePrefix="test-";
var pagePostfix=".xul";
var currentPageTag;


var profName = "";
var profDir = "";

var toolkitCore;


// hack for inability to gray out finish button in first screen on first load
var isProfileData = false;

function wizardPageLoaded(tag) {
	dump("**********wizardPageLoaded\n");

	if (firstTime) {
		Startup();
	}

    currentPageTag = tag;
	dump("currentPageTag: "+currentPageTag+"\n");

	populatePage();
}


function loadPage(thePage)
{
	if (!firstTime) {
		saveData();
	}

	dump("**********loadPage\n");
	dump("thePage: "+thePage+"\n");
	displayPage(thePage);

	firstTime = false;
	return(true);
}



function onNext()
{
	dump("***********onNext\n");

	if (currentPageTag == "content1_1") {
		isProfileData = true;
	}

	if (currentPageTag != "content1_2") {
		saveData();
		var nextPageTag = testMap[currentPageTag].next;
		var url = getUrlFromTag(nextPageTag);
		displayPage(url);
	}
}

function onBack()
{
	dump("***********onBack\n");

	if (currentPageTag != "content1_1") {
		saveData();
		previousPageTag = testMap[currentPageTag].previous;
		var url = getUrlFromTag(previousPageTag);
		displayPage(url);
	}
}

function displayPage(content)
{
	dump("********INSIDE DISPLAYPAGE\n\n");

	if (content != "")
	{
		var	contentFrame = document.getElementById("content");
		if (contentFrame)
		{
			contentFrame.setAttribute("src", content);
		}
	}
}


function populatePage() 
{
    dump("************initializePage\n");
	var contentWindow = window.frames["content"];
	var doc = contentWindow.document;

    for (var i in wizardHash) {
        var formElement=doc.getElementById(i);
		dump("formElement: "+formElement+"\n");

        if (formElement) {
            formElement.value = wizardHash[i];
			dump("wizardHash["+"i]: "+wizardHash[i]+"\n");
        }
    }
}

function saveData()
{
	dump("************ SAVE DATA\n");

	var contentWindow = window.frames["content"];
	var doc = contentWindow.document;

	var inputs = doc.getElementsByTagName("FORM")[0].elements;

	dump("There are " + inputs.length + " input tags\n");
	for (var i=0 ; i<inputs.length; i++) {
        dump("Saving: ");
	    dump("ID=" + inputs[i].id + " Value=" + inputs[i].value + "..");
		wizardHash[inputs[i].id] = inputs[i].value;
	}
    dump("done.\n");
}

function onCancel()
{
	dump("************** ON CANCEL\n");
	saveData();
	var i;
    for (i in wizardHash) {
		dump("element: "+i+"\n");
        dump("value: "+wizardHash[i]+"\n");
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
	toolkitCore = XPAppCoresManager.Find("toolkitCore");
	if (!toolkitCore) {
		toolkitCore = new ToolkitCore();
		
		if (toolkitCore) {
			toolkitCore.Init("toolkitCore");
		}
	}
}

function Finish(opener)
{

	dump("******FINISH ROUTINE\n");

	if (isProfileData) {
		saveData();
		processCreateProfileData();

		if (opener) {
			opener.CreateProfile();
		}
	}
	else {
		alert("You need to enter minimum profile info before clicking on finish.\n\nYou can do this by clicking on the next button.\n");
	}

}

function processCreateProfileData()
{
	//Process Create Profile Data

	dump("******processCreateProfileData ROUTINE\n");

	var i;
	var data = "";

    for (i in wizardHash) {
		dump("element: "+i+"\n");
        dump("value: "+wizardHash[i]+"\n");
		
		if (i == "ProfileName") {
			profName = wizardHash[i];
			data = data+i+"="+profName+"%";
		}

		if (i == "ProfileDir") {
			profDir = wizardHash[i];
			data = data+i+"="+profDir+"%";
		}
    }

	dump("data: "+data+"\n");
    dump("calling javascript reflection\n");
    var profileCore = XPAppCoresManager.Find("ProfileCore");
    if (!profileCore)
    {
	    //dump("!profileCore\n");
		profileCore = new ProfileCore();
		//dump("!profileCore\n");

		if (profileCore) {
			//dump("after ! yes profileCore in if loop\n");
			profileCore.Init("ProfileCore");
		}
		else {
			dump("profile not created\n");
		}
	}

	if (profileCore) {
	    //dump("yes profileCore\n");
		dump("********DATA: "+data+"\n\n");
		profileCore.CreateNewProfile(data);
		profileCore.StartCommunicator(profName);
	}
	
	toolkitCore = XPAppCoresManager.Find("toolkitCore");
	if (!toolkitCore) {
		toolkitCore = new ToolkitCore();
		
		if (toolkitCore) {
			toolkitCore.Init("toolkitCore");
		}
	}
	if (toolkitCore) {
		toolkitCore.CloseWindow(parent);
	}

}