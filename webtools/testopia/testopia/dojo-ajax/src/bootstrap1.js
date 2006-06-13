/**
* @file bootstrap1.js
*
* bootstrap file that runs before hostenv_*.js file.
*
* @author Copyright 2004 Mark D. Anderson (mda@discerning.com)
* @author Licensed under the Academic Free License 2.1 http://www.opensource.org/licenses/afl-2.1.php
*
* $Id: bootstrap1.js,v 1.2 2006/07/28 22:32:35 ghendricks%novell.com Exp $
*/

/**
 * The global djConfig can be set prior to loading the library, to override
 * certain settings.  It does not exist under dojo.* so that it can be set
 * before the dojo variable exists. Setting any of these variables *after* the
 * library has loaded does nothing at all. The variables that can be set are
 * as follows:
 */

/**
 * dj_global is an alias for the top-level global object in the host
 * environment (the "window" object in a browser).
 */
var dj_global = this; //typeof window == 'undefined' ? this : window;

/**
 *  True if name is defined, either on obj or globally if obj is false
 *  Note that 'defined' and 'exists' are not the same concept. 
 */
function dj_undef(name, obj){
	if(!obj){ obj = dj_global; }
	// exception if obj is not an Object
	return (typeof obj[name] == "undefined");
}

/**
 * djConfig is the global configuration object 
 */
if(dj_undef("djConfig")){ 
	var djConfig = {}; 
}

/**
 * dojo is the root variable of (almost all) our public symbols.
 */
if(dj_undef("dojo")){ 
	var dojo = {}; 
}

dojo.version = {
	major: 0, minor: 2, patch: 2, flag: "+",
	revision: Number("$Rev: 3802 $".match(/[0-9]+/)[0]),
	toString: function(){
		with(dojo.version){
			return major + "." + minor + "." + patch + flag + " (" + revision + ")";
		}
	}
}

/**
 * get 'obj[name]' if it is defined, otherwise create as Object if 'create' is true, otherwise return null
 * caveat: 'defined' and 'exists' are not the same concept
 */
dojo.evalProp = function(name, obj, create){
	return (obj && !dj_undef(name, obj) ? obj[name] : (create ? (obj[name]={}) : undefined));
}

/**
 * Parse a reference specified as a string descriptor into an object reference and a property name.
 */
dojo.parseObjPath = function(objpath, context, create){
	var obj = (context ? context : dj_global);
	var names = objpath.split('.');
	var prop = names.pop();
	for (var i=0,l=names.length;i<l && obj;i++){
		obj = dojo.evalProp(names[i], obj, create);
	}
	return {obj: obj, prop: prop};
}

/*
 * evaluate a string like "A.B" without using eval.
 */
dojo.evalObjPath = function(objpath, create){
	if(typeof objpath != "string"){ 
		return dj_global; 
	}
	// fast path for no periods
	if(objpath.indexOf('.') == -1){
		return dojo.evalProp(objpath, dj_global, create);
	}
	with (dojo.parseObjPath(objpath, dj_global, create)){
		return dojo.evalProp(prop, obj, create);
	}	
}

// ****************************************************************
// global public utils
// ****************************************************************

/*
 * utility to print an Error. 
 * TODO: overriding Error.prototype.toString won't accomplish this?
 * ... since natively generated Error objects do not always reflect such things?
 */
dojo.errorToString = function(excep){
	return ((!dj_undef("message", excep)) ? excep.message : (dj_undef("description", excep) ? excep : excep.description ));
}

/**
* Throws an Error object given the string err. For now, will also do a println
* to the user first.
*/
dojo.raise = function(message, excep){
	if(excep){
		message = message + ": "+dojo.errorToString(excep);
	}
	var he = dojo.hostenv;
	if((!dj_undef("hostenv", dojo))&&(!dj_undef("println", dojo.hostenv))){ 
		dojo.hostenv.println("FATAL: " + message);
	}
	throw Error(message);
}

//Stub functions so things don't break.
dojo.debug = function(){}
dojo.debugShallow = function(obj){}
dojo.profile = { start: function(){}, end: function(){}, stop: function(){}, dump: function(){} };

/**
 * We put eval() in this separate function to keep down the size of the trapped
 * evaluation context.
 *
 * Note that:
 * - JSC eval() takes an optional second argument which can be 'unsafe'.
 * - Mozilla/SpiderMonkey eval() takes an optional second argument which is the
 *   scope object for new symbols.
*/
function dj_eval(s){ return dj_global.eval ? dj_global.eval(s) : eval(s); }


/**
 * Convenience for throwing an exception because some function is not
 * implemented.
 */
dojo.unimplemented = function(funcname, extra){
	// FIXME: need to move this away from dj_*
	var mess = "'" + funcname + "' not implemented";
	if((!dj_undef(extra))&&(extra)){ mess += " " + extra; }
	dojo.raise(mess);
}

/**
 * Convenience for informing of deprecated behaviour.
 */
dojo.deprecated = function(behaviour, extra, removal){
	var mess = "DEPRECATED: " + behaviour;
	if(extra){ mess += " " + extra; }
	if(removal){ mess += " -- will be removed in version: " + removal; }
	dojo.debug(mess);
}

/**
 * Does inheritance
 */
dojo.inherits = function(subclass, superclass){
	if(typeof superclass != 'function'){ 
		dojo.raise("dojo.inherits: superclass argument ["+superclass+"] must be a function (subclass: [" + subclass + "']");
	}
	subclass.prototype = new superclass();
	subclass.prototype.constructor = subclass;
	subclass.superclass = superclass.prototype;
	// DEPRICATED: super is a reserved word, use 'superclass'
	subclass['super'] = superclass.prototype;
}

// an object that authors use determine what host we are running under
dojo.render = (function(){

	function vscaffold(prefs, names){
		var tmp = {
			capable: false,
			support: {
				builtin: false,
				plugin: false
			},
			prefixes: prefs
		};
		for(var x in names){
			tmp[x] = false;
		}
		return tmp;
	}

	return {
		name: "",
		ver: dojo.version,
		os: { win: false, linux: false, osx: false },
		html: vscaffold(["html"], ["ie", "opera", "khtml", "safari", "moz"]),
		svg: vscaffold(["svg"], ["corel", "adobe", "batik"]),
		vml: vscaffold(["vml"], ["ie"]),
		swf: vscaffold(["Swf", "Flash", "Mm"], ["mm"]),
		swt: vscaffold(["Swt"], ["ibm"])
	};
})();

// ****************************************************************
// dojo.hostenv methods that must be defined in hostenv_*.js
// ****************************************************************

/**
 * The interface definining the interaction with the EcmaScript host environment.
*/

/*
 * None of these methods should ever be called directly by library users.
 * Instead public methods such as loadModule should be called instead.
 */
dojo.hostenv = (function(){

	// default configuration options
	var config = {
		isDebug: false,
		allowQueryConfig: false,
		baseScriptUri: "",
		baseRelativePath: "",
		libraryScriptUri: "",
		iePreventClobber: false,
		ieClobberMinimal: true,
		preventBackButtonFix: true,
		searchIds: [],
		parseWidgets: true
	};

	if (typeof djConfig == "undefined") { djConfig = config; }
	else {
		for (var option in config) {
			if (typeof djConfig[option] == "undefined") {
				djConfig[option] = config[option];
			}
		}
	}

	return {
		name_: '(unset)',
		version_: '(unset)',

		/**
		 * Return the name of the hostenv.
		 */
		getName: function(){ return this.name_; },

		/**
		* Return the version of the hostenv.
		*/
		getVersion: function(){ return this.version_; },

		/**
		 * Read the plain/text contents at the specified uri.  If getText() is
		 * not implemented, then it is necessary to override loadUri() with an
		 * implementation that doesn't rely on it.
		 */
		getText: function(uri){
			dojo.unimplemented('getText', "uri=" + uri);
		}
	};
})();

/**
 * Return the base script uri that other scripts are found relative to.
 * It is either the empty string, or a non-empty string ending in '/'.
 */
dojo.hostenv.getBaseScriptUri = function(){
	if(djConfig.baseScriptUri.length){ 
		return djConfig.baseScriptUri;
	}
	var uri = new String(djConfig.libraryScriptUri||djConfig.baseRelativePath);
	if (!uri) { dojo.raise("Nothing returned by getLibraryScriptUri(): " + uri); }

	var lastslash = uri.lastIndexOf('/');
	djConfig.baseScriptUri = djConfig.baseRelativePath;
	return djConfig.baseScriptUri;
}
