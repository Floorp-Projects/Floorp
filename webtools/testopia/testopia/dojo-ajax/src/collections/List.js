/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.collections.List");
dojo.require("dojo.collections.Collections");

dojo.collections.List = function(dictionary){
	dojo.deprecated("dojo.collections.List", "Use dojo.collections.Dictionary instead.");
	return new dojo.collections.Dictionary(dictionary);
}
