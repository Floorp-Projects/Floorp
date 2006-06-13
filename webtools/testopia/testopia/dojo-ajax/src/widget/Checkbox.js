/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.Checkbox");

dojo.require("dojo.widget.*");
dojo.require("dojo.event");
dojo.require("dojo.html");

dojo.widget.tags.addParseTreeHandler("dojo:Checkbox");

dojo.widget.Checkbox = function(){
	dojo.widget.Widget.call(this);
};
dojo.inherits(dojo.widget.Checkbox, dojo.widget.Widget);

dojo.lang.extend(dojo.widget.Checkbox, {
	widgetType: "Checkbox"
});

dojo.requireAfterIf("html", "dojo.widget.html.Checkbox");
