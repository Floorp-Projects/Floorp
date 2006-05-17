function Startup()
{
  PlaySoundCheck();

  // if we don't have the alert service, hide the pref UI for using alerts to notify on download completion
  // see bug #158711
  var downloadDoneNotificationAlertUI = document.getElementById("finishedNotificationAlert");
  downloadDoneNotificationAlertUI.hidden = !("@mozilla.org/alerts-service;1" in Components.classes);
}

function PlaySoundCheck()
{
  var playSound = document.getElementById("finishedNotificationSound").checked;

  var disableCustomUI = !playSound;
  var finishedSoundUrl = document.getElementById("finishedSoundUrl");

  finishedSoundUrl.disabled = disableCustomUI;
  document.getElementById("preview").disabled = disableCustomUI || (finishedSoundUrl.value == "");
  document.getElementById("browse").disabled = disableCustomUI;
}

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function Browse()
{
  var fp = Components.classes["@mozilla.org/filepicker;1"]
                     .createInstance(nsIFilePicker);

  // XXX todo, persist the last sound directory and pass it in
  // XXX todo filter by .wav
  fp.init(window, document.getElementById("browse").getAttribute("filepickertitle"), nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterAll);

  var ret = fp.show();
  if (ret == nsIFilePicker.returnOK) {
    var soundUrl = document.getElementById("finishedSoundUrl");
    // convert the nsILocalFile into a nsIFile url 
    soundUrl.value = fp.fileURL.spec;
  }

  document.getElementById("preview").disabled =
    (document.getElementById("finishedSoundUrl").value == "");
}

var gSound = null;

function PreviewSound()
{
  var soundURL = document.getElementById("finishedSoundUrl").value;

  if (!gSound)
    gSound = Components.classes["@mozilla.org/sound;1"].createInstance(Components.interfaces.nsISound);

  if (soundURL.indexOf("file://") == -1) {
    // XXX todo see if we can create a nsIURL from the native file path
    // otherwise, play a system sound
    gSound.playSystemSound(soundURL);
  }
  else {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                              .getService(Components.interfaces.nsIIOService);
    var url = ioService.newURI(soundURL, null, null);
    gSound.play(url)
  }
}
