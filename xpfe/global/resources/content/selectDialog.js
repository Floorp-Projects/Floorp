function selectDialogOnLoad()
{
	dump("selectDialogOnLoad \n");
	doSetOKCancel( commonDialogOnOK, commonDialogOnCancel );
	
	param = window.arguments[0].QueryInterface( Components.interfaces.nsIDialogParamBlock  );
	if( !param )
		dump( " error getting param block interface\n" );
	
	var messageText = param.GetString( 0 );
	dump("message: "+ messageText +"\n");
	{
		 var messageFragment;

    	// Let the caller use "\n" to cause breaks
    	// Translate these into <br> tags
		 var messageParent = (document.getElementById("info.txt"));
	   	 done = false;
	   	 while (!done) {
	      breakIndex =   messageText.indexOf('\n');
	      if (breakIndex == 0) {
	        // Ignore break at the first character
	        messageText = messageText.slice(1);
	        dump("Found break at begining\n");
	        messageFragment = "";
	      } else if (breakIndex > 0) {
	        // The fragment up to the break
	        messageFragment = messageText.slice(0, breakIndex);

	        // Chop off fragment we just found from remaining string
	        messageText = messageText.slice(breakIndex+1);
	      } else {
	        // "\n" not found. We're done
	        done = true;
	        messageFragment = messageText;
	      }
	      messageNode = document.createTextNode(messageFragment);
	      if (messageNode)
	        messageParent.appendChild(messageNode);

	      // This is needed when the default namespace of the document is XUL
	      breakNode = document.createElementWithNameSpace("BR", "http://www.w3.org/TR/REC-html40");
	      if (breakNode)
	        messageParent.appendChild(breakNode);
	    }
	}
	
	var windowTitle = param.GetString( 1 );
	window.title = windowTitle;
	 
	listBox = document.getElementById("list");
	var numItems = param.GetInt( 2 )
	
	for ( i = 2; i <= numItems+1; i++ )
	{
		var newString = param.GetString( i );
		dump("setting string "+newString+"\n");
		var newOption = new Option( newString );
		listBox.options[listBox.options.length] = newOption;	
	}
	dump("number of items "+ listBox.options.length +" "+listBox.length+"\n");
	listBox.selectedIndex = 0;

	// Move to the right location
	moveToAlertPosition();
	param.SetInt(0, 1 );
}






function commonDialogOnOK()
{
	dump("commonDialogOnOK \n");
	param.SetInt(2, listBox.selectedIndex );
	param.SetInt(0, 0 );
	return true;
}

function commonDialogOnCancel()
{
	dump("commonDialogOnCancel \n");
	param.SetInt(2, listBox.selectedIndex );
	param.SetInt(0, 1 );
	return true;
}