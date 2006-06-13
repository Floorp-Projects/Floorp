/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.debug.Firebug");

dojo.debug.firebug = function(){}
dojo.debug.firebug.printfire = function () {
	printfire=function(){}
	printfire.args = arguments;
	var ev = document.createEvent("Events");
	ev.initEvent("printfire", false, true);
	dispatchEvent(ev);
}

if (dojo.render.html.moz) {
	dojo.hostenv.println=dojo.debug.firebug.printfire;
}
