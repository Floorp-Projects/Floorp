var bundle;
var profile = Components.classes["component://netscape/profile/manager"].getService();
profile = profile.QueryInterface(Components.interfaces.nsIProfile);

function handleOKButton()
{
	profile.automigrate = true;
	return true;
}
function handleCancelButton()
{
	profile.automigrate = false;
	return true;
}

function onLoad()
{
  bundle = srGetStrBundle("chrome://profile/locale/migration.properties");
	doSetOKCancel(handleOKButton, handleCancelButton);
  var okButton = document.getElementById("ok");
  var cancelButton = document.getElementById("cancel");
  if( !okButton || !cancelButton )
    return false;
  okButton.setAttribute( "value", bundle.GetStringFromName( "migrate" ) );
  okButton.setAttribute( "class", ( okButton.getAttribute( "class" ) + " padded" ) );
  cancelButton.setAttribute( "value", bundle.GetStringFromName( "newprofile" ) );
  cancelButton.setAttribute( "class", ( cancelButton.getAttribute( "class" ) + " padded" ) );
  centerWindowOnScreen();
}

