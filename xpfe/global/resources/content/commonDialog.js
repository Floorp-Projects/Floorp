function commonDialogOnLoad()
{
	dump("commonDialogOnLoad \n");
	doSetOKCancel( commonDialogOnOK, commonDialogOnCancel );
	
	param = window.arguments[0].QueryInterface( Components.interfaces.nsIDialogParamBlock  );
	if( !param )
		dump( " error getting param block interface\n" );
	
	var msg = param.GetString( 0 );
	dump("message: "+ msg +"\n");
	SetElementText("info.txt", msg ); 
	
	msg = param.GetString( 3 );
	dump("title message: "+ msg +"\n");
	SetElementText("info.header", msg ); 
	
	
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
		dump( "Setting OK Button to "+buttonText+"\n");
		var okButton = document.getElementById("ok");
		okButton.setAttribute("value", buttonText);
	}
	
	buttonText = param.GetString( 9 );
	if ( buttonText != "" )
	{
	
		dump( "Setting Cancel Button to"+buttonText+"\n");
		var cancelButton = document.getElementById("cancel");
		cancelButton.setAttribute( "value",buttonText);
	}
	
	// Adjust number buttons
	var numButtons = param.GetInt( 2 );
	if ( numButtons == 1 )
	{
		var element = document.getElementById("cancel");
		if ( element )
		{
			dump( "hide button \n" );
			element.setAttribute("style", "display:none;"  );
		}
		else
		{
			dump( "couldn't find button \n");	
		}
	}
	
	// Set the Checkbox
	dump(" set checkbox \n");
	var checkMsg = param.GetString(  1 );
	dump("check box msg is "+ checkMsg +"\n");
	if ( checkMsg != "" )
	{	
		var prompt = (document.getElementById("checkboxLabel"));
    	if ( prompt )
    	{
    		dump(" setting message \n" );
    		prompt.childNodes[1].nodeValue = checkMsg;
    	}
		var checkValue = param.GetInt( 1 );
		var element=document.getElementById("checkbox" );
		var checkbool =  checkValue > 0 ? true : false;
		element.checked = checkbool;
	}
	else
	{
		dump("not visibile \n");
		var element = document.getElementById("checkboxLabel");
		element.setAttribute("style","display: none;" );
	}
	// handle the edit fields
	dump("set editfields \n");
	
	
	
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
		 	 	dump("hiding password");
		 		var element = document.getElementById("loginEditField");
				element.setAttribute("style","display: none;" );
				// Now hide the meanless text
				var element = document.getElementById("password.text");
				element.setAttribute("style", "display:none;"  );
				var element = document.getElementById("dialog.password");
				dump("give keyboard focus to password edit field \n");
				element.focus();
		 	 }
		 	 else
		 	 {
		 		dump("hiding password");
		 		var element = document.getElementById("passwordEditfield");
				element.setAttribute("style","display: none;" );
				// Now hide the meanless text
				var element = document.getElementById("login.text");
				element.setAttribute("style", "display:none;"  );
				
			}
			break;
	 	case 0:
	 		dump("hide editfields \n");
			var element = document.getElementById("editFields");
			element.setAttribute("style","display: none;" );
			
			break;
	}
	

	// Move to the right location
	moveToAlertPosition();
}

function onCheckboxClick()
{
	
	var element = document.getElementById("checkbox" );
	param.SetInt( 1, element.checked );
	dump("setting checkbox to "+ element.checked+"\n");
}

function SetElementText( elementID, text )
{
	dump("setting "+elementID+" to "+text +"\n");
	var element = document.getElementById(elementID);
	if( element )
		element.childNodes[0].nodeValue = text;
	else
		dump("couldn't find element \n");
}


function commonDialogOnOK()
{
	dump("commonDialogOnOK \n");
	param.SetInt(0, 0 );
	var element = document.getElementById("dialog.loginname");
	param.SetString( 6, element.value );
	dump(" login name - "+ element.value+ "\n");
	
	element = document.getElementById("dialog.password");
	param.SetString( 7, element.value );
	dump(" password - "+ element.value+ "\n");
	return true;
}

function commonDialogOnCancel()
{
	dump("commonDialogOnCancel \n");
	param.SetInt(0, 1 );
	return true;
}