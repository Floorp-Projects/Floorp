function commonDialogOnLoad()
{
	dump("commonDialogOnLoad \n");
	doSetOKCancel( commonDialogOnOK, commonDialogOnCancel, commonDialogOnButton2, commonDialogOnButton3 );
	
	param = window.arguments[0].QueryInterface( Components.interfaces.nsIDialogParamBlock  );
	if( !param )
		dump( " error getting param block interface\n" );
	
	var messageText = param.GetString( 0 );
//	dump("message: "+ msg +"\n");
	//SetElementText("info.txt", msg ); 
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
	var msg = param.GetString( 3 );
//	dump("title message: "+ msg +"\n");
	SetElementText("info.header", msg ); 
	
	var windowTitle = param.GetString( 12 );
	window.title = windowTitle;
	
	var iconURL = param.GetString(2 ); 
	var element = document.getElementById("info.icon");
	if( element )
		element.src = iconURL;
	else
		dump("couldn't find icon element \n");
	// Set button names
	var buttonText = param.GetString( 8 );
	if ( buttonText != "" )
	{
//		dump( "Setting OK Button to "+buttonText+"\n");
		var okButton = document.getElementById("ok");
		okButton.setAttribute("value", buttonText);
	}
	
	buttonText = param.GetString( 9 );
	if ( buttonText != "" )
	{
	
//		dump( "Setting Cancel Button to"+buttonText+"\n");
		var cancelButton = document.getElementById("cancel");
		cancelButton.setAttribute( "value",buttonText);
	}
	
	// Adjust number buttons
	var numButtons = param.GetInt( 2 );
	if ( numButtons == 1 )
	{
		
	}
	
	switch ( numButtons )
	{
		case 4:
			{
				var button = document.getElementById("Button3");
				button.setAttribute("style", "display:inline;");
				var buttonText = param.GetString( 11 );
				button.setAttribute( "value",buttonText);
			}
		case 3:
			{
				var button = document.getElementById("Button2");
				button.setAttribute("style", "display:inline;");
				var buttonText = param.GetString( 10 );
				button.setAttribute( "value",buttonText);
			}
			break;
		case 1:
			var element = document.getElementById("cancel");
			if ( element )
			{
	//			dump( "hide button \n" );
				element.setAttribute("style", "display:none;"  );
			}
			else
			{
	//			dump( "couldn't find button \n");	
			}
			break;	
	}
	
	
	
	// Set the Checkbox
//	dump(" set checkbox \n");
	var checkMsg = param.GetString(  1 );
//	dump("check box msg is "+ checkMsg +"\n");
	if ( checkMsg != "" )
	{	
		var prompt = (document.getElementById("checkboxLabel"));
    	if ( prompt )
    	{
 //   		dump(" setting message \n" );
    		prompt.childNodes[1].nodeValue = checkMsg;
    	}
		var checkValue = param.GetInt( 1 );
		var element=document.getElementById("checkbox" );
		var checkbool =  checkValue > 0 ? true : false;
		element.checked = checkbool;
	}
	else
	{
//		dump("not visibile \n");
		var element = document.getElementById("checkboxLabel");
		element.setAttribute("style","display: none;" );
	}
	// handle the edit fields
//	dump("set editfields \n");
	
	
	
	var element = document.getElementById("dialog.password");
	
	element.value = param.GetString( 7 );
	
	element = document.getElementById("dialog.loginname");
	element.value = param.GetString( 6 );
	var numEditfields = param.GetInt( 3 );
	switch( numEditfields )
	{
		case 2:
			var element = document.getElementById("dialog.loginname");
			element.focus();
			break;
	 	case 1:
	 		var editfield1Password = param.GetInt( 4 );
	 		if ( editfield1Password == 1 )
		 	 {
//		 	 	dump("hiding password");
		 		var element = document.getElementById("loginEditField");
				element.setAttribute("style","display: none;" );
				// Now hide the meanless text
				var element = document.getElementById("password.text");
				element.setAttribute("style", "display:none;"  );
				var element = document.getElementById("dialog.password");
//				dump("give keyboard focus to password edit field \n");
				element.focus();
		 	 }
		 	 else
		 	 {
//		 		dump("hiding password");
		 		var element = document.getElementById("passwordEditfield");
				element.setAttribute("style","display: none;" );
				// Now hide the meanless text
				var element = document.getElementById("login.text");
				element.setAttribute("style", "display:none;"  );
				
			}
			break;
	 	case 0:
//	 		dump("hide editfields \n");
			var element = document.getElementById("editFields");
			element.setAttribute("style","display: none;" );
			
			break;
	}
	
	// set the pressed button to cancel to handle the case where the close box is pressed
	param.SetInt(0, 1 );
	// Move to the right location
	moveToAlertPosition();
}

function onCheckboxClick()
{
	
	var element = document.getElementById("checkbox" );
	param.SetInt( 1, element.checked );
//	dump("setting checkbox to "+ element.checked+"\n");
}

function SetElementText( elementID, text )
{
//	dump("setting "+elementID+" to "+text +"\n");
	var element = document.getElementById(elementID);
	if( element )
		element.childNodes[0].nodeValue = text;
	else
		dump("couldn't find element \n");
}


function commonDialogOnOK()
{
//	dump("commonDialogOnOK \n");
	param.SetInt(0, 0 );
	var element = document.getElementById("dialog.loginname");
	param.SetString( 6, element.value );
//	dump(" login name - "+ element.value+ "\n");
	
	element = document.getElementById("dialog.password");
	param.SetString( 7, element.value );
//	dump(" password - "+ element.value+ "\n");
	return true;
}

function commonDialogOnCancel()
{
//	dump("commonDialogOnCancel \n");
	param.SetInt(0, 1 );
	return true;
}

function commonDialogOnButton2()
{
	param.SetInt(0, 2 );
	return true;
}

function commonDialogOnButton3()
{
	param.SetInt(0, 3 );
	return true;
}