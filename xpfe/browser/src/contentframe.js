// -*- Mode: Java -*-

var sidebarURI = 'resource:/res/rdf/sidebar-browser.xul';
var isSidebarOpen = false;
var counter = 0;

function Init() {
  var pref = Components.classes['component://netscape/preferences'];
  if (pref) {
    pref = pref.getService();
  }
  if (pref) {
    pref = pref.QueryInterface(Components.interfaces.nsIPref);
  }
  if (pref) {
    pref.SetDefaultIntPref('sidebar.width', 170);
    //    pref.SetIntPref(pref.GetIntPref('sidebar.width'));
    pref.SetDefaultBoolPref('sidebar.open', true);
    if (pref.GetBoolPref('sidebar.open')) {
      toggleOpenClose();
    }
  }
}

function toggleOpenClose() {
  // Get the open width and update the pref state
  var pref = Components.classes['component://netscape/preferences'];
  if (pref) {
   pref = pref.getService();
  }
  if (pref) {
    pref = pref.QueryInterface(Components.interfaces.nsIPref);
  }
  var width = 0;

  if (pref) {
    pref.SetBoolPref('sidebar.open', !isSidebarOpen);
    width = pref.GetIntPref('sidebar.width');
  }

  if (isSidebarOpen)
  {
    // Close it
    var container = document.getElementById('container');
    var sidebar = container.firstChild;
    sidebar.setAttribute('style','width:0px; visibility:hidden');
    sidebar.setAttribute('src','about:blank');
	dump("*** Removing Sidebar: " + container.firstChild);
    container.removeChild(container.firstChild);	

    var grippy = document.getElementById('grippy');
    grippy.setAttribute('open','');

    isSidebarOpen = false;
  }
  else
  {
    // Open it
    var container = document.getElementById('container');
    var sidebar = container.firstChild;
    sidebar.setAttribute('style','width:' + width + 'px; visibility:visible');
    sidebar.setAttribute('src',sidebarURI);
	dump("Opening sidebar...");
	
	if (counter > 0) {
      var sidebar = document.createElement('html:iframe');
      sidebar.setAttribute('src','resource:/res/rdf/sidebar-browser.xul');
      sidebar.setAttribute('class','sidebarframe');
	  sidebar.setAttribute('id','sidebarframe');
	  sidebar.setAttribute('style','width:' + width + 'px; visibility:visible');
	  dump("*** Inserting sidebar before: " + container.firstChild);
      container.insertBefore(sidebar,container.firstChild);	
      //container.appendChild(sidebar);
    }    

    var grippy = document.getElementById('grippy');
    grippy.setAttribute('open','true');

    isSidebarOpen = true;
  }  

  counter++;
}

// To get around "window.onload" not working in viewer.
function Boot()
{
  var root = document.documentElement;
  if (root == null) {
    setTimeout(Boot, 0);
  } else {
    Init();
  }
}

setTimeout('Boot()', 0);
