function Startup()
{
  PlaySoundCheck();

  // if we can't get the alert service, hide the pref UI for using alerts to notify on new mail
  // see bug #158711
  try {
    var alertService = Components.classes["@mozilla.org/alerts-service;1"].getService(Components.interfaces.nsIAlertsService);
  }
  catch(ex) {
    var newMailNotificationAlertUI = document.getElementById("newMailNotificationAlert");
    newMailNotificationAlertUI.setAttribute("hidden","true");
  }
}

function PlaySoundCheck()
{
  var playSound = document.getElementById("newMailNotification").checked;
  var playSoundType = document.getElementById("newMailNotificationType");
  playSoundType.disabled = !playSound;

  var disableCustomUI = !(playSound && playSoundType.value == 1);
  var mailnewsSoundFileUrl = document.getElementById("mailnewsSoundFileUrl");

  mailnewsSoundFileUrl.disabled = disableCustomUI
  document.getElementById("preview").disabled = disableCustomUI || (mailnewsSoundFileUrl.value == "");
  document.getElementById("browse").disabled = disableCustomUI;
}

function onCustomWavInput()
{
  document.getElementById("preview").disabled = (document.getElementById("mailnewsSoundFileUrl").value == "");
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
    var mailnewsSoundFileUrl = document.getElementById("mailnewsSoundFileUrl");
    mailnewsSoundFileUrl.value = fp.file.path;
  }

  onCustomWavInput();
}

var gSound = null;

function PreviewSound()
{
  var soundURL = document.getElementById("mailnewsSoundFileUrl").value;

  if (!gSound)
    gSound = Components.classes["@mozilla.org/sound;1"].createInstance(Components.interfaces.nsISound);

  if (soundURL.indexOf("file://") == -1) {
    // XXX todo see if we can create a nsIURL from the native file path
    // otherwise, play a system sound
    gSound.playSystemSound(soundURL);
  }
  else {
    var url = Components.classes["@mozilla.org/network/standard-url;1"].createInstance(Components.interfaces.nsIURL);
    url.spec = soundURL;
    gSound.play(url)
  }
}
