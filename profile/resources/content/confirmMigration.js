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
	doSetOKCancel(handleOKButton, handleCancelButton);
}
