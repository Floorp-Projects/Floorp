/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.EditorTreeContextMenu");
dojo.provide("dojo.widget.EditorTreeMenuItem");

dojo.require("dojo.event.*");
dojo.require("dojo.dnd.*");
dojo.require("dojo.fx.html");
dojo.require("dojo.io.*");
dojo.require("dojo.widget.Container");
dojo.require("dojo.widget.Menu2");


dojo.widget.tags.addParseTreeHandler("dojo:EditorTreeContextMenu");
dojo.widget.tags.addParseTreeHandler("dojo:EditorTreeMenuItem");



dojo.widget.EditorTreeContextMenu = function() {
	dojo.widget.PopupMenu2.call(this);
	this.widgetType = "EditorTreeContextMenu";

	this.eventNames =  {
		open: ""
	};


}


dojo.inherits(dojo.widget.EditorTreeContextMenu, dojo.widget.PopupMenu2);

dojo.lang.extend(dojo.widget.EditorTreeContextMenu, {

	initialize: function(args, frag) {

		var result = dojo.widget.PopupMenu2.prototype.initialize.apply(this, arguments);

		if (args['eventNaming'] == "default" || args['eventnaming'] == "default" ) { // IE || FF
			for (eventName in this.eventNames) {
				this.eventNames[eventName] = this.widgetId+"/"+eventName;
			}
		}

		return result;

	},

	postCreate: function(){
		var result = dojo.widget.PopupMenu2.prototype.postCreate.apply(this, arguments);

		var subItems = this.getChildrenOfType('EditorTreeMenuItem')

		for(var i=0; i<subItems.length; i++) {
			dojo.event.topic.subscribe(this.eventNames.open, subItems[i], "menuOpen")
		}


		return result;
	},

	open: function(x, y, parentMenu, explodeSrc){

		var result = dojo.widget.PopupMenu2.prototype.open.apply(this, arguments);

		/* publish many events here about structural changes */
		dojo.event.topic.publish(this.eventNames.open, { menu:this });

		return result;
	}



});






dojo.widget.EditorTreeMenuItem = function() {
	dojo.widget.MenuItem2.call(this);
	this.widgetType = "EditorTreeMenuItem";

}


dojo.inherits(dojo.widget.EditorTreeMenuItem, dojo.widget.MenuItem2);

dojo.lang.extend(dojo.widget.EditorTreeMenuItem, {
	for_folders: true,


	getTreeNode: function() {
		var menu = this;

		while (menu.widgetType != 'EditorTreeContextMenu') {
			menu = menu.parent;
		}

		var source = menu.getTopOpenEvent().target;

		while (!source.treeNode && source.tagName != 'body') {
			source = source.parentNode;
		}
		if (source.tagName == 'body') {
			dojo.raise("treeNode not detected");
		}
		var treeNode = dojo.widget.manager.getWidgetById(source.treeNode);

		return treeNode;
	},

	menuOpen: function(message) {
		var treeNode = this.getTreeNode();

		/* manage for folders status */
		if (!treeNode.isFolder && this.for_folders==false) {
			this.setDisabled(true);
		}
		else {
			this.setDisabled(false);
		}
	}



});