/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.Tooltip");
dojo.require("dojo.widget.Widget");

dojo.widget.tags.addParseTreeHandler("dojo:tooltip");

dojo.widget.Tooltip = function(){
	dojo.widget.Widget.call(this);

	this.widgetType = "Tooltip";
	this.isContainer = true;
}
dojo.inherits(dojo.widget.Tooltip, dojo.widget.Widget);

dojo.requireAfterIf("html", "dojo.widget.html.Tooltip");
