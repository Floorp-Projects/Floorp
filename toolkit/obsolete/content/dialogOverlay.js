/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var doOKFunction = 0;
var doCancelFunction = 0;
var doButton2Function = 0;
var doButton3Function = 0;

// call this from dialog onload() to allow ok and cancel to call your code
// functions should return true if they want the dialog to close
function doSetOKCancel(okFunc, cancelFunc, button2Func, button3Func )
{
	//dump("top.window.navigator.platform: " + top.window.navigator.platform + "\n");
	
	doOKFunction = okFunc;
	doCancelFunction = cancelFunc;
	doButton2Function = button2Func;
	doButton3Function = button3Func;
}

function doOKButton()
{
	var close = true;
	
	if ( doOKFunction )
		close = doOKFunction();
	
	if (close && top)
		top.window.close();
}

function doCancelButton()
{
	var close = true;
	
	if ( doCancelFunction )
		close = doCancelFunction();
	
	if (close && top)
		top.window.close();
}

function doButton2()
{
	var close = true;
	
	if ( doButton2Function )
		close = doButton2Function();
	
	if (close && top)
		top.window.close();
}

function doButton3()
{
	var close = true;
	
	if ( doButton3Function )
		close = doButton3Function();
	
	if (close && top)
		top.window.close();
}

function moveToAlertPosition()
{
    // hack. we need this so the window has something like its final size
    if (window.outerWidth == 1) {
        dump("Trying to position a sizeless window; caller should have called sizeToContent() or sizeTo(). See bug 75649.\n");
        sizeToContent();
    }

    if (opener) {
        var xOffset = (opener.outerWidth - window.outerWidth) / 2;
        var yOffset = opener.outerHeight / 5;

        var newX = opener.screenX + xOffset;
        var newY = opener.screenY + yOffset;
    } else {
        newX = (screen.availWidth - window.outerWidth) / 2;
        newY = (screen.availHeight - window.outerHeight) / 2;
    }

	// ensure the window is fully onscreen (if smaller than the screen)
	if (newX < screen.availLeft)
		newX = screen.availLeft + 20;
	if ((newX + window.outerWidth) > (screen.availLeft + screen.availWidth))
		newX = (screen.availLeft + screen.availWidth) - window.outerWidth - 20;

	if (newY < screen.availTop)
		newY = screen.availTop + 20;
	if ((newY + window.outerHeight) > (screen.availTop + screen.availHeight))
		newY = (screen.availTop + screen.availHeight) - window.outerHeight - 60;

	window.moveTo( newX, newY );
}

function centerWindowOnScreen()
{
	var xOffset = screen.availWidth/2 - window.outerWidth/2;
	var yOffset = screen.availHeight/2 - window.outerHeight/2; //(opener.outerHeight *2)/10;
	
	xOffset = ( xOffset > 0 ) ? xOffset : 0;
  yOffset = ( yOffset > 0 ) ? yOffset : 0;
	window.moveTo( xOffset, yOffset);
}
