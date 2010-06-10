var controller = {};  Components.utils.import('resource://mozmill/modules/controller.js', controller);
var events = {}; Components.utils.import('resource://mozmill/modules/events.js', events);
var utils = {}; Components.utils.import('resource://mozmill/modules/utils.js', utils);
var elementslib = {}; Components.utils.import('resource://mozmill/modules/elementslib.js', elementslib);
var frame = {}; Components.utils.import('resource://mozmill/modules/frame.js', frame);

var that = this;

var hwindow = Components.classes["@mozilla.org/appshell/appShellService;1"]
                .getService(Components.interfaces.nsIAppShellService)
                .hiddenDOMWindow;
                
var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
           .getService(Components.interfaces.nsIWindowMediator);

var dir = function(obj){
 for (prop in obj){
    shell.send(prop);
  }
}
           
var shell = new function(){
  this.hist = [];
  this.histPos = 0;
  this.histLength = 20;
  
  //generate a new output entry node
  this.entry = function(val){
    var nd = document.createElement('div');
    nd.style.textAlign = "left";
    nd.style.paddingLeft = "5px";
    nd.style.paddingBottom = "1px";
    nd.style.font = "12px arial";
    nd.innerHTML = val;
    nd.style.display = "block";
    nd.style.width = "99%";

    return nd;
  }  
  //generate a new output entry node
  this.cmdEntry = function(val){
    nd = shell.entry(val);
    nd.style.fontWeight = "bold";
    return nd;
  };
  
  this.sin = function(){
    return document.getElementById('shellInput');
  };
  
  this.sout = function(){
    return document.getElementById('shellOutput');
  };
  
  this.sendCmd = function(s){
    shell.sout().insertBefore(shell.cmdEntry('<font color="blue">mmsh%</font> <font color="tan">'+s+'</font>'), shell.sout().childNodes[0]);
  };

  //send output to console
  this.send = function(s){
    if (s == undefined){
      return;
    }  
    shell.sout().insertBefore(shell.entry(s), shell.sout().childNodes[0]);
  };
  
  this.getWindows = function(){
     var enumerator = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                      .getService(Components.interfaces.nsIWindowMediator)
                      .getEnumerator("");
      var s = "";
      //define an array we can access
      shell.windows = [];
      windows = shell.windows;
      var c = 1;
      while(enumerator.hasMoreElements()) {
        var win = enumerator.getNext();
        shell.windows.push(win);
        c++;
      }
  }
  
  this.handle = function(cmd){
    //if the command has spaces -- args
    var cmdArr = cmd.split(' ');

    switch(cmdArr[0]){
    //clear window 
    case 'clear':
      shell.sout().innerHTML = "";
      break;
    
    case 'windows':
      shell.getWindows();
      for (win in shell.windows){
        shell.send( win+'. '+shell.windows[win].document.documentElement.getAttribute('windowtype') + ': ' + shell.windows[win].title); 
      }
      shell.sendCmd(cmd);
      break;
      
    case 'dir':
      //if has an arg
      if (cmdArr[1]){
        try {
          var arg = eval(cmdArr[1]);
          for (prop in arg){
            shell.send(prop);
          }
        } catch(err){
          shell.send('Error: '+err);
        }
      }
      else {
        for (prop in that){
          shell.send(prop);
        }
      }
      shell.sendCmd(cmd);
      break;    
    case 'help':
      var opts = [];
      opts.push('dir -- default shows you the current scope, \'dir obj\' or \'dir(obj)\' will show you the properties of the object.');
      opts.push('window -- reference to current content window.');
      opts.push('windows -- show you all the open in the browser.');
      opts.push('windows[x] -- access the window object of your choice');
      opts.push('elementslib -- bag of fun tricks for doing element lookups in the browser.');
      opts.push('hwindow -- ...');
      opts.push('controller -- ...');
      opts.push('events -- ...');
      opts.push('utils -- ...');      
      opts.push('clear -- reset the output.');

      while(opts.length != 0){
        this.send(opts.pop());
      }
      this.sendCmd(cmd);
      break;  
    //defaut is to eval
    default:
       try {
         var res = eval.call(that,cmd);
         if ((cmd.indexOf('=') == -1) && (res == null)){
           shell.send(cmd + ' is null.')
         }
         else {
           shell.send(res);
         }
       }
       catch(err){
         shell.send('<font color="red">Error:'+err+"</font>");
       }
       shell.sendCmd(cmd);
    }
    
    shell.sin().value = "";
    shell.sin().focus();
  };
  
  this.omc = function(event){
    if (event.target.value == "Type commands here..."){
      event.target.value = "";
    }
  };
  
  this.init = function(){
    document.getElementById('shellInput').addEventListener("keypress", shell.okp, false);
    document.getElementById('shellInput').addEventListener("mousedown", shell.omc, false);
    
    document.getElementById('shellInput').addEventListener("keydown", function(event){
      if (event.target.value == "Type commands here..."){
        event.target.value = "";
      }
      //if there is a command history
      if (shell.hist.length != 0){
        //uparrow
        if ((event.keyCode == 38) && (event.charCode == 0) && (event.shiftKey == true)){
          if (shell.histPos == shell.hist.length -1){
            shell.histPos = 0;
          } else {
            shell.histPos++;
          }
          shell.sin().value = shell.hist[shell.histPos];
        }
        //downarrow
        if ((event.keyCode == 40) && (event.charCode == 0) && (event.shiftKey == true)){
          if (shell.histPos == 0){
            shell.histPos = shell.hist.length -1;
          } else {
           shell.histPos--; 
          }
          shell.sin().value = shell.hist[shell.histPos];
        }
      }
    }, false);
    
  };
  
  this.enter = function(event){
    var inp = document.getElementById('shellInput');
    //inp.value = strings.trim(inp.value);
    //ignore empty returns
    // if ((inp.value == "") || (inp.value == " ")){
    //       return;
    //     }
    //if we have less than histLength
    //if (shell.hist.length < shell.histLength){
      shell.hist.unshift(inp.value);
      shell.histPos = shell.hist.length -1;
    // }
    // else {
    //   shell.hist.pop();
    //   shell.hist.unshift(inp.value);
    // }
    //pass input commands to the handler
    shell.handle(inp.value);
  };
  
  this.okp = function(event){
    if ((event.keyCode == 13) && (event.shiftKey == false)){
       event.preventDefault();
       shell.enter(event);
     }
  };
};

shell.init();
