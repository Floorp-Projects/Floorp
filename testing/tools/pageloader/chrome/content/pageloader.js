var NUM_CYCLES = 5;

var pages;
var pageIndex;
var results;
var start_time;
var end_time;
var cycle;
var report;

function plInit() {
  try { 
    pageIndex = 0;
    cycle = 0;
    results = new Object();
    if (! pages) {
      pages = plLoadURLsFromFile();
    }
    if (pages.length == 0) {
      alert('no pages to test');
      plStop(true);
    }
    report = new Report(pages);
    plLoadPage();
  } catch(e) {
    alert(e);
    plStop(true);
  }
}

function plLoadPage() {
  try {
    start_time = new Date();
    p = pages[pageIndex];
    var startButton = document.getElementById('plStartButton');
    startButton.setAttribute('disabled', 'true');
    this.content = document.getElementById('contentPageloader');
    this.content.addEventListener('load', plLoadHandler, true);
    this.content.loadURI(p);
  } catch (e) {
    alert(e);
    plStop(true);
  }
}

function plLoadHandler(evt) {
  if (evt.type == 'load') {
    window.setTimeout('reallyHandle()', 500);
  } else {
    alert('Unknown event type: '+evt.type);
    plStop(true);
  }
}

function reallyHandle() {
    if (pageIndex < pages.length) {
      try { 
        end_time = new Date();    
        var pageName = pages[pageIndex];
        results[pageName] = (end_time - start_time);
        start_time = new Date();
        dump(pageName+" took "+results[pageName]+"\n");
        plReport();
        pageIndex++;
        plLoadPage();
      } catch(e) {
        alert(e);
        plStop(true);
      }
    } else {
      plStop(false);
    }
}

function plReport() {
    try {
      var reportNode = document.getElementById('report');
      var pageName = pages[pageIndex];
      var time = results[pageName];
      report.recordTime(pageIndex, time);
    } catch(e) {
      alert(e);
      plStop(false);
    }
}

function plStop(force) {
  try {
    pageIndex = 0;
    results = new Object;
    if (force == false) {
      if (cycle < NUM_CYCLES) {
        cycle++;
        plLoadPage();
        return;
      } else {
        dump(report.getReport()+"\n");
      }
    }
    var startButton = document.getElementById('plStartButton');
    startButton.setAttribute('disabled', 'false');
    this.content.removeEventListener('load', plLoadHandler, true);
    //goQuitApplication();
  } catch(e) {
    alert(e);
  }
}

/* Returns array */
function plLoadURLsFromFile() {
  try {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    
    var fp = Components.classes["@mozilla.org/filepicker;1"]
                 .createInstance(nsIFilePicker);
    fp.init(window, "Dialog Title", nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText);
    
    var rv = fp.show();
    if (rv == nsIFilePicker.returnOK)
    {
      var file = fp.file;
      var data = "";
      var fstream = 
                  Components.classes["@mozilla.org/network/file-input-stream;1"]
                  .createInstance(Components.interfaces.nsIFileInputStream);
      var sstream = Components.classes["@mozilla.org/scriptableinputstream;1"]
                .createInstance(Components.interfaces.nsIScriptableInputStream);
      fstream.init(file, -1, 0, 0);
      sstream.init(fstream); 
      
      var str = sstream.read(4096);
      while (str.length > 0) {
        data += str;
        str = sstream.read(4096);
      }
      
      sstream.close();
      fstream.close();
      var p = data.split("\n");
      // discard result of final split (EOF)
      p.pop()
      return p;
    }
  } catch (e) {
    alert(e);
  }
}
