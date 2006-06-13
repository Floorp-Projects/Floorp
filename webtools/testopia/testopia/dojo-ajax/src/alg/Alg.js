/*
	Copyright (c) 2004-2005, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.alg.Alg");
dojo.require("dojo.lang");
dj_deprecated("dojo.alg.Alg is deprecated, use dojo.lang instead");

dojo.alg.find = function(arr, val){ return dojo.lang.find(arr, val); }

dojo.alg.inArray = function(arr, val){
	return dojo.lang.inArray(arr, val);
}
dojo.alg.inArr = dojo.alg.inArray; // for backwards compatibility

dojo.alg.getNameInObj = function(ns, item){
	return dojo.lang.getNameInObj(ns, item);
}

// is this the right place for this?
dojo.alg.has = function(obj, name){
	return dojo.lang.has(obj, name);
}

dojo.alg.forEach = function(arr, unary_func, fix_length){
	return dojo.lang.forEach(arr, unary_func, fix_length);
}

dojo.alg.for_each = dojo.alg.forEach; // burst compat

dojo.alg.map = function(arr, obj, unary_func){
	return dojo.lang.map(arr, obj, unary_func);
}

dojo.alg.tryThese = function(){
	return dojo.lang.tryThese.apply(dojo.lang, arguments);
}

dojo.alg.delayThese = function(farr, cb, delay, onend){
	return dojo.lang.delayThese.apply(dojo.lang, arguments);
}

dojo.alg.for_each_call = dojo.alg.map; // burst compat
