//Determines of the event is accel, meaning
//ctrl or cmd depending on your platform
//but none of the other modified keys
function isAccel(e){
  var plat = navigator.platform;
  var isMac = (plat.indexOf('Mac') != -1);
  var modifiers = e.altKey;
  
  if ((isMac) && (!modifiers) && (e.metaKey) && (!e.ctrlKey) && (!e.shiftKey))
    return true;
  else if ((!isMac) && (!modifiers) && (e.ctrlKey) && (e.shiftKey) && (!e.metaKey))
    return true;
  return false;
};

//window onkeypress handler for various keyboard
//shortcuts
function onkeypress(e){
  if (isAccel(e)){
    switch(String.fromCharCode(e.charCode).toLowerCase()){
      case 'a':
        showFileMenu();
        e.preventDefault();
      break;
      case 'g':
        e.preventDefault();
        align();
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
      case 'd':
        e.preventDefault();
        MozMillrec.toggle();
      break;
      case 'e':
        e.preventDefault();
        saveAsFile();
      break;
      case 's':
        e.preventDefault();
        saveFile();
      break;
      case 'u':
        e.preventDefault();
        closeFile();
      break;
      case 'n':
        e.preventDefault();
        newTemplate();
      break;
      case 't':
        e.preventDefault();
        runDirectory();
      break;
      case 'l':
        e.preventDefault();
        logicalClear();
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
