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
    this.loadRegistryData(this);
}

function NYI() { throw Components.results.NS_ERROR_NOT_IMPLEMENTED; }

var proto = CategoryManager.prototype;
var nsICategoryManager = Components.interfaces.nsICategoryManager;
var categoriesKey;              // registry key for category data
var registry;                   // registry handle

proto.getCategoryEntry = function (category, entry) {
    var cat = this.categories[category];
    if (!cat)
        return null;
    if ("override" in cat)
        return cat.override.getCategoryEntry(category, entry);
    var table = cat.table;
    if (entry in table)
        return table[entry];
    if ("fallback" in cat)
        return cat.fallback.getCategoryEntry(category, entry);
    return null;
};

proto.getCategoryEntryRaw = function (category, entry) {
    var table;
    var cat = this.categories[category];
    if (!(cat && entry in (table = cat.table)))
        return null;
    if (entry in table)
        return table[entry];
    if ("fallback" in cat)
        return cat.fallback.getCategoryEntry(category, entry);
    return null;
}

function addEntryToRegistry(category, entry, value)
{
    var categorySubtree;
    try {
        categorySubtree = registry.getSubtreeRaw(categoriesKey, category);
    } catch (e if
             e instanceof Components.interfaces.nsIXPCException &&
             e.result === 0x80510003 /* XXX NS_ERROR_REG_NOT_FOUND */) { 
        categorySubtree = registry.addSubtreeRaw(categoriesKey, category);
    }
    registry.setString(categorySubtree, entry, value);
}

proto.addCategoryEntry = function (category, entry, value, persist, replace) {
    if (!(category in this.categories))
        this.categories[category] = { name:category, table:{ } };
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
        addEntryToRegistry(category, entry, value);
    return oldValue;
}

function deleteEntryFromRegistry(category, entry)
{
    try {
        var categorySubtree = registry.getSubtreeRaw(categoriesKey, category);
        registry.deleteValue(categorySubtree, entry);
    } catch (e if
             e instanceof Components.interfaces.nsIXPCException &&
             e.result === 0x80510003 /* XXX NS_ERROR_REG_NOT_FOUND */) {
        return false;
    }
    return true;
}

proto.deleteCategoryEntry = function (category, entry, persist) {
    if (!(category in this.categories))
        return null;
    var table = this.categories[category].table;
    var old = table[entry];
    delete table[entry];
    if (persist)
        deleteEntryFromRegistry(category, entry);
    return old;
};

function deleteCategoryFromRegistry(category) {
    try {
        registry.removeSubtreeRaw(categoriesKey, category);
    } catch (e if
             e instanceof Components.interfaces.nsIXPCException &&
             e.result === 0x80510003 /* XXX NS_ERROR_REG_NOT_FOUND */) {
        return false;
    }
    return true;
}        

proto.deleteCategory = function (category, persist) {
   delete this.categories[category];
   /*
    * Don't check for delete success above, because persist means to
    * remove from registry, even if someone has already done a
    * non-persistent removal before.
    */
   if (persist)
       deleteCategoryFromRegistry(category);
};

proto.enumerateCategory = function (category) {
    return new CategoryEnumerator(this.categories[category] || { });
}

proto.getCategoryContents = NYI;

proto.registerCategoryHandler = function(category, handler, mode) {
    if (!(category in this.categories))
        this.categories[category] = { table:{ } };

    var old;
    var category = this.categories[category];
    if (mode == nsICategoryManager.OVERRIDE) {
        old = category.override || null;
        category.override = handler;
    } else if (mode == nsICategoryManager.FALLBACK) {
        old = category.fallback || null;
        category.fallback = handler;
    } else {
        throw Components.results.NS_ERROR_INVALID_ARG;
    }
    /* XXX shaver: store CID in the registry, repopulate at startup? */
    return old;
};

proto.unregisterCategoryHandler = NYI;

proto.loadRegistryData = function() {
    var nsIRegistry = Components.interfaces.nsIRegistry;
    registry = Components.classes["component://netscape/registry"].
        createInstance(nsIRegistry);
    registry.openWellKnownRegistry(nsIRegistry.ApplicationComponentRegistry);
    try {
        categoriesKey =
            registry.getSubtree(nsIRegistry.Common,
                                "Software/Mozilla/XPCOM/Categories");
    } catch (e if
             e instanceof Components.interfaces.nsIXPCException &&
             e.result === 0x80510003 /* XXX NS_ERROR_REG_NOT_FOUND */) {
        dump("creating Categories registry point\n");
        categoriesKey =
            registry.addSubtree(nsIRegistry.Common,
                                "Software/Mozilla/XPCOM/Categories");
        return;
    }

    var en = registry.enumerateSubtrees(categoriesKey);

    /*
     * Waterson's right: the nsIEnumerator interface is basically
     * complete crap.  There is no way to do anything, really, without
     * gobs of exceptions and other nonsense, because apparently boolean
     * out parameters are rocket science.
     *
     * And don't get me started about COMFALSE.  (I'm sure this seemed like
     * a good idea at the time, but it really sucks right now.)
     *
     * Do not look directly at the following code.
     */
    try { en.first(); } catch (e) { return; }

    while (en.isDone(), Components.lastResult != Components.results.NS_OK) {
        try {
            node = en.currentItem();
            node = node.QueryInterface(Components.interfaces.nsIRegistryNode);
        } catch (e if e instanceof Components.interfaces.nsIXPCException) {
            try { en.next(); } catch (e) { }
            continue;
        }
        var category = node.name;
        var catenum = registry.enumerateValues(node.key);
        try {
            catenum.first(); 
        } catch (e) {
            continue;
        }
        while(catenum.isDone(),
              Components.lastResult != Components.results.NS_OK) {
            var entry;
            try {
                entry = catenum.currentItem();
                entry = entry.QueryInterface(Components.interfaces.nsIRegistryValue);
            } catch (e if e instanceof Components.interfaces.nsIXPCException) {
                dump("not a value?\n");
                try { catenum.next(); } catch (e) { }
                continue;
            }
            try {
                var value = registry.getString(node.key, entry.name);
                this.addCategoryEntry(category, entry.name, value);
            } catch (e) {
                dump ("no " + entry.name + " in " + category + "?\n");
            }
            try { catenum.next(); } catch (e) { }
        }
        try { en.next(); } catch (e) { }
    }
}

function CategoryEnumerator(category) {
    this.contents = contents = [];
    for (var i in category.table)
        contents.push(i);
};    

CategoryEnumerator.prototype = {
    hasMoreElements: function() {
        return this.index !== this.contents.length;
    },
    
    getNext: function() {
        if (!this.hasMoreElements())
            return null;        // XXX?
        var str = Components.classes["component://netscape/supports-string"]
            .createInterface(Components.interfaces.nsISupportsString);
        str.data = this.contents[this.index++];
        return str;
    },

    index: 0
};

var module = {
    registerSelf: function (compMgr, fileSpec, location, type) {
        compMgr.registerComponentWithType(this.myCID,
                                          "Category Manager",
                                          "mozilla.categorymanager.1",
                                          fileSpec, location, true, true, 
                                          type);
    },

    getClassObject: function (compMgr, cid, iid) {
        if (!cid.equals(this.myCID))
            throw Components.results.NS_ERROR_NO_INTERFACE;
        
        if (!iid.equals(Components.interfaces.nsIFactory))
            throw Components.results.NS_ERROR_NOT_IMPLEMENTED;

        return this.myFactory;
    },

    canUnload: function () {
        dump("unloading category manager\n");
        try { registry.close(); } catch (e) { }
    },

    myCID: Components.ID("{16d222a6-1dd2-11b2-b693-f38b02c021b2}"),

    myFactory: {
        CreateInstance: function (outer, iid) {
            if (outer != null)
                throw Components.results.NS_ERROR_NO_AGGREGATION;
            
            if (!(iid.equals(nsICategoryManager) ||
                  iid.equals(Components.interfaces.nsISupports))) {
                throw Components.results.NS_ERROR_INVALID_ARG;
            }

            return new CategoryManager();
        }
    }
};

function NSGetModule(compMgr, fileSpec) { return module; }

