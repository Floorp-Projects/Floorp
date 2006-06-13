/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.TabPane");
dojo.provide("dojo.widget.html.TabPane");
dojo.provide("dojo.widget.Tab");
dojo.provide("dojo.widget.html.Tab");

dojo.require("dojo.widget.*");
dojo.require("dojo.widget.LayoutPane");
dojo.require("dojo.event.*");
dojo.require("dojo.html");
dojo.require("dojo.style");

//////////////////////////////////////////
// TabPane -- a set of Tabs
//////////////////////////////////////////
dojo.widget.html.TabPane = function() {
	dojo.widget.html.LayoutPane.call(this);
}
dojo.inherits(dojo.widget.html.TabPane, dojo.widget.html.LayoutPane);

dojo.lang.extend(dojo.widget.html.TabPane, {
	widgetType: "TabPane",

	// Constructor arguments
	labelPosition: "top",
	useVisibility: false,		// true-->use visibility:hidden instead of display:none


	templateCssPath: dojo.uri.dojoUri("src/widget/templates/HtmlTabPane.css"),

	selectedTab: "",		// initially selected tab (widgetId)

	fillInTemplate: function(args, frag) {
		dojo.widget.html.TabPane.superclass.fillInTemplate.call(this, args, frag);
		
		dojo.style.insertCssFile(this.templateCssPath, null, true);
		dojo.html.prependClass(this.domNode, "dojoTabPane");
	},

	postCreate: function(args, frag) {
		// Create <ul> with special formatting to store all the tab labels
		// TODO: set "bottom" css tag if label is on bottom
		this.ul = document.createElement("ul");
		dojo.html.addClass(this.ul, "tabs");
		dojo.html.addClass(this.ul, this.labelPosition);

		// Load all the tabs, creating a label for each one
		for(var i=0; i<this.children.length; i++){
			this._setupTab(this.children[i]);
		}
		dojo.widget.html.TabPane.superclass.postCreate.call(this, args, frag);

		// Put tab labels in a panel on the top (or bottom)
		this.filterAllowed(this, 'labelPosition', ['top', 'bottom']);
		this.labelPanel = dojo.widget.createWidget("LayoutPane", {layoutAlign: this.labelPosition});
		this.labelPanel.domNode.appendChild(this.ul);
		dojo.widget.html.TabPane.superclass.addChild.call(this, this.labelPanel);

		// workaround CSS loading race condition bug
		dojo.lang.setTimeout(this, this.onResized, 50);
	},

	addChild: function(child, overrideContainerNode, pos, ref, insertIndex){
		this._setupTab(child);
		dojo.widget.html.TabPane.superclass.addChild.call(this,child, overrideContainerNode, pos, ref, insertIndex);
	},

	_setupTab: function(tab){
		tab.layoutAlign = "client";
		tab.domNode.style.display="none";
		dojo.html.prependClass(tab.domNode, "dojoTabPanel");

		// Create label
		tab.li = document.createElement("li");
		var span = document.createElement("span");
		span.innerHTML = tab.label;
		dojo.html.disableSelection(span);
		tab.li.appendChild(span);
		this.ul.appendChild(tab.li);
		
		var self = this;
		dojo.event.connect(tab.li, "onclick", function(){ self.selectTab(tab); });
		
		if(!this.selectedTabWidget || this.selectedTab==tab.widgetId || tab.selected){
			this.selectedTabWidget=tab;
		}
	},

	selectTab: function(tab) {
		// Deselect old tab and select new one
		if (this.selectedTabWidget) {
			this._hideTab(this.selectedTabWidget);
		}
		this.selectedTabWidget = tab;
		this._showTab(tab);
	},
	
	_showTab: function(tab) {
		dojo.html.addClass(tab.li, "current");
		tab.selected=true;
		if ( this.useVisibility && !dojo.render.html.ie ) {
			tab.domNode.style.visibility="visible";
		} else {
			tab.show();
		}
	},

	_hideTab: function(tab) {
		dojo.html.removeClass(tab.li, "current");
		tab.selected=false;
		if( this.useVisibility ){
			tab.domNode.style.visibility="hidden";
		}else{
			tab.hide();
		}
	},

	onResized: function() {
		// Display the selected tab
		if(this.selectedTabWidget){
			this.selectTab(this.selectedTabWidget);
		}
		dojo.widget.html.TabPane.superclass.onResized.call(this);
	}
});
dojo.widget.tags.addParseTreeHandler("dojo:TabPane");

// These arguments can be specified for the children of a TabPane.
// Since any widget can be specified as a TabPane child, mix them
// into the base widget class.  (This is a hack, but it's effective.)
dojo.lang.extend(dojo.widget.Widget, {
	label: "",
	selected: false	// is this tab currently selected?
});

// Deprecated class.  TabPane can take any widget as input.
// Use ContentPane, LayoutPane, etc.
dojo.widget.html.Tab = function() {
	dojo.widget.html.LayoutPane.call(this);
}
dojo.inherits(dojo.widget.html.Tab, dojo.widget.html.LayoutPane);
dojo.lang.extend(dojo.widget.html.Tab, {
	widgetType: "Tab"
});
dojo.widget.tags.addParseTreeHandler("dojo:Tab");

