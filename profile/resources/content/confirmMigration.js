var bundle;
var profile = Components.classes["@mozilla.org/profile/manager;1"].getService();
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
  try {
    bundle = srGetStrBundle("chrome://communicator/locale/profile/migration.properties");
  }
  catch (ex) {
    dump("please fix bug #26291\n");
  }

	doSetOKCancel(handleOKButton, handleCancelButton);
  var okButton = document.getElementById("ok");
  var cancelButton = document.getElementById("cancel");
  if( !okButton || !cancelButton )
    return false;

  try {
    okButton.setAttribute( "value", bundle.GetStringFromName( "migrate" ) );
  }
  catch (ex) {
    dump("please fix bug #26291\n");
    okButton.setAttribute( "value", "Migrate *");
  }

  okButton.setAttribute( "class", ( okButton.getAttribute( "class" ) + " padded" ) );

  try {
    cancelButton.setAttribute( "value", bundle.GetStringFromName( "newprofile" ) );
  }
  catch (ex) {
    dump("please fix bug #26291\n");
    cancelButton.setAttribute("value","New Profile *");
  }

  cancelButton.setAttribute( "class", ( cancelButton.getAttribute( "class" ) + " padded" ) );
  centerWindowOnScreen();
}

