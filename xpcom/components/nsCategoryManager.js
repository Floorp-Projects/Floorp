/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

function CategoryManager() {
    this.categories = { };
}
function NYI () { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; }

var proto = CategoryManager.prototype;
var nsICategoryManager = Components.interfaces.nsICategoryManager;

proto.getCategoryEntry = function (category, entry) {
    var cat = this.categories[category];
    if (!cat)
	return null;
    if ("override" in cat)
	return cat.override.getCategoryEntry(category, entry);
    var table = cat.table;
    if (entry in table)
	return table[entry];
    if ("fallback" in cat) {
        dump("\n--fallback: " + cat.fallback + "\n");
	return cat.fallback.getCategoryEntry(category, entry);
    }
    return null;
};

proto.getCategoryEntryRaw = function (category, entry) {
    var table;
    var cat = this.categories[category];
    if (!cat ||
	(!entry in (table = cat.table)))
	return null;
    if (entry in table)
	return table[entry];
    if ("fallback" in cat)
	return cat.fallback.getCategoryEntry(category, entry);
    return null;
}

proto.addCategoryEntry = function (category, entry, value, persist, replace) {
    if (!(category in this.categories)) {
        dump("aCE: creating category \"" + category + "\"\n");
	this.categories[category] = { name:category, table:{ } };
    }
    var table = this.categories[category].table;
    var oldValue;
    if (entry in table) {
	if (!replace)
	    throw Components.results.NS_ERROR_INVALID_ARG;
	oldValue = table[entry];
    } else {
	oldValue = null;
    }
    table[entry] = value;
    if (persist)
	// need registry
	throw Components.results.NS_ERROR_NOT_IMPLEMENTED;
    return oldValue;
}

proto.enumerateCategory = NYI;
proto.getCategoryContents = NYI;

proto.registerCategoryHandler = function(category, handler, mode) {
    dump("nCM: handler.getCategoryEntry: " + handler.getCategoryEntry + "\n");
    dump("typeof handler: " + typeof handler + "\n");
    /*
    dump('rCH: "' + category + '".' +
         (mode == nsICategoryManager.fallback ? "fallback" :
          (mode == nsICategoryManager.override ? "override" : 
           mode)) + " = " + handler + "\n");
    */
    if (!(category in this.categories)) {
        dump("rCH: creating category \"" + category + "\"\n");
	this.categories[category] = { table:{ } };
    }
    var old;
    var category = this.categories[category];
    if (mode == nsICategoryManager.override) {
	old = category.override || null;
	category.override = handler;
    } else if (mode == nsICategoryManager.fallback) {
	old = category.fallback || null;
	category.fallback = handler;
    } else {
        dump("\nregisterCategoryHandler: illegal mode " + mode + "\n\n");
        throw Components.results.NS_ERROR_INVALID_ARG;
    }
    return old;
};
proto.unregisterCategoryHandler = NYI;

var module = {
    RegisterSelf:function (compMgr, fileSpec, location, type) {
	compMgr.registerComponentWithType(this.myCID,
					  "Category Manager",
					  "mozilla.categorymanager.1",
					  fileSpec, location, true, true, 
					  type);
    },
    GetClassObject:function (compMgr, cid, iid) {
	if (!cid.equals(this.myCID))
	    throw Components.results.NS_ERROR_NO_INTERFACE;
	
	if (!iid.equals(Components.interfaces.nsIFactory))
	    throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

	return this.myFactory;
    },
    myCID:Components.ID("{16d222a6-1dd2-11b2-b693-f38b02c021b2}"),
    myFactory:{
	CreateInstance:function (outer, iid) {
	    if (outer != null)
	        throw Components.results.NS_ERROR_NO_AGGREGATION;
	    
	    if (!(iid.equals(nsICategoryManager) ||
		  iid.equals(Components.interfaces.nsISupports)))
	        throw Components.results.NS_ERROR_INVALID_ARG;
	    return new CategoryManager();
	}
    }
};

function NSGetModule(compMgr, fileSpec) { return module; }
