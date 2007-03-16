var NUM_CYCLES = 5;

var pages;
var pageIndex;
var results;
var start_time;
var end_time;
var cycle;
var report;
var running = false;

function plInit() {
  if (running) {
    return;
  }
  running = true;
  try { 
    pageIndex = 0;
    cycle = 0;
    results = new Object();
    if (! pages) {
      var file;
      try {
        file = plDefaultFile();
      } catch(e) {
        dumpLine(e);
      }
      if (! file.exists()) {
        try {
          file = plFilePicker();
        } catch(e) {
          dumpLine(e);
        }
      }
      pages = plLoadURLsFromFile(file);
    }
    if (! pages ) {
      alert('could not load URLs, quitting');
      plStop(true);
    }
    if (pages.length == 0) {
      alert('no pages to test, quitting');
      plStop(true);
    }
    report = new Report(pages);
    plLoadPage();
  } catch(e) {
    dumpLine(e);
    plStop(true);
  }
}

function plLoadPage() {
  try {
    start_time = new Date();
    p = pages[pageIndex];
    this.content = document.getElementById('contentPageloader');
    this.content.addEventListener('load', plLoadHandler, true);
    this.content.loadURI(p);
  } catch (e) {
    dumpLine(e);
    plStop(true);
  }
}

function plLoadHandler(evt) {
  if (evt.type == 'load') {
    window.setTimeout('reallyHandle()', 500);
  } else {
    dumpLine('Unknown event type: '+evt.type);
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
        dumpLine(pageName+" took "+results[pageName]);
        plReport();
        pageIndex++;
        plLoadPage();
      } catch(e) {
        dumpLine(e);
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
      dumpLine(e);
      plStop(false);
    }
}

function plStop(force) {
  try {
    if (force == false) {
      pageIndex = 0;
      results = new Object;
      if (cycle < NUM_CYCLES) {
        cycle++;
        plLoadPage();
        return;
      } else {
        dumpLine(report.getReport());
      }
    }
    this.content.removeEventListener('load', plLoadHandler, true);
  } catch(e) {
    dumpLine(e);
  }
  goQuitApplication();
}

/* Returns nsilocalfile */
function plDefaultFile() {
  try {
    const nsIIOService = Components.interfaces.nsIIOService;
    var dirService = 
      Components.classes["@mozilla.org/file/directory_service;1"].
      getService(Components.interfaces.nsIProperties);
    var profileDir = dirService.get("ProfD", 
                     Components.interfaces.nsILocalFile);
    var file = Components.classes["@mozilla.org/file/local;1"].
               createInstance(Components.interfaces.nsILocalFile);
    var path = profileDir.path;
    file.initWithPath(path);
    file.append("urls.txt");
    dumpLine('will attempt to load default file '+file.path);
    return file;
  } catch (e) {
    dumpLine(e);
  }
}


/* Returns nsifile */
function plFilePicker() {
  try {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    
    var fp = Components.classes["@mozilla.org/filepicker;1"]
                 .createInstance(nsIFilePicker);
    fp.init(window, "Dialog Title", nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterAll | nsIFilePicker.filterText);
    var rv = fp.show();
    if (rv == nsIFilePicker.returnOK) {
      return fp.file;
    }
  } catch (e) {
    dumpLine(e);
  }
}
    
/* Returns array */
function plLoadURLsFromFile(file) {
  try {
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
  } catch (e) {
    dumpLine(e);
  }
}

function dumpLine(str) {
  dump(str);
  dump("\n");
}
