/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.widget.HslColorPicker");

dojo.require("dojo.widget.*");
dojo.require("dojo.widget.Widget");
dojo.require("dojo.graphics.color");
dojo.widget.tags.addParseTreeHandler("dojo:hslcolorpicker");

dojo.requireAfterIf(dojo.render.svg.support.builtin, "dojo.widget.svg.HslColorPicker");

dojo.widget.HslColorPicker=function(){
	dojo.widget.Widget.call(this);
	this.widgetType = "HslColorPicker";
	this.isContainer = false;
}
dojo.inherits(dojo.widget.HslColorPicker, dojo.widget.Widget);
