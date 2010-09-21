var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);

var copyInspector = function() {
  MozMilldx.inspectorToClipboard();
  $("#tabs").tabs().tabs("select", 0);
}

function showFileMenu() {
  $("#fileMenu").click();
}

function openNewWindow() {
  window.open('');
}

function testFinished(){
  $("#tabs").tabs().tabs("select", 1);
  window.focus();
}

function swapTabs(tab){
  $('editorTab').style.display = 'none';
  $('outputTab').style.display = 'none';
  $('eventsTab').style.display = 'none';
  $('shellTab').style.display = 'none';
  
  $('editorHead').style.background = '#aaa';
  $('outputHead').style.background = '#aaa';
  $('eventsHead').style.background = '#aaa';
  $('shellHead').style.background = '#aaa';
  
  $(tab+'Tab').style.display = 'block';
  $(tab+'Head').style.background = 'white';
}

function logicalClear(){
  $('#resOut')[0].innerHTML = '';
}

function accessOutput(){
  var n = $('resOut');
  var txt = '';
  for (var c = 0; c < n.childNodes.length; c++){
    if (n.childNodes[c].textContent){
      txt += n.childNodes[c].textContent + '\n';  
    }
    else{
      txt += n.childNodes[c].value + '\n';
    }
  }
  if (txt == undefined){ return; }
  copyToClipboard(txt);
}

var copyToClipboard = function(str){
  const gClipboardHelper = Components.classes["@mozilla.org/widget/clipboardhelper;1"] .getService(Components.interfaces.nsIClipboardHelper); 
  gClipboardHelper.copyString(str);
}

var showFileDialog = function(){
  $("#fileDialog").dialog("open");
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
