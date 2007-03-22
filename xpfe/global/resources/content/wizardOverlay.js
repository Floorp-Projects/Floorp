/**
 *  Wizard button controllers.
 *  - Note: 
 *  -   less infrastructure is provided here than for dialog buttons, as 
 *  -   closing is not automatically desirable for many wizards (e.g. profile
 *  -   creation) where proper application shutdown is required. thus these 
 *  -   functions simply pass this responsibility on to the wizard designer.
 *  -
 *  - Use: Include this JS file in your wizard XUL and the accompanying 
 *  -      wizardOverlay.xul file as an overlay. Then set the overlay handlers
 *  -      with doSetWizardButtons(). It is recommended you use this overlay
 *  -      with the WizardManager wizard infrastructure. If you do that, you
 *  -      don't need to do anything here. Otherwise, use doSetWizardButtons()
 *  -      with false or null passed in as the first parameter, and the names
 *  -      of your functions passed in as the remaining parameters, see below.
 *  -
 *  - Ben Goodger (04/11/99)  
 **/

var doNextFunction    = null;
var doBackFunction    = null;
var doFinishFunction  = null;
var doCancelFunction  = null;

// call this from dialog onload() to allow buttons to call your code.
function doSetWizardButtons( wizardManager, nextFunc, backFunc, finishFunc, cancelFunc )
{
  if(wizardManager) {
    doNextFunction    = wizardManager.onNext;
    doBackFunction    = wizardManager.onBack;
    doFinishFunction  = wizardManager.onFinish;
    doCancelFunction  = wizardManager.onCancel;
  } else {
  	doNextFunction    = nextFunc;
    doBackFunction    = backFunc;
    doFinishFunction  = finishFunc;
    doCancelFunction  = cancelFunc; 
  }
}

// calls function specified for "next" button click.
function doNextButton()
{
	if ( doNextFunction )
		doNextFunction();
}

// calls function specified for "back" button click.
function doBackButton()
{
	if ( doBackFunction )
		doBackFunction();
}

// calls function specified for "finish" button click.
function doFinishButton()
{
	if ( doFinishFunction )
  	doFinishFunction();
}

// calls function specified for "cancel" button click.
function doCancelButton()
{
	if ( doCancelFunction )
		doCancelFunction();
}


