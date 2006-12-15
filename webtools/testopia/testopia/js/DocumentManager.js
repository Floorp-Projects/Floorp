dojo.require("dojo.widget.Editor2");  
dojo.require("dojo.widget.Editor2Plugin.DialogCommands");  
dojo.require("dojo.widget.*");  
dojo.require("dojo.dom.*");  
dojo.require("dojo.event.*");  
function DocumentManager(/*string*/ prefix, /*string*/action, /*array of strings*/otherListeners, /*bool*/ directEdit) {
    if (prefix.length > 0) {
	this.prefix = prefix
	if ( arguments.length > 1 ) {
	    this.action = action;
	} else {
	    this.action = "Commit";
	}
	if ( arguments.length > 2 ) {
	    this.others = otherListeners;
	} else {
	    this.others = [];
	}
	if ( arguments.length > 3 ) {
	    this.directEdit = directEdit;
	} else {
	    this.directEdit = false;
	}
	this.div;
	this.widget;
	this.helpPar;
	dojo.addOnLoad( dojo.lang.hitch(this, "handleLoad" ) );
    }
}
new DocumentManager("");
DocumentManager.prototype.setDirectEdit = function( bool ) {
    this.directEdit = bool;
}
DocumentManager.prototype.handleLoad = function() {
    this.div = document.getElementById(this.prefix + "Div");
    var f = dojo.dom.getFirstAncestorByTag(this.div, "form");
    dojo.event.connect(f, "onsubmit", this, "handleSubmit");
    this.helpPar = document.createElement("p");
    this.helpPar.setAttribute("style", "text-align: right;cursor: default");
    dojo.dom.insertAfter(this.helpPar, this.div);
    if ( this.directEdit ) {
	return this.gotoEditMode();
    } else {
	var l = document.createElement("a");
	l.setAttribute("title", "Click here to edit, or double-click document text.");
	var t = document.createTextNode("Edit Document");
	l.appendChild(t);
	this.helpPar.appendChild(l);
	dojo.event.connect(l, "onclick", this, "gotoEditMode");
	dojo.event.connect(this.div, "ondblclick", this, "gotoEditMode");
	for (i = 0; i < this.others.length; ++i) {
	    var n = document.getElementById(this.others[i]);
	    dojo.event.connect(n, "ondblclick", this, "gotoEditMode");
	}
    }
}
DocumentManager.prototype.handleSubmit = function (evt) {
    var el = document.createElement("input");
    el.setAttribute("id", this.prefix);
    el.setAttribute("name", this.prefix);
    el.setAttribute("type", "hidden");
    var value;
    if (this.widget) {
	value = this.widget.getEditorContent();
    } else {
	value = "";
	for (i = 0; i < this.div.childNodes.length; ++i) {
	    value = value + dojo.dom.innerXML(this.div.childNodes[i]);
	}
    }
    el.setAttribute("value", value);
    dojo.dom.insertAfter(el, this.div);
}
DocumentManager.prototype.gotoEditMode = function(evt) {
    if (this.widget) {
	return;
    } else {
	dojo.dom.removeChildren(this.helpPar);
	var b = document.createElement("input");
	b.setAttribute("type", "SUBMIT");
	b.setAttribute("name", "action");
	b.setAttribute("id", "action");
	b.setAttribute("class", "tr_button");
	b.setAttribute("value", this.action);
	this.helpPar.appendChild(b);
	this.widget = dojo.widget.createWidget("Editor2", {}, this.div);
    }
}
