var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);



var showFileDialog = function(){
  $("#fileDialog").dialog("open");
}

var showTestDialog = function(){
  $("#testDialog").dialog("open");
}

var showOptionDialog = function(){
  $("#optionDialog").dialog("open");
}

var showHelpDialog = function(){
  $("#helpDialog").dialog("open");
}

var showRecordDialog = function(){
  $("#tabs").tabs().tabs("select", 0); 
  $("#recordDialog").dialog("open");
  MozMillrec.on();
  $("#recordDialog").dialog().parents(".ui-dialog:first").find(".ui-dialog-buttonpane button")[1].innerHTML = "Stop";
}

//Align mozmill to all the other open windows in a way that makes it usable
var align = function(){
  var enumerator = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator)
                     .getEnumerator("");
  while(enumerator.hasMoreElements()) {
    var win = enumerator.getNext();
    if (win.document.title != 'MozMill IDE'){
      var wintype = win.document.documentElement.getAttribute("windowtype");
      //move to top left corner
      win.screenY = 0;
      win.screenX = 0;

      //make only browser windows big
      if (wintype == "navigator:browser"){
        var width = window.screen.availWidth/2.5;
        var height = window.screen.availHeight;
        win.resizeTo((window.screen.availWidth - width), window.screen.availHeight);        
      }
    }
    else {
      var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                 .getService(Components.interfaces.nsIWindowMediator);
      var latestbrowser = wm.getMostRecentWindow('navigator:browser');
      
      //if there is no most recent browser window, use whatever window
      if (!latestbrowser){
        var latestbrowser = wm.getMostRecentWindow('');
      }
      
      win.screenX = latestbrowser.innerWidth;
      win.screenY = 0;
    }
  }  
    return true;
};
