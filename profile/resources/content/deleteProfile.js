var bundle = window.opener.bundle;

function Startup()
{
  doSetOKCancel( onDontDeleteFiles, onCancel, onDeleteFiles, null );
  var okButton = document.getElementById("ok");
  var Button2 = document.getElementById("Button2");
  var cancelButton = document.getElementById("cancel");
  okButton.setAttribute( "value", bundle.GetStringFromName("dontDeleteFiles") );
  okButton.setAttribute( "class", ( okButton.getAttribute("class") + " padded" ) );
  Button2.setAttribute( "value", bundle.GetStringFromName("deleteFiles") );
  Button2.setAttribute( "class", ( okButton.getAttribute("class") + " padded" ) );
  Button2.setAttribute( "style", "display: inherit;" );
  cancelButton.setAttribute( "value", bundle.GetStringFromName("cancel") );
  cancelButton.setAttribute( "class", ( cancelButton.getAttribute("class") + " padded" ) );
}

function onDeleteFiles()
{
  opener.DeleteProfile( true );
  window.close();
}

function onDontDeleteFiles()
{
  opener.DeleteProfile( false );
  window.close();
}

function onCancel()
{
  window.close();
}