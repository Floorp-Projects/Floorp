var profileCore = "";
var selected    = null;
var currProfile = "";

function CreateProfile()
{
	// Need to call CreateNewProfile xuls
	// var win = window.openDialog('resource://res/profile/cpwManager.xul', 'Creator', 'chrome');
	dump("\ngot here\n");
	//this.location.replace(this.location);
	this.location.replace("resource:/res/profile/cpwManager.xul");
	//this.location = "resource:/res/profile/cpwManager.xul";
}

function MigrateProfile(override)
{
	// Hack to avoid writing two copies of this
	if (override) 
		selected = override;

	if (!selected)
	{
		dump("Select a profile to migrate.\n");
		return;
	}

	var name = selected.getAttribute("rowName");
	var migrate = selected.getAttribute("rowMigrate");

	if (migrate != "true")
	{
		dump("This profile doesn't need migration.\n");
		return;
	}

	profileCore.MigrateProfile(name);
	this.location.replace(this.location);
}

function MigrateAllProfiles()
{
	var body = document.getElementById("theTreeBody");

	var child = body.firstChild;

	while (child)
	{
		if (child.getAttribute("rowMigrate") == "true")
		{
			MigrateProfile(child);
		}
		child = child.nextSibling;
	}
}

function RenameProfile(w)
{
	if (!selected)
	{
		dump("Select a profile to migrate.\n");
		return;
	}

	var newName = w.document.getElementById("NewName").value;
	var oldName = selected.getAttribute("rowName");
	var migrate = selected.getAttribute("rowMigrate");

	if (migrate == "true")
	{
		dump("Migrate this profile before renaming it.\n");
		return;
	}

	//dump("RenameProfile : " + oldName + " to " + newName + "\n");
	profileCore.RenameProfile(oldName, newName);
	this.location.replace(this.location);
}

function DeleteProfile()
{
	if (!selected)
	{
		dump("Select a profile to delete.\n");
		return;
	}

	var migrate = selected.getAttribute("rowMigrate");

	var name = selected.getAttribute("rowName");
	//dump("Delete '" + name + "'\n");
	profileCore.DeleteProfile(name);
	this.location.replace(this.location);
}

function StartCommunicator()
{
	dump("************Inside Start Communicator prof\n");
	if (!selected)
	{
		dump("Select a profile to migrate.\n");
		return;
	}

	var migrate = selected.getAttribute("rowMigrate");
	var name = selected.getAttribute("rowName");

	if (migrate == "true")
	{
		dump("Migrate this profile before using it to start communicator.\n");
		return;
	}

	dump("************name: "+name+"\n");
	profileCore.StartCommunicator(name);
	ExitApp();
}

function ExitApp()
{
	var toolkitCore = XPAppCoresManager.Find("toolkitCore");
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

function showSelection(node)
{
	dump("************** In showSelection routine!!!!!!!!! \n");
	// (see tree's onclick definition)
	// Tree events originate in the smallest clickable object which is the cell.  The object 
	// originating the event is available as event.target.  We want the cell's row, so we go 
	// one further and get event.target.parentNode.
	selected = node;
	var num = node.getAttribute("rowNum");
	dump("num: "+num+"\n");

	var name = node.getAttribute("rowName");
	dump("name: "+name+"\n");

	//dump("Selected " + num + " : " + name + "\n");
}

function addTreeItem(num, name, migrate)
{
  dump("Adding element " + num + " : " + name + "\n");
  var body = document.getElementById("theTreeBody");

  var newitem = document.createElement('treeitem');
  var newrow = document.createElement('treerow');
  newrow.setAttribute("rowNum", num);
  newrow.setAttribute("rowName", name);
  newrow.setAttribute("rowMigrate", migrate);

  var elem = document.createElement('treecell');

  // Hack in a differentation for migration
  if (migrate)
	  var text = document.createTextNode('Migrate');
  else
	  var text = document.createTextNode('');

  elem.appendChild(text);
  newrow.appendChild(elem);

  var elem = document.createElement('treecell');
  var text = document.createTextNode(name);
  elem.appendChild(text);
  newrow.appendChild(elem);

  newitem.appendChild(newrow);
  body.appendChild(newitem);
}

function loadElements()
{
	dump("****************hacked onload handler adds elements to tree widget\n");
	var profileList = "";

	profileCore = Components.classes["component://netscape/profile/profile-services"].createInstance();
	profileCore = profileCore.QueryInterface(Components.interfaces.nsIProfileServices);
	dump("profile = " + profileCore + "\n");
	
	profileList = profileCore.GetProfileList();	

	//dump("Got profile list of '" + profileList + "'\n");
	profileList = profileList.split(",");
	currProfile = profileCore.GetCurrentProfile();

	for (var i=0; i < profileList.length; i++)
	{
		var pvals = profileList[i].split(" - ");
		addTreeItem(i, pvals[0], (pvals[1] == "migrate"));
	}
}

function openRename()
{
	if (!selected)
		dump("Select a profile to rename.\n");
	else
	{
		var migrate = selected.getAttribute("rowMigrate");
		if (migrate == "true")
			dump("Migrate the profile before renaming it.\n");
		else
			var win = window.openDialog('pmrename.xul', 'Renamer', 'chrome');
	}
}


function ConfirmDelete() 
{
	if (!selected)
	{
		dump("Select a profile to delete.\n");
		return;
	}

	var migrate = selected.getAttribute("rowMigrate");
	var name = selected.getAttribute("rowName");

	if (migrate == "true")
	{
		dump("Migrate this profile before deleting it.\n");
		return;
	}

    var win = window.openDialog('pmDelete.xul', 'Deleter', 'chrome');
    return win;
}


function ConfirmMigrateAll() 
{
    var win = window.openDialog('pmMigrateAll.xul', 'MigrateAll', 'chrome');
    return win;
}


// -------------------------------------------- begin Hack for OnLoad handling
setTimeout('loadElements()', 0);
// -------------------------------------------- end   Hack for OnLoad handling
