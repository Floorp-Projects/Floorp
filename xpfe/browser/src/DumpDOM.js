// ----------------------
// DumpDOM(node)
//
// Call this function to dump the contents of the DOM starting at the specified node.
// Use node = document.documentElement to dump every element of the current document.
// Use node = top.window.document.documentElement to dump every element.
// ----------------------
function DumpDOM(node)
{
	dump("--------------------- DumpDOM ---------------------\n");
	
	DumpNodeAndChildren(node, "");
	
	dump("------------------- End DumpDOM -------------------\n");
}


// This function does the work of DumpDOM by recursively calling itself to explore the tree
function DumpNodeAndChildren(node, prefix)
{
	dump(prefix + "<" + node.nodeName);

	if ( node.nodeType == 1 )
	{
		// id
		var text = node.getAttribute('id');
		if ( text && text[0] != '$' )
			dump(" id=\"" + text + "\"");
		
		DumpAttribute(node, "name");
		DumpAttribute(node, "class");
		DumpAttribute(node, "style");
		DumpAttribute(node, "flex");
		DumpAttribute(node, "value");
		DumpAttribute(node, "src");
		DumpAttribute(node, "onclick");
		DumpAttribute(node, "onchange");
	}
	
	if ( node.nodeName == "#text" )
		dump(" = \"" + node.data + "\"");
	
	dump(">\n");
	
	// dump IFRAME && FRAME DOM
	if ( node.nodeName == "IFRAME" || node.nodeName == "FRAME" )
	{
		if ( node.name )
		{
			var wind = top.frames[node.name];
			if ( wind && wind.document && wind.document.documentElement )
			{
				dump(prefix + "----------- " + node.nodeName + " -----------\n");
				DumpNodeAndChildren(wind.document.documentElement, prefix + "  ");
				dump(prefix + "--------- End " + node.nodeName + " ---------\n");
			}
		}
	}
	// children of nodes (other than frames)
	else if ( node.childNodes )
	{
		for ( var child = 0; child < node.childNodes.length; child++ )
			DumpNodeAndChildren(node.childNodes[child], prefix + "  ");
	} 
}


function DumpAttribute(node, attribute)
{
	var text = node.getAttribute(attribute);
	if ( text )
		dump(" " + attribute + "=\"" + text + "\"");
}
