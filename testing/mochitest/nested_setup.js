
var gTestURL = '';

function addPermissions()
{
  SpecialPowers.pushPermissions(
    [{ type: "browser", allow: true, context: document }],
    addPreferences);
}

function addPreferences()
{
  SpecialPowers.pushPrefEnv(
    {"set": [["dom.mozBrowserFramesEnabled", true]]},
    insertFrame);
}

function insertFrame()
{
  SpecialPowers.nestedFrameSetup();

  var iframe = document.createElement('iframe');
  iframe.id = 'nested-parent-frame';
  iframe.width = "100%";
  iframe.height = "100%";
  iframe.scoring = "no";
  iframe.setAttribute("remote", "true");
  iframe.setAttribute("mozbrowser", "true");
  iframe.src = gTestURL;
  document.getElementById("holder-div").appendChild(iframe);
}