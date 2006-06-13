/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.html.Container");

dojo.require("dojo.widget.*");
dojo.require("dojo.widget.Container");

dojo.widget.html.Container = function(){
	dojo.widget.HtmlWidget.call(this);
}

dojo.inherits(dojo.widget.html.Container, dojo.widget.HtmlWidget);

dojo.lang.extend(dojo.widget.html.Container, {
	widgetType: "Container",

	isContainer: true,
	containerNode: null,
	domNode: null,

	onResized: function() {
		// Clients should override this function to do special processing,
		// then call this.notifyChildrenOfResize() to notify children of resize
		this.notifyChildrenOfResize();
	},
	
	notifyChildrenOfResize: function() {
		for(var i=0; i<this.children.length; i++) {
			var child = this.children[i];
			//dojo.debug(this.widgetId + " resizing child " + child.widgetId);
			if ( child.onResized ) {
				child.onResized();
			}
		}
	}
});

dojo.widget.tags.addParseTreeHandler("dojo:Container");
