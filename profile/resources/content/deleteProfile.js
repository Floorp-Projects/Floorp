var bundle = window.opener.bundle;

function Startup()
{
  doSetOKCancel( onDontDeleteFiles, onCancel, onDeleteFiles, null );
  var okButton = document.getElementById("ok");
  var Button2 = document.getElementById("Button2");
  var cancelButton = document.getElementById("cancel");
  
  try {
    okButton.setAttribute( "value", bundle.GetStringFromName("dontDeleteFiles") );
    Button2.setAttribute( "value", bundle.GetStringFromName("deleteFiles") );
    cancelButton.setAttribute( "value", bundle.GetStringFromName("cancel") );
  } catch(e) {
    okButton.setAttribute( "value", "Don't Delete Files Yah" );
    Button2.setAttribute( "value", "Delete Files Yah" );
    cancelButton.setAttribute( "value", "Cancel Yah" );
  }
  Button2.removeAttribute("collapsed");
  okButton.focus();
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