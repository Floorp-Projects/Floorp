const nsIFilePicker       = Components.interfaces.nsIFilePicker;
const nsILocalFile        = Components.interfaces.nsILocalFile;
const nsIProperties       = Components.interfaces.nsIProperties;
const nsIIOService        = Components.interfaces.nsIIOService;
const nsIFileHandler      = Components.interfaces.nsIFileProtocolHandler;
const nsIURIFixup         = Components.interfaces.nsIURIFixup;
const kDownloadDirPref    = "browser.download.dir";

var gPrompt;
var gLocation;
var gChooseButton;
var gFolderField;
var gFinishedSound;
var gChooseButtonLocked;
var gPromptLocked;
var gFinishedSoundLocked;
var gIOService;
var gSound = null;

function Startup()
{
  gIOService = Components.classes["@mozilla.org/network/io-service;1"]
                         .getService(nsIIOService);
  gPrompt        = document.getElementById("autoDownload");
  gLocation      = document.getElementById("downloadLocation");
  gChooseButton  = document.getElementById("chooseDownloadFolder");
  gFolderField   = document.getElementById("downloadFolder");
  gFinishedSound = document.getElementById("finishedSoundUrl");

  var prefWindow = parent.hPrefWindow;
  gChooseButtonLocked  = prefWindow.getPrefIsLocked(kDownloadDirPref);
  gPromptLocked        = prefWindow.getPrefIsLocked("browser.download.autoDownload");
  gFinishedSoundLocked = prefWindow.getPrefIsLocked("browser.download.finished_sound_url");

  var dir = prefWindow.getPref("localfile", kDownloadDirPref);
  if (dir == "!/!ERROR_UNDEFINED_PREF!/!")
    dir = null;

  if (!dir)
  {
    try
    {
      // no default_dir folder found; default to profile directory
      var dirSvc = Components.classes["@mozilla.org/file/directory_service;1"]
                             .getService(nsIProperties);
      dir = dirSvc.get("ProfD", nsILocalFile);

      // now remember the new assumption
      prefWindow.setPref("localfile", kDownloadDirPref, dir);
    }
    catch (ex)
    {
    }
  }

  // if both pref and dir svc fail leave this field blank else show path
  if (dir)
    gFolderField.value = (/Mac/.test(navigator.platform)) ? dir.leafName : dir.path;

  setPrefDLElements();

  // Make sure sound_url setting is actually a URL
  try {
    var URIFixup = Components.classes["@mozilla.org/docshell/urifixup;1"]
                             .getService(nsIURIFixup);
    gFinishedSound.value = URIFixup.createFixupURI(gFinishedSound.value, nsIURIFixup.FIXUP_FLAG_NONE).spec;
  } catch (ex) { }

  PlaySoundCheck();

  // if we don't have the alert service, hide the pref UI for using alerts to notify on download completion
  // see bug #158711
  var downloadDoneNotificationAlertUI = document.getElementById("finishedNotificationAlert");
  downloadDoneNotificationAlertUI.hidden = !("@mozilla.org/alerts-service;1" in Components.classes);
}

function setPrefDLElements()
{
  if (!gPromptLocked)
    gLocation.disabled = (gPrompt.value == "true");
  gFolderField.disabled = gChooseButtonLocked;
  gChooseButton.disabled = gChooseButtonLocked;
}

function prefDownloadSelectFolder()
{
  var prefWindow = parent.hPrefWindow;
  var initialDir = prefWindow.getPref("localfile", kDownloadDirPref);

  // file picker will open at default location if no pref set
  if (initialDir == "!/!ERROR_UNDEFINED_PREF!/!")
    initialDir = null;

  var fpParams = {
    fp: makeFilePicker(),
    fptitle: "downloadfolder",
    fpdir: initialDir,
    fpmode: nsIFilePicker.modeGetFolder,
    fpfiletype: null,
    fpext: null,
    fpfilters: nsIFilePicker.filterAll
  };

  var ret = poseFilePicker(fpParams);
  if (ret == nsIFilePicker.returnOK) {
    var localFile = fpParams.fp.file.QueryInterface(nsILocalFile);
    prefWindow.setPref("localfile", kDownloadDirPref, localFile);
    gFolderField.value = (/Mac/.test(navigator.platform)) ? fpParams.fp.file.leafName : fpParams.fp.file.path;
  }
}

function makeFilePicker()
{
  return Components.classes["@mozilla.org/filepicker;1"]
                   .createInstance(nsIFilePicker);
}

function poseFilePicker(aFpP)
{
  var prefutilitiesBundle = document.getElementById("bundle_prefutilities");
  var title = prefutilitiesBundle.getString(aFpP.fptitle);
  var fp = aFpP.fp; // simply for smaller readable code

  fp.init(window, title, aFpP.fpmode);
  if (aFpP.fpdir)
    fp.displayDirectory = aFpP.fpdir;

  if (aFpP.fpfiletype) {
    var filetype = prefutilitiesBundle.getString(aFpP.fpfiletype);
    fp.appendFilter(filetype, aFpP.fpext);
  }
  if (aFpP.fpfilters)
    fp.appendFilters(aFpP.fpfilters);
  return fp.show();
}

function PlaySoundCheck()
{
  var disableCustomUI = !document.getElementById("finishedNotificationSound").checked;

  gFinishedSound.disabled = disableCustomUI || gFinishedSoundLocked;
  document.getElementById("preview").disabled = disableCustomUI;
  document.getElementById("browse").disabled = disableCustomUI || gFinishedSoundLocked;
}

function Browse()
{
  var initialDir = null;
  if (gFinishedSound.value != "") {
    var fileHandler = gIOService.getProtocolHandler("file")
                                .QueryInterface(nsIFileHandler);
    initialDir = fileHandler.getFileFromURLSpec(gFinishedSound.value)
                            .parent.QueryInterface(nsILocalFile);
  }
  var fpParams = {
    fp: makeFilePicker(),
    fptitle: "choosesound",
    fpdir: initialDir,
    fpmode: nsIFilePicker.modeOpen,
    fpfiletype: "SoundFiles",
    fpext: "*.wav; *.wave",
    fpfilters: nsIFilePicker.filterAll
  };

  var ret = poseFilePicker(fpParams);
  if (ret == nsIFilePicker.returnOK) {
    // convert the nsILocalFile into a nsIFile url 
    gFinishedSound.value = fpParams.fp.fileURL.spec;
  }
}

function PreviewSound()
{
  if (!gSound)
    gSound = Components.classes["@mozilla.org/sound;1"].createInstance(Components.interfaces.nsISound);

  if (gFinishedSound.value != "")
    gSound.play(gIOService.newURI(gFinishedSound.value, null, null));
  else
    gSound.beep();
}
