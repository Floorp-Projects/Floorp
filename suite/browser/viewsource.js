function onLoadViewSource() 
{
   var docShellElement = document.getElementById("content-frame");
	var docShell = docShellElement.docShell;

	docShell.viewMode = Components.interfaces.nsIDocShell.viewSource;

	var webNav = docShell.QueryInterface(Components.interfaces.nsIWebNavigation);
	webNav.loadURI(window.arguments[0]);
}
