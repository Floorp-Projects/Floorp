function Startup()
{
  var bundle = document.getElementById("bundle_profileManager");
  doSetOKCancel(onDontDeleteFiles, onCancel, onDeleteFiles, null);
  var okButton = document.getElementById("ok");
  var Button2 = document.getElementById("Button2");
  var cancelButton = document.getElementById("cancel");

  okButton.setAttribute("label", bundle.getString("dontDeleteFiles"));
  Button2.setAttribute("label", bundle.getString("deleteFiles"));
  cancelButton.setAttribute("label", bundle.getString("cancel"));

  Button2.removeAttribute("collapsed");
  okButton.focus();
}

function onDeleteFiles()
{
  opener.DeleteProfile(true);
  window.close();
}

function onDontDeleteFiles()
{
  opener.DeleteProfile(false);
  window.close();
}

function onCancel()
{
  window.close();
}
