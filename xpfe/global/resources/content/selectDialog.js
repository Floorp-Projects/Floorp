function selectDialogOnLoad()
{
	dump("selectDialogOnLoad \n");
	doSetOKCancel( commonDialogOnOK, commonDialogOnCancel );
	
	param = window.arguments[0].QueryInterface( Components.interfaces.nsIDialogParamBlock  );
	if( !param )
		dump( " error getting param block interface\n" );
	
	var msg = param.GetString( 0 );
	dump("message: "+ msg +"\n");
	SetElementText("info.txt", msg ); 
	listBox = document.getElementById("list");
	var numItems = param.GetInt( 2 )
	
	for ( i = 1; i <= numItems; i++ )
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
	param.SetInt(2, listBox.selectedIndex );
	return true;
}

function commonDialogOnCancel()
{
	dump("commonDialogOnCancel \n");
	param.SetInt(2, listBox.selectedIndex );
	param.SetInt(0, 1 );
	return true;
}