function goPageSetup()
{
}


function goQuitApplication()
{
	var editorShell = Components.classes["component://netscape/editor/editorshell"].createInstance();
	editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
	
	if ( editorShell )
		editorShell.Exit();
}


function goNewCardDialog(selectedAB)
{
	window.openDialog("chrome://addressbook/content/abNewCardDialog.xul",
					  "",
					  "chrome,resizeable=no,modal",
					  {selectedAB:selectedAB});
}


function goEditCardDialog(abURI, card, okCallback)
{
	window.openDialog("chrome://addressbook/content/abEditCardDialog.xul",
					  "",
					  "chrome,resizeable=no,modal",
					  {abURI:abURI, card:card, okCallback:okCallback});
}

