var gMigrationBundle;
var profile = Components.classes["@mozilla.org/profile/manager;1"].getService();
profile = profile.QueryInterface(Components.interfaces.nsIProfileInternal);

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
  gMigrationBundle = document.getElementById("bundle_migration");

  doSetOKCancel(handleOKButton, handleCancelButton);
  var okButton = document.getElementById("ok");
  var cancelButton = document.getElementById("cancel");
  if (!okButton || !cancelButton)
    return false;

  okButton.setAttribute("label", gMigrationBundle.getString("migrate"));
  okButton.setAttribute("class", okButton.getAttribute("class") + " padded");
  cancelButton.setAttribute("label", gMigrationBundle.getString("newprofile"));
  cancelButton.setAttribute("class", cancelButton.getAttribute("class") + " padded");
  okButton.focus();
  centerWindowOnScreen();
  return true;
}

