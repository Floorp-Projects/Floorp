var doOKFunction = 0;
var doCancelFunction = 0;


// call this from dialog onload() to allow ok and cancel to call your code
// functions should return true if they want the dialog to close
function doSetOKCancel(okFunc, cancelFunc)
{
	//dump("top.window.navigator.platform: " + top.window.navigator.platform + "\n");
	
	doOKFunction = okFunc;
	doCancelFunction = cancelFunc;
}

function doOKButton()
{
	var close = true;
	
	if ( doOKFunction )
		close = doOKFunction();
	
	if ( close )
		top.window.close();
}

function doCancelButton()
{
	var close = true;
	
	if ( doCancelFunction )
		close = doCancelFunction();
	
	if ( close )
		top.window.close();
}

function moveToAlertPosition()
{
	var xOffset = opener.outerWidth/2 - window.outerWidth/2;
	var yOffset = (opener.outerHeight *2)/10;
	
	xOffset  = xOffset> 0 ? xOffset : 0;
	dump( "Move window by " + xOffset + ","+yOffset+"\n");
	dump( "screen x "+ opener.screenX +"screen y "+ opener.screenY +"\n");
	window.moveTo( opener.screenX + xOffset, opener.screenY + yOffset );

}