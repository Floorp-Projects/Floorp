
var gDownloadView = null;
var gDownloadViewChildren = null;

function Startup()
{
  gDownloadView = document.getElementById("downloadView");
  gDownloadViewChildren = document.getElementById("downloadViewChildren");
  
  // Select the first item in the view, if any. 
  if (gDownloadViewChildren.hasChildNodes()) 
    gDownloadView.selectItem(gDownloadViewChildren.firstChild);
   
  gDownloadView.controllers.appendController(downloadViewController);
}

function Shutdown()
{

}

var downloadView = {
  onClick: function downloadView_click(aEvent)
  {
  
  },
  
  onSelect: function downloadView_select(aEvent)
  {
  
  },
  
};

var downloadViewController = {
  supportsCommand: function dVC_supportsCommand (aCommand)
  {
    switch (aCommand) {
    case "cmd_downloadFile":
    case "cmd_properties":
    case "cmd_pause":
    case "cmd_delete":
    case "cmd_openfile":
    case "cmd_showinshell":
    case "cmd_selectAll":
      return true;
    }
    return false;
  },
  
  isCommandEnabled: function dVC_isCommandEnabled (aCommand)
  {
    var cmds = ["cmd_properties", "cmd_pause", "cmd_delete",
                "cmd_openfile", "cmd_showinshell"];
    var selectionCount = gDownloadView.selectedItems.length;
    switch (aCommand) {
    case "cmd_downloadFile":
      return true;
    case "cmd_properties":
    case "cmd_openfile":
    case "cmd_showinshell":
      return selectionCount == 1;
    case "cmd_pause":
    case "cmd_delete":
      return selectionCount > 0;
    case "cmd_selectAll":
      return gDownloadViewChildren.childNodes.length != selectionCount;
    default:
    }
  },
  
  doCommand: function dVC_doCommand (aCommand)
  {
    dump("*** command = " + aCommand + "\n");
    switch (aCommand) {
    case "cmd_downloadFile":
      dump("*** show a dialog that lets a user specify a URL to download\n");
      break;
    case "cmd_properties":
      dump("*** show properties for selected item\n");
      break;
    case "cmd_openfile":
      dump("*** launch the file for the selected item\n");
      break;
    case "cmd_showinshell":
      dump("*** show the containing folder for the selected item\n");
      break;
    case "cmd_pause":
      dump("*** pause the transfer for the selected item\n");
      break;
    case "cmd_delete":
      dump("*** delete entries for the selection\n");
      // a) Prompt user to confirm end of transfers in progress
      // b) End transfers
      // c) Delete entries from datasource
      break;
    case "cmd_selectAll":
      gDownloadView.selectAll();
      break;
    default:
    }
  },  
  
  onEvent: function dVC_onEvent (aEvent)
  {
    switch (aEvent) {
    case "tree-select":
      this.onCommandUpdate();
    }
  },

  onCommandUpdate: function dVC_onCommandUpdate ()
  {
    var cmds = ["cmd_properties", "cmd_pause", "cmd_delete",
                "cmd_openfile", "cmd_showinshell"];
    for (var command in cmds)
      goUpdateCommand(cmds[command]);
  }
};

