function goQuitApplication()
{
	var editorShell = Components.classes["component://netscape/editor/editorshell"].createInstance();
	editorShell = editorShell.QueryInterface(Components.interfaces.nsIEditorShell);
	
	if ( editorShell )
		editorShell.Exit();
}
