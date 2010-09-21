var editor = {
  index : 0,

  tabs : [],

  currentTab : null,

  tempCount : 0,

  resize : function(width, height) {
    if(width)
      this.width = width;
    if(height)
      this.height = height;

    if(this.currentTab) {
      $(this.currentTab.editorElement).width(this.width);
      $(this.currentTab.editorElement).height(this.height);
      this.currentTab.editorEnv.dimensionsChanged();
    }
  },

  switchTab : function(index) {
    if(index == undefined)
      index = this.tabs.length - 1;
    if(index < 0)
      return;

    var tabSelect = document.getElementById("editor-tab-select");
    tabSelect.selectedIndex = index;

    if(this.currentTab)
      $(this.currentTab.editorElement).hide();

    this.index = index;
    this.currentTab = this.tabs[index];
    this.resize();
    $(this.currentTab.editorElement).show();
    this.currentTab.editor.focus = true;
    this.currentTab.editorEnv.dimensionsChanged()
  },

  closeCurrentTab : function() {
    this.currentTab.destroy();
    this.currentTab = '';
    this.tabs.splice(this.index, 1);

    var tabSelect = document.getElementById("editor-tab-select");
    var option = tabSelect.getElementsByTagName("option")[this.index];
    tabSelect.removeChild(option);

    this.switchTab();
  },

  getTabForFile : function(filename) {
    for(var i = 0; i < this.tabs.length; i++) {
      if(this.tabs[i].filename == filename)
        return i;
    }
    return -1;
  },

  openNew : function(content, filename) {
    if(!filename) {
      this.tempCount++;
      filename = utils.tempfile("mozmill.utils.temp" + this.tempCount).path;
      var tabName = "temp " + this.tempCount;
    }
    else
      var tabName = getBasename(filename);

    var option = $('<option></option>').val(this.tabs.length - 1).html(tabName);
    $("#editor-tab-select").append(option);

    var newTab = new editorTab(content, filename);
    this.tabs.push(newTab);

    // will switch to tab when it has loaded
  },

  getContent : function() {
    return this.currentTab.getContent();
  },

  setContent : function(content) {
    this.currentTab.setContent(content);
  },

  getFilename : function() {
    return this.currentTab.filename;
  },

  showFilename : function(filename) {
    $("#editor-tab-select option").eq(editor.index).html(filename);
  },

  changeFilename : function(filename) {
    this.currentTab.filename = filename;
    this.showFilename(getBasename(filename));
  },

  onFileChanged : function() {
    var selected = $("#editor-tab-select :selected");
    selected.html(selected.html().replace("*", "")).append("*");

    // remove listener until saving to prevent typing slow down
    editor.currentTab.editor.textChanged.remove(editor.onFileChanged);
  },

  onFileSaved : function() {
    var selected = $("#editor-tab-select :selected");
    selected.html(selected.html().replace("*", ""));
    editor.currentTab.editor.textChanged.add(editor.onFileChanged);
  }
}


function editorTab(content, filename) {
  var elem = $("<pre></pre>").addClass("bespin").appendTo("#editors");
  elem.val(content);
  var bespinElement = elem.get(0);
  var editorObject = this;

  bespin.useBespin(bespinElement, {
    settings: {"tabstop": 4},
    syntax: "js", 
    stealFocus: true})
  .then(function(env) {
    editorObject.editorEnv = env;
    editorObject.editor = env.editor;
    env.editor.textChanged.add(editor.onFileChanged);
    editor.switchTab();
    env.settings.set("fontsize", 13);
  });

  this.editorElement = bespinElement;
  this.filename = filename;
}

editorTab.prototype = {
  setContent : function(content) {
    this.editor.value = content;
  },

  getContent : function() {
    return this.editor.value;
  },

  destroy : function() {
    this.editorElement.parentNode.removeChild(this.editorElement);
  }
}
