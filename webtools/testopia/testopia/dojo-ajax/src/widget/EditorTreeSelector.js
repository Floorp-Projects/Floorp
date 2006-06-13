/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/


dojo.provide("dojo.widget.EditorTreeSelector");

dojo.require("dojo.widget.Container");
dojo.require("dojo.widget.Tree");

dojo.widget.tags.addParseTreeHandler("dojo:EditorTreeSelector");


dojo.widget.EditorTreeSelector = function() {
	dojo.widget.HtmlWidget.call(this);
}

dojo.inherits(dojo.widget.EditorTreeSelector, dojo.widget.HtmlWidget);


dojo.lang.extend(dojo.widget.EditorTreeSelector, {
	widgetType: "EditorTreeSelector",
	selectedNode: null

});



