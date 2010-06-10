//Determines of the event is accel, meaning
//ctrl or cmd depending on your platform
//but none of the other modified keys
function isAccel(e){
  var plat = navigator.platform;
  var isMac = (plat.indexOf('Mac') != -1);
  var modifiers = e.altKey;
  
  if ((isMac) && (!modifiers) && (e.metaKey) && (!e.ctrlKey) && (!e.shiftKey)){
    return true;
  }
  else if ((!isMac) && (!modifiers) && (e.ctrlKey) && (e.shiftKey) && (!e.metaKey)){
    
    return true;
  }
  return false;
};

//window onkeypress handler for various keyboard
//shortcuts
window.onkeypress = function(e){
  //if a test is in the middle of running, don't allow shortcuts to go through
  if ($("#runningStatus")[0].innerHTML.indexOf("Running") != -1){
    return;
  }
  if (isAccel(e)){ 
    switch(String.fromCharCode(e.charCode).toLowerCase()){
      case 'z':
         var node = window.frames['frame_editorInput'].document.getElementById('tab_file_'+encodeURIComponent(window.openFn));
         var disp = node.getElementsByTagName("strong")[0].style.display;
         if (disp != "inline"){
           return false;
         }
      break;
      case 'g':
        e.preventDefault();
        align();
      break;
      case 'f':
        e.preventDefault();
        showFileDialog();
      break;
      case 't':
        e.preventDefault();
        showTestDialog();
      break;
      case 'p':
        e.preventDefault();
        showOptionDialog();
      break;
      case 'i':
        e.preventDefault();
        showInspectDialog();
      break;
      case 'd':
        e.preventDefault();
        showRecordDialog();
      break;
      case 'o':
        e.preventDefault();
        openFile();
      break;
      case 'b':
        e.preventDefault();
        newFile();
      break;
      case 'r':
        e.preventDefault();
        runEditor();
      break;
      case 'e':
        e.preventDefault();
        saveAsFile();
      break;
      case 's':
        e.preventDefault();
        saveFile();
      break;
      case 'w':
        e.preventDefault();
        closeFile();
      break;
      case 'n':
        e.preventDefault();
        openNewWindow();
      break;
      case 'l':
        e.preventDefault();
        logicalClear();
      break;
      case 'm':
        e.preventDefault();
        genBoiler();
      break;
      case 'k':
        toggleHideEditor();
      break;
      case '1':
        e.preventDefault();
        $("#tabs").tabs().tabs("select", 0);
      break;
      case '2':
        e.preventDefault();
        $("#tabs").tabs().tabs("select", 1);
      break;
      case '3':
        e.preventDefault();
        $("#tabs").tabs().tabs("select", 2);
      break;
      case '4':
        e.preventDefault();
        $("#tabs").tabs().tabs("select", 3);
      break;
      default:
        return;
    }
  }
};