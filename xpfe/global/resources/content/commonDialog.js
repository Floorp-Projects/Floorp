function commonDialogOnLoad()
{
  dump("commonDialogOnLoad \n");
  doSetOKCancel( commonDialogOnOK, commonDialogOnCancel, commonDialogOnButton2, commonDialogOnButton3 );
	
	param = window.arguments[0].QueryInterface( Components.interfaces.nsIDialogParamBlock  );
	if( !param )
		dump( " error getting param block interface\n" );
	
	var messageText = param.GetString( 0 );
  var messageFragment;

  // Let the caller use "\n" to cause breaks
  // Translate these into <br> tags
  var messageParent = document.getElementById("info.txt");
  var done = false;
  while (!done) 
  {
    breakIndex =   messageText.indexOf('\n');
    if (breakIndex == 0) {
      // Ignore break at the first character
      messageText = messageText.slice(1);
      dump("Found break at begining\n");
      messageFragment = "";
    } 
    else if (breakIndex > 0) {
      // The fragment up to the break
      messageFragment = messageText.slice(0, breakIndex);
      // Chop off fragment we just found from remaining string
      messageText = messageText.slice(breakIndex+1);
    } 
    else {
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
  
  // set a bold header message
	var msg = param.GetString( 3 );
	SetElementText("info.header", msg ); 
	
  // set the window title
	var windowTitle = param.GetString( 12 );
	window.title = windowTitle;
	
  // set the icon. hm.. this is potentially unskinnable.
  var iconURL = param.GetString( 2 ); 
  // hack around the unskinnable code by looking into the url and attempting to get something meaningful from it:
  var fileTag = iconURL.substring( ( iconURL.lastIndexOf("/") + 1 ), iconURL.lastIndexOf(".") );  // BAD BAD HACKERY EVIL ICK WRONG BAD 
  switch ( fileTag ) 
  {
    case "alert-icon":
      var iconClass = "alert-icon";
      break;
    case "error-icon":
      var iconClass = "error-icon";
      break;
    case "question-icon":
      var iconClass = "question-icon";
      break;
    case "message-icon":
    default:
      var iconClass = "message-icon";
      break;
  }
	var element = document.getElementById("icon");
	if( element )
		element.setAttribute( "class", iconClass );

	// Set button names
	var buttonText = param.GetString( 8 );
	if ( buttonText != "" )
	{
		var okButton = document.getElementById("ok");
		okButton.setAttribute("value", buttonText);
	}
	
	buttonText = param.GetString( 9 );
	if ( buttonText != "" )
	{
		var cancelButton = document.getElementById("cancel");
		cancelButton.setAttribute( "value",buttonText);
	}
	
	// Adjust number buttons
	var numButtons = param.GetInt( 2 );
	if ( numButtons == 1 ) ;
	
	switch ( numButtons )
	{
		case 4:
      var button = document.getElementById("Button3");
      button.setAttribute("style", "display:inline;");
      var buttonText = param.GetString( 11 );
      button.setAttribute( "value",buttonText);
		case 3:
      var button = document.getElementById("Button2");
      button.setAttribute("style", "display:inline;");
      var buttonText = param.GetString( 10 );
      button.setAttribute( "value",buttonText);
			break;
		case 1:
			var element = document.getElementById("cancel");
			if ( element )
				element.setAttribute("style", "display:none;"  );
			break;	
    default:
      break;
	}
	
	// Set the Checkbox
	var checkMsg = param.GetString(  1 );
	if ( checkMsg != "" )
	{	
		var prompt = (document.getElementById("checkboxLabel"));
   	if ( prompt )
   		prompt.childNodes[1].nodeValue = checkMsg;
		var checkValue = param.GetInt( 1 );
		var element=document.getElementById("checkbox" );
		var checkbool =  checkValue > 0 ? true : false;
		element.checked = checkbool;
	}
	else {
		var element = document.getElementById("checkboxLabel");
		element.setAttribute("style","display: none;" );
	}

	// handle the edit fields
	var numEditfields = param.GetInt( 3 );
	switch( numEditfields )
	{
		case 2:
			var element = document.getElementById("dialog.password2");
			element.value = param.GetString( 7 );
			var editMsg = param.GetString( 5 );
   		if (editMsg) 
				SetElementText("password2.text", editMsg ); 

	 		var editfield1Password = param.GetInt( 4 );
   		if ( editfield1Password == 1 ) {
				var element = document.getElementById("dialog.password1");
				element.value = param.GetString( 6 );

				var editMsg1 = param.GetString( 4 );
 				if (editMsg1)
					SetElementText("password1.text", editMsg1 ); 
		 		var element = document.getElementById("loginEditField");
				element.setAttribute("style","display: none;" );
				var element = document.getElementById("dialog.password1");
				element.focus();
      }
			else {
				var element = document.getElementById("dialog.loginname");
				element.value = param.GetString( 6 );

				var editMsg1 = param.GetString( 4 );
        if (editMsg1)
					SetElementText("login.text", editMsg1 ); 
		 		var element = document.getElementById("password1EditField");
				element.setAttribute("style","display: none;" );
				var element = document.getElementById("dialog.loginname");
				element.focus();
      }
			break;
	 	case 1:
	 		var editfield1Password = param.GetInt( 4 );
	 		if ( editfield1Password == 1 ) {
				var element = document.getElementById("dialog.password1");
				element.value = param.GetString( 6 );

//				var editMsg1 = param.GetString( 4 );
//				if (editMsg1) {
//					SetElementText("password1.text", editMsg1 ); 
//				}
				// Now hide the meaningless text
				var element = document.getElementById("password1.text");
				element.setAttribute("style", "display:none;"  );
		 		var element = document.getElementById("loginEditField");
				element.setAttribute("style","display: none;" );
		 		var element = document.getElementById("password2EditField");
				element.setAttribute("style","display: none;" );
				var element = document.getElementById("dialog.password1");
				element.focus();
      }
		 	else {
				var element = document.getElementById("dialog.loginname");
				element.value = param.GetString( 6 );

				var editMsg1 = param.GetString( 4 );
        if (editMsg1)
					SetElementText("login.text", editMsg1 ); 
				// Now hide the meaningless text
				var element = document.getElementById("login.text");
				element.setAttribute("style", "display:none;"  );
		 		var element = document.getElementById("password1EditField");
				element.setAttribute("style","display: none;" );
		 		var element = document.getElementById("password2EditField");
				element.setAttribute("style","display: none;" );
				var element = document.getElementById("dialog.loginname");
				element.focus();
			}
			break;
	 	case 0:
      var element = document.getElementById("loginEditField");
      element.setAttribute("style", "display:none;"  );
     	var element = document.getElementById("password1EditField");
      element.setAttribute("style","display: none;" );
     	var element = document.getElementById("password2EditField");
      element.setAttribute("style","display: none;" );
			break;
	}
	
	// set the pressed button to cancel to handle the case where the close box is pressed
	param.SetInt(0, 1 );

	// resize the window to the content
	window.sizeToContent();

	// Move to the right location
	moveToAlertPosition();
}

function onCheckboxClick()
{
	param.SetInt( 1, (param.GetInt( 1 ) ? 0 : 1 ));
}

// set the text on a text container
function SetElementText( elementID, text )
{
	var element = document.getElementById( elementID );
	while( element.hasChildNodes() )
    element.removeChild( element.firstChild );
  var text = document.createTextNode( text );
  element.appendChild( text );
}

// set an attribute on an element
function SetElementAttribute( elementID, attribute, value )
{
  var element = document.getElementById( elementID );
  element.setAttribute( attribute, value );
}

function commonDialogOnOK()
{
	param.SetInt(0, 0 );
	var element1, element2;
	var numEditfields = param.GetInt( 3 );
	if (numEditfields == 2) {
		var editfield1Password = param.GetInt( 4 );
		if ( editfield1Password == 1 )
			element1 = document.getElementById("dialog.password1");
		else
			element1 = document.getElementById("dialog.loginname");
		param.SetString( 6, element1.value );
		element2 = document.getElementById("dialog.password2");
		param.SetString( 7, element2.value );
	} 
  else if (numEditfields == 1) {
		var editfield1Password = param.GetInt( 4 );
		if ( editfield1Password == 1 ) {
			element1 = document.getElementById("dialog.password1");
			param.SetString( 6, element1.value );
		} 
    else {
			element1 = document.getElementById("dialog.loginname");
			param.SetString( 6, element1.value );
		}
	}
	return true;
}

function commonDialogOnCancel()
{
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

