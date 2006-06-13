/**
* @file bootstrap1.js
*
* bootstrap file that runs before hostenv_*.js file.
*
* @author Copyright 2004 Mark D. Anderson (mda@discerning.com)
* @author Licensed under the Academic Free License 2.1 http://www.opensource.org/licenses/afl-2.1.php
*
* $Id: dojo.js.uncompressed.js,v 1.3 2006/07/28 22:32:36 ghendricks%novell.com Exp $
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

/*
 * loader.js - runs before the hostenv_*.js file. Contains all of the package loading methods.
 */

//A semi-colon is at the start of the line because after doing a build, this function definition
//get compressed onto the same line as the last line in bootstrap1.js. That list line is just a
//curly bracket, and the browser complains about that syntax. The semicolon fixes it. Putting it
//here instead of at the end of bootstrap1.js, since it is more of an issue for this file, (using
//the closure), and bootstrap1.js could change in the future.
;(function(){
	//Additional properties for dojo.hostenv
	var _addHostEnv = {
		pkgFileName: "__package__",
	
		// for recursion protection
		loading_modules_: {},
		loaded_modules_: {},
		addedToLoadingCount: [],
		removedFromLoadingCount: [],
	
		inFlightCount: 0,
	
		// FIXME: it should be possible to pull module prefixes in from djConfig
		modulePrefixes_: {
			dojo: {name: "dojo", value: "src"}
		},
	
	
		setModulePrefix: function(module, prefix){
			this.modulePrefixes_[module] = {name: module, value: prefix};
		},
	
		getModulePrefix: function(module){
			var mp = this.modulePrefixes_;
			if((mp[module])&&(mp[module]["name"])){
				return mp[module].value;
			}
			return module;
		},
	
		getTextStack: [],
		loadUriStack: [],
		loadedUris: [],
	
		//WARNING: This variable is referenced by packages outside of bootstrap: FloatingPane.js and undo/browser.js
		post_load_: false,
		
		//Egad! Lots of test files push on this directly instead of using dojo.addOnLoad.
		modulesLoadedListeners: []
	};
	
	//Add all of these properties to dojo.hostenv
	for(var param in _addHostEnv){
		dojo.hostenv[param] = _addHostEnv[param];
	}
})();

/**
 * Loads and interprets the script located at relpath, which is relative to the
 * script root directory.  If the script is found but its interpretation causes
 * a runtime exception, that exception is not caught by us, so the caller will
 * see it.  We return a true value if and only if the script is found.
 *
 * For now, we do not have an implementation of a true search path.  We
 * consider only the single base script uri, as returned by getBaseScriptUri().
 *
 * @param relpath A relative path to a script (no leading '/', and typically
 * ending in '.js').
 * @param module A module whose existance to check for after loading a path.
 * Can be used to determine success or failure of the load.
 */
dojo.hostenv.loadPath = function(relpath, module /*optional*/, cb /*optional*/){
	if((relpath.charAt(0) == '/')||(relpath.match(/^\w+:/))){
		dojo.raise("relpath '" + relpath + "'; must be relative");
	}
	var uri = this.getBaseScriptUri() + relpath;
	if(djConfig.cacheBust && dojo.render.html.capable) { uri += "?" + String(djConfig.cacheBust).replace(/\W+/g,""); }
	try{
		return ((!module) ? this.loadUri(uri, cb) : this.loadUriAndCheck(uri, module, cb));
	}catch(e){
		dojo.debug(e);
		return false;
	}
}

/**
 * Reads the contents of the URI, and evaluates the contents.
 * Returns true if it succeeded. Returns false if the URI reading failed.
 * Throws if the evaluation throws.
 * The result of the eval is not available to the caller.
 */
dojo.hostenv.loadUri = function(uri, cb){
	if(this.loadedUris[uri]){
		return;
	}
	var contents = this.getText(uri, null, true);
	if(contents == null){ return 0; }
	this.loadedUris[uri] = true;
	var value = dj_eval(contents);
	return 1;
}

// FIXME: probably need to add logging to this method
dojo.hostenv.loadUriAndCheck = function(uri, module, cb){
	var ok = true;
	try{
		ok = this.loadUri(uri, cb);
	}catch(e){
		dojo.debug("failed loading ", uri, " with error: ", e);
	}
	return ((ok)&&(this.findModule(module, false))) ? true : false;
}

dojo.loaded = function(){ }

dojo.hostenv.loaded = function(){
	this.post_load_ = true;
	var mll = this.modulesLoadedListeners;
	//Clear listeners so new ones can be added
	//For other xdomain package loads after the initial load.
	this.modulesLoadedListeners = [];
	for(var x=0; x<mll.length; x++){
		mll[x]();
	}
	dojo.loaded();
}

/*
Call styles:
	dojo.addOnLoad(functionPointer)
	dojo.addOnLoad(object, "functionName")
*/
dojo.addOnLoad = function(obj, fcnName) {
	var dh = dojo.hostenv;
	if(arguments.length == 1) {
		dh.modulesLoadedListeners.push(obj);
	} else if(arguments.length > 1) {
		dh.modulesLoadedListeners.push(function() {
			obj[fcnName]();
		});
	}

	//Added for xdomain loading. dojo.addOnLoad is used to
	//indicate callbacks after doing some dojo.require() statements.
	//In the xdomain case, if all the requires are loaded (after initial
	//page load), then immediately call any listeners.
	if(dh.post_load_ && dh.inFlightCount == 0){
		dh.callLoaded();
	}
}

dojo.hostenv.modulesLoaded = function(){
	if(this.post_load_){ return; }
	if((this.loadUriStack.length==0)&&(this.getTextStack.length==0)){
		if(this.inFlightCount > 0){ 
			dojo.debug("files still in flight!");
			return;
		}
		dojo.hostenv.callLoaded();
	}
}

dojo.hostenv.callLoaded = function(){
	if(typeof setTimeout == "object"){
		setTimeout("dojo.hostenv.loaded();", 0);
	}else{
		dojo.hostenv.loaded();
	}
}

/**
* loadModule("A.B") first checks to see if symbol A.B is defined. 
* If it is, it is simply returned (nothing to do).
*
* If it is not defined, it will look for "A/B.js" in the script root directory,
* followed by "A.js".
*
* It throws if it cannot find a file to load, or if the symbol A.B is not
* defined after loading.
*
* It returns the object A.B.
*
* This does nothing about importing symbols into the current package.
* It is presumed that the caller will take care of that. For example, to import
* all symbols:
*
*    with (dojo.hostenv.loadModule("A.B")) {
*       ...
*    }
*
* And to import just the leaf symbol:
*
*    var B = dojo.hostenv.loadModule("A.B");
*    ...
*
* dj_load is an alias for dojo.hostenv.loadModule
*/
dojo.hostenv._global_omit_module_check = false;
dojo.hostenv.loadModule = function(modulename, exact_only, omit_module_check){
	if(!modulename){ return; }
	omit_module_check = this._global_omit_module_check || omit_module_check;
	var module = this.findModule(modulename, false);
	if(module){
		return module;
	}

	// protect against infinite recursion from mutual dependencies
	if(dj_undef(modulename, this.loading_modules_)){
		this.addedToLoadingCount.push(modulename);
	}
	this.loading_modules_[modulename] = 1;

	// convert periods to slashes
	var relpath = modulename.replace(/\./g, '/') + '.js';

	var syms = modulename.split(".");
	var nsyms = modulename.split(".");
	for (var i = syms.length - 1; i > 0; i--) {
		var parentModule = syms.slice(0, i).join(".");
		var parentModulePath = this.getModulePrefix(parentModule);
		if (parentModulePath != parentModule) {
			syms.splice(0, i, parentModulePath);
			break;
		}
	}
	var last = syms[syms.length - 1];
	// figure out if we're looking for a full package, if so, we want to do
	// things slightly diffrently
	if(last=="*"){
		modulename = (nsyms.slice(0, -1)).join('.');

		while(syms.length){
			syms.pop();
			syms.push(this.pkgFileName);
			relpath = syms.join("/") + '.js';
			if(relpath.charAt(0)=="/"){
				relpath = relpath.slice(1);
			}
			ok = this.loadPath(relpath, ((!omit_module_check) ? modulename : null));
			if(ok){ break; }
			syms.pop();
		}
	}else{
		relpath = syms.join("/") + '.js';
		modulename = nsyms.join('.');
		var ok = this.loadPath(relpath, ((!omit_module_check) ? modulename : null));
		if((!ok)&&(!exact_only)){
			syms.pop();
			while(syms.length){
				relpath = syms.join('/') + '.js';
				ok = this.loadPath(relpath, ((!omit_module_check) ? modulename : null));
				if(ok){ break; }
				syms.pop();
				relpath = syms.join('/') + '/'+this.pkgFileName+'.js';
				if(relpath.charAt(0)=="/"){
					relpath = relpath.slice(1);
				}
				ok = this.loadPath(relpath, ((!omit_module_check) ? modulename : null));
				if(ok){ break; }
			}
		}

		if((!ok)&&(!omit_module_check)){
			dojo.raise("Could not load '" + modulename + "'; last tried '" + relpath + "'");
		}
	}

	// check that the symbol was defined
	//Don't bother if we're doing xdomain (asynchronous) loading.
	if(!omit_module_check && !this["isXDomain"]){
		// pass in false so we can give better error
		module = this.findModule(modulename, false);
		if(!module){
			dojo.raise("symbol '" + modulename + "' is not defined after loading '" + relpath + "'"); 
		}
	}

	return module;
}

/**
* startPackage("A.B") follows the path, and at each level creates a new empty
* object or uses what already exists. It returns the result.
*/
dojo.hostenv.startPackage = function(packname){
	var modref = dojo.evalObjPath((packname.split(".").slice(0, -1)).join('.'));
	this.loaded_modules_[(new String(packname)).toLowerCase()] = modref;

	var syms = packname.split(/\./);
	if(syms[syms.length-1]=="*"){
		syms.pop();
	}
	return dojo.evalObjPath(syms.join("."), true);
}

/**
 * findModule("A.B") returns the object A.B if it exists, otherwise null.
 * @param modulename A string like 'A.B'.
 * @param must_exist Optional, defualt false. throw instead of returning null
 * if the module does not currently exist.
 */
dojo.hostenv.findModule = function(modulename, must_exist){
	// check cache
	/*
	if(!dj_undef(modulename, this.modules_)){
		return this.modules_[modulename];
	}
	*/

	var lmn = (new String(modulename)).toLowerCase();

	if(this.loaded_modules_[lmn]){
		return this.loaded_modules_[lmn];
	}

	// see if symbol is defined anyway
	var module = dojo.evalObjPath(modulename);
	if((modulename)&&(typeof module != 'undefined')&&(module)){
		this.loaded_modules_[lmn] = module;
		return module;
	}

	if(must_exist){
		dojo.raise("no loaded module named '" + modulename + "'");
	}
	return null;
}

//Start of old bootstrap2:

/*
 * This method taks a "map" of arrays which one can use to optionally load dojo
 * modules. The map is indexed by the possible dojo.hostenv.name_ values, with
 * two additional values: "default" and "common". The items in the "default"
 * array will be loaded if none of the other items have been choosen based on
 * the hostenv.name_ item. The items in the "common" array will _always_ be
 * loaded, regardless of which list is chosen.  Here's how it's normally
 * called:
 *
 *	dojo.kwCompoundRequire({
 *		browser: [
 *			["foo.bar.baz", true, true], // an example that passes multiple args to loadModule()
 *			"foo.sample.*",
 *			"foo.test,
 *		],
 *		default: [ "foo.sample.*" ],
 *		common: [ "really.important.module.*" ]
 *	});
 */
dojo.kwCompoundRequire = function(modMap){
	var common = modMap["common"]||[];
	var result = (modMap[dojo.hostenv.name_]) ? common.concat(modMap[dojo.hostenv.name_]||[]) : common.concat(modMap["default"]||[]);

	for(var x=0; x<result.length; x++){
		var curr = result[x];
		if(curr.constructor == Array){
			dojo.hostenv.loadModule.apply(dojo.hostenv, curr);
		}else{
			dojo.hostenv.loadModule(curr);
		}
	}
}

dojo.require = function(){
	dojo.hostenv.loadModule.apply(dojo.hostenv, arguments);
}

dojo.requireIf = function(){
	if((arguments[0] === true)||(arguments[0]=="common")||(arguments[0] && dojo.render[arguments[0]].capable)){
		var args = [];
		for (var i = 1; i < arguments.length; i++) { args.push(arguments[i]); }
		dojo.require.apply(dojo, args);
	}
}

dojo.requireAfterIf = dojo.requireIf;

dojo.provide = function(){
	return dojo.hostenv.startPackage.apply(dojo.hostenv, arguments);
}

dojo.setModulePrefix = function(module, prefix){
	return dojo.hostenv.setModulePrefix(module, prefix);
}

// determine if an object supports a given method
// useful for longer api chains where you have to test each object in the chain
dojo.exists = function(obj, name){
	var p = name.split(".");
	for(var i = 0; i < p.length; i++){
	if(!(obj[p[i]])) return false;
		obj = obj[p[i]];
	}
	return true;
}

// make jsc shut up (so we can use jsc to sanity check the code even if it will never run it).
/*@cc_on
@if (@_jscript_version >= 7)
var window; var XMLHttpRequest;
@end
@*/

if(typeof window == 'undefined'){
	dojo.raise("no window object");
}

// attempt to figure out the path to dojo if it isn't set in the config
(function() {
	// before we get any further with the config options, try to pick them out
	// of the URL. Most of this code is from NW
	if(djConfig.allowQueryConfig){
		var baseUrl = document.location.toString(); // FIXME: use location.query instead?
		var params = baseUrl.split("?", 2);
		if(params.length > 1){
			var paramStr = params[1];
			var pairs = paramStr.split("&");
			for(var x in pairs){
				var sp = pairs[x].split("=");
				// FIXME: is this eval dangerous?
				if((sp[0].length > 9)&&(sp[0].substr(0, 9) == "djConfig.")){
					var opt = sp[0].substr(9);
					try{
						djConfig[opt]=eval(sp[1]);
					}catch(e){
						djConfig[opt]=sp[1];
					}
				}
			}
		}
	}

	if(((djConfig["baseScriptUri"] == "")||(djConfig["baseRelativePath"] == "")) &&(document && document.getElementsByTagName)){
		var scripts = document.getElementsByTagName("script");
		var rePkg = /(__package__|dojo|bootstrap1)\.js([\?\.]|$)/i;
		for(var i = 0; i < scripts.length; i++) {
			var src = scripts[i].getAttribute("src");
			if(!src) { continue; }
			var m = src.match(rePkg);
			if(m) {
				root = src.substring(0, m.index);
				if(src.indexOf("bootstrap1") > -1) { root += "../"; }
				if(!this["djConfig"]) { djConfig = {}; }
				if(djConfig["baseScriptUri"] == "") { djConfig["baseScriptUri"] = root; }
				if(djConfig["baseRelativePath"] == "") { djConfig["baseRelativePath"] = root; }
				break;
			}
		}
	}

	// fill in the rendering support information in dojo.render.*
	var dr = dojo.render;
	var drh = dojo.render.html;
	var drs = dojo.render.svg;
	var dua = drh.UA = navigator.userAgent;
	var dav = drh.AV = navigator.appVersion;
	var t = true;
	var f = false;
	drh.capable = t;
	drh.support.builtin = t;

	dr.ver = parseFloat(drh.AV);
	dr.os.mac = dav.indexOf("Macintosh") >= 0;
	dr.os.win = dav.indexOf("Windows") >= 0;
	// could also be Solaris or something, but it's the same browser
	dr.os.linux = dav.indexOf("X11") >= 0;

	drh.opera = dua.indexOf("Opera") >= 0;
	drh.khtml = (dav.indexOf("Konqueror") >= 0)||(dav.indexOf("Safari") >= 0);
	drh.safari = dav.indexOf("Safari") >= 0;
	var geckoPos = dua.indexOf("Gecko");
	drh.mozilla = drh.moz = (geckoPos >= 0)&&(!drh.khtml);
	if (drh.mozilla) {
		// gecko version is YYYYMMDD
		drh.geckoVersion = dua.substring(geckoPos + 6, geckoPos + 14);
	}
	drh.ie = (document.all)&&(!drh.opera);
	drh.ie50 = drh.ie && dav.indexOf("MSIE 5.0")>=0;
	drh.ie55 = drh.ie && dav.indexOf("MSIE 5.5")>=0;
	drh.ie60 = drh.ie && dav.indexOf("MSIE 6.0")>=0;

	dr.vml.capable=drh.ie;
	drs.capable = f;
	drs.support.plugin = f;
	drs.support.builtin = f;
	if (document.implementation
		&& document.implementation.hasFeature
		&& document.implementation.hasFeature("org.w3c.dom.svg", "1.0")
	){
		drs.capable = t;
		drs.support.builtin = t;
		drs.support.plugin = f;
	}
})();

dojo.hostenv.startPackage("dojo.hostenv");

dojo.render.name = dojo.hostenv.name_ = 'browser';
dojo.hostenv.searchIds = [];

// These are in order of decreasing likelihood; this will change in time.
var DJ_XMLHTTP_PROGIDS = ['Msxml2.XMLHTTP', 'Microsoft.XMLHTTP', 'Msxml2.XMLHTTP.4.0'];

dojo.hostenv.getXmlhttpObject = function(){
    var http = null;
	var last_e = null;
	try{ http = new XMLHttpRequest(); }catch(e){}
    if(!http){
		for(var i=0; i<3; ++i){
			var progid = DJ_XMLHTTP_PROGIDS[i];
			try{
				http = new ActiveXObject(progid);
			}catch(e){
				last_e = e;
			}

			if(http){
				DJ_XMLHTTP_PROGIDS = [progid];  // so faster next time
				break;
			}
		}

		/*if(http && !http.toString) {
			http.toString = function() { "[object XMLHttpRequest]"; }
		}*/
	}

	if(!http){
		return dojo.raise("XMLHTTP not available", last_e);
	}

	return http;
}

/**
 * Read the contents of the specified uri and return those contents.
 *
 * @param uri A relative or absolute uri. If absolute, it still must be in the
 * same "domain" as we are.
 *
 * @param async_cb If not specified, load synchronously. If specified, load
 * asynchronously, and use async_cb as the progress handler which takes the
 * xmlhttp object as its argument. If async_cb, this function returns null.
 *
 * @param fail_ok Default false. If fail_ok and !async_cb and loading fails,
 * return null instead of throwing.
 */
dojo.hostenv.getText = function(uri, async_cb, fail_ok){

	var http = this.getXmlhttpObject();

	if(async_cb){
		http.onreadystatechange = function(){
			if((4==http.readyState)&&(http["status"])){
				if(http.status==200){
					// dojo.debug("LOADED URI: "+uri);
					async_cb(http.responseText);
				}
			}
		}
	}

	http.open('GET', uri, async_cb ? true : false);
	try {
		http.send(null);
	} catch (e) {
		if (fail_ok && !async_cb) {
			return null;
		} else {
			throw e;
		}
	}
	if(async_cb){
		return null;
	}

	return http.responseText;
}

/*
 * It turns out that if we check *right now*, as this script file is being loaded,
 * then the last script element in the window DOM is ourselves.
 * That is because any subsequent script elements haven't shown up in the document
 * object yet.
 */
 /*
function dj_last_script_src() {
    var scripts = window.document.getElementsByTagName('script');
    if(scripts.length < 1){
		dojo.raise("No script elements in window.document, so can't figure out my script src");
	}
    var script = scripts[scripts.length - 1];
    var src = script.src;
    if(!src){
		dojo.raise("Last script element (out of " + scripts.length + ") has no src");
	}
    return src;
}

if(!dojo.hostenv["library_script_uri_"]){
	dojo.hostenv.library_script_uri_ = dj_last_script_src();
}
*/

dojo.hostenv.defaultDebugContainerId = 'dojoDebug';
dojo.hostenv._println_buffer = [];
dojo.hostenv._println_safe = false;
dojo.hostenv.println = function (line){
	if(!dojo.hostenv._println_safe){
		dojo.hostenv._println_buffer.push(line);
	}else{
		try {
			var console = document.getElementById(djConfig.debugContainerId ?
				djConfig.debugContainerId : dojo.hostenv.defaultDebugContainerId);
			if(!console) { console = document.getElementsByTagName("body")[0] || document.body; }

			var div = document.createElement("div");
			div.appendChild(document.createTextNode(line));
			console.appendChild(div);
		} catch (e) {
			try{
				// safari needs the output wrapped in an element for some reason
				document.write("<div>" + line + "</div>");
			}catch(e2){
				window.status = line;
			}
		}
	}
}

dojo.addOnLoad(function(){
	dojo.hostenv._println_safe = true;
	while(dojo.hostenv._println_buffer.length > 0){
		dojo.hostenv.println(dojo.hostenv._println_buffer.shift());
	}
});

function dj_addNodeEvtHdlr (node, evtName, fp, capture){
	var oldHandler = node["on"+evtName] || function(){};
	node["on"+evtName] = function(){
		fp.apply(node, arguments);
		oldHandler.apply(node, arguments);
	}
	return true;
}


/* Uncomment this to allow init after DOMLoad, not after window.onload

// Mozilla exposes the event we could use
if (dojo.render.html.mozilla) {
   document.addEventListener("DOMContentLoaded", dj_load_init, null);
}
// for Internet Explorer. readyState will not be achieved on init call, but dojo doesn't need it
//Tighten up the comments below to allow init after DOMLoad, not after window.onload
/ * @cc_on @ * /
/ * @if (@_win32)
    document.write("<script defer>dj_load_init()<"+"/script>");
/ * @end @ * /
*/

// default for other browsers
// potential TODO: apply setTimeout approach for other browsers
// that will cause flickering though ( document is loaded and THEN is processed)
// maybe show/hide required in this case..
// TODO: other browsers may support DOMContentLoaded/defer attribute. Add them to above.
dj_addNodeEvtHdlr(window, "load", function(){
	// allow multiple calls, only first one will take effect
	if(arguments.callee.initialized){ return; }
	arguments.callee.initialized = true;

	var initFunc = function(){
		//perform initialization
		if(dojo.render.html.ie){
			dojo.hostenv.makeWidgets();
		}
	};

	if(dojo.hostenv.inFlightCount == 0){
		initFunc();
		dojo.hostenv.modulesLoaded();
	}else{
		dojo.addOnLoad(initFunc);
	}
});

dojo.hostenv.makeWidgets = function(){
	// you can put searchIds in djConfig and dojo.hostenv at the moment
	// we should probably eventually move to one or the other
	var sids = [];
	if(djConfig.searchIds && djConfig.searchIds.length > 0) {
		sids = sids.concat(djConfig.searchIds);
	}
	if(dojo.hostenv.searchIds && dojo.hostenv.searchIds.length > 0) {
		sids = sids.concat(dojo.hostenv.searchIds);
	}

	if((djConfig.parseWidgets)||(sids.length > 0)){
		if(dojo.evalObjPath("dojo.widget.Parse")){
			// we must do this on a delay to avoid:
			//	http://www.shaftek.org/blog/archives/000212.html
			// IE is such a tremendous peice of shit.
			try{
				var parser = new dojo.xml.Parse();
				if(sids.length > 0){
					for(var x=0; x<sids.length; x++){
						var tmpNode = document.getElementById(sids[x]);
						if(!tmpNode){ continue; }
						var frag = parser.parseElement(tmpNode, null, true);
						dojo.widget.getParser().createComponents(frag);
					}
				}else if(djConfig.parseWidgets){
					var frag  = parser.parseElement(document.getElementsByTagName("body")[0] || document.body, null, true);
					dojo.widget.getParser().createComponents(frag);
				}
			}catch(e){
				dojo.debug("auto-build-widgets error:", e);
			}
		}
	}
}

dojo.addOnLoad(function(){
	if(!dojo.render.html.ie) {
		dojo.hostenv.makeWidgets();
	}
});

try {
	if (dojo.render.html.ie) {
		//	easier and safer VML addition.  Thanks Emil!
		document.namespaces.add("v", "urn:schemas-microsoft-com:vml");
		document.createStyleSheet().addRule("v\\:*", "behavior:url(#default#VML)");
	}
} catch (e) { }

// stub, over-ridden by debugging code. This will at least keep us from
// breaking when it's not included
dojo.hostenv.writeIncludes = function(){}

dojo.byId = function(id, doc){
	if(id && (typeof id == "string" || id instanceof String)){
		if(!doc){ doc = document; }
		return doc.getElementById(id);
	}
	return id; // assume it's a node
}

//Semicolon is for when this file is integrated with a custom build on one line
//with some other file's contents. Sometimes that makes things not get defined
//properly, particularly with the using the closure below to do all the work.
;(function(){
	//Don't do this work if dojo.js has already done it.
	if(typeof dj_usingBootstrap != "undefined"){
		return;
	}

	var isRhino = false;
	var isSpidermonkey = false;
	var isDashboard = false;
	if((typeof this["load"] == "function")&&(typeof this["Packages"] == "function")){
		isRhino = true;
	}else if(typeof this["load"] == "function"){
		isSpidermonkey  = true;
	}else if(window.widget){
		isDashboard = true;
	}

	var tmps = [];
	if((this["djConfig"])&&((djConfig["isDebug"])||(djConfig["debugAtAllCosts"]))){
		tmps.push("debug.js");
	}

	if((this["djConfig"])&&(djConfig["debugAtAllCosts"])&&(!isRhino)&&(!isDashboard)){
		tmps.push("browser_debug.js");
	}

	//Support compatibility packages. Right now this only allows setting one
	//compatibility package. Might need to revisit later down the line to support
	//more than one.
	if((this["djConfig"])&&(djConfig["compat"])){
		tmps.push("compat/" + djConfig["compat"] + ".js");
	}

	var loaderRoot = djConfig["baseScriptUri"];
	if((this["djConfig"])&&(djConfig["baseLoaderUri"])){
		loaderRoot = djConfig["baseLoaderUri"];
	}

	for(var x=0; x < tmps.length; x++){
		var spath = loaderRoot+"src/"+tmps[x];
		if(isRhino||isSpidermonkey){
			load(spath);
		} else {
			try {
				document.write("<scr"+"ipt type='text/javascript' src='"+spath+"'></scr"+"ipt>");
			} catch (e) {
				var script = document.createElement("script");
				script.src = spath;
				document.getElementsByTagName("head")[0].appendChild(script);
			}
		}
	}
})();

dojo.provide("dojo.lang.common");

dojo.require("dojo.lang");

/*
 * Adds the given properties/methods to the specified object
 */
dojo.lang.mixin = function(obj, props){
	var tobj = {};
	for(var x in props){
		// the "tobj" condition avoid copying properties in "props"
		// inherited from Object.prototype.  For example, if obj has a custom
		// toString() method, don't overwrite it with the toString() method
		// that props inherited from Object.protoype
		if(typeof tobj[x] == "undefined" || tobj[x] != props[x]) {
			obj[x] = props[x];
		}
	}
	// IE doesn't recognize custom toStrings in for..in
	if(dojo.render.html.ie && dojo.lang.isFunction(props["toString"]) && props["toString"] != obj["toString"]) {
		obj.toString = props.toString;
	}
	return obj;
}

/*
 * Adds the given properties/methods to the specified object's prototype
 */
dojo.lang.extend = function(ctor, props){
	this.mixin(ctor.prototype, props);
}

/**
 * See if val is in arr. Call signatures:
 *  find(array, value, identity) // recommended
 *  find(value, array, identity)
**/
dojo.lang.find = function(	/*Array*/	arr, 
							/*Object*/	val,
							/*boolean*/	identity,
							/*boolean*/	findLast){
	// support both (arr, val) and (val, arr)
	if(!dojo.lang.isArrayLike(arr) && dojo.lang.isArrayLike(val)) {
		var a = arr;
		arr = val;
		val = a;
	}
	var isString = dojo.lang.isString(arr);
	if(isString) { arr = arr.split(""); }

	if(findLast) {
		var step = -1;
		var i = arr.length - 1;
		var end = -1;
	} else {
		var step = 1;
		var i = 0;
		var end = arr.length;
	}
	if(identity){
		while(i != end) {
			if(arr[i] === val){ return i; }
			i += step;
		}
	}else{
		while(i != end) {
			if(arr[i] == val){ return i; }
			i += step;
		}
	}
	return -1;
}

dojo.lang.indexOf = dojo.lang.find;

dojo.lang.findLast = function(/*Array*/ arr, /*Object*/ val, /*boolean*/ identity){
	return dojo.lang.find(arr, val, identity, true);
}

dojo.lang.lastIndexOf = dojo.lang.findLast;

dojo.lang.inArray = function(arr /*Array*/, val /*Object*/){
	return dojo.lang.find(arr, val) > -1; // return: boolean
}

/**
 * Partial implmentation of is* functions from
 * http://www.crockford.com/javascript/recommend.html
 * NOTE: some of these may not be the best thing to use in all situations
 * as they aren't part of core JS and therefore can't work in every case.
 * See WARNING messages inline for tips.
 *
 * The following is* functions are fairly "safe"
 */

dojo.lang.isObject = function(wh) {
	return typeof wh == "object" || dojo.lang.isArray(wh) || dojo.lang.isFunction(wh);
}

dojo.lang.isArray = function(wh) {
	return (wh instanceof Array || typeof wh == "array");
}

dojo.lang.isArrayLike = function(wh) {
	if(dojo.lang.isString(wh)){ return false; }
	if(dojo.lang.isFunction(wh)){ return false; } // keeps out built-in ctors (Number, String, ...) which have length properties
	if(dojo.lang.isArray(wh)){ return true; }
	if(typeof wh != "undefined" && wh
		&& dojo.lang.isNumber(wh.length) && isFinite(wh.length)){ return true; }
	return false;
}

dojo.lang.isFunction = function(wh) {
	return (wh instanceof Function || typeof wh == "function");
}

dojo.lang.isString = function(wh) {
	return (wh instanceof String || typeof wh == "string");
}

dojo.lang.isAlien = function(wh) {
	return !dojo.lang.isFunction() && /\{\s*\[native code\]\s*\}/.test(String(wh));
}

dojo.lang.isBoolean = function(wh) {
	return (wh instanceof Boolean || typeof wh == "boolean");
}

/**
 * The following is***() functions are somewhat "unsafe". Fortunately,
 * there are workarounds the the language provides and are mentioned
 * in the WARNING messages.
 *
 * WARNING: In most cases, isNaN(wh) is sufficient to determine whether or not
 * something is a number or can be used as such. For example, a number or string
 * can be used interchangably when accessing array items (arr["1"] is the same as
 * arr[1]) and isNaN will return false for both values ("1" and 1). Should you
 * use isNumber("1"), that will return false, which is generally not too useful.
 * Also, isNumber(NaN) returns true, again, this isn't generally useful, but there
 * are corner cases (like when you want to make sure that two things are really
 * the same type of thing). That is really where isNumber "shines".
 *
 * RECOMMENDATION: Use isNaN(wh) when possible
 */
dojo.lang.isNumber = function(wh) {
	return (wh instanceof Number || typeof wh == "number");
}

/**
 * WARNING: In some cases, isUndefined will not behave as you
 * might expect. If you do isUndefined(foo) and there is no earlier
 * reference to foo, an error will be thrown before isUndefined is
 * called. It behaves correctly if you scope yor object first, i.e.
 * isUndefined(foo.bar) where foo is an object and bar isn't a
 * property of the object.
 *
 * RECOMMENDATION: Use `typeof foo == "undefined"` when possible
 *
 * FIXME: Should isUndefined go away since it is error prone?
 */
dojo.lang.isUndefined = function(wh) {
	return ((wh == undefined)&&(typeof wh == "undefined"));
}

// end Crockford functions

dojo.provide("dojo.lang");
dojo.provide("dojo.lang.Lang");

dojo.require("dojo.lang.common");

dojo.provide("dojo.lang.func");

dojo.require("dojo.lang.common");

/**
 * Runs a function in a given scope (thisObject), can
 * also be used to preserve scope.
 *
 * hitch(foo, "bar"); // runs foo.bar() in the scope of foo
 * hitch(foo, myFunction); // runs myFunction in the scope of foo
 */
dojo.lang.hitch = function(thisObject, method) {
	if(dojo.lang.isString(method)) {
		var fcn = thisObject[method];
	} else {
		var fcn = method;
	}

	return function() {
		return fcn.apply(thisObject, arguments);
	}
}

dojo.lang.anonCtr = 0;
dojo.lang.anon = {};
dojo.lang.nameAnonFunc = function(anonFuncPtr, namespaceObj){
	var nso = (namespaceObj || dojo.lang.anon);
	if((dj_global["djConfig"])&&(djConfig["slowAnonFuncLookups"] == true)){
		for(var x in nso){
			if(nso[x] === anonFuncPtr){
				return x;
			}
		}
	}
	var ret = "__"+dojo.lang.anonCtr++;
	while(typeof nso[ret] != "undefined"){
		ret = "__"+dojo.lang.anonCtr++;
	}
	nso[ret] = anonFuncPtr;
	return ret;
}

dojo.lang.forward = function(funcName){
	// Returns a function that forwards a method call to this.func(...)
	return function(){
		return this[funcName].apply(this, arguments);
	};
}

dojo.lang.curry = function(ns, func /* args ... */){
	var outerArgs = [];
	ns = ns||dj_global;
	if(dojo.lang.isString(func)){
		func = ns[func];
	}
	for(var x=2; x<arguments.length; x++){
		outerArgs.push(arguments[x]);
	}
	// since the event system replaces the original function with a new
	// join-point runner with an arity of 0, we check to see if it's left us
	// any clues about the original arity in lieu of the function's actual
	// length property
	var ecount = (func["__preJoinArity"]||func.length) - outerArgs.length;
	// borrowed from svend tofte
	function gather(nextArgs, innerArgs, expected){
		var texpected = expected;
		var totalArgs = innerArgs.slice(0); // copy
		for(var x=0; x<nextArgs.length; x++){
			totalArgs.push(nextArgs[x]);
		}
		// check the list of provided nextArgs to see if it, plus the
		// number of innerArgs already supplied, meets the total
		// expected.
		expected = expected-nextArgs.length;
		if(expected<=0){
			var res = func.apply(ns, totalArgs);
			expected = texpected;
			return res;
		}else{
			return function(){
				return gather(arguments,// check to see if we've been run
										// with enough args
							totalArgs,	// a copy
							expected);	// how many more do we need to run?;
			}
		}
	}
	return gather([], outerArgs, ecount);
}

dojo.lang.curryArguments = function(ns, func, args, offset){
	var targs = [];
	var x = offset||0;
	for(x=offset; x<args.length; x++){
		targs.push(args[x]); // ensure that it's an arr
	}
	return dojo.lang.curry.apply(dojo.lang, [ns, func].concat(targs));
}

dojo.lang.tryThese = function(){
	for(var x=0; x<arguments.length; x++){
		try{
			if(typeof arguments[x] == "function"){
				var ret = (arguments[x]());
				if(ret){
					return ret;
				}
			}
		}catch(e){
			dojo.debug(e);
		}
	}
}

dojo.lang.delayThese = function(farr, cb, delay, onend){
	/**
	 * alternate: (array funcArray, function callback, function onend)
	 * alternate: (array funcArray, function callback)
	 * alternate: (array funcArray)
	 */
	if(!farr.length){ 
		if(typeof onend == "function"){
			onend();
		}
		return;
	}
	if((typeof delay == "undefined")&&(typeof cb == "number")){
		delay = cb;
		cb = function(){};
	}else if(!cb){
		cb = function(){};
		if(!delay){ delay = 0; }
	}
	setTimeout(function(){
		(farr.shift())();
		cb();
		dojo.lang.delayThese(farr, cb, delay, onend);
	}, delay);
}

dojo.provide("dojo.lang.array");

dojo.require("dojo.lang.common");

// FIXME: Is this worthless since you can do: if(name in obj)
// is this the right place for this?
dojo.lang.has = function(obj, name){
	try{
		return (typeof obj[name] != "undefined");
	}catch(e){ return false; }
}

dojo.lang.isEmpty = function(obj) {
	if(dojo.lang.isObject(obj)) {
		var tmp = {};
		var count = 0;
		for(var x in obj){
			if(obj[x] && (!tmp[x])){
				count++;
				break;
			} 
		}
		return (count == 0);
	} else if(dojo.lang.isArrayLike(obj) || dojo.lang.isString(obj)) {
		return obj.length == 0;
	}
}

dojo.lang.map = function(arr, obj, unary_func){
	var isString = dojo.lang.isString(arr);
	if(isString){
		arr = arr.split("");
	}
	if(dojo.lang.isFunction(obj)&&(!unary_func)){
		unary_func = obj;
		obj = dj_global;
	}else if(dojo.lang.isFunction(obj) && unary_func){
		// ff 1.5 compat
		var tmpObj = obj;
		obj = unary_func;
		unary_func = tmpObj;
	}
	if(Array.map){
	 	var outArr = Array.map(arr, unary_func, obj);
	}else{
		var outArr = [];
		for(var i=0;i<arr.length;++i){
			outArr.push(unary_func.call(obj, arr[i]));
		}
	}
	if(isString) {
		return outArr.join("");
	} else {
		return outArr;
	}
}

// http://developer.mozilla.org/en/docs/Core_JavaScript_1.5_Reference:Global_Objects:Array:forEach
dojo.lang.forEach = function(anArray /* Array */, callback /* Function */, thisObject /* Object */){
	if(dojo.lang.isString(anArray)){ 
		anArray = anArray.split(""); 
	}
	if(Array.forEach){
		Array.forEach(anArray, callback, thisObject);
	}else{
		// FIXME: there are several ways of handilng thisObject. Is dj_global always the default context?
		if(!thisObject){
			thisObject=dj_global;
		}
		for(var i=0,l=anArray.length; i<l; i++){ 
			callback.call(thisObject, anArray[i], i, anArray);
		}
	}
}

dojo.lang._everyOrSome = function(every, arr, callback, thisObject){
	if(dojo.lang.isString(arr)){ 
		arr = arr.split(""); 
	}
	if(Array.every){
		return Array[ (every) ? "every" : "some" ](arr, callback, thisObject);
	}else{
		if(!thisObject){
			thisObject = dj_global;
		}
		for(var i=0,l=arr.length; i<l; i++){
			var result = callback.call(thisObject, arr[i], i, arr);
			if((every)&&(!result)){
				return false;
			}else if((!every)&&(result)){
				return true;
			}
		}
		return (every) ? true : false;
	}
}

dojo.lang.every = function(arr, callback, thisObject){
	return this._everyOrSome(true, arr, callback, thisObject);
}

dojo.lang.some = function(arr, callback, thisObject){
	return this._everyOrSome(false, arr, callback, thisObject);
}

dojo.lang.filter = function(arr, callback, thisObject) {
	var isString = dojo.lang.isString(arr);
	if(isString) { arr = arr.split(""); }
	if(Array.filter) {
		var outArr = Array.filter(arr, callback, thisObject);
	} else {
		if(!thisObject) {
			if(arguments.length >= 3) { dojo.raise("thisObject doesn't exist!"); }
			thisObject = dj_global;
		}

		var outArr = [];
		for(var i = 0; i < arr.length; i++) {
			if(callback.call(thisObject, arr[i], i, arr)) {
				outArr.push(arr[i]);
			}
		}
	}
	if(isString) {
		return outArr.join("");
	} else {
		return outArr;
	}
}

/**
 * Creates a 1-D array out of all the arguments passed,
 * unravelling any array-like objects in the process
 *
 * Ex:
 * unnest(1, 2, 3) ==> [1, 2, 3]
 * unnest(1, [2, [3], [[[4]]]]) ==> [1, 2, 3, 4]
 */
dojo.lang.unnest = function(/* ... */) {
	var out = [];
	for(var i = 0; i < arguments.length; i++) {
		if(dojo.lang.isArrayLike(arguments[i])) {
			var add = dojo.lang.unnest.apply(this, arguments[i]);
			out = out.concat(add);
		} else {
			out.push(arguments[i]);
		}
	}
	return out;
}

/**
 * Converts an array-like object (i.e. arguments, DOMCollection)
 * to an array
**/
dojo.lang.toArray = function(arrayLike, startOffset) {
	var array = [];
	for(var i = startOffset||0; i < arrayLike.length; i++) {
		array.push(arrayLike[i]);
	}
	return array;
}

dojo.provide("dojo.dom");
dojo.require("dojo.lang.array");

dojo.dom.ELEMENT_NODE                  = 1;
dojo.dom.ATTRIBUTE_NODE                = 2;
dojo.dom.TEXT_NODE                     = 3;
dojo.dom.CDATA_SECTION_NODE            = 4;
dojo.dom.ENTITY_REFERENCE_NODE         = 5;
dojo.dom.ENTITY_NODE                   = 6;
dojo.dom.PROCESSING_INSTRUCTION_NODE   = 7;
dojo.dom.COMMENT_NODE                  = 8;
dojo.dom.DOCUMENT_NODE                 = 9;
dojo.dom.DOCUMENT_TYPE_NODE            = 10;
dojo.dom.DOCUMENT_FRAGMENT_NODE        = 11;
dojo.dom.NOTATION_NODE                 = 12;
	
dojo.dom.dojoml = "http://www.dojotoolkit.org/2004/dojoml";

/**
 *	comprehensive list of XML namespaces
**/
dojo.dom.xmlns = {
	svg : "http://www.w3.org/2000/svg",
	smil : "http://www.w3.org/2001/SMIL20/",
	mml : "http://www.w3.org/1998/Math/MathML",
	cml : "http://www.xml-cml.org",
	xlink : "http://www.w3.org/1999/xlink",
	xhtml : "http://www.w3.org/1999/xhtml",
	xul : "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul",
	xbl : "http://www.mozilla.org/xbl",
	fo : "http://www.w3.org/1999/XSL/Format",
	xsl : "http://www.w3.org/1999/XSL/Transform",
	xslt : "http://www.w3.org/1999/XSL/Transform",
	xi : "http://www.w3.org/2001/XInclude",
	xforms : "http://www.w3.org/2002/01/xforms",
	saxon : "http://icl.com/saxon",
	xalan : "http://xml.apache.org/xslt",
	xsd : "http://www.w3.org/2001/XMLSchema",
	dt: "http://www.w3.org/2001/XMLSchema-datatypes",
	xsi : "http://www.w3.org/2001/XMLSchema-instance",
	rdf : "http://www.w3.org/1999/02/22-rdf-syntax-ns#",
	rdfs : "http://www.w3.org/2000/01/rdf-schema#",
	dc : "http://purl.org/dc/elements/1.1/",
	dcq: "http://purl.org/dc/qualifiers/1.0",
	"soap-env" : "http://schemas.xmlsoap.org/soap/envelope/",
	wsdl : "http://schemas.xmlsoap.org/wsdl/",
	AdobeExtensions : "http://ns.adobe.com/AdobeSVGViewerExtensions/3.0/"
};

dojo.dom.isNode = function(wh){
	if(typeof Element == "object") {
		try {
			return wh instanceof Element;
		} catch(E) {}
	} else {
		// best-guess
		return wh && !isNaN(wh.nodeType);
	}
}

dojo.dom.getTagName = function(node){
	dojo.deprecated("dojo.dom.getTagName", "use node.tagName instead", "0.4");

	var tagName = node.tagName;
	if(tagName.substr(0,5).toLowerCase()!="dojo:"){
		
		if(tagName.substr(0,4).toLowerCase()=="dojo"){
			// FIXME: this assuumes tag names are always lower case
			return "dojo:" + tagName.substring(4).toLowerCase();
		}

		// allow lower-casing
		var djt = node.getAttribute("dojoType")||node.getAttribute("dojotype");
		if(djt){
			return "dojo:"+djt.toLowerCase();
		}
		
		if((node.getAttributeNS)&&(node.getAttributeNS(this.dojoml,"type"))){
			return "dojo:" + node.getAttributeNS(this.dojoml,"type").toLowerCase();
		}
		try{
			// FIXME: IE really really doesn't like this, so we squelch
			// errors for it
			djt = node.getAttribute("dojo:type");
		}catch(e){ /* FIXME: log? */ }
		if(djt){
			return "dojo:"+djt.toLowerCase();
		}

		if((!dj_global["djConfig"])||(!djConfig["ignoreClassNames"])){
			// FIXME: should we make this optionally enabled via djConfig?
			var classes = node.className||node.getAttribute("class");
			// FIXME: following line, without check for existence of classes.indexOf
			// breaks firefox 1.5's svg widgets
			if((classes)&&(classes.indexOf)&&(classes.indexOf("dojo-") != -1)){
				var aclasses = classes.split(" ");
				for(var x=0; x<aclasses.length; x++){
					if((aclasses[x].length>5)&&(aclasses[x].indexOf("dojo-")>=0)){
						return "dojo:"+aclasses[x].substr(5).toLowerCase();
					}
				}
			}
		}

	}
	return tagName.toLowerCase();
}

dojo.dom.getUniqueId = function(){
	do {
		var id = "dj_unique_" + (++arguments.callee._idIncrement);
	}while(document.getElementById(id));
	return id;
}
dojo.dom.getUniqueId._idIncrement = 0;

dojo.dom.firstElement = dojo.dom.getFirstChildElement = function(parentNode, tagName){
	var node = parentNode.firstChild;
	while(node && node.nodeType != dojo.dom.ELEMENT_NODE){
		node = node.nextSibling;
	}
	if(tagName && node && node.tagName && node.tagName.toLowerCase() != tagName.toLowerCase()) {
		node = dojo.dom.nextElement(node, tagName);
	}
	return node;
}

dojo.dom.lastElement = dojo.dom.getLastChildElement = function(parentNode, tagName){
	var node = parentNode.lastChild;
	while(node && node.nodeType != dojo.dom.ELEMENT_NODE) {
		node = node.previousSibling;
	}
	if(tagName && node && node.tagName && node.tagName.toLowerCase() != tagName.toLowerCase()) {
		node = dojo.dom.prevElement(node, tagName);
	}
	return node;
}

dojo.dom.nextElement = dojo.dom.getNextSiblingElement = function(node, tagName){
	if(!node) { return null; }
	do {
		node = node.nextSibling;
	} while(node && node.nodeType != dojo.dom.ELEMENT_NODE);

	if(node && tagName && tagName.toLowerCase() != node.tagName.toLowerCase()) {
		return dojo.dom.nextElement(node, tagName);
	}
	return node;
}

dojo.dom.prevElement = dojo.dom.getPreviousSiblingElement = function(node, tagName){
	if(!node) { return null; }
	if(tagName) { tagName = tagName.toLowerCase(); }
	do {
		node = node.previousSibling;
	} while(node && node.nodeType != dojo.dom.ELEMENT_NODE);

	if(node && tagName && tagName.toLowerCase() != node.tagName.toLowerCase()) {
		return dojo.dom.prevElement(node, tagName);
	}
	return node;
}

// TODO: hmph
/*this.forEachChildTag = function(node, unaryFunc) {
	var child = this.getFirstChildTag(node);
	while(child) {
		if(unaryFunc(child) == "break") { break; }
		child = this.getNextSiblingTag(child);
	}
}*/

dojo.dom.moveChildren = function(srcNode, destNode, trim){
	var count = 0;
	if(trim) {
		while(srcNode.hasChildNodes() &&
			srcNode.firstChild.nodeType == dojo.dom.TEXT_NODE) {
			srcNode.removeChild(srcNode.firstChild);
		}
		while(srcNode.hasChildNodes() &&
			srcNode.lastChild.nodeType == dojo.dom.TEXT_NODE) {
			srcNode.removeChild(srcNode.lastChild);
		}
	}
	while(srcNode.hasChildNodes()){
		destNode.appendChild(srcNode.firstChild);
		count++;
	}
	return count;
}

dojo.dom.copyChildren = function(srcNode, destNode, trim){
	var clonedNode = srcNode.cloneNode(true);
	return this.moveChildren(clonedNode, destNode, trim);
}

dojo.dom.removeChildren = function(node){
	var count = node.childNodes.length;
	while(node.hasChildNodes()){ node.removeChild(node.firstChild); }
	return count;
}

dojo.dom.replaceChildren = function(node, newChild){
	// FIXME: what if newChild is an array-like object?
	dojo.dom.removeChildren(node);
	node.appendChild(newChild);
}

dojo.dom.removeNode = function(node){
	if(node && node.parentNode){
		// return a ref to the removed child
		return node.parentNode.removeChild(node);
	}
}

dojo.dom.getAncestors = function(node, filterFunction, returnFirstHit) {
	var ancestors = [];
	var isFunction = dojo.lang.isFunction(filterFunction);
	while(node) {
		if (!isFunction || filterFunction(node)) {
			ancestors.push(node);
		}
		if (returnFirstHit && ancestors.length > 0) { return ancestors[0]; }
		
		node = node.parentNode;
	}
	if (returnFirstHit) { return null; }
	return ancestors;
}

dojo.dom.getAncestorsByTag = function(node, tag, returnFirstHit) {
	tag = tag.toLowerCase();
	return dojo.dom.getAncestors(node, function(el){
		return ((el.tagName)&&(el.tagName.toLowerCase() == tag));
	}, returnFirstHit);
}

dojo.dom.getFirstAncestorByTag = function(node, tag) {
	return dojo.dom.getAncestorsByTag(node, tag, true);
}

dojo.dom.isDescendantOf = function(node, ancestor, guaranteeDescendant){
	// guaranteeDescendant allows us to be a "true" isDescendantOf function
	if(guaranteeDescendant && node) { node = node.parentNode; }
	while(node) {
		if(node == ancestor){ return true; }
		node = node.parentNode;
	}
	return false;
}

dojo.dom.innerXML = function(node){
	if(node.innerXML){
		return node.innerXML;
	}else if(typeof XMLSerializer != "undefined"){
		return (new XMLSerializer()).serializeToString(node);
	}
}

dojo.dom.createDocumentFromText = function(str, mimetype){
	if(!mimetype) { mimetype = "text/xml"; }
	if(typeof DOMParser != "undefined") {
		var parser = new DOMParser();
		return parser.parseFromString(str, mimetype);
	}else if(typeof ActiveXObject != "undefined"){
		var domDoc = new ActiveXObject("Microsoft.XMLDOM");
		if(domDoc) {
			domDoc.async = false;
			domDoc.loadXML(str);
			return domDoc;
		}else{
			dojo.debug("toXml didn't work?");
		}
	/*
	}else if((dojo.render.html.capable)&&(dojo.render.html.safari)){
		// FIXME: this doesn't appear to work!
		// from: http://web-graphics.com/mtarchive/001606.php
		// var xml = '<?xml version="1.0"?>'+str;
		var mtype = "text/xml";
		var xml = '<?xml version="1.0"?>'+str;
		var url = "data:"+mtype+";charset=utf-8,"+encodeURIComponent(xml);
		var req = new XMLHttpRequest();
		req.open("GET", url, false);
		req.overrideMimeType(mtype);
		req.send(null);
		return req.responseXML;
	*/
	}else if(document.createElement){
		// FIXME: this may change all tags to uppercase!
		var tmp = document.createElement("xml");
		tmp.innerHTML = str;
		if(document.implementation && document.implementation.createDocument) {
			var xmlDoc = document.implementation.createDocument("foo", "", null);
			for(var i = 0; i < tmp.childNodes.length; i++) {
				xmlDoc.importNode(tmp.childNodes.item(i), true);
			}
			return xmlDoc;
		}
		// FIXME: probably not a good idea to have to return an HTML fragment
		// FIXME: the tmp.doc.firstChild is as tested from IE, so it may not
		// work that way across the board
		return tmp.document && tmp.document.firstChild ?
			tmp.document.firstChild : tmp;
	}
	return null;
}

dojo.dom.prependChild = function(node, parent) {
	if(parent.firstChild) {
		parent.insertBefore(node, parent.firstChild);
	} else {
		parent.appendChild(node);
	}
	return true;
}

dojo.dom.insertBefore = function(node, ref, force){
	if (force != true &&
		(node === ref || node.nextSibling === ref)){ return false; }
	var parent = ref.parentNode;
	parent.insertBefore(node, ref);
	return true;
}

dojo.dom.insertAfter = function(node, ref, force){
	var pn = ref.parentNode;
	if(ref == pn.lastChild){
		if((force != true)&&(node === ref)){
			return false;
		}
		pn.appendChild(node);
	}else{
		return this.insertBefore(node, ref.nextSibling, force);
	}
	return true;
}

dojo.dom.insertAtPosition = function(node, ref, position){
	if((!node)||(!ref)||(!position)){ return false; }
	switch(position.toLowerCase()){
		case "before":
			return dojo.dom.insertBefore(node, ref);
		case "after":
			return dojo.dom.insertAfter(node, ref);
		case "first":
			if(ref.firstChild){
				return dojo.dom.insertBefore(node, ref.firstChild);
			}else{
				ref.appendChild(node);
				return true;
			}
			break;
		default: // aka: last
			ref.appendChild(node);
			return true;
	}
}

dojo.dom.insertAtIndex = function(node, containingNode, insertionIndex){
	var siblingNodes = containingNode.childNodes;

	// if there aren't any kids yet, just add it to the beginning

	if (!siblingNodes.length){
		containingNode.appendChild(node);
		return true;
	}

	// otherwise we need to walk the childNodes
	// and find our spot

	var after = null;

	for(var i=0; i<siblingNodes.length; i++){

		var sibling_index = siblingNodes.item(i)["getAttribute"] ? parseInt(siblingNodes.item(i).getAttribute("dojoinsertionindex")) : -1;

		if (sibling_index < insertionIndex){
			after = siblingNodes.item(i);
		}
	}

	if (after){
		// add it after the node in {after}

		return dojo.dom.insertAfter(node, after);
	}else{
		// add it to the start

		return dojo.dom.insertBefore(node, siblingNodes.item(0));
	}
}
	
/**
 * implementation of the DOM Level 3 attribute.
 * 
 * @param node The node to scan for text
 * @param text Optional, set the text to this value.
 */
dojo.dom.textContent = function(node, text){
	if (text) {
		dojo.dom.replaceChildren(node, document.createTextNode(text));
		return text;
	} else {
		var _result = "";
		if (node == null) { return _result; }
		for (var i = 0; i < node.childNodes.length; i++) {
			switch (node.childNodes[i].nodeType) {
				case 1: // ELEMENT_NODE
				case 5: // ENTITY_REFERENCE_NODE
					_result += dojo.dom.textContent(node.childNodes[i]);
					break;
				case 3: // TEXT_NODE
				case 2: // ATTRIBUTE_NODE
				case 4: // CDATA_SECTION_NODE
					_result += node.childNodes[i].nodeValue;
					break;
				default:
					break;
			}
		}
		return _result;
	}
}

dojo.dom.collectionToArray = function(collection){
	dojo.deprecated("dojo.dom.collectionToArray", "use dojo.lang.toArray instead", "0.4");
	return dojo.lang.toArray(collection);
}

dojo.dom.hasParent = function (node) {
	return node && node.parentNode && dojo.dom.isNode(node.parentNode);
}

/**
 * Determines if node has any of the provided tag names and
 * returns the tag name that matches, empty string otherwise.
 *
 * Examples:
 *
 * myFooNode = <foo />
 * isTag(myFooNode, "foo"); // returns "foo"
 * isTag(myFooNode, "bar"); // returns ""
 * isTag(myFooNode, "FOO"); // returns ""
 * isTag(myFooNode, "hey", "foo", "bar"); // returns "foo"
**/
dojo.dom.isTag = function(node /* ... */) {
	if(node && node.tagName) {
		var arr = dojo.lang.toArray(arguments, 1);
		return arr[ dojo.lang.find(node.tagName, arr) ] || "";
	}
	return "";
}

dojo.provide("dojo.graphics.color");
dojo.require("dojo.lang.array");

// TODO: rewrite the "x2y" methods to take advantage of the parsing
//       abilities of the Color object. Also, beef up the Color
//       object (as possible) to parse most common formats

// takes an r, g, b, a(lpha) value, [r, g, b, a] array, "rgb(...)" string, hex string (#aaa, #aaaaaa, aaaaaaa)
dojo.graphics.color.Color = function(r, g, b, a) {
	// dojo.debug("r:", r[0], "g:", r[1], "b:", r[2]);
	if(dojo.lang.isArray(r)) {
		this.r = r[0];
		this.g = r[1];
		this.b = r[2];
		this.a = r[3]||1.0;
	} else if(dojo.lang.isString(r)) {
		var rgb = dojo.graphics.color.extractRGB(r);
		this.r = rgb[0];
		this.g = rgb[1];
		this.b = rgb[2];
		this.a = g||1.0;
	} else if(r instanceof dojo.graphics.color.Color) {
		this.r = r.r;
		this.b = r.b;
		this.g = r.g;
		this.a = r.a;
	} else {
		this.r = r;
		this.g = g;
		this.b = b;
		this.a = a;
	}
}

dojo.graphics.color.Color.fromArray = function(arr) {
	return new dojo.graphics.color.Color(arr[0], arr[1], arr[2], arr[3]);
}

dojo.lang.extend(dojo.graphics.color.Color, {
	toRgb: function(includeAlpha) {
		if(includeAlpha) {
			return this.toRgba();
		} else {
			return [this.r, this.g, this.b];
		}
	},

	toRgba: function() {
		return [this.r, this.g, this.b, this.a];
	},

	toHex: function() {
		return dojo.graphics.color.rgb2hex(this.toRgb());
	},

	toCss: function() {
		return "rgb(" + this.toRgb().join() + ")";
	},

	toString: function() {
		return this.toHex(); // decent default?
	},

	blend: function(color, weight) {
		return dojo.graphics.color.blend(this.toRgb(), new Color(color).toRgb(), weight);
	}
});

dojo.graphics.color.named = {
	white:      [255,255,255],
	black:      [0,0,0],
	red:        [255,0,0],
	green:	    [0,255,0],
	blue:       [0,0,255],
	navy:       [0,0,128],
	gray:       [128,128,128],
	silver:     [192,192,192]
};

// blend colors a and b (both as RGB array or hex strings) with weight from -1 to +1, 0 being a 50/50 blend
dojo.graphics.color.blend = function(a, b, weight) {
	if(typeof a == "string") { return dojo.graphics.color.blendHex(a, b, weight); }
	if(!weight) { weight = 0; }
	else if(weight > 1) { weight = 1; }
	else if(weight < -1) { weight = -1; }
	var c = new Array(3);
	for(var i = 0; i < 3; i++) {
		var half = Math.abs(a[i] - b[i])/2;
		c[i] = Math.floor(Math.min(a[i], b[i]) + half + (half * weight));
	}
	return c;
}

// very convenient blend that takes and returns hex values
// (will get called automatically by blend when blend gets strings)
dojo.graphics.color.blendHex = function(a, b, weight) {
	return dojo.graphics.color.rgb2hex(dojo.graphics.color.blend(dojo.graphics.color.hex2rgb(a), dojo.graphics.color.hex2rgb(b), weight));
}

// get RGB array from css-style color declarations
dojo.graphics.color.extractRGB = function(color) {
	var hex = "0123456789abcdef";
	color = color.toLowerCase();
	if( color.indexOf("rgb") == 0 ) {
		var matches = color.match(/rgba*\((\d+), *(\d+), *(\d+)/i);
		var ret = matches.splice(1, 3);
		return ret;
	} else {
		var colors = dojo.graphics.color.hex2rgb(color);
		if(colors) {
			return colors;
		} else {
			// named color (how many do we support?)
			return dojo.graphics.color.named[color] || [255, 255, 255];
		}
	}
}

dojo.graphics.color.hex2rgb = function(hex) {
	var hexNum = "0123456789ABCDEF";
	var rgb = new Array(3);
	if( hex.indexOf("#") == 0 ) { hex = hex.substring(1); }
	hex = hex.toUpperCase();
	if(hex.replace(new RegExp("["+hexNum+"]", "g"), "") != "") {
		return null;
	}
	if( hex.length == 3 ) {
		rgb[0] = hex.charAt(0) + hex.charAt(0)
		rgb[1] = hex.charAt(1) + hex.charAt(1)
		rgb[2] = hex.charAt(2) + hex.charAt(2);
	} else {
		rgb[0] = hex.substring(0, 2);
		rgb[1] = hex.substring(2, 4);
		rgb[2] = hex.substring(4);
	}
	for(var i = 0; i < rgb.length; i++) {
		rgb[i] = hexNum.indexOf(rgb[i].charAt(0)) * 16 + hexNum.indexOf(rgb[i].charAt(1));
	}
	return rgb;
}

dojo.graphics.color.rgb2hex = function(r, g, b) {
	if(dojo.lang.isArray(r)) {
		g = r[1] || 0;
		b = r[2] || 0;
		r = r[0] || 0;
	}
	var ret = dojo.lang.map([r, g, b], function(x) {
		x = new Number(x);
		var s = x.toString(16);
		while(s.length < 2) { s = "0" + s; }
		return s;
	});
	ret.unshift("#");
	return ret.join("");
}

dojo.provide("dojo.uri.Uri");

dojo.uri = new function() {
	this.joinPath = function() {
		// DEPRECATED: use the dojo.uri.Uri object instead
		var arr = [];
		for(var i = 0; i < arguments.length; i++) { arr.push(arguments[i]); }
		return arr.join("/").replace(/\/{2,}/g, "/").replace(/((https*|ftps*):)/i, "$1/");
	}
	
	this.dojoUri = function (uri) {
		// returns a Uri object resolved relative to the dojo root
		return new dojo.uri.Uri(dojo.hostenv.getBaseScriptUri(), uri);
	}
		
	this.Uri = function (/*uri1, uri2, [...]*/) {
		// An object representing a Uri.
		// Each argument is evaluated in order relative to the next until
		// a conanical uri is producued. To get an absolute Uri relative
		// to the current document use
		//      new dojo.uri.Uri(document.baseURI, uri)

		// TODO: support for IPv6, see RFC 2732

		// resolve uri components relative to each other
		var uri = arguments[0];
		for (var i = 1; i < arguments.length; i++) {
			if(!arguments[i]) { continue; }

			// Safari doesn't support this.constructor so we have to be explicit
			var relobj = new dojo.uri.Uri(arguments[i].toString());
			var uriobj = new dojo.uri.Uri(uri.toString());

			if (relobj.path == "" && relobj.scheme == null &&
				relobj.authority == null && relobj.query == null) {
				if (relobj.fragment != null) { uriobj.fragment = relobj.fragment; }
				relobj = uriobj;
			} else if (relobj.scheme == null) {
				relobj.scheme = uriobj.scheme;
			
				if (relobj.authority == null) {
					relobj.authority = uriobj.authority;
					
					if (relobj.path.charAt(0) != "/") {
						var path = uriobj.path.substring(0,
							uriobj.path.lastIndexOf("/") + 1) + relobj.path;

						var segs = path.split("/");
						for (var j = 0; j < segs.length; j++) {
							if (segs[j] == ".") {
								if (j == segs.length - 1) { segs[j] = ""; }
								else { segs.splice(j, 1); j--; }
							} else if (j > 0 && !(j == 1 && segs[0] == "") &&
								segs[j] == ".." && segs[j-1] != "..") {

								if (j == segs.length - 1) { segs.splice(j, 1); segs[j - 1] = ""; }
								else { segs.splice(j - 1, 2); j -= 2; }
							}
						}
						relobj.path = segs.join("/");
					}
				}
			}

			uri = "";
			if (relobj.scheme != null) { uri += relobj.scheme + ":"; }
			if (relobj.authority != null) { uri += "//" + relobj.authority; }
			uri += relobj.path;
			if (relobj.query != null) { uri += "?" + relobj.query; }
			if (relobj.fragment != null) { uri += "#" + relobj.fragment; }
		}

		this.uri = uri.toString();

		// break the uri into its main components
		var regexp = "^(([^:/?#]+):)?(//([^/?#]*))?([^?#]*)(\\?([^#]*))?(#(.*))?$";
	    var r = this.uri.match(new RegExp(regexp));

		this.scheme = r[2] || (r[1] ? "" : null);
		this.authority = r[4] || (r[3] ? "" : null);
		this.path = r[5]; // can never be undefined
		this.query = r[7] || (r[6] ? "" : null);
		this.fragment  = r[9] || (r[8] ? "" : null);
		
		if (this.authority != null) {
			// server based naming authority
			regexp = "^((([^:]+:)?([^@]+))@)?([^:]*)(:([0-9]+))?$";
			r = this.authority.match(new RegExp(regexp));
			
			this.user = r[3] || null;
			this.password = r[4] || null;
			this.host = r[5];
			this.port = r[7] || null;
		}
	
		this.toString = function(){ return this.uri; }
	}
};

dojo.provide("dojo.style");
dojo.require("dojo.graphics.color");
dojo.require("dojo.uri.Uri");
dojo.require("dojo.lang.common");

(function(){
	var h = dojo.render.html;
	var ds = dojo.style;
	var db = document["body"]||document["documentElement"];

	ds.boxSizing = {
		MARGIN_BOX: "margin-box",
		BORDER_BOX: "border-box",
		PADDING_BOX: "padding-box",
		CONTENT_BOX: "content-box"
	};
	var bs = ds.boxSizing;
	
	ds.getBoxSizing = function(node){
		if((h.ie)||(h.opera)){ 
			var cm = document["compatMode"];
			if((cm == "BackCompat")||(cm == "QuirksMode")){ 
				return bs.BORDER_BOX; 
			}else{
				return bs.CONTENT_BOX; 
			}
		}else{
			if(arguments.length == 0){ node = document.documentElement; }
			var sizing = ds.getStyle(node, "-moz-box-sizing");
			if(!sizing){ sizing = ds.getStyle(node, "box-sizing"); }
			return (sizing ? sizing : bs.CONTENT_BOX);
		}
	}

	/*

	The following several function use the dimensions shown below

		+-------------------------+
		|  margin                 |
		| +---------------------+ |
		| |  border             | |
		| | +-----------------+ | |
		| | |  padding        | | |
		| | | +-------------+ | | |
		| | | |   content   | | | |
		| | | +-------------+ | | |
		| | +-|-------------|-+ | |
		| +-|-|-------------|-|-+ |
		+-|-|-|-------------|-|-|-+
		| | | |             | | | |
		| | | |<- content ->| | | |
		| |<------ inner ------>| |
		|<-------- outer -------->|
		+-------------------------+

		* content-box

		|m|b|p|             |p|b|m|
		| |<------ offset ----->| |
		| | |<---- client --->| | |
		| | | |<-- width -->| | | |

		* border-box

		|m|b|p|             |p|b|m|
		| |<------ offset ----->| |
		| | |<---- client --->| | |
		| |<------ width ------>| |
	*/

	/*
		Notes:

		General:
			- Uncomputable values are returned as NaN.
			- setOuterWidth/Height return *false* if the outer size could not
			  be computed, otherwise *true*.
			- (sjmiles) knows no way to find the calculated values for auto-margins. 
			- All returned values are floating point in 'px' units. If a
			  non-zero computed style value is not specified in 'px', NaN is
			  returned.

		FF:
			- styles specified as '0' (unitless 0) show computed as '0pt'.

		IE:
			- clientWidth/Height are unreliable (0 unless the object has 'layout').
			- margins must be specified in px, or 0 (in any unit) for any
			  sizing function to work. Otherwise margins detect as 'auto'.
			- padding can be empty or, if specified, must be in px, or 0 (in
			  any unit) for any sizing function to work.

		Safari:
			- Safari defaults padding values to 'auto'.

		See the unit tests for examples of (un)computable values in a given browser.

	*/

	// FIXME: these work for some elements (e.g. DIV) but not others (e.g. TABLE, TEXTAREA)

	ds.isBorderBox = function(node){
		return (ds.getBoxSizing(node) == bs.BORDER_BOX);
	}

	ds.getUnitValue = function(node, cssSelector, autoIsZero){
		var s = ds.getComputedStyle(node, cssSelector);
		if((!s)||((s == 'auto')&&(autoIsZero))){ return { value: 0, units: 'px' }; }
		if(dojo.lang.isUndefined(s)){return ds.getUnitValue.bad;}
		// FIXME: is regex inefficient vs. parseInt or some manual test? 
		var match = s.match(/(\-?[\d.]+)([a-z%]*)/i);
		if (!match){return ds.getUnitValue.bad;}
		return { value: Number(match[1]), units: match[2].toLowerCase() };
	}
	// FIXME: 'bad' value should be 0?
	ds.getUnitValue.bad = { value: NaN, units: '' };
	
	ds.getPixelValue = function(node, cssSelector, autoIsZero){
		var result = ds.getUnitValue(node, cssSelector, autoIsZero);
		// FIXME: there is serious debate as to whether or not this is the right solution
		if(isNaN(result.value)){ return 0; }
		// FIXME: code exists for converting other units to px (see Dean Edward's IE7) 
		// but there are cross-browser complexities
		if((result.value)&&(result.units != 'px')){ return NaN; }
		return result.value;
	}
	
	// FIXME: deprecated
	ds.getNumericStyle = function() {
		dojo.deprecated('dojo.(style|html).getNumericStyle', 'in favor of dojo.(style|html).getPixelValue', '0.4');
		return ds.getPixelValue.apply(this, arguments); 
	}

	ds.setPositivePixelValue = function(node, selector, value){
		if(isNaN(value)){return false;}
		node.style[selector] = Math.max(0, value) + 'px'; 
		return true;
	}
	
	ds._sumPixelValues = function(node, selectors, autoIsZero){
		var total = 0;
		for(x=0; x<selectors.length; x++){
			total += ds.getPixelValue(node, selectors[x], autoIsZero);
		}
		return total;
	}

	ds.isPositionAbsolute = function(node){
		return (ds.getComputedStyle(node, 'position') == 'absolute');
	}

	ds.getBorderExtent = function(node, side){
		return (ds.getStyle(node, 'border-' + side + '-style') == 'none' ? 0 : ds.getPixelValue(node, 'border-' + side + '-width'));
	}

	ds.getMarginWidth = function(node){
		return ds._sumPixelValues(node, ["margin-left", "margin-right"], ds.isPositionAbsolute(node));
	}

	ds.getBorderWidth = function(node){
		return ds.getBorderExtent(node, 'left') + ds.getBorderExtent(node, 'right');
	}

	ds.getPaddingWidth = function(node){
		return ds._sumPixelValues(node, ["padding-left", "padding-right"], true);
	}

	ds.getPadBorderWidth = function(node) {
		return ds.getPaddingWidth(node) + ds.getBorderWidth(node);
	}
	
	ds.getContentBoxWidth = function(node){
		node = dojo.byId(node);
		return node.offsetWidth - ds.getPadBorderWidth(node);
	}

	ds.getBorderBoxWidth = function(node){
		node = dojo.byId(node);
		return node.offsetWidth;
	}

	ds.getMarginBoxWidth = function(node){
		return ds.getInnerWidth(node) + ds.getMarginWidth(node);
	}

	ds.setContentBoxWidth = function(node, pxWidth){
		node = dojo.byId(node);
		if (ds.isBorderBox(node)){
			pxWidth += ds.getPadBorderWidth(node);
		}
		return ds.setPositivePixelValue(node, "width", pxWidth);
	}

	ds.setMarginBoxWidth = function(node, pxWidth){
		node = dojo.byId(node);
		if (!ds.isBorderBox(node)){
			pxWidth -= ds.getPadBorderWidth(node);
		}
		pxWidth -= ds.getMarginWidth(node);
		return ds.setPositivePixelValue(node, "width", pxWidth);
	}

	// FIXME: deprecate and remove
	ds.getContentWidth = ds.getContentBoxWidth;
	ds.getInnerWidth = ds.getBorderBoxWidth;
	ds.getOuterWidth = ds.getMarginBoxWidth;
	ds.setContentWidth = ds.setContentBoxWidth;
	ds.setOuterWidth = ds.setMarginBoxWidth;

	ds.getMarginHeight = function(node){
		return ds._sumPixelValues(node, ["margin-top", "margin-bottom"], ds.isPositionAbsolute(node));
	}

	ds.getBorderHeight = function(node){
		return ds.getBorderExtent(node, 'top') + ds.getBorderExtent(node, 'bottom');
	}

	ds.getPaddingHeight = function(node){
		return ds._sumPixelValues(node, ["padding-top", "padding-bottom"], true);
	}

	ds.getPadBorderHeight = function(node) {
		return ds.getPaddingHeight(node) + ds.getBorderHeight(node);
	}
	
	ds.getContentBoxHeight = function(node){
		node = dojo.byId(node);
		return node.offsetHeight - ds.getPadBorderHeight(node);
	}

	ds.getBorderBoxHeight = function(node){
		node = dojo.byId(node);
		return node.offsetHeight; // FIXME: does this work?
	}

	ds.getMarginBoxHeight = function(node){
		return ds.getInnerHeight(node) + ds.getMarginHeight(node);
	}

	ds.setContentBoxHeight = function(node, pxHeight){
		node = dojo.byId(node);
		if (ds.isBorderBox(node)){
			pxHeight += ds.getPadBorderHeight(node);
		}
		return ds.setPositivePixelValue(node, "height", pxHeight);
	}

	ds.setMarginBoxHeight = function(node, pxHeight){
		node = dojo.byId(node);
		if (!ds.isBorderBox(node)){
			pxHeight -= ds.getPadBorderHeight(node);
		}
		pxHeight -= ds.getMarginHeight(node);
		return ds.setPositivePixelValue(node, "height", pxHeight);
	}

	// FIXME: deprecate and remove
	ds.getContentHeight = ds.getContentBoxHeight;
	ds.getInnerHeight = ds.getBorderBoxHeight;
	ds.getOuterHeight = ds.getMarginBoxHeight;
	ds.setContentHeight = ds.setContentBoxHeight;
	ds.setOuterHeight = ds.setMarginBoxHeight;

	/**
	 * dojo.style.getAbsolutePosition(xyz, true) returns xyz's position relative to the document.
	 * Itells you where you would position a node
	 * inside document.body such that it was on top of xyz.  Most people set the flag to true when calling
	 * getAbsolutePosition().
	 *
	 * dojo.style.getAbsolutePosition(xyz, false) returns xyz's position relative to the viewport.
	 * It returns the position that would be returned
	 * by event.clientX/Y if the mouse were directly over the top/left of this node.
	 */
	ds.getAbsolutePosition = ds.abs = function(node, includeScroll){
		var ret = [];
		ret.x = ret.y = 0;
		var st = dojo.html.getScrollTop();
		var sl = dojo.html.getScrollLeft();

		if(h.ie){
			with(node.getBoundingClientRect()){
				ret.x = left-2;
				ret.y = top-2;
			}
/**
		}else if(document.getBoxObjectFor){
			// mozilla
			var bo=document.getBoxObjectFor(node);
			ret.x=bo.x-sl;
			ret.y=bo.y-st;
**/		}else{
			if(node["offsetParent"]){
				var endNode;		
				// in Safari, if the node is an absolutely positioned child of
				// the body and the body has a margin the offset of the child
				// and the body contain the body's margins, so we need to end
				// at the body
				if(	(h.safari)&&
					(node.style.getPropertyValue("position") == "absolute")&&
					(node.parentNode == db)){
					endNode = db;
				}else{
					endNode = db.parentNode;
				}
				
				if(node.parentNode != db){
					ret.x -= ds.sumAncestorProperties(node, "scrollLeft");
					ret.y -= ds.sumAncestorProperties(node, "scrollTop");
				}
				do{
					var n = node["offsetLeft"];
					ret.x += isNaN(n) ? 0 : n;
					var m = node["offsetTop"];
					ret.y += isNaN(m) ? 0 : m;
					node = node.offsetParent;
				}while((node != endNode)&&(node != null));
			}else if(node["x"]&&node["y"]){
				ret.x += isNaN(node.x) ? 0 : node.x;
				ret.y += isNaN(node.y) ? 0 : node.y;
			}
		}

		// account for document scrolling!
		if(includeScroll){
			ret.y += st;
			ret.x += sl;
		}

		ret[0] = ret.x;
		ret[1] = ret.y;
		return ret;
	}

	ds.sumAncestorProperties = function(node, prop){
		node = dojo.byId(node);
		if(!node){ return 0; } // FIXME: throw an error?
		
		var retVal = 0;
		while(node){
			var val = node[prop];
			if(val){
				retVal += val - 0;
			}
			node = node.parentNode;
		}
		return retVal;
	}

	ds.getTotalOffset = function(node, type, includeScroll){
		node = dojo.byId(node);
		return ds.abs(node, includeScroll)[(type == "top") ? "y" : "x"];
	}

	ds.getAbsoluteX = ds.totalOffsetLeft = function(node, includeScroll){
		return ds.getTotalOffset(node, "left", includeScroll);
	}

	ds.getAbsoluteY = ds.totalOffsetTop = function(node, includeScroll){
		return ds.getTotalOffset(node, "top", includeScroll);
	}

	ds.styleSheet = null;

	// FIXME: this is a really basic stub for adding and removing cssRules, but
	// it assumes that you know the index of the cssRule that you want to add 
	// or remove, making it less than useful.  So we need something that can 
	// search for the selector that you you want to remove.
	ds.insertCssRule = function(selector, declaration, index) {
		if (!ds.styleSheet) {
			if (document.createStyleSheet) { // IE
				ds.styleSheet = document.createStyleSheet();
			} else if (document.styleSheets[0]) { // rest
				// FIXME: should create a new style sheet here
				// fall back on an exsiting style sheet
				ds.styleSheet = document.styleSheets[0];
			} else { return null; } // fail
		}

		if (arguments.length < 3) { // index may == 0
			if (ds.styleSheet.cssRules) { // W3
				index = ds.styleSheet.cssRules.length;
			} else if (ds.styleSheet.rules) { // IE
				index = ds.styleSheet.rules.length;
			} else { return null; } // fail
		}

		if (ds.styleSheet.insertRule) { // W3
			var rule = selector + " { " + declaration + " }";
			return ds.styleSheet.insertRule(rule, index);
		} else if (ds.styleSheet.addRule) { // IE
			return ds.styleSheet.addRule(selector, declaration, index);
		} else { return null; } // fail
	}

	ds.removeCssRule = function(index){
		if(!ds.styleSheet){
			dojo.debug("no stylesheet defined for removing rules");
			return false;
		}
		if(h.ie){
			if(!index){
				index = ds.styleSheet.rules.length;
				ds.styleSheet.removeRule(index);
			}
		}else if(document.styleSheets[0]){
			if(!index){
				index = ds.styleSheet.cssRules.length;
			}
			ds.styleSheet.deleteRule(index);
		}
		return true;
	}

	// calls css by XmlHTTP and inserts it into DOM as <style [widgetType="widgetType"]> *downloaded cssText*</style>
	ds.insertCssFile = function(URI, doc, checkDuplicates){
		if(!URI){ return; }
		if(!doc){ doc = document; }
		var cssStr = dojo.hostenv.getText(URI);
		cssStr = ds.fixPathsInCssText(cssStr, URI);

		if(checkDuplicates){
			var styles = doc.getElementsByTagName("style");
			var cssText = "";
			for(var i = 0; i<styles.length; i++){
				cssText = (styles[i].styleSheet && styles[i].styleSheet.cssText) ? styles[i].styleSheet.cssText : styles[i].innerHTML;
				if(cssStr == cssText){ return; }
			}
		}

		var style = ds.insertCssText(cssStr);
		// insert custom attribute ex dbgHref="../foo.css" usefull when debugging in DOM inspectors, no?
		if(style && djConfig.isDebug){
			style.setAttribute("dbgHref", URI);
		}
		return style
	}

	// DomNode Style  = insertCssText(String ".dojoMenu {color: green;}"[, DomDoc document, dojo.uri.Uri Url ])
	ds.insertCssText = function(cssStr, doc, URI){
		if(!cssStr){ return; }
		if(!doc){ doc = document; }
		if(URI){// fix paths in cssStr
			cssStr = ds.fixPathsInCssText(cssStr, URI);
		}
		var style = doc.createElement("style");
		style.setAttribute("type", "text/css");
		if(style.styleSheet){// IE
			style.styleSheet.cssText = cssStr;
		} else {// w3c
			var cssText = doc.createTextNode(cssStr);
			style.appendChild(cssText);
		}
		var head = doc.getElementsByTagName("head")[0];
		if(!head){ // must have a head tag 
			dojo.debug("No head tag in document, aborting styles");
		}else{
			head.appendChild(style);
		}
		return style;
	}

	// String cssText = fixPathsInCssText(String cssStr, dojo.uri.Uri URI)
	// usage: cssText comes from dojoroot/src/widget/templates/HtmlFoobar.css
	// 	it has .dojoFoo { background-image: url(images/bar.png);} 
	//	then uri should point to dojoroot/src/widget/templates/
	ds.fixPathsInCssText = function(cssStr, URI){
		if(!cssStr || !URI){ return; }
		var pos = 0; var str = ""; var url = "";
		while(pos!=-1){
			pos = 0;url = "";
			pos = cssStr.indexOf("url(", pos);
			if(pos<0){ break; }
			str += cssStr.slice(0,pos+4);
			cssStr = cssStr.substring(pos+4, cssStr.length);
			url += cssStr.match(/^[\t\s\w()\/.\\'"-:#=&?]*\)/)[0]; // url string
			cssStr = cssStr.substring(url.length-1, cssStr.length); // remove url from css string til next loop
			url = url.replace(/^[\s\t]*(['"]?)([\w()\/.\\'"-:#=&?]*)\1[\s\t]*?\)/,"$2"); // clean string
			if(url.search(/(file|https?|ftps?):\/\//)==-1){
				url = (new dojo.uri.Uri(URI,url).toString());
			}
			str += url;
		};
		return str+cssStr;
	}

	ds.getBackgroundColor = function(node) {
		node = dojo.byId(node);
		var color;
		do{
			color = ds.getStyle(node, "background-color");
			// Safari doesn't say "transparent"
			if(color.toLowerCase() == "rgba(0, 0, 0, 0)") { color = "transparent"; }
			if(node == document.getElementsByTagName("body")[0]) { node = null; break; }
			node = node.parentNode;
		}while(node && dojo.lang.inArray(color, ["transparent", ""]));
		if(color == "transparent"){
			color = [255, 255, 255, 0];
		}else{
			color = dojo.graphics.color.extractRGB(color);
		}
		return color;
	}

	ds.getComputedStyle = function(node, cssSelector, inValue){
		node = dojo.byId(node);
		// cssSelector may actually be in camel case, so force selector version
		var cssSelector = ds.toSelectorCase(cssSelector);
		var property = ds.toCamelCase(cssSelector);
		if(!node || !node.style){
			return inValue;
		}else if(document.defaultView){ // W3, gecko, KHTML
			try{			
				var cs = document.defaultView.getComputedStyle(node, "");
				if (cs){ 
					return cs.getPropertyValue(cssSelector);
				} 
			}catch(e){ // reports are that Safari can throw an exception above
				if (node.style.getPropertyValue){ // W3
					return node.style.getPropertyValue(cssSelector);
				}else return inValue;
			}
		}else if(node.currentStyle){ // IE
			return node.currentStyle[property];
		}if(node.style.getPropertyValue){ // W3
			return node.style.getPropertyValue(cssSelector);
		}else{
			return inValue;
		}
	}

	/** 
	 * Retrieve a property value from a node's style object.
	 */
	ds.getStyleProperty = function(node, cssSelector){
		node = dojo.byId(node);
		// FIXME: should we use node.style.getPropertyValue over style[property]?
		// style[property] works in all (modern) browsers, getPropertyValue is W3 but not supported in IE
		// FIXME: what about runtimeStyle?
		return (node && node.style ? node.style[ds.toCamelCase(cssSelector)] : undefined);
	}

	/** 
	 * Retrieve a property value from a node's style object.
	 */
	ds.getStyle = function(node, cssSelector){
		var value = ds.getStyleProperty(node, cssSelector);
		return (value ? value : ds.getComputedStyle(node, cssSelector));
	}

	ds.setStyle = function(node, cssSelector, value){
		node = dojo.byId(node);
		if(node && node.style){
			var camelCased = ds.toCamelCase(cssSelector);
			node.style[camelCased] = value;
		}
	}

	ds.toCamelCase = function(selector) {
		var arr = selector.split('-'), cc = arr[0];
		for(var i = 1; i < arr.length; i++) {
			cc += arr[i].charAt(0).toUpperCase() + arr[i].substring(1);
		}
		return cc;		
	}

	ds.toSelectorCase = function(selector) {
		return selector.replace(/([A-Z])/g, "-$1" ).toLowerCase() ;
	}

	/* float between 0.0 (transparent) and 1.0 (opaque) */
	ds.setOpacity = function setOpacity(node, opacity, dontFixOpacity) {
		node = dojo.byId(node);
		if(!dontFixOpacity){
			if( opacity >= 1.0){
				if(h.ie){
					ds.clearOpacity(node);
					return;
				}else{
					opacity = 0.999999;
				}
			}else if( opacity < 0.0){ opacity = 0; }
		}
		if(h.ie){
			if(node.nodeName.toLowerCase() == "tr"){
				// FIXME: is this too naive? will we get more than we want?
				var tds = node.getElementsByTagName("td");
				for(var x=0; x<tds.length; x++){
					tds[x].style.filter = "Alpha(Opacity="+opacity*100+")";
				}
			}
			node.style.filter = "Alpha(Opacity="+opacity*100+")";
		}else if(h.moz){
			node.style.opacity = opacity; // ffox 1.0 directly supports "opacity"
			node.style.MozOpacity = opacity;
		}else if(h.safari){
			node.style.opacity = opacity; // 1.3 directly supports "opacity"
			node.style.KhtmlOpacity = opacity;
		}else{
			node.style.opacity = opacity;
		}
	}
		
	ds.getOpacity = function getOpacity (node){
		node = dojo.byId(node);
		if(h.ie){
			var opac = (node.filters && node.filters.alpha &&
				typeof node.filters.alpha.opacity == "number"
				? node.filters.alpha.opacity : 100) / 100;
		}else{
			var opac = node.style.opacity || node.style.MozOpacity ||
				node.style.KhtmlOpacity || 1;
		}
		return opac >= 0.999999 ? 1.0 : Number(opac);
	}

	ds.clearOpacity = function clearOpacity(node){
		node = dojo.byId(node);
		var ns = node.style;
		if(h.ie){
			try {
				if( node.filters && node.filters.alpha ){
					ns.filter = ""; // FIXME: may get rid of other filter effects
				}
			} catch(e) {
				/*
				 * IE7 gives error if node.filters not set;
				 * don't know why or how to workaround (other than this)
				 */
			}
		}else if(h.moz){
			ns.opacity = 1;
			ns.MozOpacity = 1;
		}else if(h.safari){
			ns.opacity = 1;
			ns.KhtmlOpacity = 1;
		}else{
			ns.opacity = 1;
		}
	}

	ds._toggle = function(node, tester, setter){
		node = dojo.byId(node);
		setter(node, !tester(node));
		return tester(node);
	}

	// show/hide are library constructs

	// show() 
	// if the node.style.display == 'none' then 
	// set style.display to '' or the value cached by hide()
	ds.show = function(node){
		node = dojo.byId(node);
		if(ds.getStyleProperty(node, 'display')=='none'){
			ds.setStyle(node, 'display', (node.dojoDisplayCache||''));
			node.dojoDisplayCache = undefined;	// cannot use delete on a node in IE6
		}
	}

	// if the node.style.display == 'none' then 
	// set style.display to '' or the value cached by hide()
	ds.hide = function(node){
		node = dojo.byId(node);
		if(typeof node["dojoDisplayCache"] == "undefined"){ // it could == '', so we cannot say !node.dojoDisplayCount
			var d = ds.getStyleProperty(node, 'display')
			if(d!='none'){
				node.dojoDisplayCache = d;
			}
		}
		ds.setStyle(node, 'display', 'none');
	}

	// setShowing() calls show() if showing is true, hide() otherwise
	ds.setShowing = function(node, showing){
		ds[(showing ? 'show' : 'hide')](node);
	}

	// isShowing() is true if the node.style.display is not 'none'
	// FIXME: returns true if node is bad, isHidden would be easier to make correct
	ds.isShowing = function(node){
		return (ds.getStyleProperty(node, 'display') != 'none');
	}

	// Call setShowing() on node with the complement of isShowing(), then return the new value of isShowing()
	ds.toggleShowing = function(node){
		return ds._toggle(node, ds.isShowing, ds.setShowing);
	}

	// display is a CSS concept

	// Simple mapping of tag names to display values
	// FIXME: simplistic 
	ds.displayMap = { tr: '', td: '', th: '', img: 'inline', span: 'inline', input: 'inline', button: 'inline' };

	// Suggest a value for the display property that will show 'node' based on it's tag
	ds.suggestDisplayByTagName = function(node)
	{
		node = dojo.byId(node);
		if(node && node.tagName){
			var tag = node.tagName.toLowerCase();
			return (tag in ds.displayMap ? ds.displayMap[tag] : 'block');
		}
	}

	// setDisplay() sets the value of style.display to value of 'display' parameter if it is a string.
	// Otherwise, if 'display' is false, set style.display to 'none'.
	// Finally, set 'display' to a suggested display value based on the node's tag
	ds.setDisplay = function(node, display){
		ds.setStyle(node, 'display', (dojo.lang.isString(display) ? display : (display ? ds.suggestDisplayByTagName(node) : 'none')));
	}

	// isDisplayed() is true if the the computed display style for node is not 'none'
	// FIXME: returns true if node is bad, isNotDisplayed would be easier to make correct
	ds.isDisplayed = function(node){
		return (ds.getComputedStyle(node, 'display') != 'none');
	}

	// Call setDisplay() on node with the complement of isDisplayed(), then
	// return the new value of isDisplayed()
	ds.toggleDisplay = function(node){
		return ds._toggle(node, ds.isDisplayed, ds.setDisplay);
	}

	// visibility is a CSS concept

	// setVisibility() sets the value of style.visibility to value of
	// 'visibility' parameter if it is a string.
	// Otherwise, if 'visibility' is false, set style.visibility to 'hidden'.
	// Finally, set style.visibility to 'visible'.
	ds.setVisibility = function(node, visibility){
		ds.setStyle(node, 'visibility', (dojo.lang.isString(visibility) ? visibility : (visibility ? 'visible' : 'hidden')));
	}

	// isVisible() is true if the the computed visibility style for node is not 'hidden'
	// FIXME: returns true if node is bad, isInvisible would be easier to make correct
	ds.isVisible = function(node){
		return (ds.getComputedStyle(node, 'visibility') != 'hidden');
	}

	// Call setVisibility() on node with the complement of isVisible(), then
	// return the new value of isVisible()
	ds.toggleVisibility = function(node){
		return ds._toggle(node, ds.isVisible, ds.setVisibility);
	}

	// in: coordinate array [x,y,w,h] or dom node
	// return: coordinate array
	ds.toCoordinateArray = function(coords, includeScroll) {
		if(dojo.lang.isArray(coords)){
			// coords is already an array (of format [x,y,w,h]), just return it
			while ( coords.length < 4 ) { coords.push(0); }
			while ( coords.length > 4 ) { coords.pop(); }
			var ret = coords;
		} else {
			// coords is an dom object (or dom object id); return it's coordinates
			var node = dojo.byId(coords);
			var pos = ds.getAbsolutePosition(node, includeScroll);
			var ret = [
				pos.x,
				pos.y,
				ds.getBorderBoxWidth(node),
				ds.getBorderBoxHeight(node)
			];
		}
		ret.x = ret[0];
		ret.y = ret[1];
		ret.w = ret[2];
		ret.h = ret[3];
		return ret;
	};
})();

dojo.provide("dojo.string.common");

dojo.require("dojo.string");

/**
 * Trim whitespace from 'str'. If 'wh' > 0,
 * only trim from start, if 'wh' < 0, only trim
 * from end, otherwise trim both ends
 */
dojo.string.trim = function(str, wh){
	if(!str.replace){ return str; }
	if(!str.length){ return str; }
	var re = (wh > 0) ? (/^\s+/) : (wh < 0) ? (/\s+$/) : (/^\s+|\s+$/g);
	return str.replace(re, "");
}

/**
 * Trim whitespace at the beginning of 'str'
 */
dojo.string.trimStart = function(str) {
	return dojo.string.trim(str, 1);
}

/**
 * Trim whitespace at the end of 'str'
 */
dojo.string.trimEnd = function(str) {
	return dojo.string.trim(str, -1);
}

/**
 * Return 'str' repeated 'count' times, optionally
 * placing 'separator' between each rep
 */
dojo.string.repeat = function(str, count, separator) {
	var out = "";
	for(var i = 0; i < count; i++) {
		out += str;
		if(separator && i < count - 1) {
			out += separator;
		}
	}
	return out;
}

/**
 * Pad 'str' to guarantee that it is at least 'len' length
 * with the character 'c' at either the start (dir=1) or
 * end (dir=-1) of the string
 */
dojo.string.pad = function(str, len/*=2*/, c/*='0'*/, dir/*=1*/) {
	var out = String(str);
	if(!c) {
		c = '0';
	}
	if(!dir) {
		dir = 1;
	}
	while(out.length < len) {
		if(dir > 0) {
			out = c + out;
		} else {
			out += c;
		}
	}
	return out;
}

/** same as dojo.string.pad(str, len, c, 1) */
dojo.string.padLeft = function(str, len, c) {
	return dojo.string.pad(str, len, c, 1);
}

/** same as dojo.string.pad(str, len, c, -1) */
dojo.string.padRight = function(str, len, c) {
	return dojo.string.pad(str, len, c, -1);
}

dojo.provide("dojo.string");
dojo.require("dojo.string.common");

dojo.provide("dojo.html");

dojo.require("dojo.lang.func");
dojo.require("dojo.dom");
dojo.require("dojo.style");
dojo.require("dojo.string");

dojo.lang.mixin(dojo.html, dojo.dom);
dojo.lang.mixin(dojo.html, dojo.style);

// FIXME: we are going to assume that we can throw any and every rendering
// engine into the IE 5.x box model. In Mozilla, we do this w/ CSS.
// Need to investigate for KHTML and Opera

dojo.html.clearSelection = function(){
	try{
		if(window["getSelection"]){ 
			if(dojo.render.html.safari){
				// pulled from WebCore/ecma/kjs_window.cpp, line 2536
				window.getSelection().collapse();
			}else{
				window.getSelection().removeAllRanges();
			}
		}else if(document.selection){
			if(document.selection.empty){
				document.selection.empty();
			}else if(document.selection.clear){
				document.selection.clear();
			}
		}
		return true;
	}catch(e){
		dojo.debug(e);
		return false;
	}
}

dojo.html.disableSelection = function(element){
	element = dojo.byId(element)||document.body;
	var h = dojo.render.html;
	
	if(h.mozilla){
		element.style.MozUserSelect = "none";
	}else if(h.safari){
		element.style.KhtmlUserSelect = "none"; 
	}else if(h.ie){
		element.unselectable = "on";
	}else{
		return false;
	}
	return true;
}

dojo.html.enableSelection = function(element){
	element = dojo.byId(element)||document.body;
	
	var h = dojo.render.html;
	if(h.mozilla){ 
		element.style.MozUserSelect = ""; 
	}else if(h.safari){
		element.style.KhtmlUserSelect = "";
	}else if(h.ie){
		element.unselectable = "off";
	}else{
		return false;
	}
	return true;
}

dojo.html.selectElement = function(element){
	element = dojo.byId(element);
	if(document.selection && document.body.createTextRange){ // IE
		var range = document.body.createTextRange();
		range.moveToElementText(element);
		range.select();
	}else if(window["getSelection"]){
		var selection = window.getSelection();
		// FIXME: does this work on Safari?
		if(selection["selectAllChildren"]){ // Mozilla
			selection.selectAllChildren(element);
		}
	}
}

dojo.html.selectInputText = function(element){
	element = dojo.byId(element);
	if(document.selection && document.body.createTextRange){ // IE
		var range = element.createTextRange();
		range.moveStart("character", 0);
		range.moveEnd("character", element.value.length);
		range.select();
	}else if(window["getSelection"]){
		var selection = window.getSelection();
		// FIXME: does this work on Safari?
		element.setSelectionRange(0, element.value.length);
	}
	element.focus();
}


dojo.html.isSelectionCollapsed = function(){
	if(document["selection"]){ // IE
		return document.selection.createRange().text == "";
	}else if(window["getSelection"]){
		var selection = window.getSelection();
		if(dojo.lang.isString(selection)){ // Safari
			return selection == "";
		}else{ // Mozilla/W3
			return selection.isCollapsed;
		}
	}
}

dojo.html.getEventTarget = function(evt){
	if(!evt) { evt = window.event || {} };
	var t = (evt.srcElement ? evt.srcElement : (evt.target ? evt.target : null));
	while((t)&&(t.nodeType!=1)){ t = t.parentNode; }
	return t;
}

dojo.html.getDocumentWidth = function(){
	dojo.deprecated("dojo.html.getDocument* has been deprecated in favor of dojo.html.getViewport*");
	return dojo.html.getViewportWidth();
}

dojo.html.getDocumentHeight = function(){
	dojo.deprecated("dojo.html.getDocument* has been deprecated in favor of dojo.html.getViewport*");
	return dojo.html.getViewportHeight();
}

dojo.html.getDocumentSize = function(){
	dojo.deprecated("dojo.html.getDocument* has been deprecated in favor of dojo.html.getViewport*");
	return dojo.html.getViewportSize();
}

dojo.html.getViewportWidth = function(){
	var w = 0;

	if(window.innerWidth){
		w = window.innerWidth;
	}

	if(dojo.exists(document, "documentElement.clientWidth")){
		// IE6 Strict
		var w2 = document.documentElement.clientWidth;
		// this lets us account for scrollbars
		if(!w || w2 && w2 < w) {
			w = w2;
		}
		return w;
	}

	if(document.body){
		// IE
		return document.body.clientWidth;
	}

	return 0;
}

dojo.html.getViewportHeight = function(){
	if (window.innerHeight){
		return window.innerHeight;
	}

	if (dojo.exists(document, "documentElement.clientHeight")){
		// IE6 Strict
		return document.documentElement.clientHeight;
	}

	if (document.body){
		// IE
		return document.body.clientHeight;
	}

	return 0;
}

dojo.html.getViewportSize = function(){
	var ret = [dojo.html.getViewportWidth(), dojo.html.getViewportHeight()];
	ret.w = ret[0];
	ret.h = ret[1];
	return ret;
}

dojo.html.getScrollTop = function(){
	return window.pageYOffset || document.documentElement.scrollTop || document.body.scrollTop || 0;
}

dojo.html.getScrollLeft = function(){
	return window.pageXOffset || document.documentElement.scrollLeft || document.body.scrollLeft || 0;
}

dojo.html.getScrollOffset = function(){
	var off = [dojo.html.getScrollLeft(), dojo.html.getScrollTop()];
	off.x = off[0];
	off.y = off[1];
	return off;
}

dojo.html.getParentOfType = function(node, type){
	dojo.deprecated("dojo.html.getParentOfType has been deprecated in favor of dojo.html.getParentByType*");
	return dojo.html.getParentByType(node, type);
}

dojo.html.getParentByType = function(node, type) {
	var parent = dojo.byId(node);
	type = type.toLowerCase();
	while((parent)&&(parent.nodeName.toLowerCase()!=type)){
		if(parent==(document["body"]||document["documentElement"])){
			return null;
		}
		parent = parent.parentNode;
	}
	return parent;
}

// RAR: this function comes from nwidgets and is more-or-less unmodified.
// We should probably look ant Burst and f(m)'s equivalents
dojo.html.getAttribute = function(node, attr){
	node = dojo.byId(node);
	// FIXME: need to add support for attr-specific accessors
	if((!node)||(!node.getAttribute)){
		// if(attr !== 'nwType'){
		//	alert("getAttr of '" + attr + "' with bad node"); 
		// }
		return null;
	}
	var ta = typeof attr == 'string' ? attr : new String(attr);

	// first try the approach most likely to succeed
	var v = node.getAttribute(ta.toUpperCase());
	if((v)&&(typeof v == 'string')&&(v!="")){ return v; }

	// try returning the attributes value, if we couldn't get it as a string
	if(v && v.value){ return v.value; }

	// this should work on Opera 7, but it's a little on the crashy side
	if((node.getAttributeNode)&&(node.getAttributeNode(ta))){
		return (node.getAttributeNode(ta)).value;
	}else if(node.getAttribute(ta)){
		return node.getAttribute(ta);
	}else if(node.getAttribute(ta.toLowerCase())){
		return node.getAttribute(ta.toLowerCase());
	}
	return null;
}
	
/**
 *	Determines whether or not the specified node carries a value for the
 *	attribute in question.
 */
dojo.html.hasAttribute = function(node, attr){
	node = dojo.byId(node);
	return dojo.html.getAttribute(node, attr) ? true : false;
}
	
/**
 * Returns the string value of the list of CSS classes currently assigned
 * directly to the node in question. Returns an empty string if no class attribute
 * is found;
 */
dojo.html.getClass = function(node){
	node = dojo.byId(node);
	if(!node){ return ""; }
	var cs = "";
	if(node.className){
		cs = node.className;
	}else if(dojo.html.hasAttribute(node, "class")){
		cs = dojo.html.getAttribute(node, "class");
	}
	return dojo.string.trim(cs);
}

/**
 * Returns an array of CSS classes currently assigned
 * directly to the node in question. Returns an empty array if no classes
 * are found;
 */
dojo.html.getClasses = function(node) {
	var c = dojo.html.getClass(node);
	return (c == "") ? [] : c.split(/\s+/g);
}

/**
 * Returns whether or not the specified classname is a portion of the
 * class list currently applied to the node. Does not cover cascaded
 * styles, only classes directly applied to the node.
 */
dojo.html.hasClass = function(node, classname){
	return dojo.lang.inArray(dojo.html.getClasses(node), classname);
}

/**
 * Adds the specified class to the beginning of the class list on the
 * passed node. This gives the specified class the highest precidence
 * when style cascading is calculated for the node. Returns true or
 * false; indicating success or failure of the operation, respectively.
 */
dojo.html.prependClass = function(node, classStr){
	classStr += " " + dojo.html.getClass(node);
	return dojo.html.setClass(node, classStr);
}

/**
 * Adds the specified class to the end of the class list on the
 *	passed &node;. Returns &true; or &false; indicating success or failure.
 */
dojo.html.addClass = function(node, classStr){
	if (dojo.html.hasClass(node, classStr)) {
	  return false;
	}
	classStr = dojo.string.trim(dojo.html.getClass(node) + " " + classStr);
	return dojo.html.setClass(node, classStr);
}

/**
 *	Clobbers the existing list of classes for the node, replacing it with
 *	the list given in the 2nd argument. Returns true or false
 *	indicating success or failure.
 */
dojo.html.setClass = function(node, classStr){
	node = dojo.byId(node);
	var cs = new String(classStr);
	try{
		if(typeof node.className == "string"){
			node.className = cs;
		}else if(node.setAttribute){
			node.setAttribute("class", classStr);
			node.className = cs;
		}else{
			return false;
		}
	}catch(e){
		dojo.debug("dojo.html.setClass() failed", e);
	}
	return true;
}

/**
 * Removes the className from the node;. Returns
 * true or false indicating success or failure.
 */ 
dojo.html.removeClass = function(node, classStr, allowPartialMatches){
	var classStr = dojo.string.trim(new String(classStr));

	try{
		var cs = dojo.html.getClasses(node);
		var nca	= [];
		if(allowPartialMatches){
			for(var i = 0; i<cs.length; i++){
				if(cs[i].indexOf(classStr) == -1){ 
					nca.push(cs[i]);
				}
			}
		}else{
			for(var i=0; i<cs.length; i++){
				if(cs[i] != classStr){ 
					nca.push(cs[i]);
				}
			}
		}
		dojo.html.setClass(node, nca.join(" "));
	}catch(e){
		dojo.debug("dojo.html.removeClass() failed", e);
	}

	return true;
}

/**
 * Replaces 'oldClass' and adds 'newClass' to node
 */
dojo.html.replaceClass = function(node, newClass, oldClass) {
	dojo.html.removeClass(node, oldClass);
	dojo.html.addClass(node, newClass);
}

// Enum type for getElementsByClass classMatchType arg:
dojo.html.classMatchType = {
	ContainsAll : 0, // all of the classes are part of the node's class (default)
	ContainsAny : 1, // any of the classes are part of the node's class
	IsOnly : 2 // only all of the classes are part of the node's class
}


/**
 * Returns an array of nodes for the given classStr, children of a
 * parent, and optionally of a certain nodeType
 */
dojo.html.getElementsByClass = function(classStr, parent, nodeType, classMatchType){
	parent = dojo.byId(parent) || document;
	var classes = classStr.split(/\s+/g);
	var nodes = [];
	if( classMatchType != 1 && classMatchType != 2 ) classMatchType = 0; // make it enum
	var reClass = new RegExp("(\\s|^)((" + classes.join(")|(") + "))(\\s|$)");

	// FIXME: doesn't have correct parent support!
	if(!nodeType){ nodeType = "*"; }
	var candidateNodes = parent.getElementsByTagName(nodeType);

	var node, i = 0;
	outer:
	while (node = candidateNodes[i++]) {
		var nodeClasses = dojo.html.getClasses(node);
		if(nodeClasses.length == 0) { continue outer; }
		var matches = 0;

		for(var j = 0; j < nodeClasses.length; j++) {
			if( reClass.test(nodeClasses[j]) ) {
				if( classMatchType == dojo.html.classMatchType.ContainsAny ) {
					nodes.push(node);
					continue outer;
				} else {
					matches++;
				}
			} else {
				if( classMatchType == dojo.html.classMatchType.IsOnly ) {
					continue outer;
				}
			}
		}

		if( matches == classes.length ) {
			if( classMatchType == dojo.html.classMatchType.IsOnly && matches == nodeClasses.length ) {
				nodes.push(node);
			} else if( classMatchType == dojo.html.classMatchType.ContainsAll ) {
				nodes.push(node);
			}
		}
	}
	
	return nodes;
}

dojo.html.getElementsByClassName = dojo.html.getElementsByClass;

/**
 * Returns the mouse position relative to the document (not the viewport).
 * For example, if you have a document that is 10000px tall,
 * but your browser window is only 100px tall,
 * if you scroll to the bottom of the document and call this function it
 * will return {x: 0, y: 10000}
 */
dojo.html.getCursorPosition = function(e){
	e = e || window.event;
	var cursor = {x:0, y:0};
	if(e.pageX || e.pageY){
		cursor.x = e.pageX;
		cursor.y = e.pageY;
	}else{
		var de = document.documentElement;
		var db = document.body;
		cursor.x = e.clientX + ((de||db)["scrollLeft"]) - ((de||db)["clientLeft"]);
		cursor.y = e.clientY + ((de||db)["scrollTop"]) - ((de||db)["clientTop"]);
	}
	return cursor;
}

dojo.html.overElement = function(element, e){
	element = dojo.byId(element);
	var mouse = dojo.html.getCursorPosition(e);

	with(dojo.html){
		var top = getAbsoluteY(element, true);
		var bottom = top + getInnerHeight(element);
		var left = getAbsoluteX(element, true);
		var right = left + getInnerWidth(element);
	}
	
	return (mouse.x >= left && mouse.x <= right &&
		mouse.y >= top && mouse.y <= bottom);
}

dojo.html.setActiveStyleSheet = function(title){
	var i = 0, a, els = document.getElementsByTagName("link");
	while (a = els[i++]) {
		if(a.getAttribute("rel").indexOf("style") != -1 && a.getAttribute("title")){
			a.disabled = true;
			if (a.getAttribute("title") == title) { a.disabled = false; }
		}
	}
}

dojo.html.getActiveStyleSheet = function(){
	var i = 0, a, els = document.getElementsByTagName("link");
	while (a = els[i++]) {
		if (a.getAttribute("rel").indexOf("style") != -1 &&
			a.getAttribute("title") && !a.disabled) { return a.getAttribute("title"); }
	}
	return null;
}

dojo.html.getPreferredStyleSheet = function(){
	var i = 0, a, els = document.getElementsByTagName("link");
	while (a = els[i++]) {
		if(a.getAttribute("rel").indexOf("style") != -1
			&& a.getAttribute("rel").indexOf("alt") == -1
			&& a.getAttribute("title")) { return a.getAttribute("title"); }
	}
	return null;
}

dojo.html.body = function(){
	dojo.deprecated("dojo.html.body", "use document.body instead");
	return document.body || document.getElementsByTagName("body")[0];
}

/**
 * Like dojo.dom.isTag, except case-insensitive
**/
dojo.html.isTag = function(node /* ... */) {
	node = dojo.byId(node);
	if(node && node.tagName) {
		var arr = dojo.lang.map(dojo.lang.toArray(arguments, 1),
			function(a) { return String(a).toLowerCase(); });
		return arr[ dojo.lang.find(node.tagName.toLowerCase(), arr) ] || "";
	}
	return "";
}

dojo.html._callExtrasDeprecated = function(inFunc, args) {
	var module = "dojo.html.extras";
	dojo.deprecated("dojo.html." + inFunc + " has been moved to " + module);
	dojo["require"](module); // weird syntax to fool list-profile-deps (build)
	return dojo.html[inFunc].apply(dojo.html, args);
}

dojo.html.createNodesFromText = function() {
	return dojo.html._callExtrasDeprecated('createNodesFromText', arguments);
}

dojo.html.gravity = function() {
	return dojo.html._callExtrasDeprecated('gravity', arguments);
}

dojo.html.placeOnScreen = function() {
	return dojo.html._callExtrasDeprecated('placeOnScreen', arguments);
}

dojo.html.placeOnScreenPoint = function() {
	return dojo.html._callExtrasDeprecated('placeOnScreenPoint', arguments);
}

dojo.html.renderedTextContent = function() {
	return dojo.html._callExtrasDeprecated('renderedTextContent', arguments);
}

dojo.html.BackgroundIframe = function() {
	return dojo.html._callExtrasDeprecated('BackgroundIframe', arguments);
}

dojo.provide("dojo.lfx.Animation");
dojo.provide("dojo.lfx.Line");

dojo.require("dojo.lang.func");

/*
	Animation package based on Dan Pupius' work: http://pupius.co.uk/js/Toolkit.Drawing.js
*/
dojo.lfx.Line = function(start, end){
	this.start = start;
	this.end = end;
	if(dojo.lang.isArray(start)){
		var diff = [];
		dojo.lang.forEach(this.start, function(s,i){
			diff[i] = this.end[i] - s;
		}, this);
		
		this.getValue = function(/*float*/ n){
			var res = [];
			dojo.lang.forEach(this.start, function(s, i){
				res[i] = (diff[i] * n) + s;
			}, this);
			return res;
		}
	}else{
		var diff = end - start;
			
		this.getValue = function(/*float*/ n){
			//	summary: returns the point on the line
			//	n: a floating point number greater than 0 and less than 1
			return (diff * n) + this.start;
		}
	}
}

dojo.lfx.easeIn = function(n){
	//	summary: returns the point on an easing curve
	//	n: a floating point number greater than 0 and less than 1
	return Math.pow(n, 3);
}

dojo.lfx.easeOut = function(n){
	//	summary: returns the point on the line
	//	n: a floating point number greater than 0 and less than 1
	return ( 1 - Math.pow(1 - n, 3) );
}

dojo.lfx.easeInOut = function(n){
	//	summary: returns the point on the line
	//	n: a floating point number greater than 0 and less than 1
	return ( (3 * Math.pow(n, 2)) - (2 * Math.pow(n, 3)) );
}

dojo.lfx.IAnimation = function(){}
dojo.lang.extend(dojo.lfx.IAnimation, {
	// public properties
	curve: null,
	duration: 1000,
	easing: null,
	repeatCount: 0,
	rate: 25,
	
	// events
	handler: null,
	beforeBegin: null,
	onBegin: null,
	onAnimate: null,
	onEnd: null,
	onPlay: null,
	onPause: null,
	onStop: null,
	
	// public methods
	play: null,
	pause: null,
	stop: null,
	
	fire: function(evt, args){
		if(this[evt]){
			if(args){
				this[evt].apply(this, args);
			}else{
				this[evt].apply(this);
			}
		}
	},
	
	// private properties
	_active: false,
	_paused: false
});

dojo.lfx.Animation = function(/*Object*/ handlers, /*int*/ duration, /*Array*/ curve, /*function*/ easing, /*int*/ repeatCount, /*int*/ rate){
	//	summary
	//		a generic animation object that fires callbacks into it's handlers
	//		object at various states
	//	handlers
	//		object { 
	//			handler: function(){}, 
	//			onstart: function(){}, 
	//			onstop: function(){}, 
	//			onanimate: function(){}
	//		}
	dojo.lfx.IAnimation.call(this);
	if(dojo.lang.isNumber(handlers)||(!handlers && duration.getValue)){
		// no handlers argument:
		rate = repeatCount;
		repeatCount = easing;
		easing = curve;
		curve = duration;
		duration = handlers;
		handlers = null;
	}else if(handlers.getValue||dojo.lang.isArray(handlers)){
		// no handlers or duration:
		rate = easing;
		repeatCount = curve;
		easing = duration;
		curve = handlers;
		duration = null;
		handlers = null;
	}
	if(dojo.lang.isArray(curve)){
		this.curve = new dojo.lfx.Line(curve[0], curve[1]);
	}else{
		this.curve = curve;
	}
	if(duration != null && duration > 0){ this.duration = duration; }
	if(repeatCount){ this.repeatCount = repeatCount; }
	if(rate){ this.rate = rate; }
	if(handlers){
		this.handler = handlers.handler;
		this.beforeBegin = handlers.beforeBegin;
		this.onBegin = handlers.onBegin;
		this.onEnd = handlers.onEnd;
		this.onPlay = handlers.onPlay;
		this.onPause = handlers.onPause;
		this.onStop = handlers.onStop;
		this.onAnimate = handlers.onAnimate;
	}
	if(easing && dojo.lang.isFunction(easing)){
		this.easing=easing;
	}
}
dojo.inherits(dojo.lfx.Animation, dojo.lfx.IAnimation);
dojo.lang.extend(dojo.lfx.Animation, {
	// "private" properties
	_startTime: null,
	_endTime: null,
	_timer: null,
	_percent: 0,
	_startRepeatCount: 0,

	// public methods
	play: function(delay, gotoStart) {
		if( gotoStart ) {
			clearTimeout(this._timer);
			this._active = false;
			this._paused = false;
			this._percent = 0;
		} else if( this._active && !this._paused ) {
			return;
		}
		
		this.fire("beforeBegin");

		if(delay > 0){
			setTimeout(dojo.lang.hitch(this, function(){ this.play(null, gotoStart); }), delay);
			return;
		}
		
		this._startTime = new Date().valueOf();
		if( this._paused ) {
			this._startTime -= (this.duration * this._percent / 100);
		}
		this._endTime = this._startTime + this.duration;

		this._active = true;
		this._paused = false;
		
		var step = this._percent / 100;
		var value = this.curve.getValue(step);
		if( this._percent == 0 ) {
			if(!this._startRepeatCount) {
				this._startRepeatCount = this.repeatCount;
			}
			this.fire("handler", ["begin", value]);
			this.fire("onBegin", [value]);
		}

		this.fire("handler", ["play", value]);
		this.fire("onPlay", [value]);

		this._cycle();
	},

	pause: function() {
		clearTimeout(this._timer);
		if( !this._active ) { return; }
		this._paused = true;
		var value = this.curve.getValue(this._percent / 100);
		this.fire("handler", ["pause", value]);
		this.fire("onPause", [value]);
	},

	gotoPercent: function(pct, andPlay) {
		clearTimeout(this._timer);
		this._active = true;
		this._paused = true;
		this._percent = pct;
		if( andPlay ) { this.play(); }
	},

	stop: function(gotoEnd) {
		clearTimeout(this._timer);
		var step = this._percent / 100;
		if( gotoEnd ) {
			step = 1;
		}
		var value = this.curve.getValue(step);
		this.fire("handler", ["stop", value]);
		this.fire("onStop", [value]);
		this._active = false;
		this._paused = false;
	},

	status: function() {
		if( this._active ) {
			return this._paused ? "paused" : "playing";
		} else {
			return "stopped";
		}
	},

	// "private" methods
	_cycle: function() {
		clearTimeout(this._timer);
		if( this._active ) {
			var curr = new Date().valueOf();
			var step = (curr - this._startTime) / (this._endTime - this._startTime);

			if( step >= 1 ) {
				step = 1;
				this._percent = 100;
			} else {
				this._percent = step * 100;
			}
			
			// Perform easing
			if(this.easing && dojo.lang.isFunction(this.easing)) {
				step = this.easing(step);
			}

			var value = this.curve.getValue(step);
			this.fire("handler", ["animate", value]);
			this.fire("onAnimate", [value]);

			if( step < 1 ) {
				this._timer = setTimeout(dojo.lang.hitch(this, "_cycle"), this.rate);
			} else {
				this._active = false;
				this.fire("handler", ["end"]);
				this.fire("onEnd");

				if( this.repeatCount > 0 ) {
					this.repeatCount--;
					this.play(null, true);
				} else if( this.repeatCount == -1 ) {
					this.play(null, true);
				} else {
					if(this._startRepeatCount) {
						this.repeatCount = this._startRepeatCount;
						this._startRepeatCount = 0;
					}
				}
			}
		}
	}
});

dojo.lfx.Combine = function(){
	dojo.lfx.IAnimation.call(this);
	this._anims = [];
	this._animsEnded = 0;
	
	var anims = arguments;
	if(anims.length == 1 && (dojo.lang.isArray(anims[0]) || dojo.lang.isArrayLike(anims[0]))){
		anims = anims[0];
	}
	
	var _this = this;
	dojo.lang.forEach(anims, function(anim){
		_this._anims.push(anim);
		dojo.event.connect(anim, "onEnd", function(){ _this._onAnimsEnded(); });
	});
}
dojo.inherits(dojo.lfx.Combine, dojo.lfx.IAnimation);
dojo.lang.extend(dojo.lfx.Combine, {
	// private members
	_animsEnded: 0,
	
	// public methods
	play: function(delay, gotoStart){
		if( !this._anims.length ){ return; }

		this.fire("beforeBegin");

		if(delay > 0){
			setTimeout(dojo.lang.hitch(this, function(){ this.play(null, gotoStart); }), delay);
			return;
		}
		
		if(gotoStart || this._anims[0].percent == 0){
			this.fire("onBegin");
		}
		this.fire("onPlay");
		this._animsCall("play", null, gotoStart);
	},
	
	pause: function(){
		this.fire("onPause");
		this._animsCall("pause"); 
	},
	
	stop: function(gotoEnd){
		this.fire("onStop");
		this._animsCall("stop", gotoEnd);
	},
	
	// private methods
	_onAnimsEnded: function(){
		this._animsEnded++;
		if(this._animsEnded >= this._anims.length){
			this.fire("onEnd");
		}
	},
	
	_animsCall: function(funcName){
		var args = [];
		if(arguments.length > 1){
			for(var i = 1 ; i < arguments.length ; i++){
				args.push(arguments[i]);
			}
		}
		var _this = this;
		dojo.lang.forEach(this._anims, function(anim){
			anim[funcName](args);
		}, _this);
	}
});

dojo.lfx.Chain = function() {
	dojo.lfx.IAnimation.call(this);
	this._anims = [];
	this._currAnim = -1;
	
	var anims = arguments;
	if(anims.length == 1 && (dojo.lang.isArray(anims[0]) || dojo.lang.isArrayLike(anims[0]))){
		anims = anims[0];
	}
	
	var _this = this;
	dojo.lang.forEach(anims, function(anim, i, anims_arr){
		_this._anims.push(anim);
		if(i < anims_arr.length - 1){
			dojo.event.connect(anim, "onEnd", function(){ _this._playNext(); });
		}else{
			dojo.event.connect(anim, "onEnd", function(){ _this.fire("onEnd"); });
		}
	}, _this);
}
dojo.inherits(dojo.lfx.Chain, dojo.lfx.IAnimation);
dojo.lang.extend(dojo.lfx.Chain, {
	// private members
	_currAnim: -1,
	
	// public methods
	play: function(delay, gotoStart){
		if( !this._anims.length ) { return; }
		if( gotoStart || !this._anims[this._currAnim] ) {
			this._currAnim = 0;
		}

		this.fire("beforeBegin");
		if(delay > 0){
			setTimeout(dojo.lang.hitch(this, function(){ this.play(null, gotoStart); }), delay);
			return;
		}
		
		if( this._anims[this._currAnim] ){
			if( this._currAnim == 0 ){
				this.fire("handler", ["begin", this._currAnim]);
				this.fire("onBegin", [this._currAnim]);
			}
			this.fire("onPlay", [this._currAnim]);
			this._anims[this._currAnim].play(null, gotoStart);
		}
	},
	
	pause: function(){
		if( this._anims[this._currAnim] ) {
			this._anims[this._currAnim].pause();
			this.fire("onPause", [this._currAnim]);
		}
	},
	
	playPause: function(){
		if( this._anims.length == 0 ) { return; }
		if( this._currAnim == -1 ) { this._currAnim = 0; }
		var currAnim = this._anims[this._currAnim];
		if( currAnim ) {
			if( !currAnim._active || currAnim._paused ) {
				this.play();
			} else {
				this.pause();
			}
		}
	},
	
	stop: function(){
		if( this._anims[this._currAnim] ){
			this._anims[this._currAnim].stop();
			this.fire("onStop", [this._currAnim]);
		}
	},
	
	// private methods
	_playNext: function(){
		if( this._currAnim == -1 || this._anims.length == 0 ) { return; }
		this._currAnim++;
		if( this._anims[this._currAnim] ){
			this._anims[this._currAnim].play(null, true);
		}
	}
});

dojo.lfx.combine = function(){
	var anims = arguments;
	if(dojo.lang.isArray(arguments[0])){
		anims = arguments[0];
	}
	return new dojo.lfx.Combine(anims);
}

dojo.lfx.chain = function(){
	var anims = arguments;
	if(dojo.lang.isArray(arguments[0])){
		anims = arguments[0];
	}
	return new dojo.lfx.Chain(anims);
}

dojo.provide("dojo.lang.extras");

dojo.require("dojo.lang.common");

/**
 * Sets a timeout in milliseconds to execute a function in a given context
 * with optional arguments.
 *
 * setTimeout (Object context, function func, number delay[, arg1[, ...]]);
 * setTimeout (function func, number delay[, arg1[, ...]]);
 */
dojo.lang.setTimeout = function(func, delay){
	var context = window, argsStart = 2;
	if(!dojo.lang.isFunction(func)){
		context = func;
		func = delay;
		delay = arguments[2];
		argsStart++;
	}

	if(dojo.lang.isString(func)){
		func = context[func];
	}
	
	var args = [];
	for (var i = argsStart; i < arguments.length; i++) {
		args.push(arguments[i]);
	}
	return setTimeout(function () { func.apply(context, args); }, delay);
}

dojo.lang.getNameInObj = function(ns, item){
	if(!ns){ ns = dj_global; }

	for(var x in ns){
		if(ns[x] === item){
			return new String(x);
		}
	}
	return null;
}

dojo.lang.shallowCopy = function(obj) {
	var ret = {}, key;
	for(key in obj) {
		if(dojo.lang.isUndefined(ret[key])) {
			ret[key] = obj[key];
		}
	}
	return ret;
}

/**
 * Return the first argument that isn't undefined
 */
dojo.lang.firstValued = function(/* ... */) {
	for(var i = 0; i < arguments.length; i++) {
		if(typeof arguments[i] != "undefined") {
			return arguments[i];
		}
	}
	return undefined;
}

/**
 * Get a value from a reference specified as a string descriptor,
 * (e.g. "A.B") in the given context.
 * 
 * getObjPathValue(String objpath [, Object context, Boolean create])
 *
 * If context is not specified, dj_global is used
 * If create is true, undefined objects in the path are created.
 */
dojo.lang.getObjPathValue = function(objpath, context, create){
	with(dojo.parseObjPath(objpath, context, create)){
		return dojo.evalProp(prop, obj, create);
	}
}

/**
 * Set a value on a reference specified as a string descriptor. 
 * (e.g. "A.B") in the given context.
 * 
 * setObjPathValue(String objpath, value [, Object context, Boolean create])
 *
 * If context is not specified, dj_global is used
 * If create is true, undefined objects in the path are created.
 */
dojo.lang.setObjPathValue = function(objpath, value, context, create){
	if(arguments.length < 4){
		create = true;
	}
	with(dojo.parseObjPath(objpath, context, create)){
		if(obj && (create || (prop in obj))){
			obj[prop] = value;
		}
	}
}

dojo.provide("dojo.event");

dojo.require("dojo.lang.array");
dojo.require("dojo.lang.extras");
dojo.require("dojo.lang.func");

dojo.event = new function(){
	this.canTimeout = dojo.lang.isFunction(dj_global["setTimeout"])||dojo.lang.isAlien(dj_global["setTimeout"]);

	// FIXME: where should we put this method (not here!)?
	function interpolateArgs(args){
		var dl = dojo.lang;
		var ao = {
			srcObj: dj_global,
			srcFunc: null,
			adviceObj: dj_global,
			adviceFunc: null,
			aroundObj: null,
			aroundFunc: null,
			adviceType: (args.length>2) ? args[0] : "after",
			precedence: "last",
			once: false,
			delay: null,
			rate: 0,
			adviceMsg: false
		};

		switch(args.length){
			case 0: return;
			case 1: return;
			case 2:
				ao.srcFunc = args[0];
				ao.adviceFunc = args[1];
				break;
			case 3:
				if((dl.isObject(args[0]))&&(dl.isString(args[1]))&&(dl.isString(args[2]))){
					ao.adviceType = "after";
					ao.srcObj = args[0];
					ao.srcFunc = args[1];
					ao.adviceFunc = args[2];
				}else if((dl.isString(args[1]))&&(dl.isString(args[2]))){
					ao.srcFunc = args[1];
					ao.adviceFunc = args[2];
				}else if((dl.isObject(args[0]))&&(dl.isString(args[1]))&&(dl.isFunction(args[2]))){
					ao.adviceType = "after";
					ao.srcObj = args[0];
					ao.srcFunc = args[1];
					var tmpName  = dojo.lang.nameAnonFunc(args[2], ao.adviceObj);
					ao.adviceFunc = tmpName;
				}else if((dl.isFunction(args[0]))&&(dl.isObject(args[1]))&&(dl.isString(args[2]))){
					ao.adviceType = "after";
					ao.srcObj = dj_global;
					var tmpName  = dojo.lang.nameAnonFunc(args[0], ao.srcObj);
					ao.srcFunc = tmpName;
					ao.adviceObj = args[1];
					ao.adviceFunc = args[2];
				}
				break;
			case 4:
				if((dl.isObject(args[0]))&&(dl.isObject(args[2]))){
					// we can assume that we've got an old-style "connect" from
					// the sigslot school of event attachment. We therefore
					// assume after-advice.
					ao.adviceType = "after";
					ao.srcObj = args[0];
					ao.srcFunc = args[1];
					ao.adviceObj = args[2];
					ao.adviceFunc = args[3];
				}else if((dl.isString(args[0]))&&(dl.isString(args[1]))&&(dl.isObject(args[2]))){
					ao.adviceType = args[0];
					ao.srcObj = dj_global;
					ao.srcFunc = args[1];
					ao.adviceObj = args[2];
					ao.adviceFunc = args[3];
				}else if((dl.isString(args[0]))&&(dl.isFunction(args[1]))&&(dl.isObject(args[2]))){
					ao.adviceType = args[0];
					ao.srcObj = dj_global;
					var tmpName  = dojo.lang.nameAnonFunc(args[1], dj_global);
					ao.srcFunc = tmpName;
					ao.adviceObj = args[2];
					ao.adviceFunc = args[3];
				}else if((dl.isString(args[0]))&&(dl.isObject(args[1]))&&(dl.isString(args[2]))&&(dl.isFunction(args[3]))){
					ao.srcObj = args[1];
					ao.srcFunc = args[2];
					var tmpName  = dojo.lang.nameAnonFunc(args[3], dj_global);
					ao.adviceObj = dj_global;
					ao.adviceFunc = tmpName;
				}else if(dl.isObject(args[1])){
					ao.srcObj = args[1];
					ao.srcFunc = args[2];
					ao.adviceObj = dj_global;
					ao.adviceFunc = args[3];
				}else if(dl.isObject(args[2])){
					ao.srcObj = dj_global;
					ao.srcFunc = args[1];
					ao.adviceObj = args[2];
					ao.adviceFunc = args[3];
				}else{
					ao.srcObj = ao.adviceObj = ao.aroundObj = dj_global;
					ao.srcFunc = args[1];
					ao.adviceFunc = args[2];
					ao.aroundFunc = args[3];
				}
				break;
			case 6:
				ao.srcObj = args[1];
				ao.srcFunc = args[2];
				ao.adviceObj = args[3]
				ao.adviceFunc = args[4];
				ao.aroundFunc = args[5];
				ao.aroundObj = dj_global;
				break;
			default:
				ao.srcObj = args[1];
				ao.srcFunc = args[2];
				ao.adviceObj = args[3]
				ao.adviceFunc = args[4];
				ao.aroundObj = args[5];
				ao.aroundFunc = args[6];
				ao.once = args[7];
				ao.delay = args[8];
				ao.rate = args[9];
				ao.adviceMsg = args[10];
				break;
		}

		if(dl.isFunction(ao.aroundFunc)){
			var tmpName  = dojo.lang.nameAnonFunc(ao.aroundFunc, ao.aroundObj);
			ao.aroundFunc = tmpName;
		}

		if(!dl.isString(ao.srcFunc)){
			ao.srcFunc = dojo.lang.getNameInObj(ao.srcObj, ao.srcFunc);
		}

		if(!dl.isString(ao.adviceFunc)){
			ao.adviceFunc = dojo.lang.getNameInObj(ao.adviceObj, ao.adviceFunc);
		}

		if((ao.aroundObj)&&(!dl.isString(ao.aroundFunc))){
			ao.aroundFunc = dojo.lang.getNameInObj(ao.aroundObj, ao.aroundFunc);
		}

		if(!ao.srcObj){
			dojo.raise("bad srcObj for srcFunc: "+ao.srcFunc);
		}
		if(!ao.adviceObj){
			dojo.raise("bad adviceObj for adviceFunc: "+ao.adviceFunc);
		}
		return ao;
	}

	this.connect = function(){
		if(arguments.length == 1){
			var ao = arguments[0];
		}else{
			var ao = interpolateArgs(arguments);
		}

		if(dojo.lang.isArray(ao.srcObj) && ao.srcObj!=""){
			var tmpAO = {};
			for(var x in ao){
				tmpAO[x] = ao[x];
			}
			var mjps = [];
			dojo.lang.forEach(ao.srcObj, function(src){
				if((dojo.render.html.capable)&&(dojo.lang.isString(src))){
					src = dojo.byId(src);
					// dojo.debug(src);
				}
				tmpAO.srcObj = src;
				// dojo.debug(tmpAO.srcObj, tmpAO.srcFunc);
				// dojo.debug(tmpAO.adviceObj, tmpAO.adviceFunc);
				mjps.push(dojo.event.connect.call(dojo.event, tmpAO));
			});
			return mjps;
		}

		// FIXME: just doing a "getForMethod()" seems to be enough to put this into infinite recursion!!
		var mjp = dojo.event.MethodJoinPoint.getForMethod(ao.srcObj, ao.srcFunc);
		if(ao.adviceFunc){
			var mjp2 = dojo.event.MethodJoinPoint.getForMethod(ao.adviceObj, ao.adviceFunc);
		}

		mjp.kwAddAdvice(ao);

		return mjp;	// advanced users might want to fsck w/ the join point
					// manually
	}

	this.log = function(a1, a2){
		var kwArgs;
		if((arguments.length == 1)&&(typeof a1 == "object")){
			kwArgs = a1;
		}else{
			kwArgs = {
				srcObj: a1,
				srcFunc: a2
			};
		}
		kwArgs.adviceFunc = function(){
			var argsStr = [];
			for(var x=0; x<arguments.length; x++){
				argsStr.push(arguments[x]);
			}
			dojo.debug("("+kwArgs.srcObj+")."+kwArgs.srcFunc, ":", argsStr.join(", "));
		}
		this.kwConnect(kwArgs);
	}

	this.connectBefore = function(){
		var args = ["before"];
		for(var i = 0; i < arguments.length; i++) { args.push(arguments[i]); }
		return this.connect.apply(this, args);
	}

	this.connectAround = function(){
		var args = ["around"];
		for(var i = 0; i < arguments.length; i++) { args.push(arguments[i]); }
		return this.connect.apply(this, args);
	}

	this.connectOnce = function(){
		var ao = interpolateArgs(arguments);
		ao.once = true;
		return this.connect(ao);
	}

	this._kwConnectImpl = function(kwArgs, disconnect){
		var fn = (disconnect) ? "disconnect" : "connect";
		if(typeof kwArgs["srcFunc"] == "function"){
			kwArgs.srcObj = kwArgs["srcObj"]||dj_global;
			var tmpName  = dojo.lang.nameAnonFunc(kwArgs.srcFunc, kwArgs.srcObj);
			kwArgs.srcFunc = tmpName;
		}
		if(typeof kwArgs["adviceFunc"] == "function"){
			kwArgs.adviceObj = kwArgs["adviceObj"]||dj_global;
			var tmpName  = dojo.lang.nameAnonFunc(kwArgs.adviceFunc, kwArgs.adviceObj);
			kwArgs.adviceFunc = tmpName;
		}
		return dojo.event[fn](	(kwArgs["type"]||kwArgs["adviceType"]||"after"),
									kwArgs["srcObj"]||dj_global,
									kwArgs["srcFunc"],
									kwArgs["adviceObj"]||kwArgs["targetObj"]||dj_global,
									kwArgs["adviceFunc"]||kwArgs["targetFunc"],
									kwArgs["aroundObj"],
									kwArgs["aroundFunc"],
									kwArgs["once"],
									kwArgs["delay"],
									kwArgs["rate"],
									kwArgs["adviceMsg"]||false );
	}

	this.kwConnect = function(kwArgs){
		return this._kwConnectImpl(kwArgs, false);

	}

	this.disconnect = function(){
		var ao = interpolateArgs(arguments);
		if(!ao.adviceFunc){ return; } // nothing to disconnect
		var mjp = dojo.event.MethodJoinPoint.getForMethod(ao.srcObj, ao.srcFunc);
		return mjp.removeAdvice(ao.adviceObj, ao.adviceFunc, ao.adviceType, ao.once);
	}

	this.kwDisconnect = function(kwArgs){
		return this._kwConnectImpl(kwArgs, true);
	}
}

// exactly one of these is created whenever a method with a joint point is run,
// if there is at least one 'around' advice.
dojo.event.MethodInvocation = function(join_point, obj, args) {
	this.jp_ = join_point;
	this.object = obj;
	this.args = [];
	for(var x=0; x<args.length; x++){
		this.args[x] = args[x];
	}
	// the index of the 'around' that is currently being executed.
	this.around_index = -1;
}

dojo.event.MethodInvocation.prototype.proceed = function() {
	this.around_index++;
	if(this.around_index >= this.jp_.around.length){
		return this.jp_.object[this.jp_.methodname].apply(this.jp_.object, this.args);
		// return this.jp_.run_before_after(this.object, this.args);
	}else{
		var ti = this.jp_.around[this.around_index];
		var mobj = ti[0]||dj_global;
		var meth = ti[1];
		return mobj[meth].call(mobj, this);
	}
} 


dojo.event.MethodJoinPoint = function(obj, methname){
	this.object = obj||dj_global;
	this.methodname = methname;
	this.methodfunc = this.object[methname];
	this.before = [];
	this.after = [];
	this.around = [];
}

dojo.event.MethodJoinPoint.getForMethod = function(obj, methname) {
	// if(!(methname in obj)){
	if(!obj){ obj = dj_global; }
	if(!obj[methname]){
		// supply a do-nothing method implementation
		obj[methname] = function(){};
	}else if((!dojo.lang.isFunction(obj[methname]))&&(!dojo.lang.isAlien(obj[methname]))){
		return null; // FIXME: should we throw an exception here instead?
	}
	// we hide our joinpoint instance in obj[methname + '$joinpoint']
	var jpname = methname + "$joinpoint";
	var jpfuncname = methname + "$joinpoint$method";
	var joinpoint = obj[jpname];
	if(!joinpoint){
		var isNode = false;
		if(dojo.event["browser"]){
			if( (obj["attachEvent"])||
				(obj["nodeType"])||
				(obj["addEventListener"]) ){
				isNode = true;
				dojo.event.browser.addClobberNodeAttrs(obj, [jpname, jpfuncname, methname]);
			}
		}
		var origArity = obj[methname].length;
		obj[jpfuncname] = obj[methname];
		// joinpoint = obj[jpname] = new dojo.event.MethodJoinPoint(obj, methname);
		joinpoint = obj[jpname] = new dojo.event.MethodJoinPoint(obj, jpfuncname);
		obj[methname] = function(){ 
			var args = [];

			if((isNode)&&(!arguments.length)){
				var evt = null;
				try{
					if(obj.ownerDocument){
						evt = obj.ownerDocument.parentWindow.event;
					}else if(obj.documentElement){
						evt = obj.documentElement.ownerDocument.parentWindow.event;
					}else{
						evt = window.event;
					}
				}catch(e){
					evt = window.event;
				}

				if(evt){
					args.push(dojo.event.browser.fixEvent(evt, this));
				}
			}else{
				for(var x=0; x<arguments.length; x++){
					if((x==0)&&(isNode)&&(dojo.event.browser.isEvent(arguments[x]))){
						args.push(dojo.event.browser.fixEvent(arguments[x], this));
					}else{
						args.push(arguments[x]);
					}
				}
			}
			// return joinpoint.run.apply(joinpoint, arguments); 
			return joinpoint.run.apply(joinpoint, args); 
		}
		obj[methname].__preJoinArity = origArity;
	}
	return joinpoint;
}

dojo.lang.extend(dojo.event.MethodJoinPoint, {
	unintercept: function(){
		this.object[this.methodname] = this.methodfunc;
		this.before = [];
		this.after = [];
		this.around = [];
	},

	disconnect: dojo.lang.forward("unintercept"),

	run: function() {
		var obj = this.object||dj_global;
		var args = arguments;

		// optimization. We only compute once the array version of the arguments
		// pseudo-arr in order to prevent building it each time advice is unrolled.
		var aargs = [];
		for(var x=0; x<args.length; x++){
			aargs[x] = args[x];
		}

		var unrollAdvice  = function(marr){ 
			if(!marr){
				dojo.debug("Null argument to unrollAdvice()");
				return;
			}
		  
			var callObj = marr[0]||dj_global;
			var callFunc = marr[1];
			
			if(!callObj[callFunc]){
				dojo.raise("function \"" + callFunc + "\" does not exist on \"" + callObj + "\"");
			}
			
			var aroundObj = marr[2]||dj_global;
			var aroundFunc = marr[3];
			var msg = marr[6];
			var undef;

			var to = {
				args: [],
				jp_: this,
				object: obj,
				proceed: function(){
					return callObj[callFunc].apply(callObj, to.args);
				}
			};
			to.args = aargs;

			var delay = parseInt(marr[4]);
			var hasDelay = ((!isNaN(delay))&&(marr[4]!==null)&&(typeof marr[4] != "undefined"));
			if(marr[5]){
				var rate = parseInt(marr[5]);
				var cur = new Date();
				var timerSet = false;
				if((marr["last"])&&((cur-marr.last)<=rate)){
					if(dojo.event.canTimeout){
						if(marr["delayTimer"]){
							clearTimeout(marr.delayTimer);
						}
						var tod = parseInt(rate*2); // is rate*2 naive?
						var mcpy = dojo.lang.shallowCopy(marr);
						marr.delayTimer = setTimeout(function(){
							// FIXME: on IE at least, event objects from the
							// browser can go out of scope. How (or should?) we
							// deal with it?
							mcpy[5] = 0;
							unrollAdvice(mcpy);
						}, tod);
					}
					return;
				}else{
					marr.last = cur;
				}
			}

			// FIXME: need to enforce rates for a connection here!

			if(aroundFunc){
				// NOTE: around advice can't delay since we might otherwise depend
				// on execution order!
				aroundObj[aroundFunc].call(aroundObj, to);
			}else{
				// var tmjp = dojo.event.MethodJoinPoint.getForMethod(obj, methname);
				if((hasDelay)&&((dojo.render.html)||(dojo.render.svg))){  // FIXME: the render checks are grotty!
					dj_global["setTimeout"](function(){
						if(msg){
							callObj[callFunc].call(callObj, to); 
						}else{
							callObj[callFunc].apply(callObj, args); 
						}
					}, delay);
				}else{ // many environments can't support delay!
					if(msg){
						callObj[callFunc].call(callObj, to); 
					}else{
						callObj[callFunc].apply(callObj, args); 
					}
				}
			}
		}

		if(this.before.length>0){
			dojo.lang.forEach(this.before, unrollAdvice);
		}

		var result;
		if(this.around.length>0){
			var mi = new dojo.event.MethodInvocation(this, obj, args);
			result = mi.proceed();
		}else if(this.methodfunc){
			result = this.object[this.methodname].apply(this.object, args);
		}

		if(this.after.length>0){
			dojo.lang.forEach(this.after, unrollAdvice);
		}

		return (this.methodfunc) ? result : null;
	},

	getArr: function(kind){
		var arr = this.after;
		// FIXME: we should be able to do this through props or Array.in()
		if((typeof kind == "string")&&(kind.indexOf("before")!=-1)){
			arr = this.before;
		}else if(kind=="around"){
			arr = this.around;
		}
		return arr;
	},

	kwAddAdvice: function(args){
		this.addAdvice(	args["adviceObj"], args["adviceFunc"], 
						args["aroundObj"], args["aroundFunc"], 
						args["adviceType"], args["precedence"], 
						args["once"], args["delay"], args["rate"], 
						args["adviceMsg"]);
	},

	addAdvice: function(	thisAdviceObj, thisAdvice, 
							thisAroundObj, thisAround, 
							advice_kind, precedence, 
							once, delay, rate, asMessage){
		var arr = this.getArr(advice_kind);
		if(!arr){
			dojo.raise("bad this: " + this);
		}

		var ao = [thisAdviceObj, thisAdvice, thisAroundObj, thisAround, delay, rate, asMessage];
		
		if(once){
			if(this.hasAdvice(thisAdviceObj, thisAdvice, advice_kind, arr) >= 0){
				return;
			}
		}

		if(precedence == "first"){
			arr.unshift(ao);
		}else{
			arr.push(ao);
		}
	},

	hasAdvice: function(thisAdviceObj, thisAdvice, advice_kind, arr){
		if(!arr){ arr = this.getArr(advice_kind); }
		var ind = -1;
		for(var x=0; x<arr.length; x++){
			if((arr[x][0] == thisAdviceObj)&&(arr[x][1] == thisAdvice)){
				ind = x;
			}
		}
		return ind;
	},

	removeAdvice: function(thisAdviceObj, thisAdvice, advice_kind, once){
		var arr = this.getArr(advice_kind);
		var ind = this.hasAdvice(thisAdviceObj, thisAdvice, advice_kind, arr);
		if(ind == -1){
			return false;
		}
		while(ind != -1){
			arr.splice(ind, 1);
			if(once){ break; }
			ind = this.hasAdvice(thisAdviceObj, thisAdvice, advice_kind, arr);
		}
		return true;
	}
});

dojo.provide("dojo.lfx.html");
dojo.require("dojo.lfx.Animation");

dojo.require("dojo.html");
dojo.require("dojo.event");
dojo.require("dojo.lang.func");

dojo.lfx.html._byId = function(nodes){
	if(dojo.lang.isArrayLike(nodes)){
		if(!nodes.alreadyChecked){
			var n = [];
			dojo.lang.forEach(nodes, function(node){
				n.push(dojo.byId(node));
			});
			n.alreadyChecked = true;
			return n;
		}else{
			return nodes;
		}
	}else{
		return [dojo.byId(nodes)];
	}
}

dojo.lfx.html.propertyAnimation = function(	/*DOMNode*/ nodes, 
											/*Array*/ propertyMap, 
											/*int*/ duration,
											/*function*/ easing){
	nodes = dojo.lfx.html._byId(nodes);

	if(nodes.length==1){
		// FIXME: we're only supporting start-value filling when one node is
		// passed

		dojo.lang.forEach(propertyMap, function(prop){
			if(typeof prop["start"] == "undefined"){
				prop.start = parseInt(dojo.style.getComputedStyle(nodes[0], prop.property));
				if(isNaN(prop.start) && (prop.property == "opacity")){
					prop.start = 1;
				}
			}
		});
	}

	var coordsAsInts = function(coords){
		var cints = new Array(coords.length);
		for(var i = 0; i < coords.length; i++){
			cints[i] = Math.round(coords[i]);
		}
		return cints;
	}
	var setStyle = function(n, style){
		n = dojo.byId(n);
		if(!n || !n.style){ return; }
		for(s in style){
			if(s == "opacity"){
				dojo.style.setOpacity(n, style[s]);
			}else{
				n.style[dojo.style.toCamelCase(s)] = style[s];
			}
		}
	}
	var propLine = function(properties){
		this._properties = properties;
		this.diffs = new Array(properties.length);
		dojo.lang.forEach(properties, function(prop, i){
			// calculate the end - start to optimize a bit
			if(dojo.lang.isArray(prop.start)){
				// don't loop through the arrays
				this.diffs[i] = null;
			}else{
				this.diffs[i] = prop.end - prop.start;
			}
		}, this);
		this.getValue = function(n){
			var ret = {};
			dojo.lang.forEach(this._properties, function(prop, i){
				var value = null;
				if(dojo.lang.isArray(prop.start)){
					value = (prop.units||"rgb") + "(";
					for(var j = 0 ; j < prop.start.length ; j++){
						value += Math.round(((prop.end[j] - prop.start[j]) * n) + prop.start[j]) + (j < prop.start.length - 1 ? "," : "");
					}
					value += ")";
				}else{
					value = ((this.diffs[i]) * n) + prop.start + (prop.property != "opacity" ? prop.units||"px" : "");
				}
				ret[prop.property] = value;
			}, this);
			return ret;
		}
	}
	
	var anim = new dojo.lfx.Animation(duration, new propLine(propertyMap), easing);
	
	dojo.event.connect(anim, "onAnimate", function(propValues){
		dojo.lang.forEach(nodes, function(node){
			setStyle(node, propValues); 
		});
	});
	
	return anim;
}

dojo.lfx.html._makeFadeable = function(nodes){
	var makeFade = function(node){
		if(dojo.render.html.ie){
			// only set the zoom if the "tickle" value would be the same as the
			// default
			if( (node.style.zoom.length == 0) &&
				(dojo.style.getStyle(node, "zoom") == "normal") ){
				// make sure the node "hasLayout"
				// NOTE: this has been tested with larger and smaller user-set text
				// sizes and works fine
				node.style.zoom = "1";
				// node.style.zoom = "normal";
			}
			// don't set the width to auto if it didn't already cascade that way.
			// We don't want to f anyones designs
			if(	(node.style.width.length == 0) &&
				(dojo.style.getStyle(node, "width") == "auto") ){
				node.style.width = "auto";
			}
		}
	}
	if(dojo.lang.isArrayLike(nodes)){
		dojo.lang.forEach(nodes, makeFade);
	}else{
		makeFade(nodes);
	}
}

dojo.lfx.html.fadeIn = function(nodes, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	dojo.lfx.html._makeFadeable(nodes);
	var anim = dojo.lfx.propertyAnimation(nodes, [
		{	property: "opacity",
			start: dojo.style.getOpacity(nodes[0]),
			end: 1 } ], duration, easing);
	if(callback){
		dojo.event.connect(anim, "onEnd", function(){
			callback(nodes, anim);
		});
	}

	return anim;
}

dojo.lfx.html.fadeOut = function(nodes, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	dojo.lfx.html._makeFadeable(nodes);
	var anim = dojo.lfx.propertyAnimation(nodes, [
		{	property: "opacity",
			start: dojo.style.getOpacity(nodes[0]),
			end: 0 } ], duration, easing);
	if(callback){
		dojo.event.connect(anim, "onEnd", function(){
			callback(nodes, anim);
		});
	}

	return anim;
}

dojo.lfx.html.fadeShow = function(nodes, duration, easing, callback){
	var anim = dojo.lfx.html.fadeIn(nodes, duration, easing, callback);
	dojo.event.connect(anim, "beforeBegin", function(){
		if(dojo.lang.isArrayLike(nodes)){
			dojo.lang.forEach(nodes, dojo.style.show);
		}else{
			dojo.style.show(nodes);
		}
	});
	
	return anim;
}

dojo.lfx.html.fadeHide = function(nodes, duration, easing, callback){
	var anim = dojo.lfx.html.fadeOut(nodes, duration, easing, function(){
		if(dojo.lang.isArrayLike(nodes)){
			dojo.lang.forEach(nodes, dojo.style.hide);
		}else{
			dojo.style.hide(nodes);
		}
		if(callback){ callback(nodes, anim); }
	});
	
	return anim;
}

dojo.lfx.html.wipeIn = function(nodes, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	var anims = [];

	var init = function(node, overflow){
		if(overflow == "visible") {
			node.style.overflow = "hidden";
		}
		dojo.style.show(node);
		node.style.height = 0;
	}

	dojo.lang.forEach(nodes, function(node){
		var overflow = dojo.style.getStyle(node, "overflow");
		var initialize = function(){
			init(node, overflow);
		}
		initialize();
		
		var anim = dojo.lfx.propertyAnimation(node,
			[{	property: "height",
				start: 0,
				end: node.scrollHeight }], duration, easing);
		
		dojo.event.connect(anim, "beforeBegin", initialize);
		dojo.event.connect(anim, "onEnd", function(){
			node.style.overflow = overflow;
			node.style.height = "auto";
			if(callback){ callback(node, anim); }
		});
		anims.push(anim);
	});
	
	if(nodes.length > 1){ return dojo.lfx.combine(anims); }
	else{ return anims[0]; }
}

dojo.lfx.html.wipeOut = function(nodes, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	var anims = [];
	
	var init = function(node, overflow){
		dojo.style.show(node);
		if(overflow == "visible") {
			node.style.overflow = "hidden";
		}
	}
	dojo.lang.forEach(nodes, function(node){
		var overflow = dojo.style.getStyle(node, "overflow");
		var initialize = function(){
			init(node, overflow);
		}
		initialize();

		var anim = dojo.lfx.propertyAnimation(node,
			[{	property: "height",
				start: node.offsetHeight,
				end: 0 } ], duration, easing);
		
		dojo.event.connect(anim, "beforeBegin", initialize);
		dojo.event.connect(anim, "onEnd", function(){
			dojo.style.hide(node);
			node.style.overflow = overflow;
			if(callback){ callback(node, anim); }
		});
		anims.push(anim);
	});

	if(nodes.length > 1){ return dojo.lfx.combine(anims); }
	else { return anims[0]; }
}

dojo.lfx.html.slideTo = function(nodes, coords, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	var anims = [];

	dojo.lang.forEach(nodes, function(node){
		var top = null;
		var left = null;
		var pos = null;
		
		var init = (function(){
			var innerNode = node;
			return function(){
				top = node.offsetTop;
				left = node.offsetLeft;
				pos = dojo.style.getComputedStyle(node, 'position');

				if (pos == 'relative' || pos == 'static') {
					top = parseInt(dojo.style.getComputedStyle(node, 'top')) || 0;
					left = parseInt(dojo.style.getComputedStyle(node, 'left')) || 0;
				}
			}
		})();
		init();
		
		var anim = dojo.lfx.propertyAnimation(node,
			[{	property: "top",
				start: top,
				end: coords[0] },
			{	property: "left",
				start: left,
				end: coords[1] }], duration, easing);
		
		dojo.event.connect(anim, "beforeBegin", init);
		if(callback){
			dojo.event.connect(anim, "onEnd", function(){
				callback(node, anim);
			});
		}

		anims.push(anim);
	});
	
	if(nodes.length > 1){ return dojo.lfx.combine(anims); }
	else{ return anims[0]; }
}

dojo.lfx.html.explode = function(start, endNode, duration, easing, callback){
	var startCoords = dojo.style.toCoordinateArray(start);
	var outline = document.createElement("div");
	with(outline.style){
		position = "absolute";
		border = "1px solid black";
		display = "none";
	}
	document.body.appendChild(outline);

	endNode = dojo.byId(endNode);
	with(endNode.style){
		visibility = "hidden";
		display = "block";
	}
	var endCoords = dojo.style.toCoordinateArray(endNode);
	with(endNode.style){
		display = "none";
		visibility = "visible";
	}

	var anim = new dojo.lfx.Animation({
		beforeBegin: function(){
			dojo.style.show(outline);
		},
		onAnimate: function(value){
			with(outline.style){
				left = value[0] + "px";
				top = value[1] + "px";
				width = value[2] + "px";
				height = value[3] + "px";
			}
		},
		onEnd: function(){
			dojo.style.show(endNode);
			outline.parentNode.removeChild(outline);
		}
	}, duration, new dojo.lfx.Line(startCoords, endCoords), easing);
	if(callback){
		dojo.event.connect(anim, "onEnd", function(){
			callback(endNode, anim);
		});
	}
	return anim;
}

dojo.lfx.html.implode = function(startNode, end, duration, easing, callback){
	var startCoords = dojo.style.toCoordinateArray(startNode);
	var endCoords = dojo.style.toCoordinateArray(end);

	startNode = dojo.byId(startNode);
	var outline = document.createElement("div");
	with(outline.style){
		position = "absolute";
		border = "1px solid black";
		display = "none";
	}
	document.body.appendChild(outline);

	var anim = new dojo.lfx.Animation({
		beforeBegin: function(){
			dojo.style.hide(startNode);
			dojo.style.show(outline);
		},
		onAnimate: function(value){
			with(outline.style){
				left = value[0] + "px";
				top = value[1] + "px";
				width = value[2] + "px";
				height = value[3] + "px";
			}
		},
		onEnd: function(){
			outline.parentNode.removeChild(outline);
		}
	}, duration, new dojo.lfx.Line(startCoords, endCoords), easing);
	if(callback){
		dojo.event.connect(anim, "onEnd", function(){
			callback(startNode, anim);
		});
	}
	return anim;
}

dojo.lfx.html.highlight = function(nodes, startColor, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	var anims = [];

	dojo.lang.forEach(nodes, function(node){
		var color = dojo.style.getBackgroundColor(node);
		var bg = dojo.style.getStyle(node, "background-color").toLowerCase();
		var wasTransparent = (bg == "transparent" || bg == "rgba(0, 0, 0, 0)");
		while(color.length > 3) { color.pop(); }

		var rgb = new dojo.graphics.color.Color(startColor).toRgb();
		var endRgb = new dojo.graphics.color.Color(color).toRgb();

		var anim = dojo.lfx.propertyAnimation(node, [{
			property: "background-color",
			start: rgb,
			end: endRgb
		}], duration, easing);

		dojo.event.connect(anim, "beforeBegin", function(){
			node.style.backgroundColor = "rgb(" + rgb.join(",") + ")";
		});

		dojo.event.connect(anim, "onEnd", function(){
			if(wasTransparent){
				node.style.backgroundColor = "transparent";
			}
			if(callback){
				callback(node, anim);
			}
		});

		anims.push(anim);
	});

	if(nodes.length > 1){ return dojo.lfx.combine(anims); }
	else{ return anims[0]; }
}

dojo.lfx.html.unhighlight = function(nodes, endColor, duration, easing, callback){
	nodes = dojo.lfx.html._byId(nodes);
	var anims = [];

	dojo.lang.forEach(nodes, function(node){
		var color = new dojo.graphics.color.Color(dojo.style.getBackgroundColor(node)).toRgb();
		var rgb = new dojo.graphics.color.Color(endColor).toRgb();
		
		var anim = dojo.lfx.propertyAnimation(node, [{
			property: "background-color",
			start: color,
			end: rgb
		}], duration, easing);

		dojo.event.connect(anim, "beforeBegin", function(){
			node.style.backgroundColor = "rgb(" + color.join(",") + ")";
		});
		if(callback){
			dojo.event.connect(anim, "onEnd", function(){
				callback(node, anim);
			});
		}

		anims.push(anim);
	});

	if(nodes.length > 1){ return dojo.lfx.combine(anims); }
	else{ return anims[0]; }
}

dojo.lang.mixin(dojo.lfx, dojo.lfx.html);

dojo.kwCompoundRequire({
	browser: ["dojo.lfx.html"],
	dashboard: ["dojo.lfx.html"]
});
dojo.provide("dojo.lfx.*");
dojo.require("dojo.event");
dojo.provide("dojo.event.topic");

dojo.event.topic = new function(){
	this.topics = {};

	this.getTopic = function(topicName){
		if(!this.topics[topicName]){
			this.topics[topicName] = new this.TopicImpl(topicName);
		}
		return this.topics[topicName];
	}

	this.registerPublisher = function(topic, obj, funcName){
		var topic = this.getTopic(topic);
		topic.registerPublisher(obj, funcName);
	}

	this.subscribe = function(topic, obj, funcName){
		var topic = this.getTopic(topic);
		topic.subscribe(obj, funcName);
	}

	this.unsubscribe = function(topic, obj, funcName){
		var topic = this.getTopic(topic);
		topic.unsubscribe(obj, funcName);
	}

	this.destroy = function(topic){
		this.getTopic(topic).destroy();
		delete this.topics[topic];
	}

	this.publish = function(topic, message){
		var topic = this.getTopic(topic);
		// if message is an array, we treat it as a set of arguments,
		// otherwise, we just pass on the arguments passed in as-is
		var args = [];
		if(arguments.length == 2 && (dojo.lang.isArray(message) || message.callee)){
			args = message;
		}else{
			var args = [];
			for(var x=1; x<arguments.length; x++){
				args.push(arguments[x]);
			}
		}
		topic.sendMessage.apply(topic, args);
	}
}

dojo.event.topic.TopicImpl = function(topicName){
	this.topicName = topicName;

	this.subscribe = function(listenerObject, listenerMethod){
		var tf = listenerMethod||listenerObject;
		var to = (!listenerMethod) ? dj_global : listenerObject;
		dojo.event.kwConnect({
			srcObj:		this, 
			srcFunc:	"sendMessage", 
			adviceObj:	to,
			adviceFunc: tf
		});
	}

	this.unsubscribe = function(listenerObject, listenerMethod){
		var tf = (!listenerMethod) ? listenerObject : listenerMethod;
		var to = (!listenerMethod) ? null : listenerObject;
		dojo.event.kwDisconnect({
			srcObj:		this, 
			srcFunc:	"sendMessage", 
			adviceObj:	to,
			adviceFunc: tf
		});
	}

	this.destroy = function(){
		dojo.event.MethodJoinPoint.getForMethod(this, "sendMessage").disconnect();
	}

	this.registerPublisher = function(publisherObject, publisherMethod){
		dojo.event.connect(publisherObject, publisherMethod, this, "sendMessage");
	}

	this.sendMessage = function(message){
		// The message has been propagated
	}
}


dojo.provide("dojo.event.browser");
dojo.require("dojo.event");

dojo_ie_clobber = new function(){
	this.clobberNodes = [];

	function nukeProp(node, prop){
		// try{ node.removeAttribute(prop); 	}catch(e){ /* squelch */ }
		try{ node[prop] = null; 			}catch(e){ /* squelch */ }
		try{ delete node[prop]; 			}catch(e){ /* squelch */ }
		// FIXME: JotLive needs this, but I'm not sure if it's too slow or not
		try{ node.removeAttribute(prop);	}catch(e){ /* squelch */ }
	}

	this.clobber = function(nodeRef){
		var na;
		var tna;
		if(nodeRef){
			tna = nodeRef.all || nodeRef.getElementsByTagName("*");
			na = [nodeRef];
			for(var x=0; x<tna.length; x++){
				// if we're gonna be clobbering the thing, at least make sure
				// we aren't trying to do it twice
				if(tna[x]["__doClobber__"]){
					na.push(tna[x]);
				}
			}
		}else{
			try{ window.onload = null; }catch(e){}
			na = (this.clobberNodes.length) ? this.clobberNodes : document.all;
		}
		tna = null;
		var basis = {};
		for(var i = na.length-1; i>=0; i=i-1){
			var el = na[i];
			if(el["__clobberAttrs__"]){
				for(var j=0; j<el.__clobberAttrs__.length; j++){
					nukeProp(el, el.__clobberAttrs__[j]);
				}
				nukeProp(el, "__clobberAttrs__");
				nukeProp(el, "__doClobber__");
			}
		}
		na = null;
	}
}

if(dojo.render.html.ie){
	window.onunload = function(){
		dojo_ie_clobber.clobber();
		try{
			if((dojo["widget"])&&(dojo.widget["manager"])){
				dojo.widget.manager.destroyAll();
			}
		}catch(e){}
		try{ window.onload = null; }catch(e){}
		try{ window.onunload = null; }catch(e){}
		dojo_ie_clobber.clobberNodes = [];
		// CollectGarbage();
	}
}

dojo.event.browser = new function(){

	var clobberIdx = 0;

	this.clean = function(node){
		if(dojo.render.html.ie){ 
			dojo_ie_clobber.clobber(node);
		}
	}

	this.addClobberNode = function(node){
		if(!node["__doClobber__"]){
			node.__doClobber__ = true;
			dojo_ie_clobber.clobberNodes.push(node);
			// this might not be the most efficient thing to do, but it's
			// much less error prone than other approaches which were
			// previously tried and failed
			node.__clobberAttrs__ = [];
		}
	}

	this.addClobberNodeAttrs = function(node, props){
		this.addClobberNode(node);
		for(var x=0; x<props.length; x++){
			node.__clobberAttrs__.push(props[x]);
		}
	}

	this.removeListener = function(node, evtName, fp, capture){
		if(!capture){ var capture = false; }
		evtName = evtName.toLowerCase();
		if(evtName.substr(0,2)=="on"){ evtName = evtName.substr(2); }
		// FIXME: this is mostly a punt, we aren't actually doing anything on IE
		if(node.removeEventListener){
			node.removeEventListener(evtName, fp, capture);
		}
	}

	this.addListener = function(node, evtName, fp, capture, dontFix){
		if(!node){ return; } // FIXME: log and/or bail?
		if(!capture){ var capture = false; }
		evtName = evtName.toLowerCase();
		if(evtName.substr(0,2)!="on"){ evtName = "on"+evtName; }

		if(!dontFix){
			// build yet another closure around fp in order to inject fixEvent
			// around the resulting event
			var newfp = function(evt){
				if(!evt){ evt = window.event; }
				var ret = fp(dojo.event.browser.fixEvent(evt, this));
				if(capture){
					dojo.event.browser.stopEvent(evt);
				}
				return ret;
			}
		}else{
			newfp = fp;
		}

		if(node.addEventListener){ 
			node.addEventListener(evtName.substr(2), newfp, capture);
			return newfp;
		}else{
			if(typeof node[evtName] == "function" ){
				var oldEvt = node[evtName];
				node[evtName] = function(e){
					oldEvt(e);
					return newfp(e);
				}
			}else{
				node[evtName]=newfp;
			}
			if(dojo.render.html.ie){
				this.addClobberNodeAttrs(node, [evtName]);
			}
			return newfp;
		}
	}

	this.isEvent = function(obj){
		// FIXME: event detection hack ... could test for additional attributes
		// if necessary
		return (typeof obj != "undefined")&&(typeof Event != "undefined")&&(obj.eventPhase);
		// Event does not support instanceof in Opera, otherwise:
		//return (typeof Event != "undefined")&&(obj instanceof Event);
	}

	this.currentEvent = null;
	
	this.callListener = function(listener, curTarget){
		if(typeof listener != 'function'){
			dojo.raise("listener not a function: " + listener);
		}
		dojo.event.browser.currentEvent.currentTarget = curTarget;
		return listener.call(curTarget, dojo.event.browser.currentEvent);
	}

	this.stopPropagation = function(){
		dojo.event.browser.currentEvent.cancelBubble = true;
	}

	this.preventDefault = function(){
	  dojo.event.browser.currentEvent.returnValue = false;
	}

	this.keys = {
		KEY_BACKSPACE: 8,
		KEY_TAB: 9,
		KEY_ENTER: 13,
		KEY_SHIFT: 16,
		KEY_CTRL: 17,
		KEY_ALT: 18,
		KEY_PAUSE: 19,
		KEY_CAPS_LOCK: 20,
		KEY_ESCAPE: 27,
		KEY_SPACE: 32,
		KEY_PAGE_UP: 33,
		KEY_PAGE_DOWN: 34,
		KEY_END: 35,
		KEY_HOME: 36,
		KEY_LEFT_ARROW: 37,
		KEY_UP_ARROW: 38,
		KEY_RIGHT_ARROW: 39,
		KEY_DOWN_ARROW: 40,
		KEY_INSERT: 45,
		KEY_DELETE: 46,
		KEY_LEFT_WINDOW: 91,
		KEY_RIGHT_WINDOW: 92,
		KEY_SELECT: 93,
		KEY_F1: 112,
		KEY_F2: 113,
		KEY_F3: 114,
		KEY_F4: 115,
		KEY_F5: 116,
		KEY_F6: 117,
		KEY_F7: 118,
		KEY_F8: 119,
		KEY_F9: 120,
		KEY_F10: 121,
		KEY_F11: 122,
		KEY_F12: 123,
		KEY_NUM_LOCK: 144,
		KEY_SCROLL_LOCK: 145
	};

	// reverse lookup
	this.revKeys = [];
	for(var key in this.keys){
		this.revKeys[this.keys[key]] = key;
	}

	this.fixEvent = function(evt, sender){
		if((!evt)&&(window["event"])){
			var evt = window.event;
		}
		
		if((evt["type"])&&(evt["type"].indexOf("key") == 0)){ // key events
			evt.keys = this.revKeys;
			// FIXME: how can we eliminate this iteration?
			for(var key in this.keys) {
				evt[key] = this.keys[key];
			}
			if((dojo.render.html.ie)&&(evt["type"] == "keypress")){
				evt.charCode = evt.keyCode;
			}
		}
	
		if(dojo.render.html.ie){
			if(!evt.target){ evt.target = evt.srcElement; }
			if(!evt.currentTarget){ evt.currentTarget = (sender ? sender : evt.srcElement); }
			if(!evt.layerX){ evt.layerX = evt.offsetX; }
			if(!evt.layerY){ evt.layerY = evt.offsetY; }
			// FIXME: scroll position query is duped from dojo.html to avoid dependency on that entire module
			if(!evt.pageX){ evt.pageX = evt.clientX + (window.pageXOffset || document.documentElement.scrollLeft || document.body.scrollLeft || 0) }
			if(!evt.pageY){ evt.pageY = evt.clientY + (window.pageYOffset || document.documentElement.scrollTop || document.body.scrollTop || 0) }
			// mouseover
			if(evt.type == "mouseover"){ evt.relatedTarget = evt.fromElement; }
			// mouseout
			if(evt.type == "mouseout"){ evt.relatedTarget = evt.toElement; }
			this.currentEvent = evt;
			evt.callListener = this.callListener;
			evt.stopPropagation = this.stopPropagation;
			evt.preventDefault = this.preventDefault;
		}
		return evt;
	}

	this.stopEvent = function(ev) {
		if(window.event){
			ev.returnValue = false;
			ev.cancelBubble = true;
		}else{
			ev.preventDefault();
			ev.stopPropagation();
		}
	}
}

dojo.kwCompoundRequire({
	common: ["dojo.event", "dojo.event.topic"],
	browser: ["dojo.event.browser"],
	dashboard: ["dojo.event.browser"]
});
dojo.provide("dojo.event.*");

/*		This is the dojo logging facility, which is imported from nWidgets
		(written by Alex Russell, CLA on file), which is patterned on the
		Python logging module, which in turn has been heavily influenced by
		log4j (execpt with some more pythonic choices, which we adopt as well).

		While the dojo logging facilities do provide a set of familiar
		interfaces, many of the details are changed to reflect the constraints
		of the browser environment. Mainly, file and syslog-style logging
		facilites are not provided, with HTTP POST and GET requests being the
		only ways of getting data from the browser back to a server. Minimal
		support for this (and XML serialization of logs) is provided, but may
		not be of practical use in a deployment environment.

		The Dojo logging classes are agnostic of any environment, and while
		default loggers are provided for browser-based interpreter
		environments, this file and the classes it define are explicitly
		designed to be portable to command-line interpreters and other
		ECMA-262v3 envrionments.

	the logger needs to accomidate:
		log "levels"
		type identifiers
		file?
		message
		tic/toc?

	The logger should ALWAYS record:
		time/date logged
		message
		type
		level
*/
// TODO: conver documentation to javadoc style once we confirm that is our choice
// TODO: define DTD for XML-formatted log messages
// TODO: write XML Formatter class
// TODO: write HTTP Handler which uses POST to send log lines/sections

// Filename:	LogCore.js
// Purpose:		a common logging infrastructure for dojo
// Classes:		dojo.logging, dojo.logging.Logger, dojo.logging.Record, dojo.logging.LogFilter
// Global Objects:	dojo.logging
// Dependencies:	none

dojo.provide("dojo.logging.Logger");
dojo.provide("dojo.log");
dojo.require("dojo.lang");

/*
	A simple data structure class that stores information for and about
	a logged event. Objects of this type are created automatically when
	an event is logged and are the internal format in which information
	about log events is kept.
*/

dojo.logging.Record = function(lvl, msg){
	this.level = lvl;
	this.message = msg;
	this.time = new Date();
	// FIXME: what other information can we receive/discover here?
}

// an empty parent (abstract) class which concrete filters should inherit from.
dojo.logging.LogFilter = function(loggerChain){
	this.passChain = loggerChain || "";
	this.filter = function(record){
		// FIXME: need to figure out a way to enforce the loggerChain
		// restriction
		return true; // pass all records
	}
}

dojo.logging.Logger = function(){
	this.cutOffLevel = 0;
	this.propagate = true;
	this.parent = null;
	// storage for dojo.logging.Record objects seen and accepted by this logger
	this.data = [];
	this.filters = [];
	this.handlers = [];
}

dojo.lang.extend(dojo.logging.Logger, {
	argsToArr: function(args){
		// utility function, reproduced from __util__ here to remove dependency
		var ret = [];
		for(var x=0; x<args.length; x++){
			ret.push(args[x]);
		}
		return ret;
	},

	setLevel: function(lvl){
		this.cutOffLevel = parseInt(lvl);
	},

	isEnabledFor: function(lvl){
		return parseInt(lvl) >= this.cutOffLevel;
	},

	getEffectiveLevel: function(){
		if((this.cutOffLevel==0)&&(this.parent)){
			return this.parent.getEffectiveLevel();
		}
		return this.cutOffLevel;
	},

	addFilter: function(flt){
		this.filters.push(flt);
		return this.filters.length-1;
	},

	removeFilterByIndex: function(fltIndex){
		if(this.filters[fltIndex]){
			delete this.filters[fltIndex];
			return true;
		}
		return false;
	},

	removeFilter: function(fltRef){
		for(var x=0; x<this.filters.length; x++){
			if(this.filters[x]===fltRef){
				delete this.filters[x];
				return true;
			}
		}
		return false;
	},

	removeAllFilters: function(){
		this.filters = []; // clobber all of them
	},

	filter: function(rec){
		for(var x=0; x<this.filters.length; x++){
			if((this.filters[x]["filter"])&&
			   (!this.filters[x].filter(rec))||
			   (rec.level<this.cutOffLevel)){
				return false;
			}
		}
		return true;
	},

	addHandler: function(hdlr){
		this.handlers.push(hdlr);
		return this.handlers.length-1;
	},

	handle: function(rec){
		if((!this.filter(rec))||(rec.level<this.cutOffLevel)){ return false; }
		for(var x=0; x<this.handlers.length; x++){
			if(this.handlers[x]["handle"]){
			   this.handlers[x].handle(rec);
			}
		}
		// FIXME: not sure what to do about records to be propagated that may have
		// been modified by the handlers or the filters at this logger. Should
		// parents always have pristine copies? or is passing the modified record
		// OK?
		// if((this.propagate)&&(this.parent)){ this.parent.handle(rec); }
		return true;
	},

	// the heart and soul of the logging system
	log: function(lvl, msg){
		if(	(this.propagate)&&(this.parent)&&
			(this.parent.rec.level>=this.cutOffLevel)){
			this.parent.log(lvl, msg);
			return false;
		}
		// FIXME: need to call logging providers here!
		this.handle(new dojo.logging.Record(lvl, msg));
		return true;
	},

	// logger helpers
	debug:function(msg){
		return this.logType("DEBUG", this.argsToArr(arguments));
	},

	info: function(msg){
		return this.logType("INFO", this.argsToArr(arguments));
	},

	warning: function(msg){
		return this.logType("WARNING", this.argsToArr(arguments));
	},

	error: function(msg){
		return this.logType("ERROR", this.argsToArr(arguments));
	},

	critical: function(msg){
		return this.logType("CRITICAL", this.argsToArr(arguments));
	},

	exception: function(msg, e, squelch){
		// FIXME: this needs to be modified to put the exception in the msg
		// if we're on Moz, we can get the following from the exception object:
		//		lineNumber
		//		message
		//		fileName
		//		stack
		//		name
		// on IE, we get:
		//		name
		//		message (from MDA?)
		//		number
		//		description (same as message!)
		if(e){
			var eparts = [e.name, (e.description||e.message)];
			if(e.fileName){
				eparts.push(e.fileName);
				eparts.push("line "+e.lineNumber);
				// eparts.push(e.stack);
			}
			msg += " "+eparts.join(" : ");
		}

		this.logType("ERROR", msg);
		if(!squelch){
			throw e;
		}
	},

	logType: function(type, args){
		var na = [dojo.logging.log.getLevel(type)];
		if(typeof args == "array"){
			na = na.concat(args);
		}else if((typeof args == "object")&&(args["length"])){
			na = na.concat(this.argsToArr(args));
			/* for(var x=0; x<args.length; x++){
				na.push(args[x]);
			} */
		}else{
			na = na.concat(this.argsToArr(arguments).slice(1));
			/* for(var x=1; x<arguments.length; x++){
				na.push(arguments[x]);
			} */
		}
		return this.log.apply(this, na);
	}
});

void(function(){
	var ptype = dojo.logging.Logger.prototype;
	ptype.warn = ptype.warning;
	ptype.err = ptype.error;
	ptype.crit = ptype.critical;
})();

// the Handler class
dojo.logging.LogHandler = function(level){
	this.cutOffLevel = (level) ? level : 0;
	this.formatter = null; // FIXME: default formatter?
	this.data = [];
	this.filters = [];
}

dojo.logging.LogHandler.prototype.setFormatter = function(fmtr){
	// FIXME: need to vet that it is indeed a formatter object
	dojo.unimplemented("setFormatter");
}

dojo.logging.LogHandler.prototype.flush = function(){
	dojo.unimplemented("flush");
}

dojo.logging.LogHandler.prototype.close = function(){
	dojo.unimplemented("close");
}

dojo.logging.LogHandler.prototype.handleError = function(){
	dojo.unimplemented("handleError");
}

dojo.logging.LogHandler.prototype.handle = function(record){
	// emits the passed record if it passes this object's filters
	if((this.filter(record))&&(record.level>=this.cutOffLevel)){
		this.emit(record);
	}
}

dojo.logging.LogHandler.prototype.emit = function(record){
	// do whatever is necessaray to actually log the record
	dojo.unimplemented("emit");
}

// set aliases since we don't want to inherit from dojo.logging.Logger
void(function(){ // begin globals protection closure
	var names = [
		"setLevel", "addFilter", "removeFilterByIndex", "removeFilter",
		"removeAllFilters", "filter"
	];
	var tgt = dojo.logging.LogHandler.prototype;
	var src = dojo.logging.Logger.prototype;
	for(var x=0; x<names.length; x++){
		tgt[names[x]] = src[names[x]];
	}
})(); // end globals protection closure

dojo.logging.log = new dojo.logging.Logger();

// an associative array of logger objects. This object inherits from
// a list of level names with their associated numeric levels
dojo.logging.log.levels = [ {"name": "DEBUG", "level": 1},
						   {"name": "INFO", "level": 2},
						   {"name": "WARNING", "level": 3},
						   {"name": "ERROR", "level": 4},
						   {"name": "CRITICAL", "level": 5} ];

dojo.logging.log.loggers = {};

dojo.logging.log.getLogger = function(name){
	if(!this.loggers[name]){
		this.loggers[name] = new dojo.logging.Logger();
		this.loggers[name].parent = this;
	}
	return this.loggers[name];
}

dojo.logging.log.getLevelName = function(lvl){
	for(var x=0; x<this.levels.length; x++){
		if(this.levels[x].level == lvl){
			return this.levels[x].name;
		}
	}
	return null;
}

dojo.logging.log.addLevelName = function(name, lvl){
	if(this.getLevelName(name)){
		this.err("could not add log level "+name+" because a level with that name already exists");
		return false;
	}
	this.levels.append({"name": name, "level": parseInt(lvl)});
	return true;
}

dojo.logging.log.getLevel = function(name){
	for(var x=0; x<this.levels.length; x++){
		if(this.levels[x].name.toUpperCase() == name.toUpperCase()){
			return this.levels[x].level;
		}
	}
	return null;
}

// a default handler class, it simply saves all of the handle()'d records in
// memory. Useful for attaching to with dojo.event.connect()
dojo.logging.MemoryLogHandler = function(level, recordsToKeep, postType, postInterval){
	// mixin style inheritance
	dojo.logging.LogHandler.call(this, level);
	// default is unlimited
	this.numRecords = (typeof djConfig['loggingNumRecords'] != 'undefined') ? djConfig['loggingNumRecords'] : ((recordsToKeep) ? recordsToKeep : -1);
	// 0=count, 1=time, -1=don't post TODO: move this to a better location for prefs
	this.postType = (typeof djConfig['loggingPostType'] != 'undefined') ? djConfig['loggingPostType'] : ( postType || -1);
	// milliseconds for time, interger for number of records, -1 for non-posting,
	this.postInterval = (typeof djConfig['loggingPostInterval'] != 'undefined') ? djConfig['loggingPostInterval'] : ( postType || -1);
	
}
// prototype inheritance
dojo.logging.MemoryLogHandler.prototype = new dojo.logging.LogHandler();

// FIXME
// dojo.inherits(dojo.logging.MemoryLogHandler, 

// over-ride base-class
dojo.logging.MemoryLogHandler.prototype.emit = function(record){
	this.data.push(record);
	if(this.numRecords != -1){
		while(this.data.length>this.numRecords){
			this.data.shift();
		}
	}
}

dojo.logging.logQueueHandler = new dojo.logging.MemoryLogHandler(0,50,0,10000);
// actual logging event handler
dojo.logging.logQueueHandler.emit = function(record){
	// we should probably abstract this in the future
	var logStr = String(dojo.log.getLevelName(record.level)+": "+record.time.toLocaleTimeString())+": "+record.message;
	if(!dj_undef("debug", dj_global)){
		dojo.debug(logStr);
	}else if((typeof dj_global["print"] == "function")&&(!dojo.render.html.capable)){
		print(logStr);
	}
	this.data.push(record);
	if(this.numRecords != -1){
		while(this.data.length>this.numRecords){
			this.data.shift();
		}
	}
}

dojo.logging.log.addHandler(dojo.logging.logQueueHandler);
dojo.log = dojo.logging.log;

dojo.kwCompoundRequire({
	common: ["dojo.logging.Logger", false, false],
	rhino: ["dojo.logging.RhinoLogger"]
});
dojo.provide("dojo.logging.*");

dojo.provide("dojo.io.IO");
dojo.require("dojo.string");
dojo.require("dojo.lang.extras");

/******************************************************************************
 *	Notes about dojo.io design:
 *	
 *	The dojo.io.* package has the unenviable task of making a lot of different
 *	types of I/O feel natural, despite a universal lack of good (or even
 *	reasonable!) I/O capability in the host environment. So lets pin this down
 *	a little bit further.
 *
 *	Rhino:
 *		perhaps the best situation anywhere. Access to Java classes allows you
 *		to do anything one might want in terms of I/O, both synchronously and
 *		async. Can open TCP sockets and perform low-latency client/server
 *		interactions. HTTP transport is available through Java HTTP client and
 *		server classes. Wish it were always this easy.
 *
 *	xpcshell:
 *		XPCOM for I/O. A cluster-fuck to be sure.
 *
 *	spidermonkey:
 *		S.O.L.
 *
 *	Browsers:
 *		Browsers generally do not provide any useable filesystem access. We are
 *		therefore limited to HTTP for moving information to and from Dojo
 *		instances living in a browser.
 *
 *		XMLHTTP:
 *			Sync or async, allows reading of arbitrary text files (including
 *			JS, which can then be eval()'d), writing requires server
 *			cooperation and is limited to HTTP mechanisms (POST and GET).
 *
 *		<iframe> hacks:
 *			iframe document hacks allow browsers to communicate asynchronously
 *			with a server via HTTP POST and GET operations. With significant
 *			effort and server cooperation, low-latency data transit between
 *			client and server can be acheived via iframe mechanisms (repubsub).
 *
 *		SVG:
 *			Adobe's SVG viewer implements helpful primitives for XML-based
 *			requests, but receipt of arbitrary text data seems unlikely w/o
 *			<![CDATA[]]> sections.
 *
 *
 *	A discussion between Dylan, Mark, Tom, and Alex helped to lay down a lot
 *	the IO API interface. A transcript of it can be found at:
 *		http://dojotoolkit.org/viewcvs/viewcvs.py/documents/irc/irc_io_api_log.txt?rev=307&view=auto
 *	
 *	Also referenced in the design of the API was the DOM 3 L&S spec:
 *		http://www.w3.org/TR/2004/REC-DOM-Level-3-LS-20040407/load-save.html
 ******************************************************************************/

// a map of the available transport options. Transports should add themselves
// by calling add(name)
dojo.io.transports = [];
dojo.io.hdlrFuncNames = [ "load", "error", "timeout" ]; // we're omitting a progress() event for now

dojo.io.Request = function(url, mimetype, transport, changeUrl){
	if((arguments.length == 1)&&(arguments[0].constructor == Object)){
		this.fromKwArgs(arguments[0]);
	}else{
		this.url = url;
		if(mimetype){ this.mimetype = mimetype; }
		if(transport){ this.transport = transport; }
		if(arguments.length >= 4){ this.changeUrl = changeUrl; }
	}
}

dojo.lang.extend(dojo.io.Request, {

	/** The URL to hit */
	url: "",
	
	/** The mime type used to interrpret the response body */
	mimetype: "text/plain",
	
	/** The HTTP method to use */
	method: "GET",
	
	/** An Object containing key-value pairs to be included with the request */
	content: undefined, // Object
	
	/** The transport medium to use */
	transport: undefined, // String
	
	/** If defined the URL of the page is physically changed */
	changeUrl: undefined, // String
	
	/** A form node to use in the request */
	formNode: undefined, // HTMLFormElement
	
	/** Whether the request should be made synchronously */
	sync: false,
	
	bindSuccess: false,

	/** Cache/look for the request in the cache before attempting to request?
	 *  NOTE: this isn't a browser cache, this is internal and would only cache in-page
	 */
	useCache: false,

	/** Prevent the browser from caching this by adding a query string argument to the URL */
	preventCache: false,
	
	// events stuff
	load: function(type, data, evt){ },
	error: function(type, error){ },
	timeout: function(type){ },
	handle: function(){ },

	//FIXME: change BrowserIO.js to use timeouts? IframeIO?
	// The number of seconds to wait until firing a timeout callback.
	// If it is zero, that means, don't do a timeout check.
	timeoutSeconds: 0,
	
	// the abort method needs to be filled in by the transport that accepts the
	// bind() request
	abort: function(){ },
	
	// backButton: function(){ },
	// forwardButton: function(){ },

	fromKwArgs: function(kwArgs){
		// normalize args
		if(kwArgs["url"]){ kwArgs.url = kwArgs.url.toString(); }
		if(kwArgs["formNode"]) { kwArgs.formNode = dojo.byId(kwArgs.formNode); }
		if(!kwArgs["method"] && kwArgs["formNode"] && kwArgs["formNode"].method) {
			kwArgs.method = kwArgs["formNode"].method;
		}
		
		// backwards compatibility
		if(!kwArgs["handle"] && kwArgs["handler"]){ kwArgs.handle = kwArgs.handler; }
		if(!kwArgs["load"] && kwArgs["loaded"]){ kwArgs.load = kwArgs.loaded; }
		if(!kwArgs["changeUrl"] && kwArgs["changeURL"]) { kwArgs.changeUrl = kwArgs.changeURL; }

		// encoding fun!
		kwArgs.encoding = dojo.lang.firstValued(kwArgs["encoding"], djConfig["bindEncoding"], "");

		kwArgs.sendTransport = dojo.lang.firstValued(kwArgs["sendTransport"], djConfig["ioSendTransport"], false);

		var isFunction = dojo.lang.isFunction;
		for(var x=0; x<dojo.io.hdlrFuncNames.length; x++){
			var fn = dojo.io.hdlrFuncNames[x];
			if(isFunction(kwArgs[fn])){ continue; }
			if(isFunction(kwArgs["handle"])){
				kwArgs[fn] = kwArgs.handle;
			}
			// handler is aliased above, shouldn't need this check
			/* else if(dojo.lang.isObject(kwArgs.handler)){
				if(isFunction(kwArgs.handler[fn])){
					kwArgs[fn] = kwArgs.handler[fn]||kwArgs.handler["handle"]||function(){};
				}
			}*/
		}
		dojo.lang.mixin(this, kwArgs);
	}

});

dojo.io.Error = function(msg, type, num){
	this.message = msg;
	this.type =  type || "unknown"; // must be one of "io", "parse", "unknown"
	this.number = num || 0; // per-substrate error number, not normalized
}

dojo.io.transports.addTransport = function(name){
	this.push(name);
	// FIXME: do we need to handle things that aren't direct children of the
	// dojo.io namespace? (say, dojo.io.foo.fooTransport?)
	this[name] = dojo.io[name];
}

// binding interface, the various implementations register their capabilities
// and the bind() method dispatches
dojo.io.bind = function(request){
	// if the request asks for a particular implementation, use it
	if(!(request instanceof dojo.io.Request)){
		try{
			request = new dojo.io.Request(request);
		}catch(e){ dojo.debug(e); }
	}
	var tsName = "";
	if(request["transport"]){
		tsName = request["transport"];
		// FIXME: it would be good to call the error handler, although we'd
		// need to use setTimeout or similar to accomplish this and we can't
		// garuntee that this facility is available.
		if(!this[tsName]){ return request; }
	}else{
		// otherwise we do our best to auto-detect what available transports
		// will handle 
		for(var x=0; x<dojo.io.transports.length; x++){
			var tmp = dojo.io.transports[x];
			if((this[tmp])&&(this[tmp].canHandle(request))){
				tsName = tmp;
			}
		}
		if(tsName == ""){ return request; }
	}
	this[tsName].bind(request);
	request.bindSuccess = true;
	return request;
}

dojo.io.queueBind = function(request){
	if(!(request instanceof dojo.io.Request)){
		try{
			request = new dojo.io.Request(request);
		}catch(e){ dojo.debug(e); }
	}

	// make sure we get called if/when we get a response
	var oldLoad = request.load;
	request.load = function(){
		dojo.io._queueBindInFlight = false;
		var ret = oldLoad.apply(this, arguments);
		dojo.io._dispatchNextQueueBind();
		return ret;
	}

	var oldErr = request.error;
	request.error = function(){
		dojo.io._queueBindInFlight = false;
		var ret = oldErr.apply(this, arguments);
		dojo.io._dispatchNextQueueBind();
		return ret;
	}

	dojo.io._bindQueue.push(request);
	dojo.io._dispatchNextQueueBind();
	return request;
}

dojo.io._dispatchNextQueueBind = function(){
	if(!dojo.io._queueBindInFlight){
		dojo.io._queueBindInFlight = true;
		if(dojo.io._bindQueue.length > 0){
			dojo.io.bind(dojo.io._bindQueue.shift());
		}else{
			dojo.io._queueBindInFlight = false;
		}
	}
}
dojo.io._bindQueue = [];
dojo.io._queueBindInFlight = false;

dojo.io.argsFromMap = function(map, encoding, last){
	var enc = /utf/i.test(encoding||"") ? encodeURIComponent : dojo.string.encodeAscii;
	var mapped = [];
	var control = new Object();
	for(var name in map){
		var domap = function(elt){
			var val = enc(name)+"="+enc(elt);
			mapped[(last == name) ? "push" : "unshift"](val);
		}
		if(!control[name]){
			var value = map[name];
			// FIXME: should be isArrayLike?
			if (dojo.lang.isArray(value)){
				dojo.lang.forEach(value, domap);
			}else{
				domap(value);
			}
		}
	}
	return mapped.join("&");
}

dojo.io.setIFrameSrc = function(iframe, src, replace){
	try{
		var r = dojo.render.html;
		// dojo.debug(iframe);
		if(!replace){
			if(r.safari){
				iframe.location = src;
			}else{
				frames[iframe.name].location = src;
			}
		}else{
			// Fun with DOM 0 incompatibilities!
			var idoc;
			if(r.ie){
				idoc = iframe.contentWindow.document;
			}else if(r.safari){
				idoc = iframe.document;
			}else{ //  if(r.moz){
				idoc = iframe.contentWindow;
			}
			idoc.location.replace(src);
		}
	}catch(e){ 
		dojo.debug(e); 
		dojo.debug("setIFrameSrc: "+e); 
	}
}

/*
dojo.io.sampleTranport = new function(){
	this.canHandle = function(kwArgs){
		// canHandle just tells dojo.io.bind() if this is a good transport to
		// use for the particular type of request.
		if(	
			(
				(kwArgs["mimetype"] == "text/plain") ||
				(kwArgs["mimetype"] == "text/html") ||
				(kwArgs["mimetype"] == "text/javascript")
			)&&(
				(kwArgs["method"] == "get") ||
				( (kwArgs["method"] == "post") && (!kwArgs["formNode"]) )
			)
		){
			return true;
		}

		return false;
	}

	this.bind = function(kwArgs){
		var hdlrObj = {};

		// set up a handler object
		for(var x=0; x<dojo.io.hdlrFuncNames.length; x++){
			var fn = dojo.io.hdlrFuncNames[x];
			if(typeof kwArgs.handler == "object"){
				if(typeof kwArgs.handler[fn] == "function"){
					hdlrObj[fn] = kwArgs.handler[fn]||kwArgs.handler["handle"];
				}
			}else if(typeof kwArgs[fn] == "function"){
				hdlrObj[fn] = kwArgs[fn];
			}else{
				hdlrObj[fn] = kwArgs["handle"]||function(){};
			}
		}

		// build a handler function that calls back to the handler obj
		var hdlrFunc = function(evt){
			if(evt.type == "onload"){
				hdlrObj.load("load", evt.data, evt);
			}else if(evt.type == "onerr"){
				var errObj = new dojo.io.Error("sampleTransport Error: "+evt.msg);
				hdlrObj.error("error", errObj);
			}
		}

		// the sample transport would attach the hdlrFunc() when sending the
		// request down the pipe at this point
		var tgtURL = kwArgs.url+"?"+dojo.io.argsFromMap(kwArgs.content);
		// sampleTransport.sendRequest(tgtURL, hdlrFunc);
	}

	dojo.io.transports.addTransport("sampleTranport");
}
*/

dojo.provide("dojo.string.extras");

dojo.require("dojo.string.common");
dojo.require("dojo.lang");

/**
 * Parameterized string function
 * str - formatted string with %{values} to be replaces
 * pairs - object of name: "value" value pairs
 * killExtra - remove all remaining %{values} after pairs are inserted
 */
dojo.string.paramString = function(str, pairs, killExtra) {
	for(var name in pairs) {
		var re = new RegExp("\\%\\{" + name + "\\}", "g");
		str = str.replace(re, pairs[name]);
	}

	if(killExtra) { str = str.replace(/%\{([^\}\s]+)\}/g, ""); }
	return str;
}

/** Uppercases the first letter of each word */
dojo.string.capitalize = function (str) {
	if (!dojo.lang.isString(str)) { return ""; }
	if (arguments.length == 0) { str = this; }
	var words = str.split(' ');
	var retval = "";
	var len = words.length;
	for (var i=0; i<len; i++) {
		var word = words[i];
		word = word.charAt(0).toUpperCase() + word.substring(1, word.length);
		retval += word;
		if (i < len-1)
			retval += " ";
	}
	
	return new String(retval);
}

/**
 * Return true if the entire string is whitespace characters
 */
dojo.string.isBlank = function (str) {
	if(!dojo.lang.isString(str)) { return true; }
	return (dojo.string.trim(str).length == 0);
}

dojo.string.encodeAscii = function(str) {
	if(!dojo.lang.isString(str)) { return str; }
	var ret = "";
	var value = escape(str);
	var match, re = /%u([0-9A-F]{4})/i;
	while((match = value.match(re))) {
		var num = Number("0x"+match[1]);
		var newVal = escape("&#" + num + ";");
		ret += value.substring(0, match.index) + newVal;
		value = value.substring(match.index+match[0].length);
	}
	ret += value.replace(/\+/g, "%2B");
	return ret;
}

dojo.string.escape = function(type, str) {
	var args = [];
	for(var i = 1; i < arguments.length; i++) { args.push(arguments[i]); }
	switch(type.toLowerCase()) {
		case "xml":
		case "html":
		case "xhtml":
			return dojo.string.escapeXml.apply(this, args);
		case "sql":
			return dojo.string.escapeSql.apply(this, args);
		case "regexp":
		case "regex":
			return dojo.string.escapeRegExp.apply(this, args);
		case "javascript":
		case "jscript":
		case "js":
			return dojo.string.escapeJavaScript.apply(this, args);
		case "ascii":
			// so it's encode, but it seems useful
			return dojo.string.encodeAscii.apply(this, args);
		default:
			return str;
	}
}

dojo.string.escapeXml = function(str, noSingleQuotes) {
	str = str.replace(/&/gm, "&amp;").replace(/</gm, "&lt;")
		.replace(/>/gm, "&gt;").replace(/"/gm, "&quot;");
	if(!noSingleQuotes) { str = str.replace(/'/gm, "&#39;"); }
	return str;
}

dojo.string.escapeSql = function(str) {
	return str.replace(/'/gm, "''");
}

dojo.string.escapeRegExp = function(str) {
	return str.replace(/\\/gm, "\\\\").replace(/([\f\b\n\t\r[\^$|?*+(){}])/gm, "\\$1");
}

dojo.string.escapeJavaScript = function(str) {
	return str.replace(/(["'\f\b\n\t\r])/gm, "\\$1");
}

dojo.string.escapeString = function(str){ 
	return ('"' + str.replace(/(["\\])/g, '\\$1') + '"'
		).replace(/[\f]/g, "\\f"
		).replace(/[\b]/g, "\\b"
		).replace(/[\n]/g, "\\n"
		).replace(/[\t]/g, "\\t"
		).replace(/[\r]/g, "\\r");
}

// TODO: make an HTML version
dojo.string.summary = function(str, len) {
	if(!len || str.length <= len) {
		return str;
	} else {
		return str.substring(0, len).replace(/\.+$/, "") + "...";
	}
}

/**
 * Returns true if 'str' ends with 'end'
 */
dojo.string.endsWith = function(str, end, ignoreCase) {
	if(ignoreCase) {
		str = str.toLowerCase();
		end = end.toLowerCase();
	}
	if((str.length - end.length) < 0){
		return false;
	}
	return str.lastIndexOf(end) == str.length - end.length;
}

/**
 * Returns true if 'str' ends with any of the arguments[2 -> n]
 */
dojo.string.endsWithAny = function(str /* , ... */) {
	for(var i = 1; i < arguments.length; i++) {
		if(dojo.string.endsWith(str, arguments[i])) {
			return true;
		}
	}
	return false;
}

/**
 * Returns true if 'str' starts with 'start'
 */
dojo.string.startsWith = function(str, start, ignoreCase) {
	if(ignoreCase) {
		str = str.toLowerCase();
		start = start.toLowerCase();
	}
	return str.indexOf(start) == 0;
}

/**
 * Returns true if 'str' starts with any of the arguments[2 -> n]
 */
dojo.string.startsWithAny = function(str /* , ... */) {
	for(var i = 1; i < arguments.length; i++) {
		if(dojo.string.startsWith(str, arguments[i])) {
			return true;
		}
	}
	return false;
}

/**
 * Returns true if 'str' starts with any of the arguments 2 -> n
 */
dojo.string.has = function(str /* , ... */) {
	for(var i = 1; i < arguments.length; i++) {
		if(str.indexOf(arguments[i]) > -1){
			return true;
		}
	}
	return false;
}

dojo.string.normalizeNewlines = function (text,newlineChar) {
	if (newlineChar == "\n") {
		text = text.replace(/\r\n/g, "\n");
		text = text.replace(/\r/g, "\n");
	} else if (newlineChar == "\r") {
		text = text.replace(/\r\n/g, "\r");
		text = text.replace(/\n/g, "\r");
	} else {
		text = text.replace(/([^\r])\n/g, "$1\r\n");
		text = text.replace(/\r([^\n])/g, "\r\n$1");
	}
	return text;
}

dojo.string.splitEscaped = function (str,charac) {
	var components = [];
	for (var i = 0, prevcomma = 0; i < str.length; i++) {
		if (str.charAt(i) == '\\') { i++; continue; }
		if (str.charAt(i) == charac) {
			components.push(str.substring(prevcomma, i));
			prevcomma = i + 1;
		}
	}
	components.push(str.substr(prevcomma));
	return components;
}

dojo.provide("dojo.undo.browser");
dojo.require("dojo.io");

try{
	if((!djConfig["preventBackButtonFix"])&&(!dojo.hostenv.post_load_)){
		document.write("<iframe style='border: 0px; width: 1px; height: 1px; position: absolute; bottom: 0px; right: 0px; visibility: visible;' name='djhistory' id='djhistory' src='"+(dojo.hostenv.getBaseScriptUri()+'iframe_history.html')+"'></iframe>");
	}
}catch(e){/* squelch */}

/* NOTES:
 *  Safari 1.2: 
 *	back button "works" fine, however it's not possible to actually
 *	DETECT that you've moved backwards by inspecting window.location.
 *	Unless there is some other means of locating.
 *	FIXME: perhaps we can poll on history.length?
 *  Safari 2.0.3+ (and probably 1.3.2+):
 *	works fine, except when changeUrl is used. When changeUrl is used,
 *	Safari jumps all the way back to whatever page was shown before
 *	the page that uses dojo.undo.browser support.
 *  IE 5.5 SP2:
 *	back button behavior is macro. It does not move back to the
 *	previous hash value, but to the last full page load. This suggests
 *	that the iframe is the correct way to capture the back button in
 *	these cases.
 *	Don't test this page using local disk for MSIE. MSIE will not create 
 *	a history list for iframe_history.html if served from a file: URL. 
 *	The XML served back from the XHR tests will also not be properly 
 *	created if served from local disk. Serve the test pages from a web 
 *	server to test in that browser.
 *  IE 6.0:
 *	same behavior as IE 5.5 SP2
 * Firefox 1.0:
 *	the back button will return us to the previous hash on the same
 *	page, thereby not requiring an iframe hack, although we do then
 *	need to run a timer to detect inter-page movement.
 */
dojo.undo.browser = {
	initialHref: window.location.href,
	initialHash: window.location.hash,

	moveForward: false,
	historyStack: [],
	forwardStack: [],
	historyIframe: null,
	bookmarkAnchor: null,
	locationTimer: null,

	/**
	 * setInitialState sets the state object and back callback for the very first page that is loaded.
	 * It is recommended that you call this method as part of an event listener that is registered via
	 * dojo.addOnLoad().
	 */
	setInitialState: function(args){
		this.initialState = {"url": this.initialHref, "kwArgs": args, "urlHash": this.initialHash};
	},

	//FIXME: Would like to support arbitrary back/forward jumps. Have to rework iframeLoaded among other things.
	//FIXME: is there a slight race condition in moz using change URL with the timer check and when
	//       the hash gets set? I think I have seen a back/forward call in quick succession, but not consistent.
	/**
	 * addToHistory takes one argument, and it is an object that defines the following functions:
	 * - To support getting back button notifications, the object argument should implement a
	 *   function called either "back", "backButton", or "handle". The string "back" will be
	 *   passed as the first and only argument to this callback.
	 * - To support getting forward button notifications, the object argument should implement a
	 *   function called either "forward", "forwardButton", or "handle". The string "forward" will be
	 *   passed as the first and only argument to this callback.
	 * - If you want the browser location string to change, define "changeUrl" on the object. If the
	 *   value of "changeUrl" is true, then a unique number will be appended to the URL as a fragment
	 *   identifier (http://some.domain.com/path#uniquenumber). If it is any other value that does
	 *   not evaluate to false, that value will be used as the fragment identifier. For example,
	 *   if changeUrl: 'page1', then the URL will look like: http://some.domain.com/path#page1
	 *   
	 * Full example:
	 * 
	 * dojo.undo.browser.addToHistory({
	 *   back: function() { alert('back pressed'); },
	 *   forward: function() { alert('forward pressed'); },
	 *   changeUrl: true
	 * });
	 */
	addToHistory: function(args){
		var hash = null;
		if(!this.historyIframe){
			this.historyIframe = window.frames["djhistory"];
		}
		if(!this.bookmarkAnchor){
			this.bookmarkAnchor = document.createElement("a");
			(document.body||document.getElementsByTagName("body")[0]).appendChild(this.bookmarkAnchor);
			this.bookmarkAnchor.style.display = "none";
		}
		if((!args["changeUrl"])||(dojo.render.html.ie)){
			var url = dojo.hostenv.getBaseScriptUri()+"iframe_history.html?"+(new Date()).getTime();
			this.moveForward = true;
			dojo.io.setIFrameSrc(this.historyIframe, url, false);
		}
		if(args["changeUrl"]){
			this.changingUrl = true;
			hash = "#"+ ((args["changeUrl"]!==true) ? args["changeUrl"] : (new Date()).getTime());
			setTimeout("window.location.href = '"+hash+"'; dojo.undo.browser.changingUrl = false;", 1);
			this.bookmarkAnchor.href = hash;
			
			if(dojo.render.html.ie){
				var oldCB = args["back"]||args["backButton"]||args["handle"];

				//The function takes handleName as a parameter, in case the
				//callback we are overriding was "handle". In that case,
				//we will need to pass the handle name to handle.
				var tcb = function(handleName){
					if(window.location.hash != ""){
						setTimeout("window.location.href = '"+hash+"';", 1);
					}
					//Use apply to set "this" to args, and to try to avoid memory leaks.
					oldCB.apply(this, [handleName]);
				}
		
				//Set interceptor function in the right place.
				if(args["back"]){
					args.back = tcb;
				}else if(args["backButton"]){
					args.backButton = tcb;
				}else if(args["handle"]){
					args.handle = tcb;
				}
		
				//If addToHistory is called, then that means we prune the
				//forward stack -- the user went back, then wanted to
				//start a new forward path.
				this.forwardStack = []; 
				var oldFW = args["forward"]||args["forwardButton"]||args["handle"];
		
				//The function takes handleName as a parameter, in case the
				//callback we are overriding was "handle". In that case,
				//we will need to pass the handle name to handle.
				var tfw = function(handleName){
					if(window.location.hash != ""){
						window.location.href = hash;
					}
					if(oldFW){ // we might not actually have one
						//Use apply to set "this" to args, and to try to avoid memory leaks.
						oldFW.apply(this, [handleName]);
					}
				}

				//Set interceptor function in the right place.
				if(args["forward"]){
					args.forward = tfw;
				}else if(args["forwardButton"]){
					args.forwardButton = tfw;
				}else if(args["handle"]){
					args.handle = tfw;
				}

			}else if(dojo.render.html.moz){
				// start the timer
				if(!this.locationTimer){
					this.locationTimer = setInterval("dojo.undo.browser.checkLocation();", 200);
				}
			}
		}

		this.historyStack.push({"url": url, "kwArgs": args, "urlHash": hash});
	},

	checkLocation: function(){
		if (!this.changingUrl){
			var hsl = this.historyStack.length;
	
			if((window.location.hash == this.initialHash)||(window.location.href == this.initialHref)&&(hsl == 1)){
				// FIXME: could this ever be a forward button?
				// we can't clear it because we still need to check for forwards. Ugg.
				// clearInterval(this.locationTimer);
				this.handleBackButton();
				return;
			}
			// first check to see if we could have gone forward. We always halt on
			// a no-hash item.
			if(this.forwardStack.length > 0){
				if(this.forwardStack[this.forwardStack.length-1].urlHash == window.location.hash){
					this.handleForwardButton();
					return;
				}
			}
	
			// ok, that didn't work, try someplace back in the history stack
			if((hsl >= 2)&&(this.historyStack[hsl-2])){
				if(this.historyStack[hsl-2].urlHash==window.location.hash){
					this.handleBackButton();
					return;
				}
			}
		}
	},

	iframeLoaded: function(evt, ifrLoc){
		var query = this._getUrlQuery(ifrLoc.href);
		if(query == null){ 
			// alert("iframeLoaded");
			// we hit the end of the history, so we should go back
			if(this.historyStack.length == 1){
				this.handleBackButton();
			}
			return;
		}
		if(this.moveForward){
			// we were expecting it, so it's not either a forward or backward movement
			this.moveForward = false;
			return;
		}

		//Check the back stack first, since it is more likely.
		//Note that only one step back or forward is supported.
		if(this.historyStack.length >= 2 && query == this._getUrlQuery(this.historyStack[this.historyStack.length-2].url)){
			this.handleBackButton();
		}
		else if(this.forwardStack.length > 0 && query == this._getUrlQuery(this.forwardStack[this.forwardStack.length-1].url)){
			this.handleForwardButton();
		}
	},

	handleBackButton: function(){
		//The "current" page is always at the top of the history stack.
		var current = this.historyStack.pop();
		if(!current){ return; }
		var last = this.historyStack[this.historyStack.length-1];
		if(!last && this.historyStack.length == 0){
			last = this.initialState;
		}
		if (last){
			if(last.kwArgs["back"]){
				last.kwArgs["back"]();
			}else if(last.kwArgs["backButton"]){
				last.kwArgs["backButton"]();
			}else if(last.kwArgs["handle"]){
				last.kwArgs.handle("back");
			}
		}
		this.forwardStack.push(current);
	},

	handleForwardButton: function(){
		var last = this.forwardStack.pop();
		if(!last){ return; }
		if(last.kwArgs["forward"]){
			last.kwArgs.forward();
		}else if(last.kwArgs["forwardButton"]){
			last.kwArgs.forwardButton();
		}else if(last.kwArgs["handle"]){
			last.kwArgs.handle("forward");
		}
		this.historyStack.push(last);
	},

	_getUrlQuery: function(url){
		var segments = url.split("?");
		if (segments.length < 2){
			return null;
		}
		else{
			return segments[1];
		}
	}
}

dojo.provide("dojo.io.BrowserIO");

dojo.require("dojo.io");
dojo.require("dojo.lang.array");
dojo.require("dojo.lang.func");
dojo.require("dojo.string.extras");
dojo.require("dojo.dom");
dojo.require("dojo.undo.browser");

dojo.io.checkChildrenForFile = function(node){
	var hasFile = false;
	var inputs = node.getElementsByTagName("input");
	dojo.lang.forEach(inputs, function(input){
		if(hasFile){ return; }
		if(input.getAttribute("type")=="file"){
			hasFile = true;
		}
	});
	return hasFile;
}

dojo.io.formHasFile = function(formNode){
	return dojo.io.checkChildrenForFile(formNode);
}

dojo.io.updateNode = function(node, urlOrArgs){
	node = dojo.byId(node);
	var args = urlOrArgs;
	if(dojo.lang.isString(urlOrArgs)){
		args = { url: urlOrArgs };
	}
	args.mimetype = "text/html";
	args.load = function(t, d, e){
		while(node.firstChild){
			if(dojo["event"]){
				try{
					dojo.event.browser.clean(node.firstChild);
				}catch(e){}
			}
			node.removeChild(node.firstChild);
		}
		node.innerHTML = d;
	};
	dojo.io.bind(args);
}

dojo.io.formFilter = function(node) {
	var type = (node.type||"").toLowerCase();
	return !node.disabled && node.name
		&& !dojo.lang.inArray(type, ["file", "submit", "image", "reset", "button"]);
}

// TODO: Move to htmlUtils
dojo.io.encodeForm = function(formNode, encoding, formFilter){
	if((!formNode)||(!formNode.tagName)||(!formNode.tagName.toLowerCase() == "form")){
		dojo.raise("Attempted to encode a non-form element.");
	}
	if(!formFilter) { formFilter = dojo.io.formFilter; }
	var enc = /utf/i.test(encoding||"") ? encodeURIComponent : dojo.string.encodeAscii;
	var values = [];

	for(var i = 0; i < formNode.elements.length; i++){
		var elm = formNode.elements[i];
		if(!elm || elm.tagName.toLowerCase() == "fieldset" || !formFilter(elm)) { continue; }
		var name = enc(elm.name);
		var type = elm.type.toLowerCase();

		if(type == "select-multiple"){
			for(var j = 0; j < elm.options.length; j++){
				if(elm.options[j].selected) {
					values.push(name + "=" + enc(elm.options[j].value));
				}
			}
		}else if(dojo.lang.inArray(type, ["radio", "checkbox"])){
			if(elm.checked){
				values.push(name + "=" + enc(elm.value));
			}
		}else{
			values.push(name + "=" + enc(elm.value));
		}
	}

	// now collect input type="image", which doesn't show up in the elements array
	var inputs = formNode.getElementsByTagName("input");
	for(var i = 0; i < inputs.length; i++) {
		var input = inputs[i];
		if(input.type.toLowerCase() == "image" && input.form == formNode
			&& formFilter(input)) {
			var name = enc(input.name);
			values.push(name + "=" + enc(input.value));
			values.push(name + ".x=0");
			values.push(name + ".y=0");
		}
	}
	return values.join("&") + "&";
}

dojo.io.FormBind = function(args) {
	this.bindArgs = {};

	if(args && args.formNode) {
		this.init(args);
	} else if(args) {
		this.init({formNode: args});
	}
}
dojo.lang.extend(dojo.io.FormBind, {
	form: null,

	bindArgs: null,

	clickedButton: null,

	init: function(args) {
		var form = dojo.byId(args.formNode);

		if(!form || !form.tagName || form.tagName.toLowerCase() != "form") {
			throw new Error("FormBind: Couldn't apply, invalid form");
		} else if(this.form == form) {
			return;
		} else if(this.form) {
			throw new Error("FormBind: Already applied to a form");
		}

		dojo.lang.mixin(this.bindArgs, args);
		this.form = form;

		this.connect(form, "onsubmit", "submit");

		for(var i = 0; i < form.elements.length; i++) {
			var node = form.elements[i];
			if(node && node.type && dojo.lang.inArray(node.type.toLowerCase(), ["submit", "button"])) {
				this.connect(node, "onclick", "click");
			}
		}

		var inputs = form.getElementsByTagName("input");
		for(var i = 0; i < inputs.length; i++) {
			var input = inputs[i];
			if(input.type.toLowerCase() == "image" && input.form == form) {
				this.connect(input, "onclick", "click");
			}
		}
	},

	onSubmit: function(form) {
		return true;
	},

	submit: function(e) {
		e.preventDefault();
		if(this.onSubmit(this.form)) {
			dojo.io.bind(dojo.lang.mixin(this.bindArgs, {
				formFilter: dojo.lang.hitch(this, "formFilter")
			}));
		}
	},

	click: function(e) {
		var node = e.currentTarget;
		if(node.disabled) { return; }
		this.clickedButton = node;
	},

	formFilter: function(node) {
		var type = (node.type||"").toLowerCase();
		var accept = false;
		if(node.disabled || !node.name) {
			accept = false;
		} else if(dojo.lang.inArray(type, ["submit", "button", "image"])) {
			if(!this.clickedButton) { this.clickedButton = node; }
			accept = node == this.clickedButton;
		} else {
			accept = !dojo.lang.inArray(type, ["file", "submit", "reset", "button"]);
		}
		return accept;
	},

	// in case you don't have dojo.event.* pulled in
	connect: function(srcObj, srcFcn, targetFcn) {
		if(dojo.evalObjPath("dojo.event.connect")) {
			dojo.event.connect(srcObj, srcFcn, this, targetFcn);
		} else {
			var fcn = dojo.lang.hitch(this, targetFcn);
			srcObj[srcFcn] = function(e) {
				if(!e) { e = window.event; }
				if(!e.currentTarget) { e.currentTarget = e.srcElement; }
				if(!e.preventDefault) { e.preventDefault = function() { window.event.returnValue = false; } }
				fcn(e);
			}
		}
	}
});

dojo.io.XMLHTTPTransport = new function(){
	var _this = this;

	var _cache = {}; // FIXME: make this public? do we even need to?
	this.useCache = false; // if this is true, we'll cache unless kwArgs.useCache = false
	this.preventCache = false; // if this is true, we'll always force GET requests to cache

	// FIXME: Should this even be a function? or do we just hard code it in the next 2 functions?
	function getCacheKey(url, query, method) {
		return url + "|" + query + "|" + method.toLowerCase();
	}

	function addToCache(url, query, method, http) {
		_cache[getCacheKey(url, query, method)] = http;
	}

	function getFromCache(url, query, method) {
		return _cache[getCacheKey(url, query, method)];
	}

	this.clearCache = function() {
		_cache = {};
	}

	// moved successful load stuff here
	function doLoad(kwArgs, http, url, query, useCache) {
		if((http.status==200)||(http.status==304)||(http.status==204)||
			(location.protocol=="file:" && (http.status==0 || http.status==undefined))||
			(location.protocol=="chrome:" && (http.status==0 || http.status==undefined))
		){
			var ret;
			if(kwArgs.method.toLowerCase() == "head"){
				var headers = http.getAllResponseHeaders();
				ret = {};
				ret.toString = function(){ return headers; }
				var values = headers.split(/[\r\n]+/g);
				for(var i = 0; i < values.length; i++) {
					var pair = values[i].match(/^([^:]+)\s*:\s*(.+)$/i);
					if(pair) {
						ret[pair[1]] = pair[2];
					}
				}
			}else if(kwArgs.mimetype == "text/javascript"){
				try{
					ret = dj_eval(http.responseText);
				}catch(e){
					dojo.debug(e);
					dojo.debug(http.responseText);
					ret = null;
				}
			}else if(kwArgs.mimetype == "text/json"){
				try{
					ret = dj_eval("("+http.responseText+")");
				}catch(e){
					dojo.debug(e);
					dojo.debug(http.responseText);
					ret = false;
				}
			}else if((kwArgs.mimetype == "application/xml")||
						(kwArgs.mimetype == "text/xml")){
				ret = http.responseXML;
				if(!ret || typeof ret == "string") {
					ret = dojo.dom.createDocumentFromText(http.responseText);
				}
			}else{
				ret = http.responseText;
			}

			if(useCache){ // only cache successful responses
				addToCache(url, query, kwArgs.method, http);
			}
			kwArgs[(typeof kwArgs.load == "function") ? "load" : "handle"]("load", ret, http, kwArgs);
		}else{
			var errObj = new dojo.io.Error("XMLHttpTransport Error: "+http.status+" "+http.statusText);
			kwArgs[(typeof kwArgs.error == "function") ? "error" : "handle"]("error", errObj, http, kwArgs);
		}
	}

	// set headers (note: Content-Type will get overriden if kwArgs.contentType is set)
	function setHeaders(http, kwArgs){
		if(kwArgs["headers"]) {
			for(var header in kwArgs["headers"]) {
				if(header.toLowerCase() == "content-type" && !kwArgs["contentType"]) {
					kwArgs["contentType"] = kwArgs["headers"][header];
				} else {
					http.setRequestHeader(header, kwArgs["headers"][header]);
				}
			}
		}
	}

	this.inFlight = [];
	this.inFlightTimer = null;

	this.startWatchingInFlight = function(){
		if(!this.inFlightTimer){
			this.inFlightTimer = setInterval("dojo.io.XMLHTTPTransport.watchInFlight();", 10);
		}
	}

	this.watchInFlight = function(){
		var now = null;
		for(var x=this.inFlight.length-1; x>=0; x--){
			var tif = this.inFlight[x];
			if(!tif){ this.inFlight.splice(x, 1); continue; }
			if(4==tif.http.readyState){
				// remove it so we can clean refs
				this.inFlight.splice(x, 1);
				doLoad(tif.req, tif.http, tif.url, tif.query, tif.useCache);
			}else if (tif.startTime){
				//See if this is a timeout case.
				if(!now){
					now = (new Date()).getTime();
				}
				if(tif.startTime + (tif.req.timeoutSeconds * 1000) < now){
					//Stop the request.
					if(typeof tif.http.abort == "function"){
						tif.http.abort();
					}

					// remove it so we can clean refs
					this.inFlight.splice(x, 1);
					tif.req[(typeof tif.req.timeout == "function") ? "timeout" : "handle"]("timeout", null, tif.http, tif.req);
				}
			}
		}

		if(this.inFlight.length == 0){
			clearInterval(this.inFlightTimer);
			this.inFlightTimer = null;
		}
	}

	var hasXmlHttp = dojo.hostenv.getXmlhttpObject() ? true : false;
	this.canHandle = function(kwArgs){
		// canHandle just tells dojo.io.bind() if this is a good transport to
		// use for the particular type of request.

		// FIXME: we need to determine when form values need to be
		// multi-part mime encoded and avoid using this transport for those
		// requests.
		return hasXmlHttp
			&& dojo.lang.inArray((kwArgs["mimetype"].toLowerCase()||""), ["text/plain", "text/html", "application/xml", "text/xml", "text/javascript", "text/json"])
			&& !( kwArgs["formNode"] && dojo.io.formHasFile(kwArgs["formNode"]) );
	}

	this.multipartBoundary = "45309FFF-BD65-4d50-99C9-36986896A96F";	// unique guid as a boundary value for multipart posts

	this.bind = function(kwArgs){
		if(!kwArgs["url"]){
			// are we performing a history action?
			if( !kwArgs["formNode"]
				&& (kwArgs["backButton"] || kwArgs["back"] || kwArgs["changeUrl"] || kwArgs["watchForURL"])
				&& (!djConfig.preventBackButtonFix)) {
        dojo.deprecated("Using dojo.io.XMLHTTPTransport.bind() to add to browser history without doing an IO request is deprecated. Use dojo.undo.browser.addToHistory() instead.");
				dojo.undo.browser.addToHistory(kwArgs);
				return true;
			}
		}

		// build this first for cache purposes
		var url = kwArgs.url;
		var query = "";
		if(kwArgs["formNode"]){
			var ta = kwArgs.formNode.getAttribute("action");
			if((ta)&&(!kwArgs["url"])){ url = ta; }
			var tp = kwArgs.formNode.getAttribute("method");
			if((tp)&&(!kwArgs["method"])){ kwArgs.method = tp; }
			query += dojo.io.encodeForm(kwArgs.formNode, kwArgs.encoding, kwArgs["formFilter"]);
		}

		if(url.indexOf("#") > -1) {
			dojo.debug("Warning: dojo.io.bind: stripping hash values from url:", url);
			url = url.split("#")[0];
		}

		if(kwArgs["file"]){
			// force post for file transfer
			kwArgs.method = "post";
		}

		if(!kwArgs["method"]){
			kwArgs.method = "get";
		}

		// guess the multipart value		
		if(kwArgs.method.toLowerCase() == "get"){
			// GET cannot use multipart
			kwArgs.multipart = false;
		}else{
			if(kwArgs["file"]){
				// enforce multipart when sending files
				kwArgs.multipart = true;
			}else if(!kwArgs["multipart"]){
				// default 
				kwArgs.multipart = false;
			}
		}

		if(kwArgs["backButton"] || kwArgs["back"] || kwArgs["changeUrl"]){
			dojo.undo.browser.addToHistory(kwArgs);
		}

		var content = kwArgs["content"] || {};

		if(kwArgs.sendTransport) {
			content["dojo.transport"] = "xmlhttp";
		}

		do { // break-block
			if(kwArgs.postContent){
				query = kwArgs.postContent;
				break;
			}

			if(content) {
				query += dojo.io.argsFromMap(content, kwArgs.encoding);
			}
			
			if(kwArgs.method.toLowerCase() == "get" || !kwArgs.multipart){
				break;
			}

			var	t = [];
			if(query.length){
				var q = query.split("&");
				for(var i = 0; i < q.length; ++i){
					if(q[i].length){
						var p = q[i].split("=");
						t.push(	"--" + this.multipartBoundary,
								"Content-Disposition: form-data; name=\"" + p[0] + "\"", 
								"",
								p[1]);
					}
				}
			}

			if(kwArgs.file){
				if(dojo.lang.isArray(kwArgs.file)){
					for(var i = 0; i < kwArgs.file.length; ++i){
						var o = kwArgs.file[i];
						t.push(	"--" + this.multipartBoundary,
								"Content-Disposition: form-data; name=\"" + o.name + "\"; filename=\"" + ("fileName" in o ? o.fileName : o.name) + "\"",
								"Content-Type: " + ("contentType" in o ? o.contentType : "application/octet-stream"),
								"",
								o.content);
					}
				}else{
					var o = kwArgs.file;
					t.push(	"--" + this.multipartBoundary,
							"Content-Disposition: form-data; name=\"" + o.name + "\"; filename=\"" + ("fileName" in o ? o.fileName : o.name) + "\"",
							"Content-Type: " + ("contentType" in o ? o.contentType : "application/octet-stream"),
							"",
							o.content);
				}
			}

			if(t.length){
				t.push("--"+this.multipartBoundary+"--", "");
				query = t.join("\r\n");
			}
		}while(false);

		// kwArgs.Connection = "close";

		var async = kwArgs["sync"] ? false : true;

		var preventCache = kwArgs["preventCache"] ||
			(this.preventCache == true && kwArgs["preventCache"] != false);
		var useCache = kwArgs["useCache"] == true ||
			(this.useCache == true && kwArgs["useCache"] != false );

		// preventCache is browser-level (add query string junk), useCache
		// is for the local cache. If we say preventCache, then don't attempt
		// to look in the cache, but if useCache is true, we still want to cache
		// the response
		if(!preventCache && useCache){
			var cachedHttp = getFromCache(url, query, kwArgs.method);
			if(cachedHttp){
				doLoad(kwArgs, cachedHttp, url, query, false);
				return;
			}
		}

		// much of this is from getText, but reproduced here because we need
		// more flexibility
		var http = dojo.hostenv.getXmlhttpObject(kwArgs);	
		var received = false;

		// build a handler function that calls back to the handler obj
		if(async){
			var startTime = 
			// FIXME: setting up this callback handler leaks on IE!!!
			this.inFlight.push({
				"req":		kwArgs,
				"http":		http,
				"url":	 	url,
				"query":	query,
				"useCache":	useCache,
				"startTime": kwArgs.timeoutSeconds ? (new Date()).getTime() : 0
			});
			this.startWatchingInFlight();
		}

		if(kwArgs.method.toLowerCase() == "post"){
			// FIXME: need to hack in more flexible Content-Type setting here!
			http.open("POST", url, async);
			setHeaders(http, kwArgs);
			http.setRequestHeader("Content-Type", kwArgs.multipart ? ("multipart/form-data; boundary=" + this.multipartBoundary) : 
				(kwArgs.contentType || "application/x-www-form-urlencoded"));
			try{
				http.send(query);
			}catch(e){
				if(typeof http.abort == "function"){
					http.abort();
				}
				doLoad(kwArgs, {status: 404}, url, query, useCache);
			}
		}else{
			var tmpUrl = url;
			if(query != "") {
				tmpUrl += (tmpUrl.indexOf("?") > -1 ? "&" : "?") + query;
			}
			if(preventCache) {
				tmpUrl += (dojo.string.endsWithAny(tmpUrl, "?", "&")
					? "" : (tmpUrl.indexOf("?") > -1 ? "&" : "?")) + "dojo.preventCache=" + new Date().valueOf();
			}
			http.open(kwArgs.method.toUpperCase(), tmpUrl, async);
			setHeaders(http, kwArgs);
			try {
				http.send(null);
			}catch(e)	{
				if(typeof http.abort == "function"){
					http.abort();
				}
				doLoad(kwArgs, {status: 404}, url, query, useCache);
			}
		}

		if( !async ) {
			doLoad(kwArgs, http, url, query, useCache);
		}

		kwArgs.abort = function(){
			return http.abort();
		}

		return;
	}
	dojo.io.transports.addTransport("XMLHTTPTransport");
}

dojo.provide("dojo.io.cookie");

dojo.io.cookie.setCookie = function(name, value, days, path, domain, secure) {
	var expires = -1;
	if(typeof days == "number" && days >= 0) {
		var d = new Date();
		d.setTime(d.getTime()+(days*24*60*60*1000));
		expires = d.toGMTString();
	}
	value = escape(value);
	document.cookie = name + "=" + value + ";"
		+ (expires != -1 ? " expires=" + expires + ";" : "")
		+ (path ? "path=" + path : "")
		+ (domain ? "; domain=" + domain : "")
		+ (secure ? "; secure" : "");
}

dojo.io.cookie.set = dojo.io.cookie.setCookie;

dojo.io.cookie.getCookie = function(name) {
	// FIXME: Which cookie should we return?
	//        If there are cookies set for different sub domains in the current
	//        scope there could be more than one cookie with the same name.
	//        I think taking the last one in the list takes the one from the
	//        deepest subdomain, which is what we're doing here.
	var idx = document.cookie.lastIndexOf(name+'=');
	if(idx == -1) { return null; }
	value = document.cookie.substring(idx+name.length+1);
	var end = value.indexOf(';');
	if(end == -1) { end = value.length; }
	value = value.substring(0, end);
	value = unescape(value);
	return value;
}

dojo.io.cookie.get = dojo.io.cookie.getCookie;

dojo.io.cookie.deleteCookie = function(name) {
	dojo.io.cookie.setCookie(name, "-", 0);
}

dojo.io.cookie.setObjectCookie = function(name, obj, days, path, domain, secure, clearCurrent) {
	if(arguments.length == 5) { // for backwards compat
		clearCurrent = domain;
		domain = null;
		secure = null;
	}
	var pairs = [], cookie, value = "";
	if(!clearCurrent) { cookie = dojo.io.cookie.getObjectCookie(name); }
	if(days >= 0) {
		if(!cookie) { cookie = {}; }
		for(var prop in obj) {
			if(prop == null) {
				delete cookie[prop];
			} else if(typeof obj[prop] == "string" || typeof obj[prop] == "number") {
				cookie[prop] = obj[prop];
			}
		}
		prop = null;
		for(var prop in cookie) {
			pairs.push(escape(prop) + "=" + escape(cookie[prop]));
		}
		value = pairs.join("&");
	}
	dojo.io.cookie.setCookie(name, value, days, path, domain, secure);
}

dojo.io.cookie.getObjectCookie = function(name) {
	var values = null, cookie = dojo.io.cookie.getCookie(name);
	if(cookie) {
		values = {};
		var pairs = cookie.split("&");
		for(var i = 0; i < pairs.length; i++) {
			var pair = pairs[i].split("=");
			var value = pair[1];
			if( isNaN(value) ) { value = unescape(pair[1]); }
			values[ unescape(pair[0]) ] = value;
		}
	}
	return values;
}

dojo.io.cookie.isSupported = function() {
	if(typeof navigator.cookieEnabled != "boolean") {
		dojo.io.cookie.setCookie("__TestingYourBrowserForCookieSupport__",
			"CookiesAllowed", 90, null);
		var cookieVal = dojo.io.cookie.getCookie("__TestingYourBrowserForCookieSupport__");
		navigator.cookieEnabled = (cookieVal == "CookiesAllowed");
		if(navigator.cookieEnabled) {
			// FIXME: should we leave this around?
			this.deleteCookie("__TestingYourBrowserForCookieSupport__");
		}
	}
	return navigator.cookieEnabled;
}

// need to leave this in for backwards-compat from 0.1 for when it gets pulled in by dojo.io.*
if(!dojo.io.cookies) { dojo.io.cookies = dojo.io.cookie; }

dojo.kwCompoundRequire({
	common: ["dojo.io"],
	rhino: ["dojo.io.RhinoIO"],
	browser: ["dojo.io.BrowserIO", "dojo.io.cookie"],
	dashboard: ["dojo.io.BrowserIO", "dojo.io.cookie"]
});
dojo.provide("dojo.io.*");

dojo.kwCompoundRequire({
	common: ["dojo.uri.Uri", false, false]
});
dojo.provide("dojo.uri.*");

dojo.provide("dojo.io.IframeIO");
dojo.require("dojo.io.BrowserIO");
dojo.require("dojo.uri.*");

// FIXME: is it possible to use the Google htmlfile hack to prevent the
// background click with this transport?

dojo.io.createIFrame = function(fname, onloadstr){
	if(window[fname]){ return window[fname]; }
	if(window.frames[fname]){ return window.frames[fname]; }
	var r = dojo.render.html;
	var cframe = null;
	var turi = dojo.uri.dojoUri("iframe_history.html?noInit=true");
	var ifrstr = ((r.ie)&&(dojo.render.os.win)) ? "<iframe name='"+fname+"' src='"+turi+"' onload='"+onloadstr+"'>" : "iframe";
	cframe = document.createElement(ifrstr);
	with(cframe){
		name = fname;
		setAttribute("name", fname);
		id = fname;
	}
	(document.body||document.getElementsByTagName("body")[0]).appendChild(cframe);
	window[fname] = cframe;
	with(cframe.style){
		position = "absolute";
		left = top = "0px";
		height = width = "1px";
		visibility = "hidden";
		/*
		if(djConfig.isDebug){
			position = "relative";
			height = "300px";
			width = "600px";
			visibility = "visible";
		}
		*/
	}

	if(!r.ie){
		dojo.io.setIFrameSrc(cframe, turi, true);
		cframe.onload = new Function(onloadstr);
	}
	return cframe;
}

// thanks burstlib!
dojo.io.iframeContentWindow = function(iframe_el) {
	var win = iframe_el.contentWindow || // IE
		dojo.io.iframeContentDocument(iframe_el).defaultView || // Moz, opera
		// Moz. TODO: is this available when defaultView isn't?
		dojo.io.iframeContentDocument(iframe_el).__parent__ || 
		(iframe_el.name && document.frames[iframe_el.name]) || null;
	return win;
}

dojo.io.iframeContentDocument = function(iframe_el){
	var doc = iframe_el.contentDocument || // W3
		(
			(iframe_el.contentWindow)&&(iframe_el.contentWindow.document)
		) ||  // IE
		(
			(iframe_el.name)&&(document.frames[iframe_el.name])&&
			(document.frames[iframe_el.name].document)
		) || null;
	return doc;
}

dojo.io.IframeTransport = new function(){
	var _this = this;
	this.currentRequest = null;
	this.requestQueue = [];
	this.iframeName = "dojoIoIframe";

	this.fireNextRequest = function(){
		if((this.currentRequest)||(this.requestQueue.length == 0)){ return; }
		// dojo.debug("fireNextRequest");
		var cr = this.currentRequest = this.requestQueue.shift();
		cr._contentToClean = [];
		var fn = cr["formNode"];
		var content = cr["content"] || {};
		if(cr.sendTransport) {
			content["dojo.transport"] = "iframe";
		}
		if(fn){
			if(content){
				// if we have things in content, we need to add them to the form
				// before submission
				for(var x in content){
					if(!fn[x]){
						var tn;
						if(dojo.render.html.ie){
							tn = document.createElement("<input type='hidden' name='"+x+"' value='"+content[x]+"'>");
							fn.appendChild(tn);
						}else{
							tn = document.createElement("input");
							fn.appendChild(tn);
							tn.type = "hidden";
							tn.name = x;
							tn.value = content[x];
						}
						cr._contentToClean.push(x);
					}else{
						fn[x].value = content[x];
					}
				}
			}
			if(cr["url"]){
				cr._originalAction = fn.getAttribute("action");
				fn.setAttribute("action", cr.url);
			}
			if(!fn.getAttribute("method")){
				fn.setAttribute("method", (cr["method"]) ? cr["method"] : "post");
			}
			cr._originalTarget = fn.getAttribute("target");
			fn.setAttribute("target", this.iframeName);
			fn.target = this.iframeName;
			fn.submit();
		}else{
			// otherwise we post a GET string by changing URL location for the
			// iframe
			var query = dojo.io.argsFromMap(this.currentRequest.content);
			var tmpUrl = (cr.url.indexOf("?") > -1 ? "&" : "?") + query;
			dojo.io.setIFrameSrc(this.iframe, tmpUrl, true);
		}
	}

	this.canHandle = function(kwArgs){
		return (
			(
				// FIXME: can we really handle text/plain and
				// text/javascript requests?
				dojo.lang.inArray(kwArgs["mimetype"], 
				[	"text/plain", "text/html", 
					"application/xml", "text/xml", 
					"text/javascript", "text/json"])
			)&&(
				// make sur we really only get used in file upload cases	
				(kwArgs["formNode"])&&(dojo.io.checkChildrenForFile(kwArgs["formNode"]))
			)&&(
				dojo.lang.inArray(kwArgs["method"].toLowerCase(), ["post", "get"])
			)&&(
				// never handle a sync request
				!  ((kwArgs["sync"])&&(kwArgs["sync"] == true))
			)
		);
	}

	this.bind = function(kwArgs){
		if(!this["iframe"]){ this.setUpIframe(); }
		this.requestQueue.push(kwArgs);
		this.fireNextRequest();
		return;
	}

	this.setUpIframe = function(){

		// NOTE: IE 5.0 and earlier Mozilla's don't support an onload event for
		//       iframes. OTOH, we don't care.
		this.iframe = dojo.io.createIFrame(this.iframeName, "dojo.io.IframeTransport.iframeOnload();");
	}

	this.iframeOnload = function(){
		if(!_this.currentRequest){
			_this.fireNextRequest();
			return;
		}

		var req = _this.currentRequest;

		// remove all the hidden content inputs
		var toClean = req._contentToClean;
		for(var i = 0; i < toClean.length; i++) {
			var key = toClean[i];
			var input = req.formNode[key];
			req.formNode.removeChild(input);
			req.formNode[key] = null;
		}
		// restore original action + target
		if(req["_originalAction"]){
			req.formNode.setAttribute("action", req._originalAction);
		}
		req.formNode.setAttribute("target", req._originalTarget);
		req.formNode.target = req._originalTarget;

		var ifr = _this.iframe;
		var ifw = dojo.io.iframeContentWindow(ifr);
		// handle successful returns
		// FIXME: how do we determine success for iframes? Is there an equiv of
		// the "status" property?
		var value;
		var success = false;

		try{
			var cmt = req.mimetype;
			if((cmt == "text/javascript")||(cmt == "text/json")){
				// FIXME: not sure what to do here? try to pull some evalulable
				// text from a textarea or cdata section? 
				// how should we set up the contract for that?
				var cd = dojo.io.iframeContentDocument(_this.iframe);
				var js = cd.getElementsByTagName("textarea")[0].value;
				if(cmt == "text/json") { js = "(" + js + ")"; }
				value = dj_eval(js);
			}else if((cmt == "application/xml")||(cmt == "text/xml")){
				value = dojo.io.iframeContentDocument(_this.iframe);
			}else{ // text/plain
				value = ifw.innerHTML;
			}
			success = true;
		}catch(e){ 
			// looks like we didn't get what we wanted!
			var errObj = new dojo.io.Error("IframeTransport Error");
			if(dojo.lang.isFunction(req["error"])){
				req.error("error", errObj, req);
			}
		}

		// don't want to mix load function errors with processing errors, thus
		// a separate try..catch
		try {
			if(success && dojo.lang.isFunction(req["load"])){
				req.load("load", value, req);
			}
		} catch(e) {
			throw e;
		} finally {
			_this.currentRequest = null;
			_this.fireNextRequest();
		}
	}

	dojo.io.transports.addTransport("IframeTransport");
}

dojo.provide("dojo.date");


/* Supplementary Date Functions
 *******************************/

dojo.date.setDayOfYear = function (dateObject, dayofyear) {
	dateObject.setMonth(0);
	dateObject.setDate(dayofyear);
	return dateObject;
}

dojo.date.getDayOfYear = function (dateObject) {
	var firstDayOfYear = new Date(dateObject.getFullYear(), 0, 1);
	return Math.floor((dateObject.getTime() -
		firstDayOfYear.getTime()) / 86400000);
}




dojo.date.setWeekOfYear = function (dateObject, week, firstDay) {
	if (arguments.length == 1) { firstDay = 0; } // Sunday
	dojo.unimplemented("dojo.date.setWeekOfYear");
}

dojo.date.getWeekOfYear = function (dateObject, firstDay) {
	if (arguments.length == 1) { firstDay = 0; } // Sunday

	// work out the first day of the year corresponding to the week
	var firstDayOfYear = new Date(dateObject.getFullYear(), 0, 1);
	var day = firstDayOfYear.getDay();
	firstDayOfYear.setDate(firstDayOfYear.getDate() -
			day + firstDay - (day > firstDay ? 7 : 0));

	return Math.floor((dateObject.getTime() -
		firstDayOfYear.getTime()) / 604800000);
}




dojo.date.setIsoWeekOfYear = function (dateObject, week, firstDay) {
	if (arguments.length == 1) { firstDay = 1; } // Monday
	dojo.unimplemented("dojo.date.setIsoWeekOfYear");
}

dojo.date.getIsoWeekOfYear = function (dateObject, firstDay) {
	if (arguments.length == 1) { firstDay = 1; } // Monday
	dojo.unimplemented("dojo.date.getIsoWeekOfYear");
}




/* ISO 8601 Functions
 *********************/

dojo.date.setIso8601 = function (dateObject, string){
	var comps = (string.indexOf("T") == -1) ? string.split(" ") : string.split("T");
	dojo.date.setIso8601Date(dateObject, comps[0]);
	if (comps.length == 2) { dojo.date.setIso8601Time(dateObject, comps[1]); }
	return dateObject;
}

dojo.date.fromIso8601 = function (string) {
	return dojo.date.setIso8601(new Date(0, 0), string);
}




dojo.date.setIso8601Date = function (dateObject, string) {
	var regexp = "^([0-9]{4})((-?([0-9]{2})(-?([0-9]{2}))?)|" +
			"(-?([0-9]{3}))|(-?W([0-9]{2})(-?([1-7]))?))?$";
	var d = string.match(new RegExp(regexp));
	if(!d) {
		dojo.debug("invalid date string: " + string);
		return false;
	}
	var year = d[1];
	var month = d[4];
	var date = d[6];
	var dayofyear = d[8];
	var week = d[10];
	var dayofweek = (d[12]) ? d[12] : 1;

	dateObject.setYear(year);
	
	if (dayofyear) { dojo.date.setDayOfYear(dateObject, Number(dayofyear)); }
	else if (week) {
		dateObject.setMonth(0);
		dateObject.setDate(1);
		var gd = dateObject.getDay();
		var day =  (gd) ? gd : 7;
		var offset = Number(dayofweek) + (7 * Number(week));
		
		if (day <= 4) { dateObject.setDate(offset + 1 - day); }
		else { dateObject.setDate(offset + 8 - day); }
	} else {
		if (month) { 
			dateObject.setDate(1);
			dateObject.setMonth(month - 1); 
		}
		if (date) { dateObject.setDate(date); }
	}
	
	return dateObject;
}

dojo.date.fromIso8601Date = function (string) {
	return dojo.date.setIso8601Date(new Date(0, 0), string);
}




dojo.date.setIso8601Time = function (dateObject, string) {
	// first strip timezone info from the end
	var timezone = "Z|(([-+])([0-9]{2})(:?([0-9]{2}))?)$";
	var d = string.match(new RegExp(timezone));

	var offset = 0; // local time if no tz info
	if (d) {
		if (d[0] != 'Z') {
			offset = (Number(d[3]) * 60) + Number(d[5]);
			offset *= ((d[2] == '-') ? 1 : -1);
		}
		offset -= dateObject.getTimezoneOffset();
		string = string.substr(0, string.length - d[0].length);
	}

	// then work out the time
	var regexp = "^([0-9]{2})(:?([0-9]{2})(:?([0-9]{2})(\.([0-9]+))?)?)?$";
	var d = string.match(new RegExp(regexp));
	if(!d) {
		dojo.debug("invalid time string: " + string);
		return false;
	}
	var hours = d[1];
	var mins = Number((d[3]) ? d[3] : 0);
	var secs = (d[5]) ? d[5] : 0;
	var ms = d[7] ? (Number("0." + d[7]) * 1000) : 0;

	dateObject.setHours(hours);
	dateObject.setMinutes(mins);
	dateObject.setSeconds(secs);
	dateObject.setMilliseconds(ms);
	
	return dateObject;
}

dojo.date.fromIso8601Time = function (string) {
	return dojo.date.setIso8601Time(new Date(0, 0), string);
}



/* Informational Functions
 **************************/

dojo.date.shortTimezones = ["IDLW", "BET", "HST", "MART", "AKST", "PST", "MST",
	"CST", "EST", "AST", "NFT", "BST", "FST", "AT", "GMT", "CET", "EET", "MSK",
	"IRT", "GST", "AFT", "AGTT", "IST", "NPT", "ALMT", "MMT", "JT", "AWST",
	"JST", "ACST", "AEST", "LHST", "VUT", "NFT", "NZT", "CHAST", "PHOT",
	"LINT"];
dojo.date.timezoneOffsets = [-720, -660, -600, -570, -540, -480, -420, -360,
	-300, -240, -210, -180, -120, -60, 0, 60, 120, 180, 210, 240, 270, 300,
	330, 345, 360, 390, 420, 480, 540, 570, 600, 630, 660, 690, 720, 765, 780,
	840];
/*
dojo.date.timezones = ["International Date Line West", "Bering Standard Time",
	"Hawaiian Standard Time", "Marquesas Time", "Alaska Standard Time",
	"Pacific Standard Time (USA)", "Mountain Standard Time",
	"Central Standard Time (USA)", "Eastern Standard Time (USA)",
	"Atlantic Standard Time", "Newfoundland Time", "Brazil Standard Time",
	"Fernando de Noronha Standard Time (Brazil)", "Azores Time",
	"Greenwich Mean Time", "Central Europe Time", "Eastern Europe Time",
	"Moscow Time", "Iran Standard Time", "Gulf Standard Time",
	"Afghanistan Time", "Aqtobe Time", "Indian Standard Time", "Nepal Time",
	"Almaty Time", "Myanmar Time", "Java Time",
	"Australian Western Standard Time", "Japan Standard Time",
	"Australian Central Standard Time", "Lord Hove Standard Time (Australia)",
	"Vanuata Time", "Norfolk Time (Australia)", "New Zealand Standard Time",
	"Chatham Standard Time (New Zealand)", "Phoenix Islands Time (Kribati)",
	"Line Islands Time (Kribati)"];
*/
dojo.date.months = ["January", "February", "March", "April", "May", "June",
	"July", "August", "September", "October", "November", "December"];
dojo.date.shortMonths = ["Jan", "Feb", "Mar", "Apr", "May", "June",
	"July", "Aug", "Sep", "Oct", "Nov", "Dec"];
dojo.date.days = ["Sunday", "Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday"];
dojo.date.shortDays = ["Sun", "Mon", "Tues", "Wed", "Thur", "Fri", "Sat"];


dojo.date.getDaysInMonth = function (dateObject) {
	var month = dateObject.getMonth();
	var days = [31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31];
	if (month == 1 && dojo.date.isLeapYear(dateObject)) { return 29; }
	else { return days[month]; }
}

dojo.date.isLeapYear = function (dateObject) {
	/*
	 * Leap years are years with an additional day YYYY-02-29, where the year
	 * number is a multiple of four with the following exception: If a year
	 * is a multiple of 100, then it is only a leap year if it is also a
	 * multiple of 400. For example, 1900 was not a leap year, but 2000 is one.
	 */
	var year = dateObject.getFullYear();
	return (year%400 == 0) ? true : (year%100 == 0) ? false : (year%4 == 0) ? true : false;
}



dojo.date.getDayName = function (dateObject) {
	return dojo.date.days[dateObject.getDay()];
}

dojo.date.getDayShortName = function (dateObject) {
	return dojo.date.shortDays[dateObject.getDay()];
}




dojo.date.getMonthName = function (dateObject) {
	return dojo.date.months[dateObject.getMonth()];
}

dojo.date.getMonthShortName = function (dateObject) {
	return dojo.date.shortMonths[dateObject.getMonth()];
}




dojo.date.getTimezoneName = function (dateObject) {
	// need to negate timezones to get it right 
	// i.e UTC+1 is CET winter, but getTimezoneOffset returns -60
	var timezoneOffset = -(dateObject.getTimezoneOffset());
	
	for (var i = 0; i < dojo.date.timezoneOffsets.length; i++) {
		if (dojo.date.timezoneOffsets[i] == timezoneOffset) {
			return dojo.date.shortTimezones[i];
		}
	}
	
	// we don't know so return it formatted as "+HH:MM"
	function $ (s) { s = String(s); while (s.length < 2) { s = "0" + s; } return s; }
	return (timezoneOffset < 0 ? "-" : "+") + $(Math.floor(Math.abs(
		timezoneOffset)/60)) + ":" + $(Math.abs(timezoneOffset)%60);
}




dojo.date.getOrdinal = function (dateObject) {
	var date = dateObject.getDate();

	if (date%100 != 11 && date%10 == 1) { return "st"; }
	else if (date%100 != 12 && date%10 == 2) { return "nd"; }
	else if (date%100 != 13 && date%10 == 3) { return "rd"; }
	else { return "th"; }
}



/* Date Formatter Functions
 ***************************/

// POSIX strftime
// see <http://www.opengroup.org/onlinepubs/007908799/xsh/strftime.html>
dojo.date.format = dojo.date.strftime = function (dateObject, format) {

	// zero pad
	var padChar = null;
	function _ (s, n) {
		s = String(s);
		n = (n || 2) - s.length;
		while (n-- > 0) { s = (padChar == null ? "0" : padChar) + s; }
		return s;
	}
	
	function $ (property) {
		switch (property) {
			case "a": // abbreviated weekday name according to the current locale
				return dojo.date.getDayShortName(dateObject); break;

			case "A": // full weekday name according to the current locale
				return dojo.date.getDayName(dateObject); break;

			case "b":
			case "h": // abbreviated month name according to the current locale
				return dojo.date.getMonthShortName(dateObject); break;
				
			case "B": // full month name according to the current locale
				return dojo.date.getMonthName(dateObject); break;
				
			case "c": // preferred date and time representation for the current
				      // locale
				return dateObject.toLocaleString(); break;

			case "C": // century number (the year divided by 100 and truncated
				      // to an integer, range 00 to 99)
				return _(Math.floor(dateObject.getFullYear()/100)); break;
				
			case "d": // day of the month as a decimal number (range 01 to 31)
				return _(dateObject.getDate()); break;
				
			case "D": // same as %m/%d/%y
				return $("m") + "/" + $("d") + "/" + $("y"); break;
					
			case "e": // day of the month as a decimal number, a single digit is
				      // preceded by a space (range ' 1' to '31')
				if (padChar == null) { padChar = " "; }
				return _(dateObject.getDate(), 2); break;
			
			case "g": // like %G, but without the century.
				break;
			
			case "G": // The 4-digit year corresponding to the ISO week number
				      // (see %V).  This has the same format and value as %Y,
				      // except that if the ISO week number belongs to the
				      // previous or next year, that year is used instead.
				break;
			
			case "F": // same as %Y-%m-%d
				return $("Y") + "-" + $("m") + "-" + $("d"); break;
				
			case "H": // hour as a decimal number using a 24-hour clock (range
				      // 00 to 23)
				return _(dateObject.getHours()); break;
				
			case "I": // hour as a decimal number using a 12-hour clock (range
				      // 01 to 12)
				return _(dateObject.getHours() % 12 || 12); break;
				
			case "j": // day of the year as a decimal number (range 001 to 366)
				return _(dojo.date.getDayOfYear(dateObject), 3); break;
				
			case "m": // month as a decimal number (range 01 to 12)
				return _(dateObject.getMonth() + 1); break;
				
			case "M": // minute as a decimal numbe
				return _(dateObject.getMinutes()); break;
			
			case "n":
				return "\n"; break;

			case "p": // either `am' or `pm' according to the given time value,
				      // or the corresponding strings for the current locale
				return dateObject.getHours() < 12 ? "am" : "pm"; break;
				
			case "r": // time in a.m. and p.m. notation
				return $("I") + ":" + $("M") + ":" + $("S") + " " + $("p"); break;
				
			case "R": // time in 24 hour notation
				return $("H") + ":" + $("M"); break;
				
			case "S": // second as a decimal number
				return _(dateObject.getSeconds()); break;

			case "t":
				return "\t"; break;

			case "T": // current time, equal to %H:%M:%S
				return $("H") + ":" + $("M") + ":" + $("S"); break;
				
			case "u": // weekday as a decimal number [1,7], with 1 representing
				      // Monday
				return String(dateObject.getDay() || 7); break;
				
			case "U": // week number of the current year as a decimal number,
				      // starting with the first Sunday as the first day of the
				      // first week
				return _(dojo.date.getWeekOfYear(dateObject)); break;

			case "V": // week number of the year (Monday as the first day of the
				      // week) as a decimal number [01,53]. If the week containing
				      // 1 January has four or more days in the new year, then it 
				      // is considered week 1. Otherwise, it is the last week of 
				      // the previous year, and the next week is week 1.
				return _(dojo.date.getIsoWeekOfYear(dateObject)); break;
				
			case "W": // week number of the current year as a decimal number,
				      // starting with the first Monday as the first day of the
				      // first week
				return _(dojo.date.getWeekOfYear(dateObject, 1)); break;
				
			case "w": // day of the week as a decimal, Sunday being 0
				return String(dateObject.getDay()); break;

			case "x": // preferred date representation for the current locale
				      // without the time
				break;

			case "X": // preferred date representation for the current locale
				      // without the time
				break;

			case "y": // year as a decimal number without a century (range 00 to
				      // 99)
				return _(dateObject.getFullYear()%100); break;
				
			case "Y": // year as a decimal number including the century
				return String(dateObject.getFullYear()); break;
			
			case "z": // time zone or name or abbreviation
				var timezoneOffset = dateObject.getTimezoneOffset();
				return (timezoneOffset < 0 ? "-" : "+") + 
					_(Math.floor(Math.abs(timezoneOffset)/60)) + ":" +
					_(Math.abs(timezoneOffset)%60); break;
				
			case "Z": // time zone or name or abbreviation
				return dojo.date.getTimezoneName(dateObject); break;
			
			case "%":
				return "%"; break;
		}
	}

	// parse the formatting string and construct the resulting string
	var string = "";
	var i = 0, index = 0, switchCase;
	while ((index = format.indexOf("%", i)) != -1) {
		string += format.substring(i, index++);
		
		// inspect modifier flag
		switch (format.charAt(index++)) {
			case "_": // Pad a numeric result string with spaces.
				padChar = " "; break;
			case "-": // Do not pad a numeric result string.
				padChar = ""; break;
			case "0": // Pad a numeric result string with zeros.
				padChar = "0"; break;
			case "^": // Convert characters in result string to upper case.
				switchCase = "upper"; break;
			case "#": // Swap the case of the result string.
				switchCase = "swap"; break;
			default: // no modifer flag so decremenet the index
				padChar = null; index--; break;
		}

		// toggle case if a flag is set
		property = $(format.charAt(index++));
		if (switchCase == "upper" ||
			(switchCase == "swap" && /[a-z]/.test(property))) {
			property = property.toUpperCase();
		} else if (switchCase == "swap" && !/[a-z]/.test(property)) {
			property = property.toLowerCase();
		}
		swicthCase = null;
		
		string += property;
		i = index;
	}
	string += format.substring(i);
	
	return string;
}

/* compare and add
 ******************/
dojo.date.compareTypes={
	// 	summary
	//	bitmask for comparison operations.
	DATE:1, TIME:2 
};
dojo.date.compare=function(/* Date */ dateA, /* Date */ dateB, /* int */ options){
	//	summary
	//	Compare two date objects by date, time, or both.
	var dA=dateA;
	var dB=dateB||new Date();
	var now=new Date();
	var opt=options||(dojo.date.compareTypes.DATE|dojo.date.compareTypes.TIME);
	var d1=new Date(
		((opt&dojo.date.compareTypes.DATE)?(dA.getFullYear()):now.getFullYear()), 
		((opt&dojo.date.compareTypes.DATE)?(dA.getMonth()):now.getMonth()), 
		((opt&dojo.date.compareTypes.DATE)?(dA.getDate()):now.getDate()), 
		((opt&dojo.date.compareTypes.TIME)?(dA.getHours()):0), 
		((opt&dojo.date.compareTypes.TIME)?(dA.getMinutes()):0), 
		((opt&dojo.date.compareTypes.TIME)?(dA.getSeconds()):0)
	);
	var d2=new Date(
		((opt&dojo.date.compareTypes.DATE)?(dB.getFullYear()):now.getFullYear()), 
		((opt&dojo.date.compareTypes.DATE)?(dB.getMonth()):now.getMonth()), 
		((opt&dojo.date.compareTypes.DATE)?(dB.getDate()):now.getDate()), 
		((opt&dojo.date.compareTypes.TIME)?(dB.getHours()):0), 
		((opt&dojo.date.compareTypes.TIME)?(dB.getMinutes()):0), 
		((opt&dojo.date.compareTypes.TIME)?(dB.getSeconds()):0)
	);
	if(d1.valueOf()>d2.valueOf()){
		return 1;	//	int
	}
	if(d1.valueOf()<d2.valueOf()){
		return -1;	//	int
	}
	return 0;	//	int
}

dojo.date.dateParts={ 
	//	summary
	//	constants for use in dojo.date.add
	YEAR:0, MONTH:1, DAY:2, HOUR:3, MINUTE:4, SECOND:5, MILLISECOND:6 
};
dojo.date.add=function(/* Date */ d, /* dojo.date.dateParts */ unit, /* int */ amount){
	var n=(amount)?amount:1;
	var v;
	switch(unit){
		case dojo.date.dateParts.YEAR:{
			v=new Date(d.getFullYear()+n, d.getMonth(), d.getDate(), d.getHours(), d.getMinutes(), d.getSeconds(), d.getMilliseconds());
			break;
		}
		case dojo.date.dateParts.MONTH:{
			v=new Date(d.getFullYear(), d.getMonth()+n, d.getDate(), d.getHours(), d.getMinutes(), d.getSeconds(), d.getMilliseconds());
			break;
		}
		case dojo.date.dateParts.HOUR:{
			v=new Date(d.getFullYear(), d.getMonth(), d.getDate(), d.getHours()+n, d.getMinutes(), d.getSeconds(), d.getMilliseconds());
			break;
		}
		case dojo.date.dateParts.MINUTE:{
			v=new Date(d.getFullYear(), d.getMonth(), d.getDate(), d.getHours(), d.getMinutes()+n, d.getSeconds(), d.getMilliseconds());
			break;
		}
		case dojo.date.dateParts.SECOND:{
			v=new Date(d.getFullYear(), d.getMonth(), d.getDate(), d.getHours(), d.getMinutes(), d.getSeconds()+n, d.getMilliseconds());
			break;
		}
		case dojo.date.dateParts.MILLISECOND:{
			v=new Date(d.getFullYear(), d.getMonth(), d.getDate(), d.getHours(), d.getMinutes(), d.getSeconds(), d.getMilliseconds()+n);
			break;
		}
		default:{
			v=new Date(d.getFullYear(), d.getMonth(), d.getDate()+n, d.getHours(), d.getMinutes(), d.getSeconds(), d.getMilliseconds());
		}
	};
	return v;	//	Date
};

/* Deprecated
 *************/


dojo.date.toString = function(date, format){
	dojo.deprecated("dojo.date.toString",
		"use dojo.date.format instead", "0.4");

	if (format.indexOf("#d") > -1) {
		format = format.replace(/#dddd/g, dojo.date.getDayOfWeekName(date));
		format = format.replace(/#ddd/g, dojo.date.getShortDayOfWeekName(date));
		format = format.replace(/#dd/g, (date.getDate().toString().length==1?"0":"")+date.getDate());
		format = format.replace(/#d/g, date.getDate());
	}

	if (format.indexOf("#M") > -1) {
		format = format.replace(/#MMMM/g, dojo.date.getMonthName(date));
		format = format.replace(/#MMM/g, dojo.date.getShortMonthName(date));
		format = format.replace(/#MM/g, ((date.getMonth()+1).toString().length==1?"0":"")+(date.getMonth()+1));
		format = format.replace(/#M/g, date.getMonth() + 1);
	}

	if (format.indexOf("#y") > -1) {
		var fullYear = date.getFullYear().toString();
		format = format.replace(/#yyyy/g, fullYear);
		format = format.replace(/#yy/g, fullYear.substring(2));
		format = format.replace(/#y/g, fullYear.substring(3));
	}

	// Return if only date needed;
	if (format.indexOf("#") == -1) {
		return format;
	}
	
	if (format.indexOf("#h") > -1) {
		var hours = date.getHours();
		hours = (hours > 12 ? hours - 12 : (hours == 0) ? 12 : hours);
		format = format.replace(/#hh/g, (hours.toString().length==1?"0":"")+hours);
		format = format.replace(/#h/g, hours);
	}
	
	if (format.indexOf("#H") > -1) {
		format = format.replace(/#HH/g, (date.getHours().toString().length==1?"0":"")+date.getHours());
		format = format.replace(/#H/g, date.getHours());
	}
	
	if (format.indexOf("#m") > -1) {
		format = format.replace(/#mm/g, (date.getMinutes().toString().length==1?"0":"")+date.getMinutes());
		format = format.replace(/#m/g, date.getMinutes());
	}

	if (format.indexOf("#s") > -1) {
		format = format.replace(/#ss/g, (date.getSeconds().toString().length==1?"0":"")+date.getSeconds());
		format = format.replace(/#s/g, date.getSeconds());
	}
	
	if (format.indexOf("#T") > -1) {
		format = format.replace(/#TT/g, date.getHours() >= 12 ? "PM" : "AM");
		format = format.replace(/#T/g, date.getHours() >= 12 ? "P" : "A");
	}

	if (format.indexOf("#t") > -1) {
		format = format.replace(/#tt/g, date.getHours() >= 12 ? "pm" : "am");
		format = format.replace(/#t/g, date.getHours() >= 12 ? "p" : "a");
	}
					
	return format;
	
}


dojo.date.daysInMonth = function (month, year) {
	dojo.deprecated("daysInMonth(month, year)",
		"replaced by getDaysInMonth(dateObject)", "0.4");
	return dojo.date.getDaysInMonth(new Date(year, month, 1));
}

/**
 *
 * Returns a string of the date in the version "January 1, 2004"
 *
 * @param date The date object
 */
dojo.date.toLongDateString = function(date) {
	dojo.deprecated("dojo.date.toLongDateString",
		'use dojo.date.format(date, "%B %e, %Y") instead', "0.4");
	return dojo.date.format(date, "%B %e, %Y")
}

/**
 *
 * Returns a string of the date in the version "Jan 1, 2004"
 *
 * @param date The date object
 */
dojo.date.toShortDateString = function(date) {
	dojo.deprecated("dojo.date.toShortDateString",
		'use dojo.date.format(date, "%b %e, %Y") instead', "0.4");
	return dojo.date.format(date, "%b %e, %Y");
}

/**
 *
 * Returns military formatted time
 *
 * @param date the date object
 */
dojo.date.toMilitaryTimeString = function(date){
	dojo.deprecated("dojo.date.toMilitaryTimeString",
		'use dojo.date.format(date, "%T")', "0.4");
	return dojo.date.format(date, "%T");
}

/**
 *
 * Returns a string of the date relative to the current date.
 *
 * @param date The date object
 *
 * Example returns:
 * - "1 minute ago"
 * - "4 minutes ago"
 * - "Yesterday"
 * - "2 days ago"
 */
dojo.date.toRelativeString = function(date) {
	var now = new Date();
	var diff = (now - date) / 1000;
	var end = " ago";
	var future = false;
	if(diff < 0) {
		future = true;
		end = " from now";
		diff = -diff;
	}

	if(diff < 60) {
		diff = Math.round(diff);
		return diff + " second" + (diff == 1 ? "" : "s") + end;
	} else if(diff < 3600) {
		diff = Math.round(diff/60);
		return diff + " minute" + (diff == 1 ? "" : "s") + end;
	} else if(diff < 3600*24 && date.getDay() == now.getDay()) {
		diff = Math.round(diff/3600);
		return diff + " hour" + (diff == 1 ? "" : "s") + end;
	} else if(diff < 3600*24*7) {
		diff = Math.round(diff/(3600*24));
		if(diff == 1) {
			return future ? "Tomorrow" : "Yesterday";
		} else {
			return diff + " days" + end;
		}
	} else {
		return dojo.date.toShortDateString(date);
	}
}

/**
 * Retrieves the day of the week the Date is set to.
 *
 * @return The day of the week
 */
dojo.date.getDayOfWeekName = function (date) {
	dojo.deprecated("dojo.date.getDayOfWeekName",
		"use dojo.date.getDayName instead", "0.4");
	return dojo.date.days[date.getDay()];
}

/**
 * Retrieves the short day of the week name the Date is set to.
 *
 * @return The short day of the week name
 */
dojo.date.getShortDayOfWeekName = function (date) {
	dojo.deprecated("dojo.date.getShortDayOfWeekName",
		"use dojo.date.getDayShortName instead", "0.4");
	return dojo.date.shortDays[date.getDay()];
}

/**
 * Retrieves the short month name the Date is set to.
 *
 * @return The short month name
 */
dojo.date.getShortMonthName = function (date) {
	dojo.deprecated("dojo.date.getShortMonthName",
		"use dojo.date.getMonthShortName instead", "0.4");
	return dojo.date.shortMonths[date.getMonth()];
}


/**
 * Convert a Date to a SQL string, optionally ignoring the HH:MM:SS portion of the Date
 */
dojo.date.toSql = function(date, noTime) {
	return dojo.date.format(date, "%F" + !noTime ? " %T" : "");
}

/**
 * Convert a SQL date string to a JavaScript Date object
 */
dojo.date.fromSql = function(sqlDate) {
	var parts = sqlDate.split(/[\- :]/g);
	while(parts.length < 6) {
		parts.push(0);
	}
	return new Date(parts[0], (parseInt(parts[1],10)-1), parts[2], parts[3], parts[4], parts[5]);
}


dojo.provide("dojo.string.Builder");
dojo.require("dojo.string");

// NOTE: testing shows that direct "+=" concatenation is *much* faster on
// Spidermoneky and Rhino, while arr.push()/arr.join() style concatenation is
// significantly quicker on IE (Jscript/wsh/etc.).

dojo.string.Builder = function(str){
	this.arrConcat = (dojo.render.html.capable && dojo.render.html["ie"]);

	var a = [];
	var b = str || "";
	var length = this.length = b.length;

	if(this.arrConcat){
		if(b.length > 0){
			a.push(b);
		}
		b = "";
	}

	this.toString = this.valueOf = function(){ 
		return (this.arrConcat) ? a.join("") : b;
	};

	this.append = function(s){
		if(this.arrConcat){
			a.push(s);
		}else{
			b+=s;
		}
		length += s.length;
		this.length = length;
		return this;
	};

	this.clear = function(){
		a = [];
		b = "";
		length = this.length = 0;
		return this;
	};

	this.remove = function(f,l){
		var s = ""; 
		if(this.arrConcat){
			b = a.join(""); 
		}
		a=[];
		if(f>0){
			s = b.substring(0, (f-1));
		}
		b = s + b.substring(f + l); 
		length = this.length = b.length; 
		if(this.arrConcat){
			a.push(b);
			b="";
		}
		return this;
	};

	this.replace = function(o,n){
		if(this.arrConcat){
			b = a.join(""); 
		}
		a = []; 
		b = b.replace(o,n); 
		length = this.length = b.length; 
		if(this.arrConcat){
			a.push(b);
			b="";
		}
		return this;
	};

	this.insert = function(idx,s){
		if(this.arrConcat){
			b = a.join(""); 
		}
		a=[];
		if(idx == 0){
			b = s + b;
		}else{
			var t = b.split("");
			t.splice(idx,0,s);
			b = t.join("")
		}
		length = this.length = b.length; 
		if(this.arrConcat){
			a.push(b); 
			b="";
		}
		return this;
	};
};

dojo.kwCompoundRequire({
	common: [
		"dojo.string",
		"dojo.string.common",
		"dojo.string.extras",
		"dojo.string.Builder"
	]
});
dojo.provide("dojo.string.*");

if(!this["dojo"]){
	alert("\"dojo/__package__.js\" is now located at \"dojo/dojo.js\". Please update your includes accordingly");
}

dojo.provide("dojo.AdapterRegistry");
dojo.require("dojo.lang.func");

dojo.AdapterRegistry = function(){
    /***
        A registry to facilitate adaptation.

        Pairs is an array of [name, check, wrap] triples
        
        All check/wrap functions in this registry should be of the same arity.
    ***/
    this.pairs = [];
}

dojo.lang.extend(dojo.AdapterRegistry, {
    register: function (name, check, wrap, /* optional */ override){
        /***
			The check function should return true if the given arguments are
			appropriate for the wrap function.

			If override is given and true, the check function will be given
			highest priority.  Otherwise, it will be the lowest priority
			adapter.
        ***/

        if (override) {
            this.pairs.unshift([name, check, wrap]);
        } else {
            this.pairs.push([name, check, wrap]);
        }
    },

    match: function (/* ... */) {
        /***
			Find an adapter for the given arguments.

			If no suitable adapter is found, throws NotFound.
        ***/
        for(var i = 0; i < this.pairs.length; i++){
            var pair = this.pairs[i];
            if(pair[1].apply(this, arguments)){
                return pair[2].apply(this, arguments);
            }
        }
		throw new Error("No match found");
        // dojo.raise("No match found");
    },

    unregister: function (name) {
        /***
			Remove a named adapter from the registry
        ***/
        for(var i = 0; i < this.pairs.length; i++){
            var pair = this.pairs[i];
            if(pair[0] == name){
                this.pairs.splice(i, 1);
                return true;
            }
        }
        return false;
    }
});

dojo.provide("dojo.json");
dojo.require("dojo.lang.func");
dojo.require("dojo.string.extras");
dojo.require("dojo.AdapterRegistry");

dojo.json = {
	jsonRegistry: new dojo.AdapterRegistry(),

	register: function(name, check, wrap, /*optional*/ override){
		/***

			Register a JSON serialization function.	 JSON serialization 
			functions should take one argument and return an object
			suitable for JSON serialization:

			- string
			- number
			- boolean
			- undefined
			- object
				- null
				- Array-like (length property that is a number)
				- Objects with a "json" method will have this method called
				- Any other object will be used as {key:value, ...} pairs
			
			If override is given, it is used as the highest priority
			JSON serialization, otherwise it will be used as the lowest.
		***/

		dojo.json.jsonRegistry.register(name, check, wrap, override);
	},

	evalJson: function(/* jsonString */ json){
		// FIXME: should this accept mozilla's optional second arg?
		try {
			return eval("(" + json + ")");
		}catch(e){
			dojo.debug(e);
			return json;
		}
	},

	evalJSON: function (json) {
		dojo.deprecated("dojo.json.evalJSON", "use dojo.json.evalJson", "0.4");
		return this.evalJson(json);
	},

	serialize: function(o){
		/***
			Create a JSON serialization of an object, note that this doesn't
			check for infinite recursion, so don't do that!
		***/

		var objtype = typeof(o);
		if(objtype == "undefined"){
			return "undefined";
		}else if((objtype == "number")||(objtype == "boolean")){
			return o + "";
		}else if(o === null){
			return "null";
		}
		if (objtype == "string") { return dojo.string.escapeString(o); }
		// recurse
		var me = arguments.callee;
		// short-circuit for objects that support "json" serialization
		// if they return "self" then just pass-through...
		var newObj;
		if(typeof(o.__json__) == "function"){
			newObj = o.__json__();
			if(o !== newObj){
				return me(newObj);
			}
		}
		if(typeof(o.json) == "function"){
			newObj = o.json();
			if (o !== newObj) {
				return me(newObj);
			}
		}
		// array
		if(objtype != "function" && typeof(o.length) == "number"){
			var res = [];
			for(var i = 0; i < o.length; i++){
				var val = me(o[i]);
				if(typeof(val) != "string"){
					val = "undefined";
				}
				res.push(val);
			}
			return "[" + res.join(",") + "]";
		}
		// look in the registry
		try {
			window.o = o;
			newObj = dojo.json.jsonRegistry.match(o);
			return me(newObj);
		}catch(e){
			// dojo.debug(e);
		}
		// it's a function with no adapter, bad
		if(objtype == "function"){
			return null;
		}
		// generic object code path
		res = [];
		for (var k in o){
			var useKey;
			if (typeof(k) == "number"){
				useKey = '"' + k + '"';
			}else if (typeof(k) == "string"){
				useKey = dojo.string.escapeString(k);
			}else{
				// skip non-string or number keys
				continue;
			}
			val = me(o[k]);
			if(typeof(val) != "string"){
				// skip non-serializable values
				continue;
			}
			res.push(useKey + ":" + val);
		}
		return "{" + res.join(",") + "}";
	}
};

dojo.provide("dojo.Deferred");
dojo.require("dojo.lang.func");

dojo.Deferred = function(/* optional */ canceller){
	/*
	NOTE: this namespace and documentation are imported wholesale 
		from MochiKit

	Encapsulates a sequence of callbacks in response to a value that
	may not yet be available.  This is modeled after the Deferred class
	from Twisted <http://twistedmatrix.com>.

	Why do we want this?  JavaScript has no threads, and even if it did,
	threads are hard.  Deferreds are a way of abstracting non-blocking
	events, such as the final response to an XMLHttpRequest.

	The sequence of callbacks is internally represented as a list
	of 2-tuples containing the callback/errback pair.  For example,
	the following call sequence::

		var d = new Deferred();
		d.addCallback(myCallback);
		d.addErrback(myErrback);
		d.addBoth(myBoth);
		d.addCallbacks(myCallback, myErrback);

	is translated into a Deferred with the following internal
	representation::

		[
			[myCallback, null],
			[null, myErrback],
			[myBoth, myBoth],
			[myCallback, myErrback]
		]

	The Deferred also keeps track of its current status (fired).
	Its status may be one of three things:

		-1: no value yet (initial condition)
		0: success
		1: error

	A Deferred will be in the error state if one of the following
	three conditions are met:

		1. The result given to callback or errback is "instanceof" Error
		2. The previous callback or errback raised an exception while
		   executing
		3. The previous callback or errback returned a value "instanceof"
			Error

	Otherwise, the Deferred will be in the success state.  The state of
	the Deferred determines the next element in the callback sequence to
	run.

	When a callback or errback occurs with the example deferred chain,
	something equivalent to the following will happen (imagine that
	exceptions are caught and returned)::

		// d.callback(result) or d.errback(result)
		if(!(result instanceof Error)){
			result = myCallback(result);
		}
		if(result instanceof Error){
			result = myErrback(result);
		}
		result = myBoth(result);
		if(result instanceof Error){
			result = myErrback(result);
		}else{
			result = myCallback(result);
		}

	The result is then stored away in case another step is added to the
	callback sequence.	Since the Deferred already has a value available,
	any new callbacks added will be called immediately.

	There are two other "advanced" details about this implementation that
	are useful:

	Callbacks are allowed to return Deferred instances themselves, so you
	can build complicated sequences of events with ease.

	The creator of the Deferred may specify a canceller.  The canceller
	is a function that will be called if Deferred.cancel is called before
	the Deferred fires.	 You can use this to implement clean aborting of
	an XMLHttpRequest, etc.	 Note that cancel will fire the deferred with
	a CancelledError (unless your canceller returns another kind of
	error), so the errbacks should be prepared to handle that error for
	cancellable Deferreds.

	*/
	
	this.chain = [];
	this.id = this._nextId();
	this.fired = -1;
	this.paused = 0;
	this.results = [null, null];
	this.canceller = canceller;
	this.silentlyCancelled = false;
};

dojo.lang.extend(dojo.Deferred, {
	getFunctionFromArgs: function(){
		var a = arguments;
		if((a[0])&&(!a[1])){
			if(dojo.lang.isFunction(a[0])){
				return a[0];
			}else if(dojo.lang.isString(a[0])){
				return dj_global[a[0]];
			}
		}else if((a[0])&&(a[1])){
			return dojo.lang.hitch(a[0], a[1]);
		}
		return null;
	},

	repr: function(){
		var state;
		if(this.fired == -1){
			state = 'unfired';
		}else if(this.fired == 0){
			state = 'success';
		} else {
			state = 'error';
		}
		return 'Deferred(' + this.id + ', ' + state + ')';
	},

	toString: dojo.lang.forward("repr"),

	_nextId: (function(){
		var n = 1;
		return function(){ return n++; };
	})(),

	cancel: function(){
		/***
		Cancels a Deferred that has not yet received a value, or is
		waiting on another Deferred as its value.

		If a canceller is defined, the canceller is called. If the
		canceller did not return an error, or there was no canceller,
		then the errback chain is started with CancelledError.
		***/
		if(this.fired == -1){
			if (this.canceller){
				this.canceller(this);
			}else{
				this.silentlyCancelled = true;
			}
			if(this.fired == -1){
				this.errback(new Error(this.repr()));
			}
		}else if(	(this.fired == 0)&&
					(this.results[0] instanceof dojo.Deferred)){
			this.results[0].cancel();
		}
	},
			

	_pause: function(){
		// Used internally to signal that it's waiting on another Deferred
		this.paused++;
	},

	_unpause: function(){
		// Used internally to signal that it's no longer waiting on
		// another Deferred.
		this.paused--;
		if ((this.paused == 0) && (this.fired >= 0)) {
			this._fire();
		}
	},

	_continue: function(res){
		// Used internally when a dependent deferred fires.
		this._resback(res);
		this._unpause();
	},

	_resback: function(res){
		// The primitive that means either callback or errback
		this.fired = ((res instanceof Error) ? 1 : 0);
		this.results[this.fired] = res;
		this._fire();
	},

	_check: function(){
		if(this.fired != -1){
			if(!this.silentlyCancelled){
				dojo.raise("already called!");
			}
			this.silentlyCancelled = false;
			return;
		}
	},

	callback: function(res){
		/*
		Begin the callback sequence with a non-error value.
		
		callback or errback should only be called once on a given
		Deferred.
		*/
		this._check();
		this._resback(res);
	},

	errback: function(res){
		// Begin the callback sequence with an error result.
		this._check();
		if(!(res instanceof Error)){
			res = new Error(res);
		}
		this._resback(res);
	},

	addBoth: function(cb, cbfn){
		/*
		Add the same function as both a callback and an errback as the
		next element on the callback sequence.	This is useful for code
		that you want to guarantee to run, e.g. a finalizer.
		*/
		var enclosed = this.getFunctionFromArgs(cb, cbfn);
		if(arguments.length > 2){
			enclosed = dojo.lang.curryArguments(null, enclosed, arguments, 2);
		}
		return this.addCallbacks(enclosed, enclosed);
	},

	addCallback: function(cb, cbfn){
		// Add a single callback to the end of the callback sequence.
		var enclosed = this.getFunctionFromArgs(cb, cbfn);
		if(arguments.length > 2){
			enclosed = dojo.lang.curryArguments(null, enclosed, arguments, 2);
		}
		return this.addCallbacks(enclosed, null);
	},

	addErrback: function(cb, cbfn){
		// Add a single callback to the end of the callback sequence.
		var enclosed = this.getFunctionFromArgs(cb, cbfn);
		if(arguments.length > 2){
			enclosed = dojo.lang.curryArguments(null, enclosed, arguments, 2);
		}
		return this.addCallbacks(null, enclosed);
		return this.addCallbacks(null, fn);
	},

	addCallbacks: function (cb, eb) {
		// Add separate callback and errback to the end of the callback
		// sequence.
		this.chain.push([cb, eb])
		if (this.fired >= 0) {
			this._fire();
		}
		return this;
	},

	_fire: function(){
		// Used internally to exhaust the callback sequence when a result
		// is available.
		var chain = this.chain;
		var fired = this.fired;
		var res = this.results[fired];
		var self = this;
		var cb = null;
		while (chain.length > 0 && this.paused == 0) {
			// Array
			var pair = chain.shift();
			var f = pair[fired];
			if (f == null) {
				continue;
			}
			try {
				res = f(res);
				fired = ((res instanceof Error) ? 1 : 0);
				if(res instanceof dojo.Deferred) {
					cb = function(res){
						self._continue(res);
					}
					this._pause();
				}
			}catch(err){
				fired = 1;
				res = err;
			}
		}
		this.fired = fired;
		this.results[fired] = res;
		if((cb)&&(this.paused)){
			// this is for "tail recursion" in case the dependent
			// deferred is already fired
			res.addBoth(cb);
		}
	}
});

dojo.provide("dojo.rpc.Deferred");
dojo.require("dojo.Deferred");

dojo.rpc.Deferred = dojo.Deferred;
dojo.rpc.Deferred.prototype = dojo.Deferred.prototype;

dojo.provide("dojo.rpc.RpcService");
dojo.require("dojo.io.*");
dojo.require("dojo.json");
dojo.require("dojo.lang.func");
dojo.require("dojo.rpc.Deferred");

dojo.rpc.RpcService = function(url){
	// summary
	// constructor for rpc base class
	if(url){
		this.connect(url);
	}
}

dojo.lang.extend(dojo.rpc.RpcService, {

	strictArgChecks: true,
	serviceUrl: "",

	parseResults: function(obj){
		// summary
		// parse the results coming back from an rpc request.  
   		// this base implementation, just returns the full object
		// subclasses should parse and only return the actual results
		return obj;
	},

	errorCallback: function(/* dojo.rpc.Deferred */ deferredRequestHandler){
		// summary
		// create callback that calls the Deferres errback method
		return function(type, obj, e){
			deferredRequestHandler.errback(e);
		}
	},

	resultCallback: function(/* dojo.rpc.Deferred */ deferredRequestHandler){
		// summary
		// create callback that calls the Deferred's callback method
		var tf = dojo.lang.hitch(this, 
			function(type, obj, e){
				var results = this.parseResults(obj||e);
				deferredRequestHandler.callback(results); 
			}
		);
		return tf;
	},


	generateMethod: function(/*string*/ method, /*array*/ parameters, /*string*/ url){
		// summary
		// generate the local bind methods for the remote object
		return dojo.lang.hitch(this, function(){
			var deferredRequestHandler = new dojo.rpc.Deferred();

			// if params weren't specified, then we can assume it's varargs
			if( (this.strictArgChecks) &&
				(parameters != null) &&
				(arguments.length != parameters.length)
			){
				// put error stuff here, no enough params
				dojo.raise("Invalid number of parameters for remote method.");
			} else {
				this.bind(method, arguments, deferredRequestHandler, url);
			}

			return deferredRequestHandler;
		});
	},

	processSmd: function(/*json*/ object){
		// summary
		// callback method for reciept of a smd object.  Parse the smd and
		// generate functions based on the description
		dojo.debug("RpcService: Processing returned SMD.");
		if(object.methods){
			dojo.lang.forEach(object.methods, function(m){
				if(m && m["name"]){
					dojo.debug("RpcService: Creating Method: this.", m.name, "()");
					this[m.name] = this.generateMethod(	m.name,
														m.parameters, 
														m["url"]||m["serviceUrl"]||m["serviceURL"]);
					if(dojo.lang.isFunction(this[m.name])){
						dojo.debug("RpcService: Successfully created", m.name, "()");
					}else{
						dojo.debug("RpcService: Failed to create", m.name, "()");
					}
				}
			}, this);
		}

		this.serviceUrl = object.serviceUrl||object.serviceURL;
		dojo.debug("RpcService: Dojo RpcService is ready for use.");
	},

	connect: function(/*String*/ smdUrl){
		// summary
		// connect to a remote url and retrieve a smd object
		dojo.debug("RpcService: Attempting to load SMD document from:", smdUrl);
		dojo.io.bind({
			url: smdUrl,
			mimetype: "text/json",
			load: dojo.lang.hitch(this, function(type, object, e){ return this.processSmd(object); }),
			sync: true
		});		
	}
});

dojo.provide("dojo.rpc.JsonService");
dojo.require("dojo.rpc.RpcService");
dojo.require("dojo.io.*");
dojo.require("dojo.json");
dojo.require("dojo.lang");

dojo.rpc.JsonService = function(args){
	// passing just the URL isn't terribly useful. It's expected that at
	// various times folks will want to specify:
	//	- just the serviceUrl (for use w/ remoteCall())
	//	- the text of the SMD to evaluate
	// 	- a raw SMD object
	//	- the SMD URL
	if(args){
		if(dojo.lang.isString(args)){
			// we assume it's an SMD file to be processed, since this was the
			// earlier function signature

			// FIXME: also accept dojo.uri.Uri objects?
			this.connect(args);
		}else{
			// otherwise we assume it's an arguments object with the following
			// (optional) properties:
			//	- serviceUrl
			//	- strictArgChecks
			//	- smdUrl
			//	- smdStr
			//	- smdObj
			if(args["smdUrl"]){
				this.connect(args.smdUrl);
			}
			if(args["smdStr"]){
				this.processSmd(dj_eval("("+args.smdStr+")"));
			}
			if(args["smdObj"]){
				this.processSmd(args.smdObj);
			}
			if(args["serviceUrl"]){
				this.serviceUrl = args.serviceUrl;
			}
			if(typeof args["strictArgChecks"] != "undefined"){
				this.strictArgChecks = args.strictArgChecks;
			}
		}
	}
}

dojo.inherits(dojo.rpc.JsonService, dojo.rpc.RpcService);

dojo.lang.extend(dojo.rpc.JsonService, {

	bustCache: false,
	
	contentType: "application/json-rpc",

	lastSubmissionId: 0,

	callRemote: function(method, params){
		var deferred = new dojo.rpc.Deferred();
		this.bind(method, params, deferred);
		return deferred;
	},

	bind: function(method, parameters, deferredRequestHandler, url){
		dojo.io.bind({
			url: url||this.serviceUrl,
			postContent: this.createRequest(method, parameters),
			method: "POST",
			contentType: this.contentType,
			mimetype: "text/json",
			load: this.resultCallback(deferredRequestHandler),
			preventCache:this.bustCache 
		});
	},

	createRequest: function(method, params){
		var req = { "params": params, "method": method, "id": ++this.lastSubmissionId };
		var data = dojo.json.serialize(req);
		dojo.debug("JsonService: JSON-RPC Request: " + data);
		return data;
	},

	parseResults: function(obj){
		if(!obj){ return; }
		if(obj["Result"]||obj["result"]){
			return obj["result"]||obj["Result"];
		}else if(obj["ResultSet"]){
			return obj["ResultSet"];
		}else{
			return obj;
		}
	}
});

dojo.kwCompoundRequire({
	common: ["dojo.rpc.JsonService", false, false]
});
dojo.provide("dojo.rpc.*");

dojo.provide("dojo.xml.Parse");

dojo.require("dojo.dom");

//TODO: determine dependencies
// currently has dependency on dojo.xml.DomUtil nodeTypes constants...

/* generic method for taking a node and parsing it into an object

TODO: WARNING: This comment is wrong!

For example, the following xml fragment

<foo bar="bar">
	<baz xyzzy="xyzzy"/>
</foo>

can be described as:

dojo.???.foo = {}
dojo.???.foo.bar = {}
dojo.???.foo.bar.value = "bar";
dojo.???.foo.baz = {}
dojo.???.foo.baz.xyzzy = {}
dojo.???.foo.baz.xyzzy.value = "xyzzy"

*/
// using documentFragment nomenclature to generalize in case we don't want to require passing a collection of nodes with a single parent
dojo.xml.Parse = function(){

	function getDojoTagName (node) {
		var tagName = node.tagName;
		if (tagName.substr(0,5).toLowerCase() != "dojo:") {
			
			if (tagName.substr(0,4).toLowerCase() == "dojo") {
				// FIXME: this assuumes tag names are always lower case
				return "dojo:" + tagName.substring(4).toLowerCase();
			}
		
			// allow lower-casing
			var djt = node.getAttribute("dojoType") || node.getAttribute("dojotype");
			if (djt) { return "dojo:" + djt.toLowerCase(); }
			
			if (node.getAttributeNS && node.getAttributeNS(dojo.dom.dojoml,"type")) {
				return "dojo:" + node.getAttributeNS(dojo.dom.dojoml,"type").toLowerCase();
			}
			try {
				// FIXME: IE really really doesn't like this, so we squelch
				// errors for it
				djt = node.getAttribute("dojo:type");
			} catch (e) { /* FIXME: log? */ }

			if (djt) { return "dojo:"+djt.toLowerCase(); }
		
			if (!dj_global["djConfig"] || !djConfig["ignoreClassNames"]) {
				// FIXME: should we make this optionally enabled via djConfig?
				var classes = node.className||node.getAttribute("class");
				// FIXME: following line, without check for existence of classes.indexOf
				// breaks firefox 1.5's svg widgets
				if (classes && classes.indexOf && classes.indexOf("dojo-") != -1) {
					var aclasses = classes.split(" ");
					for(var x=0; x<aclasses.length; x++){
						if (aclasses[x].length > 5 && aclasses[x].indexOf("dojo-") >= 0) {
							return "dojo:"+aclasses[x].substr(5).toLowerCase();
						}
					}
				}
			}
		
		}
		return tagName.toLowerCase();
	}

	this.parseElement = function(node, hasParentNodeSet, optimizeForDojoML, thisIdx){

        // if parseWidgets="false" don't search inside this node for widgets
        if (node.getAttribute("parseWidgets") == "false") {
            return {};
        }

		// TODO: make this namespace aware
		var parsedNodeSet = {};

		var tagName = getDojoTagName(node);
		parsedNodeSet[tagName] = [];
		if((!optimizeForDojoML)||(tagName.substr(0,4).toLowerCase()=="dojo")){
			var attributeSet = parseAttributes(node);
			for(var attr in attributeSet){
				if((!parsedNodeSet[tagName][attr])||(typeof parsedNodeSet[tagName][attr] != "array")){
					parsedNodeSet[tagName][attr] = [];
				}
				parsedNodeSet[tagName][attr].push(attributeSet[attr]);
			}
	
			// FIXME: we might want to make this optional or provide cloning instead of
			// referencing, but for now, we include a node reference to allow
			// instantiated components to figure out their "roots"
			parsedNodeSet[tagName].nodeRef = node;
			parsedNodeSet.tagName = tagName;
			parsedNodeSet.index = thisIdx||0;
		}
	
		var count = 0;
		var tcn, i = 0, nodes = node.childNodes;
		while(tcn = nodes[i++]){
			switch(tcn.nodeType){
				case  dojo.dom.ELEMENT_NODE: // element nodes, call this function recursively
					count++;
					var ctn = getDojoTagName(tcn);
					if(!parsedNodeSet[ctn]){
						parsedNodeSet[ctn] = [];
					}
					parsedNodeSet[ctn].push(this.parseElement(tcn, true, optimizeForDojoML, count));
					if(	(tcn.childNodes.length == 1)&&
						(tcn.childNodes.item(0).nodeType == dojo.dom.TEXT_NODE)){
						parsedNodeSet[ctn][parsedNodeSet[ctn].length-1].value = tcn.childNodes.item(0).nodeValue;
					}
					break;
				case  dojo.dom.TEXT_NODE: // if a single text node is the child, treat it as an attribute
					if(node.childNodes.length == 1) {
						parsedNodeSet[tagName].push({ value: node.childNodes.item(0).nodeValue });
					}
					break;
				default: break;
				/*
				case  dojo.dom.ATTRIBUTE_NODE: // attribute node... not meaningful here
					break;
				case  dojo.dom.CDATA_SECTION_NODE: // cdata section... not sure if this would ever be meaningful... might be...
					break;
				case  dojo.dom.ENTITY_REFERENCE_NODE: // entity reference node... not meaningful here
					break;
				case  dojo.dom.ENTITY_NODE: // entity node... not sure if this would ever be meaningful
					break;
				case  dojo.dom.PROCESSING_INSTRUCTION_NODE: // processing instruction node... not meaningful here
					break;
				case  dojo.dom.COMMENT_NODE: // comment node... not not sure if this would ever be meaningful 
					break;
				case  dojo.dom.DOCUMENT_NODE: // document node... not sure if this would ever be meaningful
					break;
				case  dojo.dom.DOCUMENT_TYPE_NODE: // document type node... not meaningful here
					break;
				case  dojo.dom.DOCUMENT_FRAGMENT_NODE: // document fragment node... not meaningful here
					break;
				case  dojo.dom.NOTATION_NODE:// notation node... not meaningful here
					break;
				*/
			}
		}
		//return (hasParentNodeSet) ? parsedNodeSet[node.tagName] : parsedNodeSet;
		return parsedNodeSet;
	}

	/* parses a set of attributes on a node into an object tree */
	function parseAttributes(node) {
		// TODO: make this namespace aware
		var parsedAttributeSet = {};
		var atts = node.attributes;
		// TODO: should we allow for duplicate attributes at this point...
		// would any of the relevant dom implementations even allow this?
		var attnode, i=0;
		while(attnode=atts[i++]) {
			if((dojo.render.html.capable)&&(dojo.render.html.ie)){
				if(!attnode){ continue; }
				if(	(typeof attnode == "object")&&
					(typeof attnode.nodeValue == 'undefined')||
					(attnode.nodeValue == null)||
					(attnode.nodeValue == '')){ 
					continue; 
				}
			}
			var nn = (attnode.nodeName.indexOf("dojo:") == -1) ? attnode.nodeName : attnode.nodeName.split("dojo:")[1];
			parsedAttributeSet[nn] = { 
				value: attnode.nodeValue 
			};
		}
		return parsedAttributeSet;
	}
}

dojo.provide("dojo.xml.domUtil");
dojo.require("dojo.graphics.color");
dojo.require("dojo.dom");
dojo.require("dojo.style");

dojo.deprecated("dojo.xml.domUtil is deprecated, use dojo.dom instead");

// for loading script:
dojo.xml.domUtil = new function(){
	this.nodeTypes = {
		ELEMENT_NODE                  : 1,
		ATTRIBUTE_NODE                : 2,
		TEXT_NODE                     : 3,
		CDATA_SECTION_NODE            : 4,
		ENTITY_REFERENCE_NODE         : 5,
		ENTITY_NODE                   : 6,
		PROCESSING_INSTRUCTION_NODE   : 7,
		COMMENT_NODE                  : 8,
		DOCUMENT_NODE                 : 9,
		DOCUMENT_TYPE_NODE            : 10,
		DOCUMENT_FRAGMENT_NODE        : 11,
		NOTATION_NODE                 : 12
	}
	
	this.dojoml = "http://www.dojotoolkit.org/2004/dojoml";
	this.idIncrement = 0;
	
	this.getTagName = function(){return dojo.dom.getTagName.apply(dojo.dom, arguments);}
	this.getUniqueId = function(){return dojo.dom.getUniqueId.apply(dojo.dom, arguments);}
	this.getFirstChildTag = function() {return dojo.dom.getFirstChildElement.apply(dojo.dom, arguments);}
	this.getLastChildTag = function() {return dojo.dom.getLastChildElement.apply(dojo.dom, arguments);}
	this.getNextSiblingTag = function() {return dojo.dom.getNextSiblingElement.apply(dojo.dom, arguments);}
	this.getPreviousSiblingTag = function() {return dojo.dom.getPreviousSiblingElement.apply(dojo.dom, arguments);}

	this.forEachChildTag = function(node, unaryFunc) {
		var child = this.getFirstChildTag(node);
		while(child) {
			if(unaryFunc(child) == "break") { break; }
			child = this.getNextSiblingTag(child);
		}
	}

	this.moveChildren = function() {return dojo.dom.moveChildren.apply(dojo.dom, arguments);}
	this.copyChildren = function() {return dojo.dom.copyChildren.apply(dojo.dom, arguments);}
	this.clearChildren = function() {return dojo.dom.removeChildren.apply(dojo.dom, arguments);}
	this.replaceChildren = function() {return dojo.dom.replaceChildren.apply(dojo.dom, arguments);}

	this.getStyle = function() {return dojo.style.getStyle.apply(dojo.style, arguments);}
	this.toCamelCase = function() {return dojo.style.toCamelCase.apply(dojo.style, arguments);}
	this.toSelectorCase = function() {return dojo.style.toSelectorCase.apply(dojo.style, arguments);}

	this.getAncestors = function(){return dojo.dom.getAncestors.apply(dojo.dom, arguments);}
	this.isChildOf = function() {return dojo.dom.isDescendantOf.apply(dojo.dom, arguments);}
	this.createDocumentFromText = function() {return dojo.dom.createDocumentFromText.apply(dojo.dom, arguments);}

	if(dojo.render.html.capable || dojo.render.svg.capable) {
		this.createNodesFromText = function(txt, wrap){return dojo.dom.createNodesFromText.apply(dojo.dom, arguments);}
	}

	this.extractRGB = function(color) { return dojo.graphics.color.extractRGB(color); }
	this.hex2rgb = function(hex) { return dojo.graphics.color.hex2rgb(hex); }
	this.rgb2hex = function(r, g, b) { return dojo.graphics.color.rgb2hex(r, g, b); }

	this.insertBefore = function() {return dojo.dom.insertBefore.apply(dojo.dom, arguments);}
	this.before = this.insertBefore;
	this.insertAfter = function() {return dojo.dom.insertAfter.apply(dojo.dom, arguments);}
	this.after = this.insertAfter
	this.insert = function(){return dojo.dom.insertAtPosition.apply(dojo.dom, arguments);}
	this.insertAtIndex = function(){return dojo.dom.insertAtIndex.apply(dojo.dom, arguments);}
	this.textContent = function () {return dojo.dom.textContent.apply(dojo.dom, arguments);}
	this.renderedTextContent = function () {return dojo.dom.renderedTextContent.apply(dojo.dom, arguments);}
	this.remove = function (node) {return dojo.dom.removeNode.apply(dojo.dom, arguments);}
}


dojo.provide("dojo.xml.htmlUtil");
dojo.require("dojo.html");
dojo.require("dojo.style");
dojo.require("dojo.dom");

dojo.deprecated("dojo.xml.htmlUtil is deprecated, use dojo.html instead");

dojo.xml.htmlUtil = new function(){
	this.styleSheet = dojo.style.styleSheet;
	
	this._clobberSelection = function(){return dojo.html.clearSelection.apply(dojo.html, arguments);}
	this.disableSelect = function(){return dojo.html.disableSelection.apply(dojo.html, arguments);}
	this.enableSelect = function(){return dojo.html.enableSelection.apply(dojo.html, arguments);}
	
	this.getInnerWidth = function(){return dojo.style.getInnerWidth.apply(dojo.style, arguments);}
	
	this.getOuterWidth = function(node){
		dojo.unimplemented("dojo.xml.htmlUtil.getOuterWidth");
	}

	this.getInnerHeight = function(){return dojo.style.getInnerHeight.apply(dojo.style, arguments);}

	this.getOuterHeight = function(node){
		dojo.unimplemented("dojo.xml.htmlUtil.getOuterHeight");
	}

	this.getTotalOffset = function(){return dojo.style.getTotalOffset.apply(dojo.style, arguments);}
	this.totalOffsetLeft = function(){return dojo.style.totalOffsetLeft.apply(dojo.style, arguments);}

	this.getAbsoluteX = this.totalOffsetLeft;

	this.totalOffsetTop = function(){return dojo.style.totalOffsetTop.apply(dojo.style, arguments);}
	
	this.getAbsoluteY = this.totalOffsetTop;

	this.getEventTarget = function(){return dojo.html.getEventTarget.apply(dojo.html, arguments);}
	this.getScrollTop = function() {return dojo.html.getScrollTop.apply(dojo.html, arguments);}
	this.getScrollLeft = function() {return dojo.html.getScrollLeft.apply(dojo.html, arguments);}

	this.evtTgt = this.getEventTarget;

	this.getParentOfType = function(){return dojo.html.getParentOfType.apply(dojo.html, arguments);}
	this.getAttribute = function(){return dojo.html.getAttribute.apply(dojo.html, arguments);}
	this.getAttr = function (node, attr) { // for backwards compat (may disappear!!!)
		dojo.deprecated("dojo.xml.htmlUtil.getAttr is deprecated, use dojo.xml.htmlUtil.getAttribute instead");
		return dojo.xml.htmlUtil.getAttribute(node, attr);
	}
	this.hasAttribute = function(){return dojo.html.hasAttribute.apply(dojo.html, arguments);}

	this.hasAttr = function (node, attr) { // for backwards compat (may disappear!!!)
		dojo.deprecated("dojo.xml.htmlUtil.hasAttr is deprecated, use dojo.xml.htmlUtil.hasAttribute instead");
		return dojo.xml.htmlUtil.hasAttribute(node, attr);
	}
	
	this.getClass = function(){return dojo.html.getClass.apply(dojo.html, arguments)}
	this.hasClass = function(){return dojo.html.hasClass.apply(dojo.html, arguments)}
	this.prependClass = function(){return dojo.html.prependClass.apply(dojo.html, arguments)}
	this.addClass = function(){return dojo.html.addClass.apply(dojo.html, arguments)}
	this.setClass = function(){return dojo.html.setClass.apply(dojo.html, arguments)}
	this.removeClass = function(){return dojo.html.removeClass.apply(dojo.html, arguments)}

	// Enum type for getElementsByClass classMatchType arg:
	this.classMatchType = {
		ContainsAll : 0, // all of the classes are part of the node's class (default)
		ContainsAny : 1, // any of the classes are part of the node's class
		IsOnly : 2 // only all of the classes are part of the node's class
	}

	this.getElementsByClass = function() {return dojo.html.getElementsByClass.apply(dojo.html, arguments)}
	this.getElementsByClassName = this.getElementsByClass;
	
	this.setOpacity = function() {return dojo.style.setOpacity.apply(dojo.style, arguments)}
	this.getOpacity = function() {return dojo.style.getOpacity.apply(dojo.style, arguments)}
	this.clearOpacity = function() {return dojo.style.clearOpacity.apply(dojo.style, arguments)}
	
	this.gravity = function(){return dojo.html.gravity.apply(dojo.html, arguments)}
	
	this.gravity.NORTH = 1;
	this.gravity.SOUTH = 1 << 1;
	this.gravity.EAST = 1 << 2;
	this.gravity.WEST = 1 << 3;
	
	this.overElement = function(){return dojo.html.overElement.apply(dojo.html, arguments)}

	this.insertCssRule = function(){return dojo.style.insertCssRule.apply(dojo.style, arguments)}
	
	this.insertCSSRule = function(selector, declaration, index){
		dojo.deprecated("dojo.xml.htmlUtil.insertCSSRule is deprecated, use dojo.xml.htmlUtil.insertCssRule instead");
		return dojo.xml.htmlUtil.insertCssRule(selector, declaration, index);
	}
	
	this.removeCssRule = function(){return dojo.style.removeCssRule.apply(dojo.style, arguments)}

	this.removeCSSRule = function(index){
		dojo.deprecated("dojo.xml.htmlUtil.removeCSSRule is deprecated, use dojo.xml.htmlUtil.removeCssRule instead");
		return dojo.xml.htmlUtil.removeCssRule(index);
	}

	this.insertCssFile = function(){return dojo.style.insertCssFile.apply(dojo.style, arguments)}

	this.insertCSSFile = function(URI, doc, checkDuplicates){
		dojo.deprecated("dojo.xml.htmlUtil.insertCSSFile is deprecated, use dojo.xml.htmlUtil.insertCssFile instead");
		return dojo.xml.htmlUtil.insertCssFile(URI, doc, checkDuplicates);
	}

	this.getBackgroundColor = function() {return dojo.style.getBackgroundColor.apply(dojo.style, arguments)}

	this.getUniqueId = function() { return dojo.dom.getUniqueId(); }

	this.getStyle = function() {return dojo.style.getStyle.apply(dojo.style, arguments)}
}

dojo.require("dojo.xml.Parse");
dojo.kwCompoundRequire({
	common:		["dojo.xml.domUtil"],
    browser: 	["dojo.xml.htmlUtil"],
    dashboard: 	["dojo.xml.htmlUtil"],
    svg: 		["dojo.xml.svgUtil"]
});
dojo.provide("dojo.xml.*");

dojo.provide("dojo.lang.type");

dojo.require("dojo.lang.common");

dojo.lang.whatAmI = function(wh) {
	try {
		if(dojo.lang.isArray(wh)) { return "array"; }
		if(dojo.lang.isFunction(wh)) { return "function"; }
		if(dojo.lang.isString(wh)) { return "string"; }
		if(dojo.lang.isNumber(wh)) { return "number"; }
		if(dojo.lang.isBoolean(wh)) { return "boolean"; }
		if(dojo.lang.isAlien(wh)) { return "alien"; }
		if(dojo.lang.isUndefined(wh)) { return "undefined"; }
		// FIXME: should this go first?
		for(var name in dojo.lang.whatAmI.custom) {
			if(dojo.lang.whatAmI.custom[name](wh)) {
				return name;
			}
		}
		if(dojo.lang.isObject(wh)) { return "object"; }
	} catch(E) {}
	return "unknown";
}
/*
 * dojo.lang.whatAmI.custom[typeName] = someFunction
 * will return typeName is someFunction(wh) returns true
 */
dojo.lang.whatAmI.custom = {};

/**
 * Returns true for values that commonly represent numbers.
 *
 * Examples:
 * <pre>
 *   dojo.lang.isNumeric(3);                 // returns true
 *   dojo.lang.isNumeric("3");               // returns true
 *   dojo.lang.isNumeric(new Number(3));     // returns true
 *   dojo.lang.isNumeric(new String("3"));   // returns true
 *
 *   dojo.lang.isNumeric(3/0);               // returns false
 *   dojo.lang.isNumeric("foo");             // returns false
 *   dojo.lang.isNumeric(new Number("foo")); // returns false
 *   dojo.lang.isNumeric(false);             // returns false
 *   dojo.lang.isNumeric(true);              // returns false
 * </pre>
 */
dojo.lang.isNumeric = function(wh){
	return (!isNaN(wh) && isFinite(wh) && (wh != null) &&
			!dojo.lang.isBoolean(wh) && !dojo.lang.isArray(wh));
}

/**
 * Returns true for any literal, and for any object that is an 
 * instance of a built-in type like String, Number, Boolean, 
 * Array, Function, or Error.
 */
dojo.lang.isBuiltIn = function(wh){
	return (dojo.lang.isArray(wh)		|| 
			dojo.lang.isFunction(wh)	|| 
			dojo.lang.isString(wh)		|| 
			dojo.lang.isNumber(wh)		|| 
			dojo.lang.isBoolean(wh)		|| 
			(wh == null)				|| 
			(wh instanceof Error)		|| 
			(typeof wh == "error") );
}

/**
 * Returns true for any object where the value of the 
 * property 'constructor' is 'Object'.  
 * 
 * Examples:
 * <pre>
 *   dojo.lang.isPureObject(new Object()); // returns true
 *   dojo.lang.isPureObject({a: 1, b: 2}); // returns true
 * 
 *   dojo.lang.isPureObject(new Date());   // returns false
 *   dojo.lang.isPureObject([11, 2, 3]);   // returns false
 * </pre>
 */
dojo.lang.isPureObject = function(wh){
	return ((wh != null) && dojo.lang.isObject(wh) && wh.constructor == Object);
}

/**
 * Given a value and a datatype, this method returns true if the
 * type of the value matches the datatype. The datatype parameter
 * can be an array of datatypes, in which case the method returns
 * true if the type of the value matches any of the datatypes.
 *
 * Examples:
 * <pre>
 *   dojo.lang.isOfType("foo", String);                // returns true
 *   dojo.lang.isOfType(12345, Number);                // returns true
 *   dojo.lang.isOfType(false, Boolean);               // returns true
 *   dojo.lang.isOfType([6, 8], Array);                // returns true
 *   dojo.lang.isOfType(dojo.lang.isOfType, Function); // returns true
 *   dojo.lang.isOfType({foo: "bar"}, Object);         // returns true
 *   dojo.lang.isOfType(new Date(), Date);             // returns true
 *   dojo.lang.isOfType(xxxxx, Date);                  // returns true
 *
 *   dojo.lang.isOfType("foo", "string");                // returns true
 *   dojo.lang.isOfType(12345, "number");                // returns true
 *   dojo.lang.isOfType(false, "boolean");               // returns true
 *   dojo.lang.isOfType([6, 8], "array");                // returns true
 *   dojo.lang.isOfType(dojo.lang.isOfType, "function"); // returns true
 *   dojo.lang.isOfType({foo: "bar"}, "object");         // returns true
 *   dojo.lang.isOfType(xxxxx, "undefined");             // returns true
 *   dojo.lang.isOfType(null, "null");                   // returns true

 *   dojo.lang.isOfType("foo", [Number, String, Boolean]); // returns true
 *   dojo.lang.isOfType(12345, [Number, String, Boolean]); // returns true
 *   dojo.lang.isOfType(false, [Number, String, Boolean]); // returns true
 *   dojo.lang.isOfType(xxxxx, "undefined");               // returns true
 * </pre>
 *
 * @param	value	Any literal value or object instance.
 * @param	type	A class of object, or a literal type, or the string name of a type, or an array with a list of types.
 * @return	Returns a boolean
 */
dojo.lang.isOfType = function(value, type) {
	if(dojo.lang.isArray(type)){
		var arrayOfTypes = type;
		for(var i in arrayOfTypes){
			var aType = arrayOfTypes[i];
			if(dojo.lang.isOfType(value, aType)) {
				return true;
			}
		}
		return false;
	}else{
		if(dojo.lang.isString(type)){
			type = type.toLowerCase();
		}
		switch (type) {
			case Array:
			case "array":
				return dojo.lang.isArray(value);
				break;
			case Function:
			case "function":
				return dojo.lang.isFunction(value);
				break;
			case String:
			case "string":
				return dojo.lang.isString(value);
				break;
			case Number:
			case "number":
				return dojo.lang.isNumber(value);
				break;
			case "numeric":
				return dojo.lang.isNumeric(value);
				break;
			case Boolean:
			case "boolean":
				return dojo.lang.isBoolean(value);
				break;
			case Object:
			case "object":
				return dojo.lang.isObject(value);
				break;
			case "pureobject":
				return dojo.lang.isPureObject(value);
				break;
			case "builtin":
				return dojo.lang.isBuiltIn(value);
				break;
			case "alien":
				return dojo.lang.isAlien(value);
				break;
			case "undefined":
				return dojo.lang.isUndefined(value);
				break;
			case null:
			case "null":
				return (value === null);
				break;
			case "optional":
				return ((value === null) || dojo.lang.isUndefined(value));
				break;
			default:
				if (dojo.lang.isFunction(type)) {
					return (value instanceof type);
				} else {
					dojo.raise("dojo.lang.isOfType() was passed an invalid type");
				}
				break;
		}
	}
	dojo.raise("If we get here, it means a bug was introduced above.");
}

/*
 * 	From reflection code, part of merge.
 *	TRT 2006-02-01
 */
dojo.lang.getObject=function(/* String */ str){
	//	summary
	//	Will return an object, if it exists, based on the name in the passed string.
	var parts=str.split("."), i=0, obj=dj_global; 
	do{ 
		obj=obj[parts[i++]]; 
	}while(i<parts.length&&obj); 
	return (obj!=dj_global)?obj:null;	//	Object
}

dojo.lang.doesObjectExist=function(/* String */ str){
	//	summary
	//	Check to see if object [str] exists, based on the passed string.
	var parts=str.split("."), i=0, obj=dj_global; 
	do{ 
		obj=obj[parts[i++]]; 
	}while(i<parts.length&&obj); 
	return (obj&&obj!=dj_global);	//	boolean
}

dojo.provide("dojo.lang.assert");

dojo.require("dojo.lang.common");
dojo.require("dojo.lang.array");
dojo.require("dojo.lang.type");

// -------------------------------------------------------------------
// Assertion methods
// -------------------------------------------------------------------

/**
 * Throws an exception if the assertion fails.
 *
 * If the asserted condition is true, this method does nothing. If the
 * condition is false, we throw an error with a error message.  
 *
 * @param	booleanValue	A boolean value, which needs to be true for the assertion to succeed.
 * @param	message	Optional. A string describing the assertion.
 * @throws	Throws an Error if 'booleanValue' is false.
 */
dojo.lang.assert = function(booleanValue, message){
	if(!booleanValue){
		var errorMessage = "An assert statement failed.\n" +
			"The method dojo.lang.assert() was called with a 'false' value.\n";
		if(message){
			errorMessage += "Here's the assert message:\n" + message + "\n";
		}
		// Use throw instead of dojo.raise, until bug #264 is fixed:
		// dojo.raise(errorMessage);
		throw new Error(errorMessage);
	}
}

/**
 * Given a value and a data type, this method checks the type of the value
 * to make sure it matches the data type, and throws an exception if there
 * is a mismatch.
 *
 * Examples:
 * <pre>
 *   dojo.lang.assertType("foo", String);
 *   dojo.lang.assertType(12345, Number);
 *   dojo.lang.assertType(false, Boolean);
 *   dojo.lang.assertType([6, 8], Array);
 *   dojo.lang.assertType(dojo.lang.assertType, Function);
 *   dojo.lang.assertType({foo: "bar"}, Object);
 *   dojo.lang.assertType(new Date(), Date);
 * </pre>
 *
 * @scope	public function
 * @param	value	Any literal value or object instance.
 * @param	type	A class of object, or a literal type, or the string name of a type, or an array with a list of types.
 * @param	message	Optional. A string describing the assertion.
 * @throws	Throws an Error if 'value' is not of type 'type'.
 */
dojo.lang.assertType = function(value, type, message){
	if(!dojo.lang.isOfType(value, type)){
		if(!message){
			if(!dojo.lang.assertType._errorMessage){
				dojo.lang.assertType._errorMessage = "Type mismatch: dojo.lang.assertType() failed.";
			}
			message = dojo.lang.assertType._errorMessage;
		}
		dojo.lang.assert(false, message);
	}
}

/**
 * Given an anonymous object and a list of expected property names, this
 * method check to make sure the object does not have any properties
 * that aren't on the list of expected properties, and throws an Error
 * if there are unexpected properties. This is useful for doing error
 * checking on keyword arguments, to make sure there aren't typos.
 *
 * Examples:
 * <pre>
 *   dojo.lang.assertValidKeywords({a: 1, b: 2}, ["a", "b"]);
 *   dojo.lang.assertValidKeywords({a: 1, b: 2}, ["a", "b", "c"]);
 *   dojo.lang.assertValidKeywords({foo: "iggy"}, ["foo"]);
 *   dojo.lang.assertValidKeywords({foo: "iggy"}, ["foo", "bar"]);
 *   dojo.lang.assertValidKeywords({foo: "iggy"}, {foo: null, bar: null});
 * </pre>
 *
 * @scope	public function
 * @param	object	An anonymous object.
 * @param	expectedProperties	An array of strings (or an object with all the expected properties).
 * @param	message	Optional. A string describing the assertion.
 * @throws	Throws an Error if 'value' is not of type 'type'.
 */
dojo.lang.assertValidKeywords = function(object, expectedProperties, message){
	var key;
	if(!message){
		if(!dojo.lang.assertValidKeywords._errorMessage){
			dojo.lang.assertValidKeywords._errorMessage = "In dojo.lang.assertValidKeywords(), found invalid keyword:";
		}
		message = dojo.lang.assertValidKeywords._errorMessage;
	}
	if(dojo.lang.isArray(expectedProperties)){
		for(key in object){
			if(!dojo.lang.inArray(expectedProperties, key)){
				dojo.lang.assert(false, message + " " + key);
			}
		}
	}else{
		for(key in object){
			if(!(key in expectedProperties)){
				dojo.lang.assert(false, message + " " + key);
			}
		}
	}
}

dojo.provide("dojo.lang.repr");

dojo.require("dojo.lang.common");
dojo.require("dojo.AdapterRegistry");
dojo.require("dojo.string.extras");

dojo.lang.reprRegistry = new dojo.AdapterRegistry();
dojo.lang.registerRepr = function(name, check, wrap, /*optional*/ override){
        /***
			Register a repr function.  repr functions should take
			one argument and return a string representation of it
			suitable for developers, primarily used when debugging.

			If override is given, it is used as the highest priority
			repr, otherwise it will be used as the lowest.
        ***/
        dojo.lang.reprRegistry.register(name, check, wrap, override);
    };

dojo.lang.repr = function(obj){
	/***
		Return a "programmer representation" for an object
	***/
	if(typeof(obj) == "undefined"){
		return "undefined";
	}else if(obj === null){
		return "null";
	}

	try{
		if(typeof(obj["__repr__"]) == 'function'){
			return obj["__repr__"]();
		}else if((typeof(obj["repr"]) == 'function')&&(obj.repr != arguments.callee)){
			return obj["repr"]();
		}
		return dojo.lang.reprRegistry.match(obj);
	}catch(e){
		if(typeof(obj.NAME) == 'string' && (
				obj.toString == Function.prototype.toString ||
				obj.toString == Object.prototype.toString
			)){
			return o.NAME;
		}
	}

	if(typeof(obj) == "function"){
		obj = (obj + "").replace(/^\s+/, "");
		var idx = obj.indexOf("{");
		if(idx != -1){
			obj = obj.substr(0, idx) + "{...}";
		}
	}
	return obj + "";
}

dojo.lang.reprArrayLike = function(arr){
	try{
		var na = dojo.lang.map(arr, dojo.lang.repr);
		return "[" + na.join(", ") + "]";
	}catch(e){ }
};

dojo.lang.reprString = function(str){ 
	dojo.deprecated("dojo.lang.reprNumber", "use `String(num)` instead", "0.4");
	return dojo.string.escapeString(str);
};

dojo.lang.reprNumber = function(num){
	dojo.deprecated("dojo.lang.reprNumber", "use `String(num)` instead", "0.4");
	return num + "";
};

(function(){
	var m = dojo.lang;
	m.registerRepr("arrayLike", m.isArrayLike, m.reprArrayLike);
	m.registerRepr("string", m.isString, m.reprString);
	m.registerRepr("numbers", m.isNumber, m.reprNumber);
	m.registerRepr("boolean", m.isBoolean, m.reprNumber);
	// m.registerRepr("numbers", m.typeMatcher("number", "boolean"), m.reprNumber);
})();

dojo.provide("dojo.lang.declare");

dojo.require("dojo.lang.common");
dojo.require("dojo.lang.extras");

/*
 * Creates a constructor: inherit and extend
 *
 * - inherits from "superclass" (via dojo.inherits, null is ok)
 * - "props" are mixed-in to the prototype (via dojo.lang.extend)
 * - can have an initializer function that fires when the class is created. 
 * - name of the class ("className" argument) is stored in "clasName" property
 * 
 * The initializer function works just like a constructor, except it has the following benefits:
 * - it doesn't fire at inheritance time (when prototyping)
 * - properties set in the initializer do not become part of subclass prototypes
 *
 * The initializer can be specified in the "init" argument, or by including a function called
 * "initializer" in "props".
 *
 * Superclass methods (inherited methods) can be invoked using "inherited" method:
 * this.inherited(<method name>[, <argument array>]);
 * - inherited will continue up the prototype chain until it finds an implementation of method
 * - nested calls to inherited are supported (i.e. inherited method "A" can succesfully call inherited("A"), and so on)
 *
 * Aliased as "dojo.declare"
 *
 * Usage:
 *
 * dojo.declare("my.classes.bar", my.classes.foo, {
 *	initializer: function() {
 *		this.myComplicatedObject = new ReallyComplicatedObject(); 
 *	},
 *	someValue: 2,
 *	aMethod: function() { doStuff(); }
 * });
 *
 */
dojo.lang.declare = function(className /*string*/, superclass /*function*/ , props /*object*/, init /*function*/){
	var ctor = function(){ 
		// get the generational context (which object [or prototype] should be constructed)
		var self = this._getPropContext();
		var s = self.constructor.superclass;
		if((s)&&(s.constructor)){
			// if this constructor is invoked directly by some constructor (my.ancestor.call(this))
			if(s.constructor==arguments.callee){
				this.inherited("constructor", arguments);
			}else{
				this._inherited(s, "constructor", arguments);
			}
		}
		if((!this.prototyping)&&(self.initializer)){
			self.initializer.apply(this, arguments);
		}
	}
	var scp = (superclass ? superclass.prototype : null);
	if(scp){
		scp.prototyping = true;
		ctor.prototype = new superclass();
		scp.prototyping = false; 
	}
	ctor.prototype.constructor = ctor;
	ctor.superclass = scp;
	dojo.lang.extend(ctor, dojo.lang.declare.base);
	props=(props||{});
	props.initializer = (props.initializer)||(init)||(function(){ });
	props.className = className;
	dojo.lang.extend(ctor, props);
	dojo.lang.setObjPathValue(className, ctor, null, true);
}
dojo.lang.declare.base = {
	_getPropContext: function() { return (this.___proto||this); },
	// cache ptype context and call method on it
	_inherited: function(ptype, method, args){
		var stack = this.___proto;
		this.___proto = ptype;
		var result = ptype[method].apply(this, (args||[]));
		this.___proto = stack;
		return result;
	},
	// searches backward thru prototype chain to find nearest ancestral iplementation of method
	inherited: function(prop, args){
		var p = this._getPropContext();
		do{
			if((!p.constructor)||(!p.constructor.superclass)){return;}
			p = p.constructor.superclass;
		}while(!(prop in p));
		return (typeof p[prop] == 'function' ? this._inherited(p, prop, args) : p[prop]);
	}
}
dojo.declare = dojo.lang.declare;

dojo.kwCompoundRequire({
	common: [
		"dojo.lang",
		"dojo.lang.common",
		"dojo.lang.assert",
		"dojo.lang.array",
		"dojo.lang.type",
		"dojo.lang.func",
		"dojo.lang.extras",
		"dojo.lang.repr",
		"dojo.lang.declare"
	]
});
dojo.provide("dojo.lang.*");

/** 
		FIXME: Write better docs.

		@author Alex Russel, alex@dojotoolkit.org
		@author Brad Neuberg, bkn3@columbia.edu 
*/
dojo.provide("dojo.storage");
dojo.provide("dojo.storage.StorageProvider");

dojo.require("dojo.lang.*");
dojo.require("dojo.event.*");


/** The base class for all storage providers. */

/** 
	 The constructor for a storage provider. You should avoid initialization
	 in the constructor; instead, define initialization in your initialize()
	 method. 
*/
dojo.storage = function(){
}

dojo.lang.extend(dojo.storage, {
	/** A put() call to a storage provider was succesful. */
	SUCCESS: "success",
	
	/** A put() call to a storage provider failed. */
	FAILED: "failed",
	
	/** A put() call to a storage provider is pending user approval. */
	PENDING: "pending",
	
	/** 
	  Returned by getMaximumSize() if this storage provider can not determine
	  the maximum amount of data it can support. 
	*/
	SIZE_NOT_AVAILABLE: "Size not available",
	
	/**
	  Returned by getMaximumSize() if this storage provider has no theoretical
	  limit on the amount of data it can store. 
	*/
	SIZE_NO_LIMIT: "No size limit",
	
	/** 
	  The namespace for all storage operations. This is useful if
	  several applications want access to the storage system from the same
	  domain but want different storage silos. 
	*/
	namespace: "dojoStorage",
	
	/**  
	  If a function is assigned to this property, then 
	  when the settings provider's UI is closed this
	  function is called. Useful, for example, if the
	  user has just cleared out all storage for this
	  provider using the settings UI, and you want to 
	  update your UI.
	*/
	onHideSettingsUI: null,

	/** 
	  Allows this storage provider to initialize itself. This is called
	  after the page has finished loading, so you can not do document.writes(). 
	*/
	initialize: function(){
	 dojo.unimplemented("dojo.storage.initialize");
	},
	
	/** 
	  Returns whether this storage provider is 
	  available on this platform. 
	
	  @returns True or false if this storage 
	  provider is supported.
	*/
	isAvailable: function(){
		dojo.unimplemented("dojo.storage.isAvailable");
	},
	
	/**
	  Puts a key and value into this storage system.

	  @param key A string key to use when retrieving 
	         this value in the future.
	  @param value A value to store; this can be 
	         any JavaScript type.
	  @param resultsHandler A callback function 
	         that will receive three arguments.
	         The first argument is one of three 
	         values: dojo.storage.SUCCESS,
	         dojo.storage.FAILED, or 
	         dojo.storage.PENDING; these values 
	         determine how the put request went. 
	         In some storage systems users can deny
	         a storage request, resulting in a 
	         dojo.storage.FAILED, while in 
	         other storage systems a storage 
	         request must wait for user approval,
	         resulting in a dojo.storage.PENDING 
	         status until the request
	         is either approved or denied, 
	         resulting in another call back
	         with dojo.storage.SUCCESS. 
  
	  The second argument in the call back is the key name
	  that was being stored.
	  
	  The third argument in the call back is an 
	  optional message that details possible error 
	  messages that might have occurred during
	  the storage process.

	  Example:
	    var resultsHandler = function(status, key, message){
	      alert("status="+status+", key="+key+", message="+message);
	    };
	    dojo.storage.put("test", "hello world", 
	                     resultsHandler);	
	*/
	put: function(key, value, resultsHandler){ 
    dojo.unimplemented("dojo.storage.put");
  },

	/**
	  Gets the value with the given key. Returns null
	  if this key is not in the storage system.
	
	  @param key A string key to get the value of.
	  @returns Returns any JavaScript object type; 
	  null if the key is not
	  present. 
	*/
	get: function(key){
    dojo.unimplemented("dojo.storage.get");
  },

	/**
	  Determines whether the storage has the given 
	  key. 
	
	    @returns Whether this key is 
	             present or not. 
	*/
	hasKey: function(key){
		if (this.get(key) != null)
			return true;
		else
			return false;
	},

	/**
	  Enumerates all of the available keys in 
	  this storage system.
	
	  @returns Array of string keys in this 
	           storage system.
	*/
	getKeys: function(){
    dojo.unimplemented("dojo.storage.getKeys");
  },

	/**
	  Completely clears this storage system of all 
	  of it's values and keys. 
	*/
	clear: function(){
    dojo.unimplemented("dojo.storage.clear");
  },
  
  /** Removes the given key from the storage system. */
  remove: function(key){
  	dojo.unimplemented("dojo.storage.remove");
  },

	/**
	  Returns whether this storage provider's 
	  values are persisted when this platform 
	  is shutdown. 
	
	  @returns True or false whether this 
	  storage is permanent. 
	*/
	isPermanent: function(){
		dojo.unimplemented("dojo.storage.isPermanent");
	},

	/**
	  The maximum storage allowed by this provider.
	
	  @returns Returns the maximum storage size 
	           supported by this provider, in 
	           thousands of bytes (i.e., if it 
	           returns 60 then this means that 60K 
	           of storage is supported).
	    
	           If this provider can not determine 
	           it's maximum size, then 
	           dojo.storage.SIZE_NOT_AVAILABLE is 
	           returned; if there is no theoretical
	           limit on the amount of storage 
	           this provider can return, then
	           dojo.storage.SIZE_NO_LIMIT is 
	           returned
	*/
	getMaximumSize: function(){
    dojo.unimplemented("dojo.storage.getMaximumSize");
  },

	/**
	  Determines whether this provider has a 
	  settings UI.
	
	  @returns True or false if this provider has 
	           the ability to show a
	           a settings UI to change it's 
	           values, change the amount of storage
	           available, etc. 
	*/
	hasSettingsUI: function(){
		return false;
	},

	/**
	  If this provider has a settings UI, it is 
	  shown. 
	*/
	showSettingsUI: function(){
	 dojo.unimplemented("dojo.storage.showSettingsUI");
	},

	/**
	  If this provider has a settings UI, hides
	  it.
	*/
	hideSettingsUI: function(){
	 dojo.unimplemented("dojo.storage.hideSettingsUI");
	},
	
	/** 
	  The provider name as a string, such as 
	  "dojo.storage.FlashStorageProvider". 
	*/
	getType: function(){
		dojo.unimplemented("dojo.storage.getType");
	},
	
	/**
	  Subclasses can call this to ensure that the key given is valid in a
	  consistent way across different storage providers. We use the lowest
	  common denominator for key values allowed: only letters, numbers, and
	  underscores are allowed. No spaces. 
	*/
	isValidKey: function(keyName){
		if (keyName == null || typeof keyName == "undefined")
			return false;
			
		return /^[0-9A-Za-z_]*$/.test(keyName);
  }
});




/**
	Initializes the storage systems and figures out the best available 
	storage options on this platform.
*/
dojo.storage.manager = new function(){
	this.currentProvider = null;
	this.available = false;
	this.initialized = false;
	this.providers = new Array();
	
	// TODO: Provide a way for applications to override the default namespace
	this.namespace = "dojo.storage";
	
	/** Initializes the storage system. */
	this.initialize = function(){
		// autodetect the best storage provider we can provide on this platform
		this.autodetect();
	}
	
	/**
	  Registers the existence of a new storage provider; used by subclasses
	  to inform the manager of their existence. 
	
	  @param name The full class name of this provider, such as 
	  "dojo.storage.browser.Flash6StorageProvider".
	  @param instance An instance of this provider, which we will use to
	  call isAvailable() on. 
	*/
	this.register = function(name, instance) {
		this.providers[this.providers.length] = instance;
		this.providers[name] = instance;
	}
	
	/**
	  Instructs the storageManager to use 
	  the given storage class for all storage requests.
	    
	  Example:
	    
	  dojo.storage.setProvider(
	         dojo.storage.browser.IEStorageProvider)
	*/
	this.setProvider = function(storageClass){
	
	}
	
	/** 
	  Autodetects the best possible persistent
	  storage provider available on this platform. 
	*/
	this.autodetect = function(){
		if(this.initialized == true) // already finished
			return;
			
		// go through each provider, seeing if it can be used
		var providerToUse = null;
		for(var i = 0; i < this.providers.length; i++) {
			providerToUse = this.providers[i];
			if(providerToUse.isAvailable()){
				break;
			}
		}	
		
		if(providerToUse == null){ // no provider available
			this.initialized = true;
			this.available = false;
			this.currentProvider = null;
			dojo.raise("No storage provider found for this platform");
		}
			
		// create this provider and copy over it's properties
		this.currentProvider = providerToUse;
	  	for(var i in providerToUse){
	  		dojo.storage[i] = providerToUse[i];
		}
		dojo.storage.manager = this;
		
		// have the provider initialize itself
		dojo.storage.initialize();
		
		this.initialized = true;
		this.available = true;
	}
	
	/** Returns whether any storage options are available. */
	this.isAvailable = function(){
		return this.available;
	}
	
	/** 
	 	Returns whether the storage system is initialized and
	 	ready to be used. 
	*/
	this.isInitialized = function(){
		// FIXME: This should _really_ not be in here, but it fixes a bug
		if(dojo.flash.ready == false){
			return false;
		}else{
			return this.initialized;
		}
	}

	/**
	  Determines if this platform supports
	  the given storage provider.
	
	  Example:
			
	  dojo.storage.manager.supportsProvider(
	    "dojo.storage.browser.InternetExplorerStorageProvider");
	*/
	this.supportsProvider = function(storageClass){
		// construct this class dynamically
		try{
			// dynamically call the given providers class level isAvailable()
			// method
			var provider = eval("new " + storageClass + "()");
			var results = provider.isAvailable();
			if(results == null || typeof results == "undefined")
				return false;
			return results;
		}catch (exception){
			dojo.debug("exception="+exception);
			return false;
		}
	}

	/** Gets the current provider. */
	this.getProvider = function(){
		return this.currentProvider;
	}
	
	/** 
	  The storage provider should call this method when it is loaded and
	  ready to be used. Clients who will use the provider will connect
	  to this method to know when they can use the storage system:
	
	  dojo.connect(dojo.storage.manager, "loaded", someInstance, 
	               someInstance.someMethod);
	*/
	this.loaded = function(){
	}
}

dojo.provide("dojo.flash");

dojo.require("dojo.string.*");
dojo.require("dojo.uri.*");


/** 
		The goal of dojo.flash is to make it easy to extend Flash's capabilities
		into an AJAX/DHTML environment. Robust, performant, reliable 
		JavaScript/Flash communication is harder than most realize when they
		delve into the topic, especially if you want it
		to work on Internet Explorer, Firefox, and Safari, and to be able to
		push around hundreds of K of information quickly. Dojo.flash makes it
		possible to support these platforms; you have to jump through a few
		hoops to get its capabilites, but if you are a library writer 
		who wants to bring Flash's storage or streaming sockets ability into
		DHTML, for example, then dojo.flash is perfect for you.
  
		Dojo.flash provides an easy object for interacting with the Flash plugin. 
		This object provides methods to determine the current version of the Flash
		plugin (dojo.flash.info); execute Flash instance methods 
		independent of the Flash version
		being used (dojo.flash.comm); write out the necessary markup to 
		dynamically insert a Flash object into the page (dojo.flash.Embed; and 
		do dynamic installation and upgrading of the current Flash plugin in 
		use (dojo.flash.Install).
		
		To use dojo.flash, you must first wait until Flash is finished loading 
		and initializing before you attempt communication or interaction. 
		To know when Flash is finished use dojo.event.connect:
		
		dojo.event.connect(dojo.flash, "loaded", myInstance, "myCallback");
		
		Then, while the page is still loading provide the file name
		and the major version of Flash that will be used for Flash/JavaScript
		communication (see "Flash Communication" below for information on the 
		different kinds of Flash/JavaScript communication supported and how they 
		depend on the version of Flash installed):
		
		dojo.flash.setSwf({flash6: "src/storage/storage_flash6.swf",
											 flash8: "src/storage/storage_flash8.swf"});
		
		This will cause dojo.flash to pick the best way of communicating
		between Flash and JavaScript based on the platform.
		
		If no SWF files are specified, then Flash is not initialized.
		
		Your Flash must use DojoExternalInterface to expose Flash methods and
		to call JavaScript; see "Flash Communication" below for details.
		
		setSwf can take an optional 'visible' attribute to control whether
		the Flash object is visible or not on the page; the default is visible:
		
		dojo.flash.setSwf({flash6: "src/storage/storage_flash6.swf",
											 flash8: "src/storage/storage_flash8.swf",
											 visible: false});
		
		Once finished, you can query Flash version information:
		
		dojo.flash.info.version
		
		Or can communicate with Flash methods that were exposed:
		
		var results = dojo.flash.comm.sayHello("Some Message");
		
		Only string values are currently supported for both arguments and
		for return results. Everything will be cast to a string on both
		the JavaScript and Flash sides.
		
		-------------------
		Flash Communication
		-------------------
		
		dojo.flash allows Flash/JavaScript communication in 
		a way that can pass large amounts of data back and forth reliably and
		very fast. The dojo.flash
		framework encapsulates the specific way in which this communication occurs,
		presenting a common interface to JavaScript irrespective of the underlying
		Flash version.
		
		There are currently three major ways to do Flash/JavaScript communication
		in the Flash community:
		
		1) Flash 6+ - Uses Flash methods, such as SetVariable and TCallLabel,
		and the fscommand handler to do communication. Strengths: Very fast,
		mature, and can send extremely large amounts of data; can do
		synchronous method calls. Problems: Does not work on Safari; works on 
		Firefox/Mac OS X only if Flash 8 plugin is installed; cryptic to work with.
		
		2) Flash 8+ - Uses ExternalInterface, which provides a way for Flash
		methods to register themselves for callbacks from JavaScript, and a way
		for Flash to call JavaScript. Strengths: Works on Safari; elegant to
		work with; can do synchronous method calls. Problems: Extremely buggy 
		(fails if there are new lines in the data, for example); performance
		degrades drastically in O(n^2) time as data grows; locks up the browser while
		it is communicating; does not work in Internet Explorer if Flash
		object is dynamically added to page with document.writeln, DOM methods,
		or innerHTML.
		
		3) Flash 6+ - Uses two seperate Flash applets, one that we 
		create over and over, passing input data into it using the PARAM tag, 
		which then uses a Flash LocalConnection to pass the data to the main Flash
		applet; communication back to Flash is accomplished using a getURL
		call with a javascript protocol handler, such as "javascript:myMethod()".
		Strengths: the most cross browser, cross platform pre-Flash 8 method
		of Flash communication known; works on Safari. Problems: Timing issues;
		clunky and complicated; slow; can only send very small amounts of
		data (several K); all method calls are asynchronous.
		
		dojo.flash.comm uses only the first two methods. This framework
		was created primarily for dojo.storage, which needs to pass very large
		amounts of data synchronously and reliably across the Flash/JavaScript
		boundary. We use the first method, the Flash 6 method, on all platforms
		that support it, while using the Flash 8 ExternalInterface method
		only on Safari with some special code to help correct ExternalInterface's
		bugs.
		
		Since dojo.flash needs to have two versions of the Flash
		file it wants to generate, a Flash 6 and a Flash 8 version to gain
		true cross-browser compatibility, several tools are provided to ease
		development on the Flash side.
		
		In your Flash file, if you want to expose Flash methods that can be
		called, use the DojoExternalInterface class to register methods. This
		class is an exact API clone of the standard ExternalInterface class, but
		can work in Flash 6+ browsers. Under the covers it uses the best
		mechanism to do communication:
		
		class HelloWorld{
			function HelloWorld(){
				// Initialize the DojoExternalInterface class
				DojoExternalInterface.initialize();
				
				// Expose your methods
				DojoExternalInterface.addCallback("sayHello", this, this.sayHello);
				
				// Tell JavaScript that you are ready to have method calls
				DojoExternalInterface.loaded();
				
				// Call some JavaScript
				var resultsReady = function(results){
					trace("Received the following results from JavaScript: " + results);
				}
				DojoExternalInterface.call("someJavaScriptMethod", resultsReady, 
																	 someParameter);
			}
			
			function sayHello(){ ... }
			
			static main(){ ... }
		}
		
		DojoExternalInterface adds two new functions to the ExternalInterface
		API: initialize() and loaded(). initialize() must be called before
		any addCallback() or call() methods are run, and loaded() must be
		called after you are finished adding your callbacks. Calling loaded()
		will fire the dojo.flash.loaded() event, so that JavaScript can know that
		Flash has finished loading and adding its callbacks, and can begin to
		interact with the Flash file.
		
		To generate your SWF files, use the ant task
		"buildFlash". You must have the open source Motion Twin ActionScript 
		compiler (mtasc) installed and in your path to use the "buildFlash"
		ant task; download and install mtasc from http://www.mtasc.org/.
		
		
		
		buildFlash usage:
		
		ant buildFlash -Ddojo.flash.file=../tests/flash/HelloWorld.as
		
		where "dojo.flash.file" is the relative path to your Flash 
		ActionScript file.
		
		This will generate two SWF files, one ending in _flash6.swf and the other
		ending in _flash8.swf in the same directory as your ActionScript method:
		
		HelloWorld_flash6.swf
		HelloWorld_flash8.swf
		
		Initialize dojo.flash with the filename and Flash communication version to
		use during page load; see the documentation for dojo.flash for details:
		
		dojo.flash.setSwf({flash6: "tests/flash/HelloWorld_flash6.swf",
											 flash8: "tests/flash/HelloWorld_flash8.swf"});
		
		Now, your Flash methods can be called from JavaScript as if they are native
		Flash methods, mirrored exactly on the JavaScript side:
		
		dojo.flash.comm.sayHello();
		
		Only Strings are supported being passed back and forth currently.
		
		JavaScript to Flash communication is synchronous; i.e., results are returned
		directly from the method call:
		
		var results = dojo.flash.comm.sayHello();
		
		Flash to JavaScript communication is asynchronous due to limitations in
		the underlying technologies; you must use a results callback to handle
		results returned by JavaScript in your Flash AS files:
		
		var resultsReady = function(results){
			trace("Received the following results from JavaScript: " + results);
		}
		DojoExternalInterface.call("someJavaScriptMethod", resultsReady);
		
		
		
		-------------------
		Notes
		-------------------
		
		If you have both Flash 6 and Flash 8 versions of your file:
		
		dojo.flash.setSwf({flash6: "tests/flash/HelloWorld_flash6.swf",
											 flash8: "tests/flash/HelloWorld_flash8.swf"});
											 
		but want to force the browser to use a certain version of Flash for
		all platforms (for testing, for example), use the djConfig
		variable 'forceFlashComm' with the version number to force:
		
		var djConfig = { forceFlashComm: 6 };
		
		Two values are currently supported, 6 and 8, for the two styles of
		communication described above. Just because you force dojo.flash
		to use a particular communication style is no guarantee that it will
		work; for example, Flash 8 communication doesn't work in Internet
		Explorer due to bugs in Flash, and Flash 6 communication does not work
		in Safari. It is best to let dojo.flash determine the best communication
		mechanism, and to use the value above only for debugging the dojo.flash
		framework itself.
		
		Also note that dojo.flash can currently only work with one Flash object
		on the page; it and the API do not yet support multiple Flash objects on
		the same page.
		
		We use some special tricks to get decent, linear performance
		out of Flash 8's ExternalInterface on Safari; see the blog
		post 
		http://codinginparadise.org/weblog/2006/02/how-to-speed-up-flash-8s.html
		for details.
		
		Your code can detect whether the Flash player is installing or having
		its version revved in two ways. First, if dojo.flash detects that
		Flash installation needs to occur, it sets dojo.flash.info.installing
		to true. Second, you can detect if installation is necessary with the
		following callback:
		
		dojo.event.connect(dojo.flash, "installing", myInstance, "myCallback");
		
		You can use this callback to delay further actions that might need Flash;
		when installation is finished the full page will be refreshed and the
		user will be placed back on your page with Flash installed.
		
		Two utility methods exist if you want to add loading and installing
		listeners without creating dependencies on dojo.event; these are
		'addLoadingListener' and 'addInstallingListener'.
		
		-------------------
		Todo/Known Issues
		-------------------

		There are several tasks I was not able to do, or did not need to fix
		to get dojo.storage out:		
		
		* When using Flash 8 communication, Flash method calls to JavaScript
		are not working properly; serialization might also be broken for certain
		invalid characters when it is Flash invoking JavaScript methods.
		The Flash side needs to have more sophisticated serialization/
		deserialization mechanisms like JavaScript currently has. The
		test_flash2.html unit tests should also be updated to have much more
		sophisticated Flash to JavaScript unit tests, including large
		amounts of data.
		
		* On Internet Explorer, after doing a basic install, the page is
		not refreshed or does not detect that Flash is now available. The way
		to fix this is to create a custom small Flash file that is pointed to
		during installation; when it is finished loading, it does a callback
		that says that Flash installation is complete on IE, and we can proceed
		to initialize the dojo.flash subsystem.
		
		@author Brad Neuberg, bkn3@columbia.edu
*/

dojo.flash = {
	flash6_version: null,
	flash8_version: null,
	ready: false,
	_visible: true,
	_loadedListeners: new Array(),
	_installingListeners: new Array(),
	
	/** Sets the SWF files and versions we are using. */
	setSwf: function(fileInfo){
		//dojo.debug("setSwf");
		if(fileInfo == null || dojo.lang.isUndefined(fileInfo)){
			return;
		}
		
		if(fileInfo.flash6 != null && !dojo.lang.isUndefined(fileInfo.flash6)){
			this.flash6_version = fileInfo.flash6;
		}
		
		if(fileInfo.flash8 != null && !dojo.lang.isUndefined(fileInfo.flash8)){
			this.flash8_version = fileInfo.flash8;
		}
		
		if(!dojo.lang.isUndefined(fileInfo.visible)){
			this._visible = fileInfo.visible;
		}
		
		// initialize ourselves		
		this._initialize();
	},
	
	/** Returns whether we are using Flash 6 for communication on this platform. */
	useFlash6: function(){
		if(this.flash6_version == null){
			return false;
		}else if (this.flash6_version != null && dojo.flash.info.commVersion == 6){
			// if we have a flash 6 version of this SWF, and this browser supports 
			// communicating using Flash 6 features...
			return true;
		}else{
			return false;
		}
	},
	
	/** Returns whether we are using Flash 8 for communication on this platform. */
	useFlash8: function(){
		if(this.flash8_version == null){
			return false;
		}else if (this.flash8_version != null && dojo.flash.info.commVersion == 8){
			// if we have a flash 8 version of this SWF, and this browser supports
			// communicating using Flash 8 features...
			return true;
		}else{
			return false;
		}
	},
	
	/** Adds a listener to know when Flash is finished loading. 
			Useful if you don't want a dependency on dojo.event. */
	addLoadedListener: function(listener){
		this._loadedListeners.push(listener);
	},

	/** Adds a listener to know if Flash is being installed. 
			Useful if you don't want a dependency on dojo.event. */
	addInstallingListener: function(listener){
		this._installingListeners.push(listener);
	},	
	
	/** 
			A callback when the Flash subsystem is finished loading and can be
			worked with. To be notified when Flash is finished loading, connect
			your callback to this method using the following:
			
			dojo.event.connect(dojo.flash, "loaded", myInstance, "myCallback");
	*/
	loaded: function(){
		//dojo.debug("dojo.flash.loaded");
		dojo.flash.ready = true;
		if(dojo.flash._loadedListeners.length > 0){
			for(var i = 0;i < dojo.flash._loadedListeners.length; i++){
				dojo.flash._loadedListeners[i].call(null);
			}
		}
	},
	
	/** 
			A callback to know if Flash is currently being installed or
			having its version revved. To be notified if Flash is installing, connect
			your callback to this method using the following:
			
			dojo.event.connect(dojo.flash, "installing", myInstance, "myCallback");
	*/
	installing: function(){
	 //dojo.debug("installing");
	 if(dojo.flash._installingListeners.length > 0){
			for(var i = 0;i < dojo.flash._installingListeners.length; i++){
				dojo.flash._installingListeners[i].call(null);
			}
		}
	},
	
	/** Initializes dojo.flash. */
	_initialize: function(){
		//dojo.debug("dojo.flash._initialize");
		// see if we need to rev or install Flash on this platform
		var installer = new dojo.flash.Install();
		dojo.flash.installer = installer;

		if(installer.needed() == true){		
			installer.install();
		}else{
			//dojo.debug("Writing object out");
			// write the flash object into the page
			dojo.flash.obj = new dojo.flash.Embed(this._visible);
			dojo.flash.obj.write(dojo.flash.info.commVersion);
			
			// initialize the way we do Flash/JavaScript communication
			dojo.flash.comm = new dojo.flash.Communicator();
		}
	}
};


/** 
		A class that helps us determine whether Flash is available,
		it's major and minor versions, and what Flash version features should
		be used for Flash/JavaScript communication. Parts of this code
		are adapted from the automatic Flash plugin detection code autogenerated 
		by the Macromedia Flash 8 authoring environment. 
		
		An instance of this class can be accessed on dojo.flash.info after
		the page is finished loading.
		
		This constructor must be called before the page is finished loading. 
*/
dojo.flash.Info = function(){
	// Visual basic helper required to detect Flash Player ActiveX control 
	// version information on Internet Explorer
	if(dojo.render.html.ie){
		document.writeln('<script language="VBScript" type="text/vbscript"\>');
		document.writeln('Function VBGetSwfVer(i)');
		document.writeln('  on error resume next');
		document.writeln('  Dim swControl, swVersion');
		document.writeln('  swVersion = 0');
		document.writeln('  set swControl = CreateObject("ShockwaveFlash.ShockwaveFlash." + CStr(i))');
		document.writeln('  if (IsObject(swControl)) then');
		document.writeln('    swVersion = swControl.GetVariable("$version")');
		document.writeln('  end if');
		document.writeln('  VBGetSwfVer = swVersion');
		document.writeln('End Function');
		document.writeln('</script\>');
	}
	
	this._detectVersion();
	this._detectCommunicationVersion();
}

dojo.flash.Info.prototype = {
	/** The full version string, such as "8r22". */
	version: -1,
	
	/** 
			The major, minor, and revisions of the plugin. For example, if the
			plugin is 8r22, then the major version is 8, the minor version is 0,
			and the revision is 22. 
	*/
	versionMajor: -1,
	versionMinor: -1,
	versionRevision: -1,
	
	/** Whether this platform has Flash already installed. */
	capable: false,
	
	/** 
			The major version number for how our Flash and JavaScript communicate.
			This can currently be the following values:
			6 - We use a combination of the Flash plugin methods, such as SetVariable
			and TCallLabel, along with fscommands, to do communication.
			8 - We use the ExternalInterface API. 
			-1 - For some reason neither method is supported, and no communication
			is possible. 
	*/
	commVersion: 6,
	
	/** Set if we are in the middle of a Flash installation session. */
	installing: false,
	
	/** 
			Asserts that this environment has the given major, minor, and revision
			numbers for the Flash player. Returns true if the player is equal
			or above the given version, false otherwise.
			
			Example: To test for Flash Player 7r14:
			
			dojo.flash.info.isVersionOrAbove(7, 0, 14)
	*/
	isVersionOrAbove: function(reqMajorVer, reqMinorVer, reqVer){
		// make the revision a decimal (i.e. transform revision 14 into
		// 0.14
		reqVer = parseFloat("." + reqVer);
		
		if(this.versionMajor >= reqMajorVer && this.versionMinor >= reqMinorVer
			 && this.versionRevision >= reqVer){
			return true;
		}else{
			return false;
		}
	},
	
	_detectVersion: function(){
		var versionStr;
		
		// loop backwards through the versions until we find the newest version	
		for(var testVersion = 25; testVersion > 0; testVersion--){
			if(dojo.render.html.ie){
				versionStr = VBGetSwfVer(testVersion);
			}else{
				versionStr = this._JSFlashInfo(testVersion);		
			}
				
			if(versionStr == -1 ){
				this.capable = false; 
				return;
			}else if(versionStr != 0){
				var versionArray;
				if(dojo.render.html.ie){
					var tempArray = versionStr.split(" ");
					var tempString = tempArray[1];
					versionArray = tempString.split(",");
				}else{
					versionArray = versionStr.split(".");
				}
					
				this.versionMajor = versionArray[0];
				this.versionMinor = versionArray[1];
				this.versionRevision = versionArray[2];
				
				// 7.0r24 == 7.24
				versionString = this.versionMajor + "." + this.versionRevision;
				this.version = parseFloat(versionString);
				
				this.capable = true;
				
				break;
			}
		}
	},
	
	/** 
			JavaScript helper required to detect Flash Player PlugIn version 
			information. Internet Explorer uses a corresponding Visual Basic
			version to interact with the Flash ActiveX control. 
	*/
	_JSFlashInfo: function(testVersion){
		// NS/Opera version >= 3 check for Flash plugin in plugin array
		if(navigator.plugins != null && navigator.plugins.length > 0){
			if(navigator.plugins["Shockwave Flash 2.0"] || 
				 navigator.plugins["Shockwave Flash"]){
				var swVer2 = navigator.plugins["Shockwave Flash 2.0"] ? " 2.0" : "";
				var flashDescription = navigator.plugins["Shockwave Flash" + swVer2].description;
				var descArray = flashDescription.split(" ");
				var tempArrayMajor = descArray[2].split(".");
				var versionMajor = tempArrayMajor[0];
				var versionMinor = tempArrayMajor[1];
				if(descArray[3] != ""){
					tempArrayMinor = descArray[3].split("r");
				}else{
					tempArrayMinor = descArray[4].split("r");
				}
				var versionRevision = tempArrayMinor[1] > 0 ? tempArrayMinor[1] : 0;
				var version = versionMajor + "." + versionMinor + "." 
											+ versionRevision;
											
				return version;
			}
		}
		
		return -1;
	},
	
	/** 
			Detects the mechanisms that should be used for Flash/JavaScript 
			communication, setting 'commVersion' to either 6 or 8. If the value is
			6, we use Flash Plugin 6+ features, such as GetVariable, TCallLabel,
			and fscommand, to do Flash/JavaScript communication; if the value is
			8, we use the ExternalInterface API for communication. 
	*/
	_detectCommunicationVersion: function(){
		if(this.capable == false){
			this.commVersion = null;
			return;
		}
		
		// detect if the user has over-ridden the default flash version
		if (typeof djConfig["forceFlashComm"] != "undefined" &&
				typeof djConfig["forceFlashComm"] != null){
			this.commVersion = djConfig["forceFlashComm"];
			return;
		}
		
		// we prefer Flash 6 features over Flash 8, because they are much faster
		// and much less buggy
		
		// at this point, we don't have a flash file to detect features on,
		// so we need to instead look at the browser environment we are in
		if(dojo.render.html.safari == true || dojo.render.html.opera == true){
			this.commVersion = 8;
		}else{
			this.commVersion = 6;
		}
	}
};

/** A class that is used to write out the Flash object into the page. */
dojo.flash.Embed = function(visible){
	this._visible = visible;
}

dojo.flash.Embed.prototype = {
	/** 
			The width of this Flash applet. The default is the minimal width
			necessary to show the Flash settings dialog. 
	*/
	width: 215,
	
	/** 
			The height of this Flash applet. The default is the minimal height
			necessary to show the Flash settings dialog. 
	*/
	height: 138,
	
	/** The id of the Flash object. */
	id: "flashObject",
	
	/** Controls whether this is a visible Flash applet or not. */
	_visible: true,
			
	/** 
			Writes the Flash into the page. This must be called before the page
			is finished loading. 
			@param flashVer The Flash version to write.
			@param doExpressInstall Whether to write out Express Install
			information. Optional value; defaults to false.
	*/
	write: function(flashVer, doExpressInstall){
		//dojo.debug("write");
		if(dojo.lang.isUndefined(doExpressInstall)){
			doExpressInstall = false;
		}
		
		// determine our container div's styling
		var containerStyle = new dojo.string.Builder();
		containerStyle.append("width: " + this.width + "px; ");
		containerStyle.append("height: " + this.height + "px; ");
		if(this._visible == false){
			containerStyle.append("position: absolute; ");
			containerStyle.append("z-index: 10000; ");
			containerStyle.append("top: -1000px; ");
			containerStyle.append("left: -1000px; ");
		}
		containerStyle = containerStyle.toString();

		// figure out the SWF file to get and how to write out the correct HTML
		// for this Flash version
		var objectHTML;
		var swfloc;
		// Flash 6
		if(flashVer == 6){
			swfloc = dojo.flash.flash6_version;
			var dojoPath = djConfig.baseRelativePath;
			swfloc = swfloc + "?baseRelativePath=" + escape(dojoPath);
			
			objectHTML = 
						  '<embed id="' + this.id + '" src="' + swfloc + '" '
						+ '    quality="high" bgcolor="#ffffff" '
						+ '    width="' + this.width + '" height="' + this.height + '" '
						+ '    name="' + this.id + '" '
						+ '    align="middle" allowScriptAccess="sameDomain" '
						+ '    type="application/x-shockwave-flash" swLiveConnect="true" '
						+ '    pluginspage="http://www.macromedia.com/go/getflashplayer">';
		}else{ // Flash 8
			swfloc = dojo.flash.flash8_version;
			var swflocObject = swfloc, swflocEmbed = swfloc;
			if(doExpressInstall){
				// the location to redirect to after installing
				var redirectURL = escape(window.location);
				document.title = document.title.slice(0, 47) + " - Flash Player Installation";
				var docTitle = escape(document.title);
				swflocObject += "?MMredirectURL=" + redirectURL
				                + "&MMplayerType=ActiveX"
				                + "&MMdoctitle="+docTitle;
				swflocEmbed += "?MMredirectURL=" + redirectURL + "&MMplayerType=PlugIn";
			}
			
			objectHTML =
				'<object classid="clsid:d27cdb6e-ae6d-11cf-96b8-444553540000" '
				  + 'codebase="http://fpdownload.macromedia.com/pub/shockwave/cabs/flash/swflash.cab#version=8,0,0,0" '
				  + 'width="' + this.width + '" '
				  + 'height="' + this.height + '" '
				  + 'id="' + this.id + '" '
				  + 'align="middle"> '
				  + '<param name="allowScriptAccess" value="sameDomain" /> '
				  + '<param name="movie" value="' + swflocObject + '" /> '
				  + '<param name="quality" value="high" /> '
				  + '<param name="bgcolor" value="#ffffff" /> '
				  + '<embed src="' + swflocEmbed + '" '
				  + 'quality="high" '
				  + 'bgcolor="#ffffff" '
				  + 'width="' + this.width + '" '
				  + 'height="' + this.height + '" '
				  + 'id="' + this.id + '" '
				  + 'name="' + this.id + '" '
				  + 'swLiveConnect="true" '
				  + 'align="middle" '
				  + 'allowScriptAccess="sameDomain" '
				  + 'type="application/x-shockwave-flash" '
				  + 'pluginspage="http://www.macromedia.com/go/getflashplayer" />'
				+ '</object>';
		}

		// now write everything out
		objectHTML = '<div id="' + this.id + 'Container" style="' + containerStyle + '"> '
						+ objectHTML
					 + '</div>';
		document.writeln(objectHTML);
	},  
	
	/** Gets the Flash object DOM node. */
	get: function(){
		//return (dojo.render.html.ie) ? window[this.id] : document[this.id];
		
		// more robust way to get Flash object; version above can break
		// communication on IE sometimes
		return document.getElementById(this.id);
	},
	
	/** Sets the visibility of this Flash object. */
	setVisible: function(visible){
		var container = dojo.byId(this.id + "Container");
		if(visible == true){
			container.style.visibility = "visible";
		}else{
			container.style.position = "absolute";
			container.style.x = "-1000px";
			container.style.y = "-1000px";
			container.style.visibility = "hidden";
		}
	},
	
	/** Centers the flash applet on the page. */
	center: function(){
		// FIXME: replace this with Dojo's centering code rather than our own
		// We want to center the Flash applet vertically and horizontally
		var elementWidth = this.width;
		var elementHeight = this.height;
    
		// get the browser width and height; the code below
		// works in IE and Firefox in compatibility, non-strict
		// mode
		var browserWidth = document.body.clientWidth;
		var browserHeight = document.body.clientHeight;
    
		// in Firefox if we are in standards compliant mode
		// (with a strict doctype), then the browser width
		// and height have to be computed from the root level
		// HTML element not the BODY element
		if(!dojo.render.html.ie && document.compatMode == "CSS1Compat"){
			browserWidth = document.body.parentNode.clientWidth;
			browserHeight = document.body.parentNode.clientHeight;
		}else if(dojo.render.html.ie && document.compatMode == "CSS1Compat"){
			// IE 6 in standards compliant mode has to be calculated
			// differently
			browserWidth = document.documentElement.clientWidth;
			browserHeight = document.documentElement.clientHeight;
		}else if(dojo.render.html.safari){ // Safari works different
			browserHeight = self.innerHeight;
		}
    
		// get where we are scrolled to in the document
		// the code below works in FireFox
		var scrolledByWidth = window.scrollX;
		var scrolledByHeight = window.scrollY;
		// compute these values differently for IE;
		// IE has two possibilities; it is either in standards
		// compatibility mode or it is not
		if(typeof scrolledByWidth == "undefined"){
			if(document.compatMode == "CSS1Compat"){ // standards mode
				scrolledByWidth = document.documentElement.scrollLeft;
				scrolledByHeight = document.documentElement.scrollTop;
			}else{ // Pre IE6 non-standards mode
				scrolledByWidth = document.body.scrollLeft;
				scrolledByHeight = document.body.scrollTop;
			}
		}

		// compute the centered position    
		var x = scrolledByWidth + (browserWidth - elementWidth) / 2;
		var y = scrolledByHeight + (browserHeight - elementHeight) / 2; 

		// set the centered position
		var container = dojo.byId(this.id + "Container");
		container.style.top = y + "px";
		container.style.left = x + "px";
	}
};


/** 
		A class that is used to communicate between Flash and JavaScript in 
		a way that can pass large amounts of data back and forth reliably,
		very fast, and with synchronous method calls. This class encapsulates the 
		specific way in which this communication occurs,
		presenting a common interface to JavaScript irrespective of the underlying
		Flash version.
*/
dojo.flash.Communicator = function(){
	if(dojo.flash.useFlash6()){
		this._writeFlash6();
	}else if (dojo.flash.useFlash8()){
		this._writeFlash8();
	}
}

dojo.flash.Communicator.prototype = {
	_writeFlash6: function(){
		var id = dojo.flash.obj.id;
		
		// global function needed for Flash 6 callback;
		// we write it out as a script tag because the VBScript hook for IE
		// callbacks does not work properly if this function is evalled() from
		// within the Dojo system
		document.writeln('<script language="JavaScript">');
		document.writeln('  function ' + id + '_DoFSCommand(command, args){ ');
		document.writeln('    dojo.flash.comm._handleFSCommand(command, args); ');
		document.writeln('}');
		document.writeln('</script>');
		
		// hook for Internet Explorer to receive FSCommands from Flash
		if(dojo.render.html.ie){
			document.writeln('<SCRIPT LANGUAGE=VBScript\> ');
			document.writeln('on error resume next ');
			document.writeln('Sub ' + id + '_FSCommand(ByVal command, ByVal args)');
			document.writeln(' call ' + id + '_DoFSCommand(command, args)');
			document.writeln('end sub');
			document.writeln('</SCRIPT\> ');
		}
	},
	
	_writeFlash8: function(){
		// nothing needs to be written out for Flash 8 communication; 
		// happens automatically
	},
	
	/** Flash 6 communication. */
	
	/** Handles fscommand's from Flash to JavaScript. Flash 6 communication. */
	_handleFSCommand: function(command, args){
		//dojo.debug("fscommand, command="+command+", args="+args);
		// Flash 8 on Mac/Firefox precedes all commands with the string "FSCommand:";
		// strip it off if it is present
		if(command != null && !dojo.lang.isUndefined(command)
			&& /^FSCommand:(.*)/.test(command) == true){
			command = command.match(/^FSCommand:(.*)/)[1];
		}
		 
		if(command == "addCallback"){ // add Flash method for JavaScript callback
			this._fscommandAddCallback(command, args);
		}else if(command == "call"){ // Flash to JavaScript method call
			this._fscommandCall(command, args);
		}else if(command == "fscommandReady"){ // see if fscommands are ready
			this._fscommandReady();
		}
	},
	
	/** Handles registering a callable Flash function. Flash 6 communication. */
	_fscommandAddCallback: function(command, args){
		var functionName = args;
			
		// do a trick, where we link this function name to our wrapper
		// function, _call, that does the actual JavaScript to Flash call
		var callFunc = function(){
			return dojo.flash.comm._call(functionName, arguments);
		};			
		dojo.flash.comm[functionName] = callFunc;
		
		// indicate that the call was successful
		dojo.flash.obj.get().SetVariable("_succeeded", true);
	},
	
	/** Handles Flash calling a JavaScript function. Flash 6 communication. */
	_fscommandCall: function(command, args){
		var plugin = dojo.flash.obj.get();
		var functionName = args;
		
		// get the number of arguments to this method call and build them up
		var numArgs = parseInt(plugin.GetVariable("_numArgs"));
		var flashArgs = new Array();
		for(var i = 0; i < numArgs; i++){
			var currentArg = plugin.GetVariable("_" + i);
			flashArgs.push(currentArg);
		}
		
		// get the function instance; we technically support more capabilities
		// than ExternalInterface, which can only call global functions; if
		// the method name has a dot in it, such as "dojo.flash.loaded", we
		// eval it so that the method gets run against an instance
		var runMe;
		if(functionName.indexOf(".") == -1){ // global function
			runMe = window[functionName];
		}else{
			// instance function
			runMe = eval(functionName);
		}
		
		// make the call and get the results
		var results = null;
		if(!dojo.lang.isUndefined(runMe) && runMe != null){
			results = runMe.apply(null, flashArgs);
		}
		
		// return the results to flash
		plugin.SetVariable("_returnResult", results);
	},
	
	/** Reports that fscommands are ready to run if executed from Flash. */
	_fscommandReady: function(){
		var plugin = dojo.flash.obj.get();
		plugin.SetVariable("fscommandReady", "true");
	},
	
	/** 
			The actual function that will execute a JavaScript to Flash call; used
			by the Flash 6 communication method. 
	*/
	_call: function(functionName, args){
		// we do JavaScript to Flash method calls by setting a Flash variable
		// "_functionName" with the function name; "_numArgs" with the number
		// of arguments; and "_0", "_1", etc for each numbered argument. Flash
		// reads these, executes the function call, and returns the result
		// in "_returnResult"
		var plugin = dojo.flash.obj.get();
		plugin.SetVariable("_functionName", functionName);
		plugin.SetVariable("_numArgs", args.length);
		for(var i = 0; i < args.length; i++){
			// unlike Flash 8's ExternalInterface, Flash 6 has no problem with
			// any special characters _except_ for the null character \0; double
			// encode this so the Flash side never sees it, but we can get it 
			// back if the value comes back to JavaScript
			var value = args[i];
			value = value.replace(/\0/g, "\\0");
			
			plugin.SetVariable("_" + i, value);
		}
		
		// now tell Flash to execute this method using the Flash Runner
		plugin.TCallLabel("/_flashRunner", "execute");
		
		// get the results
		var results = plugin.GetVariable("_returnResult");
		
		// we double encoded all null characters as //0 because Flash breaks
		// if they are present; turn the //0 back into /0
		results = results.replace(/\\0/g, "\0");
		
		return results;
	},
	
	/** Flash 8 communication. */
	
	/** 
			Registers the existence of a Flash method that we can call with
			JavaScript, using Flash 8's ExternalInterface. 
	*/
	_addExternalInterfaceCallback: function(methodName){
		var wrapperCall = function(){
			// some browsers don't like us changing values in the 'arguments' array, so
			// make a fresh copy of it
			var methodArgs = new Array(arguments.length);
			for(var i = 0; i < arguments.length; i++){
				methodArgs[i] = arguments[i];
			}
			return dojo.flash.comm._execFlash(methodName, methodArgs);
		};
		
		dojo.flash.comm[methodName] = wrapperCall;
	},
	
	/** 
			Encodes our data to get around ExternalInterface bugs.
			Flash 8 communication.
	*/
	_encodeData: function(data){
		// double encode all entity values, or they will be mis-decoded
		// by Flash when returned
		var entityRE = /\&([^;]*)\;/g;
		data = data.replace(entityRE, "&amp;$1;");
		
		// entity encode XML-ish characters, or Flash's broken XML serializer
		// breaks
		data = data.replace(/</g, "&lt;");
		data = data.replace(/>/g, "&gt;");
		
		// transforming \ into \\ doesn't work; just use a custom encoding
		data = data.replace("\\", "&custom_backslash;&custom_backslash;");
		
		data = data.replace(/\n/g, "\\n");
		data = data.replace(/\r/g, "\\r");
		data = data.replace(/\f/g, "\\f");
		data = data.replace(/\0/g, "\\0"); // null character
		data = data.replace(/\'/g, "\\\'");
		data = data.replace(/\"/g, '\\\"');
		
		return data;
	},
	
	/** 
			Decodes our data to get around ExternalInterface bugs.
			Flash 8 communication.
	*/
	_decodeData: function(data){
		if(data == null || typeof data == "undefined"){
			return data;
		}
		
		// certain XMLish characters break Flash's wire serialization for
		// ExternalInterface; these are encoded on the 
		// DojoExternalInterface side into a custom encoding, rather than
		// the standard entity encoding, because otherwise we won't be able to
		// differentiate between our own encoding and any entity characters
		// that are being used in the string itself
		data = data.replace(/\&custom_lt\;/g, "<");
		data = data.replace(/\&custom_gt\;/g, ">");
		
		// Unfortunately, Flash returns us our String with special characters
		// like newlines broken into seperate characters. So if \n represents
		// a new line, Flash returns it as "\" and "n". This means the character
		// is _not_ a newline. This forces us to eval() the string to cause
		// escaped characters to turn into their real special character values.
		data = eval('"' + data + '"');
		
		return data;
	},
	
	/** 
			Sends our method arguments over to Flash in chunks in order to
			have ExternalInterface's performance not be O(n^2).
			Flash 8 communication.
	*/
	_chunkArgumentData: function(value, argIndex){
		var plugin = dojo.flash.obj.get();
		
		// cut up the string into pieces, and push over each piece one
		// at a time
		var numSegments = Math.ceil(value.length / 1024);
		for(var i = 0; i < numSegments; i++){
			var startCut = i * 1024;
			var endCut = i * 1024 + 1024;
			if(i == (numSegments - 1)){
				endCut = i * 1024 + value.length;
			}
			
			var piece = value.substring(startCut, endCut);
			
			// encode each piece seperately, rather than the entire
			// argument data, because ocassionally a special 
			// character, such as an entity like &foobar;, will fall between
			// piece boundaries, and we _don't_ want to encode that value if
			// it falls between boundaries, or else we will end up with incorrect
			// data when we patch the pieces back together on the other side
			piece = this._encodeData(piece);
			
			// directly use the underlying CallFunction method used by
			// ExternalInterface, which is vastly faster for large strings
			// and lets us bypass some Flash serialization bugs
			plugin.CallFunction('<invoke name="chunkArgumentData" '
														+ 'returntype="javascript">'
														+ '<arguments>'
														+ '<string>' + piece + '</string>'
														+ '<number>' + argIndex + '</number>'
														+ '</arguments>'
														+ '</invoke>');
		}
	},
	
	/** 
			Gets our method return data in chunks for better performance.
			Flash 8 communication.
	*/
	_chunkReturnData: function(){
		var plugin = dojo.flash.obj.get();
		
		var numSegments = plugin.getReturnLength();
		var resultsArray = new Array();
		for(var i = 0; i < numSegments; i++){
			// directly use the underlying CallFunction method used by
			// ExternalInterface, which is vastly faster for large strings
			var piece = 
					plugin.CallFunction('<invoke name="chunkReturnData" '
															+ 'returntype="javascript">'
															+ '<arguments>'
															+ '<number>' + i + '</number>'
															+ '</arguments>'
															+ '</invoke>');
															
			// remove any leading or trailing JavaScript delimiters, which surround
			// our String when it comes back from Flash since we bypass Flash's
			// deserialization routines by directly calling CallFunction on the
			// plugin
			if(piece == '""' || piece == "''"){
				piece = "";
			}else{
				piece = piece.substring(1, piece.length-1);
			}
		
			resultsArray.push(piece);
		}
		var results = resultsArray.join("");
		
		return results;
	},
	
	/** 
			Executes a Flash method; called from the JavaScript wrapper proxy we
			create on dojo.flash.comm.
			Flash 8 communication.
	*/
	_execFlash: function(methodName, methodArgs){
		var plugin = dojo.flash.obj.get();
				
		// begin Flash method execution
		plugin.startExec();
		
		// set the number of arguments
		plugin.setNumberArguments(methodArgs.length);
		
		// chunk and send over each argument
		for(var i = 0; i < methodArgs.length; i++){
			this._chunkArgumentData(methodArgs[i], i);
		}
		
		// execute the method
		plugin.exec(methodName);
														
		// get the return result
		var results = this._chunkReturnData();
		
		// decode the results
		results = this._decodeData(results);
		
		// reset everything
		plugin.endExec();
		
		return results;

	}
}

/** 
		Figures out the best way to automatically install the Flash plugin
		for this browser and platform. Also determines if installation or
		revving of the current plugin is needed on this platform.
*/
dojo.flash.Install = function(){
}

dojo.flash.Install.prototype = {
	/** 
			Determines if installation or revving of the current plugin is 
			needed. 
	*/
	needed: function(){
		// do we even have flash?
		if(dojo.flash.info.capable == false){
			return true;
		}

		// are we on the Mac? Safari needs Flash version 8 to do Flash 8
		// communication, while Firefox/Mac needs Flash 8 to fix bugs it has
		// with Flash 6 communication
		if(dojo.render.os.mac == true && !dojo.flash.info.isVersionOrAbove(8, 0, 0)){
			return true;
		}

		// other platforms need at least Flash 6 or above
		if(!dojo.flash.info.isVersionOrAbove(6, 0, 0)){
			return true;
		}

		// otherwise we don't need installation
		return false;
	},

	/** Performs installation or revving of the Flash plugin. */
	install: function(){
		//dojo.debug("install");
		// indicate that we are installing
		dojo.flash.info.installing = true;
		dojo.flash.installing();
		
		if(dojo.flash.info.capable == false){ // we have no Flash at all
			//dojo.debug("Completely new install");
			// write out a simple Flash object to force the browser to prompt
			// the user to install things
			var installObj = new dojo.flash.Embed(false);
			installObj.write(8); // write out HTML for Flash 8 version+
		}else if(dojo.flash.info.isVersionOrAbove(6, 0, 65)){ // Express Install
			//dojo.debug("Express install");
			var installObj = new dojo.flash.Embed(false);
			installObj.write(8, true); // write out HTML for Flash 8 version+
			installObj.setVisible(true);
			installObj.center();
		}else{ // older Flash install than version 6r65
			alert("This content requires a more recent version of the Macromedia "
						+" Flash Player.");
			window.location.href = "http://www.macromedia.com/go/getflashplayer";
		}
	},
	
	/** 
			Called when the Express Install is either finished, failed, or was
			rejected by the user.
	*/
	_onInstallStatus: function(msg){
		if (msg == "Download.Complete"){
			// Installation is complete.
			dojo.flash._initialize();
		}else if(msg == "Download.Cancelled"){
			alert("This content requires a more recent version of the Macromedia "
						+" Flash Player.");
			window.location.href = "http://www.macromedia.com/go/getflashplayer";
		}else if (msg == "Download.Failed"){
			// The end user failed to download the installer due to a network failure
			alert("There was an error downloading the Flash Player update. "
						+ "Please try again later, or visit macromedia.com to download "
						+ "the latest version of the Flash plugin.");
		}	
	}
}

// find out if Flash is installed
dojo.flash.info = new dojo.flash.Info();

// vim:ts=4:noet:tw=0:

dojo.provide("dojo.storage.browser");
dojo.provide("dojo.storage.browser.FlashStorageProvider");

dojo.require("dojo.storage");
dojo.require("dojo.flash");
dojo.require("dojo.json");
dojo.require("dojo.uri.*");

/** 
		Storage provider that uses features in Flash to achieve permanent storage.
		
		@author Alex Russel, alex@dojotoolkit.org
		@author Brad Neuberg, bkn3@columbia.edu 
*/
dojo.storage.browser.FlashStorageProvider = function(){
}

dojo.inherits(dojo.storage.browser.FlashStorageProvider, dojo.storage);

// instance methods and properties
dojo.lang.extend(dojo.storage.browser.FlashStorageProvider, {
	namespace: "default",
	initialized: false,
	_available: null,
	_statusHandler: null,
	
	initialize: function(){
		// initialize our Flash
		var loadedListener = function(){
			dojo.storage._flashLoaded();
		}
		dojo.flash.addLoadedListener(loadedListener);
		var swfloc6 = dojo.uri.dojoUri("Storage_version6.swf").toString();
		var swfloc8 = dojo.uri.dojoUri("Storage_version8.swf").toString();
		dojo.flash.setSwf({flash6: swfloc6, flash8: swfloc8, visible: false});
	},
	
	isAvailable: function(){
		if(djConfig["disableFlashStorage"] == true){
			this._available = false;
		}
		
		return this._available;
	},
	
	setNamespace: function(namespace){
		this.namespace = namespace;
	},

	put: function(key, value, resultsHandler){
		if(this.isValidKey(key) == false){
			dojo.raise("Invalid key given: " + key);
		}
			
		this._statusHandler = resultsHandler;
		
		// serialize the value
		// Handle strings differently so they have better performance
		if(dojo.lang.isString(value)){
			value = "string:" + value;
		}else{
			value = dojo.json.serialize(value);
		}
		
		dojo.flash.comm.put(key, value, this.namespace);
	},

	get: function(key){
		if(this.isValidKey(key) == false){
			dojo.raise("Invalid key given: " + key);
		}
		
		var results = dojo.flash.comm.get(key, this.namespace);

		if(results == ""){
			return null;
		}
    
		// destringify the content back into a 
		// real JavaScript object
		// Handle strings differently so they have better performance
		if(!dojo.lang.isUndefined(results) && results != null 
			 && /^string:/.test(results)){
			results = results.substring("string:".length);
		}else{
			results = dojo.json.evalJson(results);
		}
    
		return results;
	},

	getKeys: function(){
		var results = dojo.flash.comm.getKeys(this.namespace);
		
		if(results == ""){
			return new Array();
		}

		// the results are returned comma seperated; split them
		results = results.split(",");
		
		return results;
	},

	clear: function(){
		dojo.flash.comm.clear(this.namespace);
	},
	
	remove: function(key){
	},
	
	isPermanent: function(){
		return true;
	},

	getMaximumSize: function(){
		return dojo.storage.SIZE_NO_LIMIT;
	},

	hasSettingsUI: function(){
		return true;
	},

	showSettingsUI: function(){
		dojo.flash.comm.showSettings();
		dojo.flash.obj.setVisible(true);
		dojo.flash.obj.center();
	},

	hideSettingsUI: function(){
		// hide the dialog
		dojo.flash.obj.setVisible(false);
		
		// call anyone who wants to know the dialog is
		// now hidden
		if(dojo.storage.onHideSettingsUI != null &&
			!dojo.lang.isUndefined(dojo.storage.onHideSettingsUI)){
			dojo.storage.onHideSettingsUI.call(null);	
		}
	},
	
	/** 
			The provider name as a string, such as 
			"dojo.storage.FlashStorageProvider". 
	*/
	getType: function(){
		return "dojo.storage.FlashStorageProvider";
	},
	
	/** Called when the Flash is finished loading. */
	_flashLoaded: function(){
		this.initialized = true;

		// indicate that this storage provider is now loaded
		dojo.storage.manager.loaded();
	},
	
	/** 
			Called if the storage system needs to tell us about the status
			of a put() request. 
	*/
	_onStatus: function(statusResult, key){
		//dojo.debug("_onStatus, statusResult="+statusResult+", key="+key);
		if(statusResult == dojo.storage.PENDING){
			dojo.flash.obj.center();
			dojo.flash.obj.setVisible(true);
		}else{
			dojo.flash.obj.setVisible(false);
		}
		
		if(!dojo.lang.isUndefined(dojo.storage._statusHandler) 
				&& dojo.storage._statusHandler != null){
			dojo.storage._statusHandler.call(null, statusResult, key);		
		}
	}
});

// register the existence of our storage providers
dojo.storage.manager.register("dojo.storage.browser.FlashStorageProvider",
                              new dojo.storage.browser.FlashStorageProvider());

// now that we are loaded and registered tell the storage manager to initialize
// itself
dojo.storage.manager.initialize();
															

dojo.kwCompoundRequire({
	common: ["dojo.storage"],
	browser: ["dojo.storage.browser"],
	dashboard: ["dojo.storage.dashboard"]
});
dojo.provide("dojo.storage.*");


dojo.provide("dojo.undo.Manager");
dojo.require("dojo.lang");

dojo.undo.Manager = function(parent) {
	this.clear();
	this._parent = parent;
};
dojo.lang.extend(dojo.undo.Manager, {
	_parent: null,
	_undoStack: null,
	_redoStack: null,
	_currentManager: null,

	canUndo: false,
	canRedo: false,

	isUndoing: false,
	isRedoing: false,

	// these events allow you to hook in and update your code (UI?) as necessary
	onUndo: function(manager, item) {},
	onRedo: function(manager, item) {},

	// fired when you do *any* undo action, which means you'll have one for every item
	// in a transaction. this is usually only useful for debugging
	onUndoAny: function(manager, item) {},
	onRedoAny: function(manager, item) {},

	_updateStatus: function() {
		this.canUndo = this._undoStack.length > 0;
		this.canRedo = this._redoStack.length > 0;
	},

	clear: function() {
		this._undoStack = [];
		this._redoStack = [];
		this._currentManager = this;

		this.isUndoing = false;
		this.isRedoing = false;

		this._updateStatus();
	},

	undo: function() {
		if(!this.canUndo) { return false; }

		this.endAllTransactions();

		this.isUndoing = true;
		var top = this._undoStack.pop();
		if(top instanceof this.constructor) {
			top.undoAll();
		} else {
			top.undo();
		}
		if(top.redo) {
			this._redoStack.push(top);
		}
		this.isUndoing = false;

		this._updateStatus();
		this.onUndo(this, top);
		if(!(top instanceof this.constructor)) {
			this.getTop().onUndoAny(this, top);
		}
		return true;
	},

	redo: function() {
		if(!this.canRedo) { return false; }

		this.isRedoing = true;
		var top = this._redoStack.pop();
		if(top instanceof this.constructor) {
			top.redoAll();
		} else {
			top.redo();
		}
		this._undoStack.push(top);
		this.isRedoing = false;

		this._updateStatus();
		this.onRedo(this, top);
		if(!(top instanceof this.constructor)) {
			this.getTop().onRedoAny(this, top);
		}
		return true;
	},

	undoAll: function() {
		while(this._undoStack.length > 0) {
			this.undo();
		}
	},

	redoAll: function() {
		while(this._redoStack.length > 0) {
			this.redo();
		}
	},

	push: function(undo, redo /* optional */, description /* optional */) {
		if(!undo) { return; }

		if(this._currentManager == this) {
			this._undoStack.push({
				undo: undo,
				redo: redo,
				description: description
			});
		} else {
			this._currentManager.push.apply(this._currentManager, arguments);
		}
		// adding a new undo-able item clears out the redo stack
		this._redoStack = [];
		this._updateStatus();
	},

	concat: function(manager) {
		if ( !manager ) { return; }

		if (this._currentManager == this ) {
			for(var x=0; x < manager._undoStack.length; x++) {
				this._undoStack.push(manager._undoStack[x]);
			}
			this._updateStatus();
		} else {
			this._currentManager.concat.apply(this._currentManager, arguments);
		}
	},

	beginTransaction: function(description /* optional */) {
		if(this._currentManager == this) {
			var mgr = new dojo.undo.Manager(this);
			mgr.description = description ? description : "";
			this._undoStack.push(mgr);
			this._currentManager = mgr;
			return mgr;
		} else {
			//for nested transactions need to make sure the top level _currentManager is set
			this._currentManager = this._currentManager.beginTransaction.apply(this._currentManager, arguments);
		}
	},

	endTransaction: function(flatten /* optional */) {
		if(this._currentManager == this) {
			if(this._parent) {
				this._parent._currentManager = this._parent;
				// don't leave empty transactions hangin' around
				if(this._undoStack.length == 0 || flatten) {
					var idx = dojo.lang.find(this._parent._undoStack, this);
					if (idx >= 0) {
						this._parent._undoStack.splice(idx, 1);
						//add the current transaction to parents undo stack
						if (flatten) {
							for(var x=0; x < this._undoStack.length; x++){
								this._parent._undoStack.splice(idx++, 0, this._undoStack[x]);
							}
							this._updateStatus();
						}
					}
				}
				return this._parent;
			}
		} else {
			//for nested transactions need to make sure the top level _currentManager is set
			this._currentManager = this._currentManager.endTransaction.apply(this._currentManager, arguments);
		}
	},

	endAllTransactions: function() {
		while(this._currentManager != this) {
			this.endTransaction();
		}
	},

	// find the top parent of an undo manager
	getTop: function() {
		if(this._parent) {
			return this._parent.getTop();
		} else {
			return this;
		}
	}
});

dojo.require("dojo.undo.Manager");
dojo.provide("dojo.undo.*");

dojo.provide("dojo.crypto");

//	enumerations for use in crypto code. Note that 0 == default, for the most part.
dojo.crypto.cipherModes={ ECB:0, CBC:1, PCBC:2, CFB:3, OFB:4, CTR:5 };
dojo.crypto.outputTypes={ Base64:0,Hex:1,String:2,Raw:3 };

dojo.require("dojo.crypto");
dojo.provide("dojo.crypto.MD5");

/*	Return to a port of Paul Johnstone's MD5 implementation
 *	http://pajhome.org.uk/crypt/md5/index.html
 *
 *	2005-12-7
 *	All conversions are internalized (no dependencies)
 *	implemented getHMAC for message digest auth.
 */
dojo.crypto.MD5 = new function(){
	var chrsz=8;
	var mask=(1<<chrsz)-1;
	function toWord(s) {
	  var wa=[];
	  for(var i=0; i<s.length*chrsz; i+=chrsz)
		wa[i>>5]|=(s.charCodeAt(i/chrsz)&mask)<<(i%32);
	  return wa;
	}
	function toString(wa){
		var s=[];
		for(var i=0; i<wa.length*32; i+=chrsz)
			s.push(String.fromCharCode((wa[i>>5]>>>(i%32))&mask));
		return s.join("");
	}
	function toHex(wa) {
		var h="0123456789abcdef";
		var s=[];
		for(var i=0; i<wa.length*4; i++){
			s.push(h.charAt((wa[i>>2]>>((i%4)*8+4))&0xF)+h.charAt((wa[i>>2]>>((i%4)*8))&0xF));
		}
		return s.join("");
	}
	function toBase64(wa){
		var p="=";
		var tab="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
		var s=[];
		for(var i=0; i<wa.length*4; i+=3){
			var t=(((wa[i>>2]>>8*(i%4))&0xFF)<<16)|(((wa[i+1>>2]>>8*((i+1)%4))&0xFF)<<8)|((wa[i+2>>2]>>8*((i+2)%4))&0xFF);
			for(var j=0; j<4; j++){
				if(i*8+j*6>wa.length*32) s.push(p);
				else s.push(tab.charAt((t>>6*(3-j))&0x3F));
			}
		}
		return s.join("");
	}
	function add(x,y) {
		var l=(x&0xFFFF)+(y&0xFFFF);
		var m=(x>>16)+(y>>16)+(l>>16);
		return (m<<16)|(l&0xFFFF);
	}
	function R(n,c){ return (n<<c)|(n>>>(32-c)); }
	function C(q,a,b,x,s,t){ return add(R(add(add(a,q),add(x,t)),s),b); }
	function FF(a,b,c,d,x,s,t){ return C((b&c)|((~b)&d),a,b,x,s,t); }
	function GG(a,b,c,d,x,s,t){ return C((b&d)|(c&(~d)),a,b,x,s,t); }
	function HH(a,b,c,d,x,s,t){ return C(b^c^d,a,b,x,s,t); }
	function II(a,b,c,d,x,s,t){ return C(c^(b|(~d)),a,b,x,s,t); }
	function core(x,len){
		x[len>>5]|=0x80<<((len)%32);
		x[(((len+64)>>>9)<<4)+14]=len;
		var a= 1732584193;
		var b=-271733879;
		var c=-1732584194;
		var d= 271733878;
		for(var i=0; i<x.length; i+=16){
			var olda=a;
			var oldb=b;
			var oldc=c;
			var oldd=d;

			a=FF(a,b,c,d,x[i+ 0],7 ,-680876936);
			d=FF(d,a,b,c,x[i+ 1],12,-389564586);
			c=FF(c,d,a,b,x[i+ 2],17, 606105819);
			b=FF(b,c,d,a,x[i+ 3],22,-1044525330);
			a=FF(a,b,c,d,x[i+ 4],7 ,-176418897);
			d=FF(d,a,b,c,x[i+ 5],12, 1200080426);
			c=FF(c,d,a,b,x[i+ 6],17,-1473231341);
			b=FF(b,c,d,a,x[i+ 7],22,-45705983);
			a=FF(a,b,c,d,x[i+ 8],7 , 1770035416);
			d=FF(d,a,b,c,x[i+ 9],12,-1958414417);
			c=FF(c,d,a,b,x[i+10],17,-42063);
			b=FF(b,c,d,a,x[i+11],22,-1990404162);
			a=FF(a,b,c,d,x[i+12],7 , 1804603682);
			d=FF(d,a,b,c,x[i+13],12,-40341101);
			c=FF(c,d,a,b,x[i+14],17,-1502002290);
			b=FF(b,c,d,a,x[i+15],22, 1236535329);

			a=GG(a,b,c,d,x[i+ 1],5 ,-165796510);
			d=GG(d,a,b,c,x[i+ 6],9 ,-1069501632);
			c=GG(c,d,a,b,x[i+11],14, 643717713);
			b=GG(b,c,d,a,x[i+ 0],20,-373897302);
			a=GG(a,b,c,d,x[i+ 5],5 ,-701558691);
			d=GG(d,a,b,c,x[i+10],9 , 38016083);
			c=GG(c,d,a,b,x[i+15],14,-660478335);
			b=GG(b,c,d,a,x[i+ 4],20,-405537848);
			a=GG(a,b,c,d,x[i+ 9],5 , 568446438);
			d=GG(d,a,b,c,x[i+14],9 ,-1019803690);
			c=GG(c,d,a,b,x[i+ 3],14,-187363961);
			b=GG(b,c,d,a,x[i+ 8],20, 1163531501);
			a=GG(a,b,c,d,x[i+13],5 ,-1444681467);
			d=GG(d,a,b,c,x[i+ 2],9 ,-51403784);
			c=GG(c,d,a,b,x[i+ 7],14, 1735328473);
			b=GG(b,c,d,a,x[i+12],20,-1926607734);

			a=HH(a,b,c,d,x[i+ 5],4 ,-378558);
			d=HH(d,a,b,c,x[i+ 8],11,-2022574463);
			c=HH(c,d,a,b,x[i+11],16, 1839030562);
			b=HH(b,c,d,a,x[i+14],23,-35309556);
			a=HH(a,b,c,d,x[i+ 1],4 ,-1530992060);
			d=HH(d,a,b,c,x[i+ 4],11, 1272893353);
			c=HH(c,d,a,b,x[i+ 7],16,-155497632);
			b=HH(b,c,d,a,x[i+10],23,-1094730640);
			a=HH(a,b,c,d,x[i+13],4 , 681279174);
			d=HH(d,a,b,c,x[i+ 0],11,-358537222);
			c=HH(c,d,a,b,x[i+ 3],16,-722521979);
			b=HH(b,c,d,a,x[i+ 6],23, 76029189);
			a=HH(a,b,c,d,x[i+ 9],4 ,-640364487);
			d=HH(d,a,b,c,x[i+12],11,-421815835);
			c=HH(c,d,a,b,x[i+15],16, 530742520);
			b=HH(b,c,d,a,x[i+ 2],23,-995338651);

			a=II(a,b,c,d,x[i+ 0],6 ,-198630844);
			d=II(d,a,b,c,x[i+ 7],10, 1126891415);
			c=II(c,d,a,b,x[i+14],15,-1416354905);
			b=II(b,c,d,a,x[i+ 5],21,-57434055);
			a=II(a,b,c,d,x[i+12],6 , 1700485571);
			d=II(d,a,b,c,x[i+ 3],10,-1894986606);
			c=II(c,d,a,b,x[i+10],15,-1051523);
			b=II(b,c,d,a,x[i+ 1],21,-2054922799);
			a=II(a,b,c,d,x[i+ 8],6 , 1873313359);
			d=II(d,a,b,c,x[i+15],10,-30611744);
			c=II(c,d,a,b,x[i+ 6],15,-1560198380);
			b=II(b,c,d,a,x[i+13],21, 1309151649);
			a=II(a,b,c,d,x[i+ 4],6 ,-145523070);
			d=II(d,a,b,c,x[i+11],10,-1120210379);
			c=II(c,d,a,b,x[i+ 2],15, 718787259);
			b=II(b,c,d,a,x[i+ 9],21,-343485551);

			a = add(a,olda);
			b = add(b,oldb);
			c = add(c,oldc);
			d = add(d,oldd);
		}
		return [a,b,c,d];
	}
	function hmac(data,key){
		var wa=toWord(key);
		if(wa.length>16) wa=core(wa,key.length*chrsz);
		var l=[], r=[];
		for(var i=0; i<16; i++){
			l[i]=wa[i]^0x36363636;
			r[i]=wa[i]^0x5c5c5c5c;
		}
		var h=core(l.concat(toWord(data)),512+data.length*chrsz);
		return core(r.concat(h),640);
	}

	//	Public functions
	this.compute=function(data,outputType){
		var out=outputType||dojo.crypto.outputTypes.Base64;
		switch(out){
			case dojo.crypto.outputTypes.Hex:{
				return toHex(core(toWord(data),data.length*chrsz));
			}
			case dojo.crypto.outputTypes.String:{
				return toString(core(toWord(data),data.length*chrsz));
			}
			default:{
				return toBase64(core(toWord(data),data.length*chrsz));
			}
		}
	};
	this.getHMAC=function(data,key,outputType){
		var out=outputType||dojo.crypto.outputTypes.Base64;
		switch(out){
			case dojo.crypto.outputTypes.Hex:{
				return toHex(hmac(data,key));
			}
			case dojo.crypto.outputTypes.String:{
				return toString(hmac(data,key));
			}
			default:{
				return toBase64(hmac(data,key));
			}
		}
	};
}();

dojo.kwCompoundRequire({
	common: [
		"dojo.crypto",
		"dojo.crypto.MD5"
	]
});
dojo.provide("dojo.crypto.*");

dojo.provide("dojo.collections.Collections");

dojo.collections={Collections:true};
dojo.collections.DictionaryEntry=function(/* string */k, /* object */v){
	//	summary
	//	return an object of type dojo.collections.DictionaryEntry
	this.key=k;
	this.value=v;
	this.valueOf=function(){ 
		return this.value; 	//	object
	};
	this.toString=function(){ 
		return String(this.value);	//	string 
	};
}

/*	Iterators
 *	The collections.Iterators (Iterator and DictionaryIterator) are built to
 *	work with the Collections included in this namespace.  However, they *can*
 *	be used with arrays and objects, respectively, should one choose to do so.
 */
dojo.collections.Iterator=function(/* array */arr){
	//	summary
	//	return an object of type dojo.collections.Iterator
	var a=arr;
	var position=0;
	this.element=a[position]||null;
	this.atEnd=function(){
		//	summary
		//	Test to see if the internal cursor has reached the end of the internal collection.
		return (position>=a.length);	//	bool
	};
	this.get=function(){
		//	summary
		//	Test to see if the internal cursor has reached the end of the internal collection.
		if(this.atEnd()){
			return null;		//	object
		}
		this.element=a[position++];
		return this.element;	//	object
	};
	this.map=function(/* function */fn, /* object? */scope){
		//	summary
		//	Functional iteration with optional scope.
		var s=scope||dj_global;
		if(Array.map){
			return Array.map(a,fn,s);	//	array
		}else{
			var arr=[];
			for(var i=0; i<a.length; i++){
				arr.push(fn.call(s,a[i]));
			}
			return arr;		//	array
		}
	};
	this.reset=function(){
		//	summary
		//	reset the internal cursor.
		position=0;
		this.element=a[position];
	};
}

/*	Notes:
 *	The DictionaryIterator no longer supports a key and value property;
 *	the reality is that you can use this to iterate over a JS object
 *	being used as a hashtable.
 */
dojo.collections.DictionaryIterator=function(/* object */obj){
	//	summary
	//	return an object of type dojo.collections.DictionaryIterator
	var a=[];	//	Create an indexing array
	for(var p in obj) {
		a.push(obj[p]);	//	fill it up
	}
	var position=0;
	this.element=a[position]||null;
	this.atEnd=function(){
		//	summary
		//	Test to see if the internal cursor has reached the end of the internal collection.
		return (position>=a.length);	//	bool
	};
	this.get=function(){
		//	summary
		//	Test to see if the internal cursor has reached the end of the internal collection.
		if(this.atEnd()){
			return null;		//	object
		}
		this.element=a[position++];
		return this.element;	//	object
	};
	this.map=function(/* function */fn, /* object? */scope){
		//	summary
		//	Functional iteration with optional scope.
		var s=scope||dj_global;
		if(Array.map){
			return Array.map(a,fn,s);	//	array
		}else{
			var arr=[];
			for(var i=0; i<a.length; i++){
				arr.push(fn.call(s,a[i]));
			}
			return arr;		//	array
		}
	};
	this.reset=function() { 
		//	summary
		//	reset the internal cursor.
		position=0; 
		this.element=a[position];
	};
};

dojo.provide("dojo.collections.ArrayList");
dojo.require("dojo.collections.Collections");

dojo.collections.ArrayList=function(/* array? */arr){
	//	summary
	//	Returns a new object of type dojo.collections.ArrayList
	var items=[];
	if(arr) items=items.concat(arr);
	this.count=items.length;
	this.add=function(/* object */obj){
		//	summary
		//	Add an element to the collection.
		items.push(obj);
		this.count=items.length;
	};
	this.addRange=function(/* array */a){
		//	summary
		//	Add a range of objects to the ArrayList
		if(a.getIterator){
			var e=a.getIterator();
			while(!e.atEnd()){
				this.add(e.get());
			}
			this.count=items.length;
		}else{
			for(var i=0; i<a.length; i++){
				items.push(a[i]);
			}
			this.count=items.length;
		}
	};
	this.clear=function(){
		//	summary
		//	Clear all elements out of the collection, and reset the count.
		items.splice(0, items.length);
		this.count=0;
	};
	this.clone=function(){
		//	summary
		//	Clone the array list
		return new dojo.collections.ArrayList(items);	//	dojo.collections.ArrayList
	};
	this.contains=function(/* object */obj){
		//	summary
		//	Check to see if the passed object is a member in the ArrayList
		for(var i=0; i < items.length; i++){
			if(items[i] == obj) {
				return true;	//	bool
			}
		}
		return false;	//	bool
	};
	this.forEach=function(/* function */ fn, /* object? */ scope){
		//	summary
		//	functional iterator, following the mozilla spec.
		var s=scope||dj_global;
		if(Array.forEach){
			Array.forEach(items, fn, s);
		}else{
			for(var i=0; i<items.length; i++){
				fn.call(s, items[i], i, items);
			}
		}
	};
	this.getIterator=function(){
		//	summary
		//	Get an Iterator for this object
		return new dojo.collections.Iterator(items);	//	dojo.collections.Iterator
	};
	this.indexOf=function(/* object */obj){
		//	summary
		//	Return the numeric index of the passed object; will return -1 if not found.
		for(var i=0; i < items.length; i++){
			if(items[i] == obj) {
				return i;	//	int
			}
		}
		return -1;	// int
	};
	this.insert=function(/* int */ i, /* object */ obj){
		//	summary
		//	Insert the passed object at index i
		items.splice(i,0,obj);
		this.count=items.length;
	};
	this.item=function(/* int */ i){
		//	summary
		//	return the element at index i
		return items[i];	//	object
	};
	this.remove=function(/* object */obj){
		//	summary
		//	Look for the passed object, and if found, remove it from the internal array.
		var i=this.indexOf(obj);
		if(i >=0) {
			items.splice(i,1);
		}
		this.count=items.length;
	};
	this.removeAt=function(/* int */ i){
		//	summary
		//	return an array with function applied to all elements
		items.splice(i,1);
		this.count=items.length;
	};
	this.reverse=function(){
		//	summary
		//	Reverse the internal array
		items.reverse();
	};
	this.sort=function(/* function? */ fn){
		//	summary
		//	sort the internal array
		if(fn){
			items.sort(fn);
		}else{
			items.sort();
		}
	};
	this.setByIndex=function(/* int */ i, /* object */ obj){
		//	summary
		//	Set an element in the array by the passed index.
		items[i]=obj;
		this.count=items.length;
	};
	this.toArray=function(){
		//	summary
		//	Return a new array with all of the items of the internal array concatenated.
		return [].concat(items);
	}
	this.toString=function(/* string */ delim){
		//	summary
		//	implementation of toString, follows [].toString();
		return items.join((delim||","));
	};
};

dojo.provide("dojo.collections.Queue");
dojo.require("dojo.collections.Collections");

dojo.collections.Queue=function(/* array? */arr){
	//	summary
	//	return an object of type dojo.collections.Queue
	var q=[];
	if (arr){
		q=q.concat(arr);
	}
	this.count=q.length;
	this.clear=function(){
		//	summary
		//	clears the internal collection
		q=[];
		this.count=q.length;
	};
	this.clone=function(){
		//	summary
		//	creates a new Queue based on this one
		return new dojo.collections.Queue(q);	//	dojo.collections.Queue
	};
	this.contains=function(/* object */ o){
		//	summary
		//	Check to see if the passed object is an element in this queue
		for(var i=0; i<q.length; i++){
			if (q[i]==o){
				return true;	//	bool
			}
		}
		return false;	//	bool
	};
	this.copyTo=function(/* array */ arr, /* int */ i){
		//	summary
		//	Copy the contents of this queue into the passed array at index i.
		arr.splice(i,0,q);
	};
	this.dequeue=function(){
		//	summary
		//	shift the first element off the queue and return it
		var r=q.shift();
		this.count=q.length;
		return r;	//	object
	};
	this.enqueue=function(/* object */ o){
		//	summary
		//	put the passed object at the end of the queue
		this.count=q.push(o);
	};
	this.forEach=function(/* function */ fn, /* object? */ scope){
		//	summary
		//	functional iterator, following the mozilla spec.
		var s=scope||dj_global;
		if(Array.forEach){
			Array.forEach(q, fn, s);
		}else{
			for(var i=0; i<items.length; i++){
				fn.call(s, q[i], i, q);
			}
		}
	};
	this.getIterator=function(){
		//	summary
		//	get an Iterator based on this queue.
		return new dojo.collections.Iterator(q);	//	dojo.collections.Iterator
	};
	this.peek=function(){
		//	summary
		//	get the next element in the queue without altering the queue.
		return q[0];
	};
	this.toArray=function(){
		//	summary
		//	return an array based on the internal array of the queue.
		return [].concat(q);
	};
};

dojo.provide("dojo.collections.Stack");
dojo.require("dojo.collections.Collections");

dojo.collections.Stack=function(/* array? */arr){
	//	summary
	//	returns an object of type dojo.collections.Stack
	var q=[];
	if (arr) q=q.concat(arr);
	this.count=q.length;
	this.clear=function(){
		//	summary
		//	Clear the internal array and reset the count
		q=[];
		this.count=q.length;
	};
	this.clone=function(){
		//	summary
		//	Create and return a clone of this Stack
		return new dojo.collections.Stack(q);
	};
	this.contains=function(/* object */o){
		//	summary
		//	check to see if the stack contains object o
		for (var i=0; i<q.length; i++){
			if (q[i] == o){
				return true;	//	bool
			}
		}
		return false;	//	bool
	};
	this.copyTo=function(/* array */ arr, /* int */ i){
		//	summary
		//	copy the stack into array arr at index i
		arr.splice(i,0,q);
	};
	this.forEach=function(/* function */ fn, /* object? */ scope){
		//	summary
		//	functional iterator, following the mozilla spec.
		var s=scope||dj_global;
		if(Array.forEach){
			Array.forEach(q, fn, s);
		}else{
			for(var i=0; i<items.length; i++){
				fn.call(s, q[i], i, q);
			}
		}
	};
	this.getIterator=function(){
		//	summary
		//	get an iterator for this collection
		return new dojo.collections.Iterator(q);	//	dojo.collections.Iterator
	};
	this.peek=function(){
		//	summary
		//	Return the next item without altering the stack itself.
		return q[(q.length-1)];	//	object
	};
	this.pop=function(){
		//	summary
		//	pop and return the next item on the stack
		var r=q.pop();
		this.count=q.length;
		return r;	//	object
	};
	this.push=function(/* object */ o){
		//	summary
		//	Push object o onto the stack
		this.count=q.push(o);
	};
	this.toArray=function(){
		//	summary
		//	create and return an array based on the internal collection
		return [].concat(q);	//	array
	};
}

dojo.require("dojo.lang");
dojo.provide("dojo.dnd.DragSource");
dojo.provide("dojo.dnd.DropTarget");
dojo.provide("dojo.dnd.DragObject");
dojo.provide("dojo.dnd.DragAndDrop");

dojo.dnd.DragSource = function(){
	var dm = dojo.dnd.dragManager;
	if(dm["registerDragSource"]){ // side-effect prevention
		dm.registerDragSource(this);
	}
}

dojo.lang.extend(dojo.dnd.DragSource, {
	type: "",

	onDragEnd: function(){
	},

	onDragStart: function(){
	},

	unregister: function(){
		dojo.dnd.dragManager.unregisterDragSource(this);
	},

	reregister: function(){
		dojo.dnd.dragManager.registerDragSource(this);
	}
});

dojo.dnd.DragObject = function(){
	var dm = dojo.dnd.dragManager;
	if(dm["registerDragObject"]){ // side-effect prevention
		dm.registerDragObject(this);
	}
}

dojo.lang.extend(dojo.dnd.DragObject, {
	type: "",

	onDragStart: function(){
		// gets called directly after being created by the DragSource
		// default action is to clone self as icon
	},

	onDragMove: function(){
		// this changes the UI for the drag icon
		//	"it moves itself"
	},

	onDragOver: function(){
	},

	onDragOut: function(){
	},

	onDragEnd: function(){
	},

	// normal aliases
	onDragLeave: this.onDragOut,
	onDragEnter: this.onDragOver,

	// non-camel aliases
	ondragout: this.onDragOut,
	ondragover: this.onDragOver
});

dojo.dnd.DropTarget = function(){
	if (this.constructor == dojo.dnd.DropTarget) { return; } // need to be subclassed
	this.acceptedTypes = [];
	dojo.dnd.dragManager.registerDropTarget(this);
}

dojo.lang.extend(dojo.dnd.DropTarget, {

	acceptsType: function(type){
		if(!dojo.lang.inArray(this.acceptedTypes, "*")){ // wildcard
			if(!dojo.lang.inArray(this.acceptedTypes, type)) { return false; }
		}
		return true;
	},

	accepts: function(dragObjects){
		if(!dojo.lang.inArray(this.acceptedTypes, "*")){ // wildcard
			for (var i = 0; i < dragObjects.length; i++) {
				if (!dojo.lang.inArray(this.acceptedTypes,
					dragObjects[i].type)) { return false; }
			}
		}
		return true;
	},

	onDragOver: function(){
	},

	onDragOut: function(){
	},

	onDragMove: function(){
	},

	onDropStart: function(){
	},

	onDrop: function(){
	},

	onDropEnd: function(){
	}
});

// NOTE: this interface is defined here for the convenience of the DragManager
// implementor. It is expected that in most cases it will be satisfied by
// extending a native event (DOM event in HTML and SVG).
dojo.dnd.DragEvent = function(){
	this.dragSource = null;
	this.dragObject = null;
	this.target = null;
	this.eventStatus = "success";
	//
	// can be one of:
	//	[	"dropSuccess", "dropFailure", "dragMove",
	//		"dragStart", "dragEnter", "dragLeave"]
	//
}

dojo.dnd.DragManager = function(){
	/*
	 *	The DragManager handles listening for low-level events and dispatching
	 *	them to higher-level primitives like drag sources and drop targets. In
	 *	order to do this, it must keep a list of the items.
	 */
}

dojo.lang.extend(dojo.dnd.DragManager, {
	selectedSources: [],
	dragObjects: [],
	dragSources: [],
	registerDragSource: function(){},
	dropTargets: [],
	registerDropTarget: function(){},
	lastDragTarget: null,
	currentDragTarget: null,
	onKeyDown: function(){},
	onMouseOut: function(){},
	onMouseMove: function(){},
	onMouseUp: function(){}
});

// NOTE: despite the existance of the DragManager class, there will be a
// singleton drag manager provided by the renderer-specific D&D support code.
// It is therefore sane for us to assign instance variables to the DragManager
// prototype

// The renderer-specific file will define the following object:
// dojo.dnd.dragManager = null;

dojo.provide("dojo.dnd.HtmlDragManager");
dojo.require("dojo.dnd.DragAndDrop");
dojo.require("dojo.event.*");
dojo.require("dojo.lang.array");
dojo.require("dojo.html");
dojo.require("dojo.style");

// NOTE: there will only ever be a single instance of HTMLDragManager, so it's
// safe to use prototype properties for book-keeping.
dojo.dnd.HtmlDragManager = function(){
}

dojo.inherits(dojo.dnd.HtmlDragManager, dojo.dnd.DragManager);

dojo.lang.extend(dojo.dnd.HtmlDragManager, {
	/**
	 * There are several sets of actions that the DnD code cares about in the
	 * HTML context:
	 *	1.) mouse-down ->
	 *			(draggable selection)
	 *			(dragObject generation)
	 *		mouse-move ->
	 *			(draggable movement)
	 *			(droppable detection)
	 *			(inform droppable)
	 *			(inform dragObject)
	 *		mouse-up
	 *			(inform/destroy dragObject)
	 *			(inform draggable)
	 *			(inform droppable)
	 *	2.) mouse-down -> mouse-down
	 *			(click-hold context menu)
	 *	3.) mouse-click ->
	 *			(draggable selection)
	 *		shift-mouse-click ->
	 *			(augment draggable selection)
	 *		mouse-down ->
	 *			(dragObject generation)
	 *		mouse-move ->
	 *			(draggable movement)
	 *			(droppable detection)
	 *			(inform droppable)
	 *			(inform dragObject)
	 *		mouse-up
	 *			(inform draggable)
	 *			(inform droppable)
	 *	4.) mouse-up
	 *			(clobber draggable selection)
	 */
	disabled: false, // to kill all dragging!
	nestedTargets: false,
	mouseDownTimer: null, // used for click-hold operations
	dsCounter: 0,
	dsPrefix: "dojoDragSource",

	// dimension calculation cache for use durring drag
	dropTargetDimensions: [],

	currentDropTarget: null,
	// currentDropTargetPoints: null,
	previousDropTarget: null,
	_dragTriggered: false,

	selectedSources: [],
	dragObjects: [],

	// mouse position properties
	currentX: null,
	currentY: null,
	lastX: null,
	lastY: null,
	mouseDownX: null,
	mouseDownY: null,
	threshold: 7,

	dropAcceptable: false,

	cancelEvent: function(e){ e.stopPropagation(); e.preventDefault();},

	// method over-rides
	registerDragSource: function(ds){
		if(ds["domNode"]){
			// FIXME: dragSource objects SHOULD have some sort of property that
			// references their DOM node, we shouldn't just be passing nodes and
			// expecting it to work.
			var dp = this.dsPrefix;
			var dpIdx = dp+"Idx_"+(this.dsCounter++);
			ds.dragSourceId = dpIdx;
			this.dragSources[dpIdx] = ds;
			ds.domNode.setAttribute(dp, dpIdx);

			// so we can drag links
			if(dojo.render.html.ie){
				dojo.event.connect(ds.domNode, "ondragstart", this.cancelEvent);
			}
		}
	},

	unregisterDragSource: function(ds){
		if (ds["domNode"]){

			var dp = this.dsPrefix;
			var dpIdx = ds.dragSourceId;
			delete ds.dragSourceId;
			delete this.dragSources[dpIdx];
			ds.domNode.setAttribute(dp, null);
		}
		if(dojo.render.html.ie){
			dojo.event.disconnect(ds.domNode, "ondragstart", this.cancelEvent );
		}
	},

	registerDropTarget: function(dt){
		this.dropTargets.push(dt);
	},

	unregisterDropTarget: function(dt){
		var index = dojo.lang.find(this.dropTargets, dt, true);
		if (index>=0) {
			this.dropTargets.splice(index, 1);
		}
	},

	getDragSource: function(e){
		var tn = e.target;
		if(tn === document.body){ return; }
		var ta = dojo.html.getAttribute(tn, this.dsPrefix);
		while((!ta)&&(tn)){
			tn = tn.parentNode;
			if((!tn)||(tn === document.body)){ return; }
			ta = dojo.html.getAttribute(tn, this.dsPrefix);
		}
		return this.dragSources[ta];
	},

	onKeyDown: function(e){
	},

	onMouseDown: function(e){
		if(this.disabled) { return; }

		// only begin on left click
		if(dojo.render.html.ie) {
			if(e.button != 1) { return; }
		} else if(e.which != 1) {
			return;
		}

		var target = e.target.nodeType == dojo.dom.TEXT_NODE ?
			e.target.parentNode : e.target;

		// do not start drag involvement if the user is interacting with
		// a form element.
		if(dojo.html.isTag(target, "button", "textarea", "input", "select", "option")) {
			return;
		}

		// find a selection object, if one is a parent of the source node
		var ds = this.getDragSource(e);
		
		// this line is important.  if we aren't selecting anything then
		// we need to return now, so preventDefault() isn't called, and thus
		// the event is propogated to other handling code
		if(!ds){ return; }

		if(!dojo.lang.inArray(this.selectedSources, ds)){
			this.selectedSources.push(ds);
		}

 		this.mouseDownX = e.pageX;
 		this.mouseDownY = e.pageY;

		// Must stop the mouse down from being propogated, or otherwise can't
		// drag links in firefox.
		// WARNING: preventing the default action on all mousedown events
		// prevents user interaction with the contents.
		e.preventDefault();

		dojo.event.connect(document, "onmousemove", this, "onMouseMove");
	},

	onMouseUp: function(e, cancel){
		// if we aren't dragging then ignore the mouse-up
		// (in particular, don't call preventDefault(), because other
		// code may need to process this event)
		if(this.selectedSources.length==0){
			return;
		}

		this.mouseDownX = null;
		this.mouseDownY = null;
		this._dragTriggered = false;
 		// e.preventDefault();
		e.dragSource = this.dragSource;
		if((!e.shiftKey)&&(!e.ctrlKey)){
			if(this.currentDropTarget) {
				this.currentDropTarget.onDropStart();
			}
			dojo.lang.forEach(this.dragObjects, function(tempDragObj){
				var ret = null;
				if(!tempDragObj){ return; }
				if(this.currentDropTarget) {
					e.dragObject = tempDragObj;

					// NOTE: we can't get anything but the current drop target
					// here since the drag shadow blocks mouse-over events.
					// This is probelematic for dropping "in" something
					var ce = this.currentDropTarget.domNode.childNodes;
					if(ce.length > 0){
						e.dropTarget = ce[0];
						while(e.dropTarget == tempDragObj.domNode){
							e.dropTarget = e.dropTarget.nextSibling;
						}
					}else{
						e.dropTarget = this.currentDropTarget.domNode;
					}
					if(this.dropAcceptable){
						ret = this.currentDropTarget.onDrop(e);
					}else{
						 this.currentDropTarget.onDragOut(e);
					}
				}

				e.dragStatus = this.dropAcceptable && ret ? "dropSuccess" : "dropFailure";
				tempDragObj.dragSource.onDragEnd(e);
				tempDragObj.onDragEnd(e);
			}, this);

			this.selectedSources = [];
			this.dragObjects = [];
			this.dragSource = null;
			if(this.currentDropTarget) {
				this.currentDropTarget.onDropEnd();
			}
		}

		dojo.event.disconnect(document, "onmousemove", this, "onMouseMove");
		this.currentDropTarget = null;
	},

	onScroll: function(){
		for(var i = 0; i < this.dragObjects.length; i++) {
			if(this.dragObjects[i].updateDragOffset) {
				this.dragObjects[i].updateDragOffset();
			}
		}
		// TODO: do not recalculate, only adjust coordinates
		this.cacheTargetLocations();
	},

	_dragStartDistance: function(x, y){
		if((!this.mouseDownX)||(!this.mouseDownX)){
			return;
		}
		var dx = Math.abs(x-this.mouseDownX);
		var dx2 = dx*dx;
		var dy = Math.abs(y-this.mouseDownY);
		var dy2 = dy*dy;
		return parseInt(Math.sqrt(dx2+dy2), 10);
	},

	cacheTargetLocations: function(){
		this.dropTargetDimensions = [];
		dojo.lang.forEach(this.dropTargets, function(tempTarget){
			var tn = tempTarget.domNode;
			if(!tn){ return; }
			var ttx = dojo.style.getAbsoluteX(tn, true);
			var tty = dojo.style.getAbsoluteY(tn, true);
			this.dropTargetDimensions.push([
				[ttx, tty],	// upper-left
				// lower-right
				[ ttx+dojo.style.getInnerWidth(tn), tty+dojo.style.getInnerHeight(tn) ],
				tempTarget
			]);
			//dojo.debug("Cached for "+tempTarget)
		}, this);
		//dojo.debug("Cache locations")
	},

	onMouseMove: function(e){
		if((dojo.render.html.ie)&&(e.button != 1)){
			// Oooops - mouse up occurred - e.g. when mouse was not over the
			// window. I don't think we can detect this for FF - but at least
			// we can be nice in IE.
			this.currentDropTarget = null;
			this.onMouseUp(e, true);
			return;
		}

		// if we've got some sources, but no drag objects, we need to send
		// onDragStart to all the right parties and get things lined up for
		// drop target detection

		if(	(this.selectedSources.length)&&
			(!this.dragObjects.length) ){
			var dx;
			var dy;
			if(!this._dragTriggered){
				this._dragTriggered = (this._dragStartDistance(e.pageX, e.pageY) > this.threshold);
				if(!this._dragTriggered){ return; }
				dx = e.pageX - this.mouseDownX;
				dy = e.pageY - this.mouseDownY;
			}

			if (this.selectedSources.length == 1) {
				this.dragSource = this.selectedSources[0];
			}

			dojo.lang.forEach(this.selectedSources, function(tempSource){
				if(!tempSource){ return; }
				var tdo = tempSource.onDragStart(e);
				if(tdo){
					tdo.onDragStart(e);

					// "bump" the drag object to account for the drag threshold
					tdo.dragOffset.top += dy;
					tdo.dragOffset.left += dx;
					tdo.dragSource = tempSource;

					this.dragObjects.push(tdo);
				}
			}, this);

			/* clean previous drop target in dragStart */
			this.previousDropTarget = null;

			this.cacheTargetLocations();
		}

		// FIXME: we need to add dragSources and dragObjects to e
		dojo.lang.forEach(this.dragObjects, function(dragObj){
			if(dragObj){ dragObj.onDragMove(e); }
		});

		// if we have a current drop target, check to see if we're outside of
		// it. If so, do all the actions that need doing.
		if(this.currentDropTarget){
			//dojo.debug(dojo.dom.hasParent(this.currentDropTarget.domNode))
			var c = dojo.style.toCoordinateArray(this.currentDropTarget.domNode, true);
			//		var dtp = this.currentDropTargetPoints;
			var dtp = [
				[c[0],c[1]], [c[0]+c[2], c[1]+c[3]]
			];
		}

		if((!this.nestedTargets)&&(dtp)&&(this.isInsideBox(e, dtp))){
			if(this.dropAcceptable){
				this.currentDropTarget.onDragMove(e, this.dragObjects);
			}
		}else{
			// FIXME: need to fix the event object!
			// see if we can find a better drop target
			var bestBox = this.findBestTarget(e);

			if(bestBox.target === null){
				if(this.currentDropTarget){
					this.currentDropTarget.onDragOut(e);
					this.previousDropTarget = this.currentDropTarget;
					this.currentDropTarget = null;
					// this.currentDropTargetPoints = null;
				}
				this.dropAcceptable = false;
				return;
			}

			if(this.currentDropTarget !== bestBox.target){
				if(this.currentDropTarget){
					this.previousDropTarget = this.currentDropTarget;
					this.currentDropTarget.onDragOut(e);
				}
				this.currentDropTarget = bestBox.target;
				// this.currentDropTargetPoints = bestBox.points;
				e.dragObjects = this.dragObjects;
				this.dropAcceptable = this.currentDropTarget.onDragOver(e);

			}else{
				if(this.dropAcceptable){
					this.currentDropTarget.onDragMove(e, this.dragObjects);
				}
			}
		}
	},

	findBestTarget: function(e) {
		var _this = this;
		var bestBox = new Object();
		bestBox.target = null;
		bestBox.points = null;
		dojo.lang.every(this.dropTargetDimensions, function(tmpDA) {
			if(!_this.isInsideBox(e, tmpDA))
				return true;
			bestBox.target = tmpDA[2];
			bestBox.points = tmpDA;
			// continue iterating only if _this.nestedTargets == true
			return Boolean(_this.nestedTargets);
		});

		return bestBox;
	},

	isInsideBox: function(e, coords){
		if(	(e.pageX > coords[0][0])&&
			(e.pageX < coords[1][0])&&
			(e.pageY > coords[0][1])&&
			(e.pageY < coords[1][1]) ){
			return true;
		}
		return false;
	},

	onMouseOver: function(e){
	},

	onMouseOut: function(e){
	}
});

dojo.dnd.dragManager = new dojo.dnd.HtmlDragManager();

// global namespace protection closure
(function(){
	var d = document;
	var dm = dojo.dnd.dragManager;
	// set up event handlers on the document
	dojo.event.connect(d, "onkeydown", 		dm, "onKeyDown");
	dojo.event.connect(d, "onmouseover",	dm, "onMouseOver");
	dojo.event.connect(d, "onmouseout", 	dm, "onMouseOut");
	dojo.event.connect(d, "onmousedown",	dm, "onMouseDown");
	dojo.event.connect(d, "onmouseup",		dm, "onMouseUp");
	// TODO: process scrolling of elements, not only window
	dojo.event.connect(window, "onscroll",	dm, "onScroll");
})();

dojo.require("dojo.html");
dojo.provide("dojo.html.extras");
dojo.require("dojo.string.extras"); 

/**
 * Calculates the mouse's direction of gravity relative to the centre
 * of the given node.
 * <p>
 * If you wanted to insert a node into a DOM tree based on the mouse
 * position you might use the following code:
 * <pre>
 * if (gravity(node, e) & gravity.NORTH) { [insert before]; }
 * else { [insert after]; }
 * </pre>
 *
 * @param node The node
 * @param e		The event containing the mouse coordinates
 * @return		 The directions, NORTH or SOUTH and EAST or WEST. These
 *						 are properties of the function.
 */
dojo.html.gravity = function(node, e){
	node = dojo.byId(node);
	var mouse = dojo.html.getCursorPosition(e);

	with (dojo.html) {
		var nodecenterx = getAbsoluteX(node, true) + (getInnerWidth(node) / 2);
		var nodecentery = getAbsoluteY(node, true) + (getInnerHeight(node) / 2);
	}
	
	with (dojo.html.gravity) {
		return ((mouse.x < nodecenterx ? WEST : EAST) |
			(mouse.y < nodecentery ? NORTH : SOUTH));
	}
}

dojo.html.gravity.NORTH = 1;
dojo.html.gravity.SOUTH = 1 << 1;
dojo.html.gravity.EAST = 1 << 2;
dojo.html.gravity.WEST = 1 << 3;


/**
 * Attempts to return the text as it would be rendered, with the line breaks
 * sorted out nicely. Unfinished.
 */
dojo.html.renderedTextContent = function(node){
	node = dojo.byId(node);
	var result = "";
	if (node == null) { return result; }
	for (var i = 0; i < node.childNodes.length; i++) {
		switch (node.childNodes[i].nodeType) {
			case 1: // ELEMENT_NODE
			case 5: // ENTITY_REFERENCE_NODE
				var display = "unknown";
				try {
					display = dojo.style.getStyle(node.childNodes[i], "display");
				} catch(E) {}
				switch (display) {
					case "block": case "list-item": case "run-in":
					case "table": case "table-row-group": case "table-header-group":
					case "table-footer-group": case "table-row": case "table-column-group":
					case "table-column": case "table-cell": case "table-caption":
						// TODO: this shouldn't insert double spaces on aligning blocks
						result += "\n";
						result += dojo.html.renderedTextContent(node.childNodes[i]);
						result += "\n";
						break;
					
					case "none": break;
					
					default:
						if(node.childNodes[i].tagName && node.childNodes[i].tagName.toLowerCase() == "br") {
							result += "\n";
						} else {
							result += dojo.html.renderedTextContent(node.childNodes[i]);
						}
						break;
				}
				break;
			case 3: // TEXT_NODE
			case 2: // ATTRIBUTE_NODE
			case 4: // CDATA_SECTION_NODE
				var text = node.childNodes[i].nodeValue;
				var textTransform = "unknown";
				try {
					textTransform = dojo.style.getStyle(node, "text-transform");
				} catch(E) {}
				switch (textTransform){
					case "capitalize": text = dojo.string.capitalize(text); break;
					case "uppercase": text = text.toUpperCase(); break;
					case "lowercase": text = text.toLowerCase(); break;
					default: break; // leave as is
				}
				// TODO: implement
				switch (textTransform){
					case "nowrap": break;
					case "pre-wrap": break;
					case "pre-line": break;
					case "pre": break; // leave as is
					default:
						// remove whitespace and collapse first space
						text = text.replace(/\s+/, " ");
						if (/\s$/.test(result)) { text.replace(/^\s/, ""); }
						break;
				}
				result += text;
				break;
			default:
				break;
		}
	}
	return result;
}

dojo.html.createNodesFromText = function(txt, trim){
	if(trim) { txt = dojo.string.trim(txt); }

	var tn = document.createElement("div");
	// tn.style.display = "none";
	tn.style.visibility= "hidden";
	document.body.appendChild(tn);
	var tableType = "none";
	if((/^<t[dh][\s\r\n>]/i).test(dojo.string.trimStart(txt))) {
		txt = "<table><tbody><tr>" + txt + "</tr></tbody></table>";
		tableType = "cell";
	} else if((/^<tr[\s\r\n>]/i).test(dojo.string.trimStart(txt))) {
		txt = "<table><tbody>" + txt + "</tbody></table>";
		tableType = "row";
	} else if((/^<(thead|tbody|tfoot)[\s\r\n>]/i).test(dojo.string.trimStart(txt))) {
		txt = "<table>" + txt + "</table>";
		tableType = "section";
	}
	tn.innerHTML = txt;
	if(tn["normalize"]){
		tn.normalize();
	}

	var parent = null;
	switch(tableType) {
		case "cell":
			parent = tn.getElementsByTagName("tr")[0];
			break;
		case "row":
			parent = tn.getElementsByTagName("tbody")[0];
			break;
		case "section":
			parent = tn.getElementsByTagName("table")[0];
			break;
		default:
			parent = tn;
			break;
	}

	/* this doesn't make much sense, I'm assuming it just meant trim() so wrap was replaced with trim
	if(wrap){ 
		var ret = [];
		// start hack
		var fc = tn.firstChild;
		ret[0] = ((fc.nodeValue == " ")||(fc.nodeValue == "\t")) ? fc.nextSibling : fc;
		// end hack
		// tn.style.display = "none";
		document.body.removeChild(tn);
		return ret;
	}
	*/
	var nodes = [];
	for(var x=0; x<parent.childNodes.length; x++){
		nodes.push(parent.childNodes[x].cloneNode(true));
	}
	tn.style.display = "none"; // FIXME: why do we do this?
	document.body.removeChild(tn);
	return nodes;
}

/* TODO: merge placeOnScreen and placeOnScreenPoint to make 1 function that allows you
 * to define which corner(s) you want to bind to. Something like so:
 *
 * kes(node, desiredX, desiredY, "TR")
 * kes(node, [desiredX, desiredY], ["TR", "BL"])
 *
 * TODO: make this function have variable call sigs
 *
 * kes(node, ptArray, cornerArray, padding, hasScroll)
 * kes(node, ptX, ptY, cornerA, cornerB, cornerC, paddingArray, hasScroll)
 */

/**
 * Keeps 'node' in the visible area of the screen while trying to
 * place closest to desiredX, desiredY. The input coordinates are
 * expected to be the desired screen position, not accounting for
 * scrolling. If you already accounted for scrolling, set 'hasScroll'
 * to true. Set padding to either a number or array for [paddingX, paddingY]
 * to put some buffer around the element you want to position.
 * NOTE: node is assumed to be absolutely or relatively positioned.
 *
 * Alternate call sig:
 *  placeOnScreen(node, [x, y], padding, hasScroll)
 *
 * Examples:
 *  placeOnScreen(node, 100, 200)
 *  placeOnScreen("myId", [800, 623], 5)
 *  placeOnScreen(node, 234, 3284, [2, 5], true)
 */
dojo.html.placeOnScreen = function(node, desiredX, desiredY, padding, hasScroll) {
	if(dojo.lang.isArray(desiredX)) {
		hasScroll = padding;
		padding = desiredY;
		desiredY = desiredX[1];
		desiredX = desiredX[0];
	}

	if(!isNaN(padding)) {
		padding = [Number(padding), Number(padding)];
	} else if(!dojo.lang.isArray(padding)) {
		padding = [0, 0];
	}

	var scroll = dojo.html.getScrollOffset();
	var view = dojo.html.getViewportSize();

	node = dojo.byId(node);
	var w = node.offsetWidth + padding[0];
	var h = node.offsetHeight + padding[1];

	if(hasScroll) {
		desiredX -= scroll.x;
		desiredY -= scroll.y;
	}

	var x = desiredX + w;
	if(x > view.w) {
		x = view.w - w;
	} else {
		x = desiredX;
	}
	x = Math.max(padding[0], x) + scroll.x;

	var y = desiredY + h;
	if(y > view.h) {
		y = view.h - h;
	} else {
		y = desiredY;
	}
	y = Math.max(padding[1], y) + scroll.y;

	node.style.left = x + "px";
	node.style.top = y + "px";

	var ret = [x, y];
	ret.x = x;
	ret.y = y;
	return ret;
}

/**
 * Like placeOnScreenPoint except that it attempts to keep one of the node's
 * corners at desiredX, desiredY.  Favors the bottom right position
 *
 * Examples placing node at mouse position (where e = [Mouse event]):
 *  placeOnScreenPoint(node, e.clientX, e.clientY);
 */
dojo.html.placeOnScreenPoint = function(node, desiredX, desiredY, padding, hasScroll) {
	if(dojo.lang.isArray(desiredX)) {
		hasScroll = padding;
		padding = desiredY;
		desiredY = desiredX[1];
		desiredX = desiredX[0];
	}

	if(!isNaN(padding)) {
		padding = [Number(padding), Number(padding)];
	} else if(!dojo.lang.isArray(padding)) {
		padding = [0, 0];
	}

	var scroll = dojo.html.getScrollOffset();
	var view = dojo.html.getViewportSize();

	node = dojo.byId(node);
	var oldDisplay = node.style.display;
	node.style.display="";
	var w = dojo.style.getInnerWidth(node);
	var h = dojo.style.getInnerHeight(node);
	node.style.display=oldDisplay;

	if(hasScroll) {
		desiredX -= scroll.x;
		desiredY -= scroll.y;
	}

	var x = -1, y = -1;
	//dojo.debug((desiredX+padding[0]) + w, "<=", view.w, "&&", (desiredY+padding[1]) + h, "<=", view.h);
	if((desiredX+padding[0]) + w <= view.w && (desiredY+padding[1]) + h <= view.h) { // TL
		x = (desiredX+padding[0]);
		y = (desiredY+padding[1]);
		//dojo.debug("TL", x, y);
	}

	//dojo.debug((desiredX-padding[0]), "<=", view.w, "&&", (desiredY+padding[1]) + h, "<=", view.h);
	if((x < 0 || y < 0) && (desiredX-padding[0]) <= view.w && (desiredY+padding[1]) + h <= view.h) { // TR
		x = (desiredX-padding[0]) - w;
		y = (desiredY+padding[1]);
		//dojo.debug("TR", x, y);
	}

	//dojo.debug((desiredX+padding[0]) + w, "<=", view.w, "&&", (desiredY-padding[1]), "<=", view.h);
	if((x < 0 || y < 0) && (desiredX+padding[0]) + w <= view.w && (desiredY-padding[1]) <= view.h) { // BL
		x = (desiredX+padding[0]);
		y = (desiredY-padding[1]) - h;
		//dojo.debug("BL", x, y);
	}

	//dojo.debug((desiredX-padding[0]), "<=", view.w, "&&", (desiredY-padding[1]), "<=", view.h);
	if((x < 0 || y < 0) && (desiredX-padding[0]) <= view.w && (desiredY-padding[1]) <= view.h) { // BR
		x = (desiredX-padding[0]) - w;
		y = (desiredY-padding[1]) - h;
		//dojo.debug("BR", x, y);
	}

	if(x < 0 || y < 0 || (x + w > view.w) || (y + h > view.h)) {
		return dojo.html.placeOnScreen(node, desiredX, desiredY, padding, hasScroll);
	}

	x += scroll.x;
	y += scroll.y;

	node.style.left = x + "px";
	node.style.top = y + "px";

	var ret = [x, y];
	ret.x = x;
	ret.y = y;
	return ret;
}

/**
 * For IE z-index schenanigans
 * Two possible uses:
 *   1. new dojo.html.BackgroundIframe(node)
 *        Makes a background iframe as a child of node, that fills area (and position) of node
 *
 *   2. new dojo.html.BackgroundIframe()
 *        Attaches frame to document.body.  User must call size() to set size.
 */
dojo.html.BackgroundIframe = function(node) {
	if(dojo.render.html.ie) {
		var html=
				 "<iframe "
				+"style='position: absolute; left: 0px; top: 0px; width: 100%; height: 100%;"
				+        "z-index: -1; filter:Alpha(Opacity=\"0\");' "
				+">";
		this.iframe = document.createElement(html);
		if(node){
			node.appendChild(this.iframe);
			this.domNode=node;
		}else{
			document.body.appendChild(this.iframe);
			this.iframe.style.display="none";
		}
	}
}
dojo.lang.extend(dojo.html.BackgroundIframe, {
	iframe: null,

	// TODO: this function shouldn't be necessary but setting width=height=100% doesn't work!
	onResized: function(){
		if(this.iframe && this.domNode){
			var w = dojo.style.getOuterWidth(this.domNode);
			var h = dojo.style.getOuterHeight(this.domNode);
			if (w  == 0 || h == 0 ){
				dojo.lang.setTimeout(this, this.onResized, 50);
				return;
			}
			var s = this.iframe.style;
			s.width = w + "px";
			s.height = h + "px";
		}
	},

	// Call this function if the iframe is connected to document.body rather
	// than the node being shadowed (TODO: erase)
	size: function(node) {
		if(!this.iframe) { return; }

		coords = dojo.style.toCoordinateArray(node, true);

		var s = this.iframe.style;
		s.width = coords.w + "px";
		s.height = coords.h + "px";
		s.left = coords.x + "px";
		s.top = coords.y + "px";
	},

	setZIndex: function(node /* or number */) {
		if(!this.iframe) { return; }

		if(dojo.dom.isNode(node)) {
			this.iframe.style.zIndex = dojo.html.getStyle(node, "z-index") - 1;
		} else if(!isNaN(node)) {
			this.iframe.style.zIndex = node;
		}
	},

	show: function() {
		if(!this.iframe) { return; }
		this.iframe.style.display = "block";
	},

	hide: function() {
		if(!this.ie) { return; }
		var s = this.iframe.style;
		s.display = "none";
	},

	remove: function() {
		dojo.dom.removeNode(this.iframe);
	}
});

dojo.provide("dojo.dnd.HtmlDragAndDrop");
dojo.provide("dojo.dnd.HtmlDragSource");
dojo.provide("dojo.dnd.HtmlDropTarget");
dojo.provide("dojo.dnd.HtmlDragObject");

dojo.require("dojo.dnd.HtmlDragManager");
dojo.require("dojo.dnd.DragAndDrop");

dojo.require("dojo.dom");
dojo.require("dojo.style");
dojo.require("dojo.html");
dojo.require("dojo.html.extras");
dojo.require("dojo.lang.extras");
dojo.require("dojo.lfx.*");

dojo.dnd.HtmlDragSource = function(node, type){
	node = dojo.byId(node);
	this.constrainToContainer = false;
	if(node){
		this.domNode = node;
		this.dragObject = node;

		// register us
		dojo.dnd.DragSource.call(this);

		// set properties that might have been clobbered by the mixin
		this.type = type||this.domNode.nodeName.toLowerCase();
	}

}

dojo.inherits(dojo.dnd.HtmlDragSource, dojo.dnd.DragSource);

dojo.lang.extend(dojo.dnd.HtmlDragSource, {
	dragClass: "", // CSS classname(s) applied to node when it is being dragged

	onDragStart: function(){
		var dragObj = new dojo.dnd.HtmlDragObject(this.dragObject, this.type);
		if(this.dragClass) { dragObj.dragClass = this.dragClass; }

		if (this.constrainToContainer) {
			dragObj.constrainTo(this.constrainingContainer || this.domNode.parentNode);
		}

		return dragObj;
	},

	setDragHandle: function(node){
		node = dojo.byId(node);
		dojo.dnd.dragManager.unregisterDragSource(this);
		this.domNode = node;
		dojo.dnd.dragManager.registerDragSource(this);
	},

	setDragTarget: function(node){
		this.dragObject = node;
	},

	constrainTo: function(container) {
		this.constrainToContainer = true;
		if (container) {
			this.constrainingContainer = container;
		}
	}
});

dojo.dnd.HtmlDragObject = function(node, type){
	this.domNode = dojo.byId(node);
	this.type = type;
	this.constrainToContainer = false;
	this.dragSource = null;
}

dojo.inherits(dojo.dnd.HtmlDragObject, dojo.dnd.DragObject);

dojo.lang.extend(dojo.dnd.HtmlDragObject, {
	dragClass: "",
	opacity: 0.5,
	createIframe: true,		// workaround IE6 bug

	// if true, node will not move in X and/or Y direction
	disableX: false,
	disableY: false,

	createDragNode: function() {
		var node = this.domNode.cloneNode(true);
		if(this.dragClass) { dojo.html.addClass(node, this.dragClass); }
		if(this.opacity < 1) { dojo.style.setOpacity(node, this.opacity); }
		if(dojo.render.html.ie && this.createIframe){
			with(node.style) {
				top="0px";
				left="0px";
			}
			var outer = document.createElement("div");
			outer.appendChild(node);
			this.bgIframe = new dojo.html.BackgroundIframe(outer);
			outer.appendChild(this.bgIframe.iframe);
			node = outer;
		}
		node.style.zIndex = 999;
		return node;
	},

	onDragStart: function(e){
		dojo.html.clearSelection();

		this.scrollOffset = dojo.html.getScrollOffset();
		this.dragStartPosition = dojo.style.getAbsolutePosition(this.domNode, true);

		this.dragOffset = {y: this.dragStartPosition.y - e.pageY,
			x: this.dragStartPosition.x - e.pageX};

		this.dragClone = this.createDragNode();

 		if ((this.domNode.parentNode.nodeName.toLowerCase() == 'body') || (dojo.style.getComputedStyle(this.domNode.parentNode,"position") == "static")) {
			this.parentPosition = {y: 0, x: 0};
		} else {
			this.parentPosition = dojo.style.getAbsolutePosition(this.domNode.parentNode, true);
		}

		if (this.constrainToContainer) {
			this.constraints = this.getConstraints();
		}

		// set up for dragging
		with(this.dragClone.style){
			position = "absolute";
			top = this.dragOffset.y + e.pageY + "px";
			left = this.dragOffset.x + e.pageX + "px";
		}

		document.body.appendChild(this.dragClone);
	},

	getConstraints: function() {

		if (this.constrainingContainer.nodeName.toLowerCase() == 'body') {
			width = dojo.html.getViewportWidth();
			height = dojo.html.getViewportHeight();
			padLeft = 0;
			padTop = 0;
		} else {
			width = dojo.style.getContentWidth(this.constrainingContainer);
			height = dojo.style.getContentHeight(this.constrainingContainer);
			padLeft = dojo.style.getPixelValue(this.constrainingContainer, "padding-left", true);
			padTop = dojo.style.getPixelValue(this.constrainingContainer, "padding-top", true);
		}

		return {
			minX: padLeft,
			minY: padTop,
			maxX: padLeft+width - dojo.style.getOuterWidth(this.domNode),
			maxY: padTop+height - dojo.style.getOuterHeight(this.domNode)
		}
	},

	updateDragOffset: function() {
		var scroll = dojo.html.getScrollOffset();
		if(scroll.y != this.scrollOffset.y) {
			var diff = scroll.y - this.scrollOffset.y;
			this.dragOffset.y += diff;
			this.scrollOffset.y = scroll.y;
		}
		if(scroll.x != this.scrollOffset.x) {
			var diff = scroll.x - this.scrollOffset.x;
			this.dragOffset.x += diff;
			this.scrollOffset.x = scroll.x;
		}
	},

	/** Moves the node to follow the mouse */
	onDragMove: function(e){
		this.updateDragOffset();
		var x = this.dragOffset.x + e.pageX;
		var y = this.dragOffset.y + e.pageY;

		if (this.constrainToContainer) {
			if (x < this.constraints.minX) { x = this.constraints.minX; }
			if (y < this.constraints.minY) { y = this.constraints.minY; }
			if (x > this.constraints.maxX) { x = this.constraints.maxX; }
			if (y > this.constraints.maxY) { y = this.constraints.maxY; }
		}

		if(!this.disableY) { this.dragClone.style.top = y + "px"; }
		if(!this.disableX) { this.dragClone.style.left = x + "px"; }
	},

	/**
	 * If the drag operation returned a success we reomve the clone of
	 * ourself from the original position. If the drag operation returned
	 * failure we slide back over to where we came from and end the operation
	 * with a little grace.
	 */
	onDragEnd: function(e){
		switch(e.dragStatus){

			case "dropSuccess":
				dojo.dom.removeNode(this.dragClone);
				this.dragClone = null;
				break;

			case "dropFailure": // slide back to the start
				var startCoords = dojo.style.getAbsolutePosition(this.dragClone, true);
				// offset the end so the effect can be seen
				var endCoords = [this.dragStartPosition.x + 1,
					this.dragStartPosition.y + 1];

				// animate
				var line = new dojo.lfx.Line(startCoords, endCoords);
				var anim = new dojo.lfx.Animation(500, line, dojo.lfx.easeOut);
				var dragObject = this;
				dojo.event.connect(anim, "onAnimate", function(e) {
					dragObject.dragClone.style.left = e[0] + "px";
					dragObject.dragClone.style.top = e[1] + "px";
				});
				dojo.event.connect(anim, "onEnd", function (e) {
					// pause for a second (not literally) and disappear
					dojo.lang.setTimeout(dojo.dom.removeNode, 200,
						dragObject.dragClone);
				});
				anim.play();
				break;
		}

		// shortly the browser will fire an onClick() event,
		// but since this was really a drag, just squelch it
		dojo.event.connect(this.domNode, "onclick", this, "squelchOnClick");
	},

	squelchOnClick: function(e){
		// squelch this onClick() event because it's the result of a drag (it's not a real click)
		e.preventDefault();

		// but if a real click comes along, allow it
		dojo.event.disconnect(this.domNode, "onclick", this, "squelchOnClick");
	},

	constrainTo: function(container) {
		this.constrainToContainer=true;
		if (container) {
			this.constrainingContainer = container;
		} else {
			this.constrainingContainer = this.domNode.parentNode;
		}
	}
});

dojo.dnd.HtmlDropTarget = function(node, types){
	if (arguments.length == 0) { return; }
	this.domNode = dojo.byId(node);
	dojo.dnd.DropTarget.call(this);
	if(types && dojo.lang.isString(types)) {
		types = [types];
	}
	this.acceptedTypes = types || [];
}
dojo.inherits(dojo.dnd.HtmlDropTarget, dojo.dnd.DropTarget);

dojo.lang.extend(dojo.dnd.HtmlDropTarget, {
	onDragOver: function(e){
		if(!this.accepts(e.dragObjects)){ return false; }

		// cache the positions of the child nodes
		this.childBoxes = [];
		for (var i = 0, child; i < this.domNode.childNodes.length; i++) {
			child = this.domNode.childNodes[i];
			if (child.nodeType != dojo.dom.ELEMENT_NODE) { continue; }
			var pos = dojo.style.getAbsolutePosition(child, true);
			var height = dojo.style.getInnerHeight(child);
			var width = dojo.style.getInnerWidth(child);
			this.childBoxes.push({top: pos.y, bottom: pos.y+height,
				left: pos.x, right: pos.x+width, node: child});
		}

		// TODO: use dummy node

		return true;
	},

	_getNodeUnderMouse: function(e){
		// find the child
		for (var i = 0, child; i < this.childBoxes.length; i++) {
			with (this.childBoxes[i]) {
				if (e.pageX >= left && e.pageX <= right &&
					e.pageY >= top && e.pageY <= bottom) { return i; }
			}
		}

		return -1;
	},

	createDropIndicator: function() {
		this.dropIndicator = document.createElement("div");
		with (this.dropIndicator.style) {
			position = "absolute";
			zIndex = 999;
			borderTopWidth = "1px";
			borderTopColor = "black";
			borderTopStyle = "solid";
			width = dojo.style.getInnerWidth(this.domNode) + "px";
			left = dojo.style.getAbsoluteX(this.domNode, true) + "px";
		}
	},

	onDragMove: function(e, dragObjects){
		var i = this._getNodeUnderMouse(e);

		if(!this.dropIndicator){
			this.createDropIndicator();
		}

		if(i < 0) {
			if(this.childBoxes.length) {
				var before = (dojo.html.gravity(this.childBoxes[0].node, e) & dojo.html.gravity.NORTH);
			} else {
				var before = true;
			}
		} else {
			var child = this.childBoxes[i];
			var before = (dojo.html.gravity(child.node, e) & dojo.html.gravity.NORTH);
		}
		this.placeIndicator(e, dragObjects, i, before);

		if(!dojo.html.hasParent(this.dropIndicator)) {
			document.body.appendChild(this.dropIndicator);
		}
	},

	/**
	 * Position the horizontal line that indicates "insert between these two items"
	 */
	placeIndicator: function(e, dragObjects, boxIndex, before) {
		with(this.dropIndicator.style){
			if (boxIndex < 0) {
				if (this.childBoxes.length) {
					top = (before ? this.childBoxes[0].top
						: this.childBoxes[this.childBoxes.length - 1].bottom) + "px";
				} else {
					top = dojo.style.getAbsoluteY(this.domNode, true) + "px";
				}
			} else {
				var child = this.childBoxes[boxIndex];
				top = (before ? child.top : child.bottom) + "px";
			}
		}
	},

	onDragOut: function(e) {
		if(this.dropIndicator) {
			dojo.dom.removeNode(this.dropIndicator);
			delete this.dropIndicator;
		}
	},

	/**
	 * Inserts the DragObject as a child of this node relative to the
	 * position of the mouse.
	 *
	 * @return true if the DragObject was inserted, false otherwise
	 */
	onDrop: function(e){
		this.onDragOut(e);

		var i = this._getNodeUnderMouse(e);

		if (i < 0) {
			if (this.childBoxes.length) {
				if (dojo.html.gravity(this.childBoxes[0].node, e) & dojo.html.gravity.NORTH) {
					return this.insert(e, this.childBoxes[0].node, "before");
				} else {
					return this.insert(e, this.childBoxes[this.childBoxes.length-1].node, "after");
				}
			}
			return this.insert(e, this.domNode, "append");
		}

		var child = this.childBoxes[i];
		if (dojo.html.gravity(child.node, e) & dojo.html.gravity.NORTH) {
			return this.insert(e, child.node, "before");
		} else {
			return this.insert(e, child.node, "after");
		}
	},

	insert: function(e, refNode, position) {
		var node = e.dragObject.domNode;

		if(position == "before") {
			return dojo.html.insertBefore(node, refNode);
		} else if(position == "after") {
			return dojo.html.insertAfter(node, refNode);
		} else if(position == "append") {
			refNode.appendChild(node);
			return true;
		}

		return false;
	}
});

dojo.kwCompoundRequire({
	common: ["dojo.dnd.DragAndDrop"],
	browser: ["dojo.dnd.HtmlDragAndDrop"],
	dashboard: ["dojo.dnd.HtmlDragAndDrop"]
});
dojo.provide("dojo.dnd.*");

dojo.provide("dojo.widget.Manager");
dojo.require("dojo.lang.array");
dojo.require("dojo.lang.func");
dojo.require("dojo.event.*");

// Manager class
dojo.widget.manager = new function(){
	this.widgets = [];
	this.widgetIds = [];
	
	// map of widgetId-->widget for widgets without parents (top level widgets)
	this.topWidgets = {};

	var widgetTypeCtr = {};
	var renderPrefixCache = [];

	this.getUniqueId = function (widgetType) {
		return widgetType + "_" + (widgetTypeCtr[widgetType] != undefined ?
			++widgetTypeCtr[widgetType] : widgetTypeCtr[widgetType] = 0);
	}

	this.add = function(widget){
		dojo.profile.start("dojo.widget.manager.add");
		this.widgets.push(widget);
		// Opera9 uses ID (caps)
		if(!widget.extraArgs["id"]){
			widget.extraArgs["id"] = widget.extraArgs["ID"];
		}
		// FIXME: the rest of this method is very slow!
		if(widget.widgetId == ""){
			if(widget["id"]){
				widget.widgetId = widget["id"];
			}else if(widget.extraArgs["id"]){
				widget.widgetId = widget.extraArgs["id"];
			}else{
				widget.widgetId = this.getUniqueId(widget.widgetType);
			}
		}
		if(this.widgetIds[widget.widgetId]){
			dojo.debug("widget ID collision on ID: "+widget.widgetId);
		}
		this.widgetIds[widget.widgetId] = widget;
		// Widget.destroy already calls removeById(), so we don't need to
		// connect() it here
		dojo.profile.end("dojo.widget.manager.add");
	}

	this.destroyAll = function(){
		for(var x=this.widgets.length-1; x>=0; x--){
			try{
				// this.widgets[x].destroyChildren();
				this.widgets[x].destroy(true);
				delete this.widgets[x];
			}catch(e){ }
		}
	}

	// FIXME: we should never allow removal of the root widget until all others
	// are removed!
	this.remove = function(widgetIndex){
		var tw = this.widgets[widgetIndex].widgetId;
		delete this.widgetIds[tw];
		this.widgets.splice(widgetIndex, 1);
	}
	
	// FIXME: suboptimal performance
	this.removeById = function(id) {
		for (var i=0; i<this.widgets.length; i++){
			if(this.widgets[i].widgetId == id){
				this.remove(i);
				break;
			}
		}
	}

	this.getWidgetById = function(id){
		return this.widgetIds[id];
	}

	this.getWidgetsByType = function(type){
		var lt = type.toLowerCase();
		var ret = [];
		dojo.lang.forEach(this.widgets, function(x){
			if(x.widgetType.toLowerCase() == lt){
				ret.push(x);
			}
		});
		return ret;
	}

	this.getWidgetsOfType = function (id) {
		dojo.deprecated("getWidgetsOfType is depecrecated, use getWidgetsByType");
		return dojo.widget.manager.getWidgetsByType(id);
	}

	this.getWidgetsByFilter = function(unaryFunc, onlyOne){
		var ret = [];
		dojo.lang.every(this.widgets, function(x){
			if(unaryFunc(x)){
				ret.push(x);
				if(onlyOne){return false;}
			}
			return true;
		});
		return (onlyOne ? ret[0] : ret);
	}

	this.getAllWidgets = function() {
		return this.widgets.concat();
	}

	//	added, trt 2006-01-20
	this.getWidgetByNode = function(/* DOMNode */ node){
		var w=this.getAllWidgets();
		for (var i=0; i<w.length; i++){
			if (w[i].domNode==node){
				return w[i];
			}
		}
		return null;
	}

	// shortcuts, baby
	this.byId = this.getWidgetById;
	this.byType = this.getWidgetsByType;
	this.byFilter = this.getWidgetsByFilter;
	this.byNode = this.getWidgetByNode;

	// map of previousally discovered implementation names to constructors
	var knownWidgetImplementations = {};

	// support manually registered widget packages
	var widgetPackages = ["dojo.widget"];
	for (var i=0; i<widgetPackages.length; i++) {
		// convenience for checking if a package exists (reverse lookup)
		widgetPackages[widgetPackages[i]] = true;
	}

	this.registerWidgetPackage = function(pname) {
		if(!widgetPackages[pname]){
			widgetPackages[pname] = true;
			widgetPackages.push(pname);
		}
	}
	
	this.getWidgetPackageList = function() {
		return dojo.lang.map(widgetPackages, function(elt) { return(elt!==true ? elt : undefined); });
	}
	
	this.getImplementation = function(widgetName, ctorObject, mixins){
		// try and find a name for the widget
		var impl = this.getImplementationName(widgetName);
		if(impl){ 
			// var tic = new Date();
			var ret = new impl(ctorObject);
			// dojo.debug(new Date() - tic);
			return ret;
		}
	}

	this.getImplementationName = function(widgetName){
		/*
		 * This is the overly-simplistic implemention of getImplementation (har
		 * har). In the future, we are going to want something that allows more
		 * freedom of expression WRT to specifying different specializations of
		 * a widget.
		 *
		 * Additionally, this implementation treats widget names as case
		 * insensitive, which does not necessarialy mesh with the markup which
		 * can construct a widget.
		 */

		var lowerCaseWidgetName = widgetName.toLowerCase();

		var impl = knownWidgetImplementations[lowerCaseWidgetName];
		if(impl){
			return impl;
		}

		// first store a list of the render prefixes we are capable of rendering
		if(!renderPrefixCache.length){
			for(var renderer in dojo.render){
				if(dojo.render[renderer]["capable"] === true){
					var prefixes = dojo.render[renderer].prefixes;
					for(var i = 0; i < prefixes.length; i++){
						renderPrefixCache.push(prefixes[i].toLowerCase());
					}
				}
			}
			// make sure we don't HAVE to prefix widget implementation names
			// with anything to get them to render
			renderPrefixCache.push("");
		}

		// look for a rendering-context specific version of our widget name
		for(var i = 0; i < widgetPackages.length; i++){
			var widgetPackage = dojo.evalObjPath(widgetPackages[i]);
			if(!widgetPackage) { continue; }

			for (var j = 0; j < renderPrefixCache.length; j++) {
				if (!widgetPackage[renderPrefixCache[j]]) { continue; }
				for (var widgetClass in widgetPackage[renderPrefixCache[j]]) {
					if (widgetClass.toLowerCase() != lowerCaseWidgetName) { continue; }
					knownWidgetImplementations[lowerCaseWidgetName] =
						widgetPackage[renderPrefixCache[j]][widgetClass];
					return knownWidgetImplementations[lowerCaseWidgetName];
				}
			}

			for (var j = 0; j < renderPrefixCache.length; j++) {
				for (var widgetClass in widgetPackage) {
					if (widgetClass.toLowerCase() !=
						(renderPrefixCache[j] + lowerCaseWidgetName)) { continue; }
	
					knownWidgetImplementations[lowerCaseWidgetName] =
						widgetPackage[widgetClass];
					return knownWidgetImplementations[lowerCaseWidgetName];
				}
			}
		}
		
		throw new Error('Could not locate "' + widgetName + '" class');
	}

	// FIXME: does it even belong in this name space?
	// NOTE: this method is implemented by DomWidget.js since not all
	// hostenv's would have an implementation.
	/*this.getWidgetFromPrimitive = function(baseRenderType){
		dojo.unimplemented("dojo.widget.manager.getWidgetFromPrimitive");
	}

	this.getWidgetFromEvent = function(nativeEvt){
		dojo.unimplemented("dojo.widget.manager.getWidgetFromEvent");
	}*/

	// Catch window resize events and notify top level widgets
	this.resizing=false;
	this.onWindowResized = function(){
		if(this.resizing){
			return;	// duplicate event
		}
		try{
			this.resizing=true;
			for(var id in this.topWidgets){
				var child = this.topWidgets[id];
				if(child.onParentResized ){
					child.onParentResized();
				}
			};
		}catch(e){
		}finally{
			this.resizing=false;
		}
	}
	if(typeof window != "undefined") {
		dojo.addOnLoad(this, 'onWindowResized');							// initial sizing
		dojo.event.connect(window, 'onresize', this, 'onWindowResized');	// window resize
	}

	// FIXME: what else?
};

(function(){
	var dw = dojo.widget;
	var dwm = dw.manager;
	var h = dojo.lang.curry(dojo.lang, "hitch", dwm);
	var g = function(oldName, newName){
		dw[(newName||oldName)] = h(oldName);
	}
	// copy the methods from the default manager (this) to the widget namespace
	g("add", "addWidget");
	g("destroyAll", "destroyAllWidgets");
	g("remove", "removeWidget");
	g("removeById", "removeWidgetById");
	g("getWidgetById");
	g("getWidgetById", "byId");
	g("getWidgetsByType");
	g("getWidgetsByFilter");
	g("getWidgetsByType", "byType");
	g("getWidgetsByFilter", "byFilter");
	g("getWidgetByNode", "byNode");
	dw.all = function(n){
		var widgets = dwm.getAllWidgets.apply(dwm, arguments);
		if(arguments.length > 0) {
			return widgets[n];
		}
		return widgets;
	}
	g("registerWidgetPackage");
	g("getImplementation", "getWidgetImplementation");
	g("getImplementationName", "getWidgetImplementationName");

	dw.widgets = dwm.widgets;
	dw.widgetIds = dwm.widgetIds;
	dw.root = dwm.root;
})();

dojo.provide("dojo.widget.Widget");
dojo.provide("dojo.widget.tags");

dojo.require("dojo.lang.func");
dojo.require("dojo.lang.array");
dojo.require("dojo.lang.extras");
dojo.require("dojo.lang.declare");
dojo.require("dojo.widget.Manager");
dojo.require("dojo.event.*");

dojo.declare("dojo.widget.Widget", null, {
	initializer: function() {								 
		// these properties aren't primitives and need to be created on a per-item
		// basis.
		this.children = [];
		// this.selection = new dojo.widget.Selection();
		// FIXME: need to replace this with context menu stuff
		this.extraArgs = {};
	},
	// FIXME: need to be able to disambiguate what our rendering context is
	//        here!
	//
	// needs to be a string with the end classname. Every subclass MUST
	// over-ride.
	//
	// base widget properties
	parent: null,
	// obviously, top-level and modal widgets should set these appropriately
	isTopLevel:  false,
	isModal: false,

	isEnabled: true,
	isHidden: false,
	isContainer: false, // can we contain other widgets?
	widgetId: "",
	widgetType: "Widget", // used for building generic widgets

	toString: function() {
		return '[Widget ' + this.widgetType + ', ' + (this.widgetId || 'NO ID') + ']';
	},

	repr: function(){
		return this.toString();
	},

	enable: function(){
		// should be over-ridden
		this.isEnabled = true;
	},

	disable: function(){
		// should be over-ridden
		this.isEnabled = false;
	},

	hide: function(){
		// should be over-ridden
		this.isHidden = true;
	},

	show: function(){
		// should be over-ridden
		this.isHidden = false;
	},

	onResized: function(){
		// Clients should override this function to do special processing,
		// then call this.notifyChildrenOfResize() to notify children of resize
		this.notifyChildrenOfResize();
	},
	
	notifyChildrenOfResize: function(){
		for(var i=0; i<this.children.length; i++){
			var child = this.children[i];
			//dojo.debug(this.widgetId + " resizing child " + child.widgetId);
			if( child.onResized ){
				child.onResized();
			}
		}
	},

	create: function(args, fragment, parentComp){
		// dojo.debug(this.widgetType, "create");
		this.satisfyPropertySets(args, fragment, parentComp);
		// dojo.debug(this.widgetType, "-> mixInProperties");
		this.mixInProperties(args, fragment, parentComp);
		// dojo.debug(this.widgetType, "-> postMixInProperties");
		this.postMixInProperties(args, fragment, parentComp);
		// dojo.debug(this.widgetType, "-> dojo.widget.manager.add");
		dojo.widget.manager.add(this);
		// dojo.debug(this.widgetType, "-> buildRendering");
		this.buildRendering(args, fragment, parentComp);
		// dojo.debug(this.widgetType, "-> initialize");
		this.initialize(args, fragment, parentComp);
		// dojo.debug(this.widgetType, "-> postInitialize");
		this.postInitialize(args, fragment, parentComp);
		// dojo.debug(this.widgetType, "-> postCreate");
		this.postCreate(args, fragment, parentComp);
		// dojo.debug(this.widgetType, "done!");
		return this;
	},

	// Destroy this widget and it's descendants
	destroy: function(finalize){
		// FIXME: this is woefully incomplete
		this.destroyChildren();
		this.uninitialize();
		this.destroyRendering(finalize);
		dojo.widget.manager.removeById(this.widgetId);
	},

	// Destroy the children of this widget, and their descendents
	destroyChildren: function(){
		while(this.children.length > 0){
			var tc = this.children[0];
			this.removeChild(tc);
			tc.destroy();
		}
	},

	getChildrenOfType: function(type, recurse){
		var ret = [];
		var isFunc = dojo.lang.isFunction(type);
		if(!isFunc){
			type = type.toLowerCase();
		}
		for(var x=0; x<this.children.length; x++){
			if(isFunc){
				if(this.children[x] instanceof type){
					ret.push(this.children[x]);
				}
			}else{
				if(this.children[x].widgetType.toLowerCase() == type){
					ret.push(this.children[x]);
				}
			}
			if(recurse){
				ret = ret.concat(this.children[x].getChildrenOfType(type, recurse));
			}
		}
		return ret;
	},

	getDescendants: function(){
		var result = [];
		var stack = [this];
		var elem;
		while (elem = stack.pop()){
			result.push(elem);
			dojo.lang.forEach(elem.children, function(elem) { stack.push(elem); });
		}
		return result;
	},

	satisfyPropertySets: function(args){
		// dojo.profile.start("satisfyPropertySets");
		// get the default propsets for our component type
		/*
		var typePropSets = []; // FIXME: need to pull these from somewhere!
		var localPropSets = []; // pull out propsets from the parser's return structure

		// for(var x=0; x<args.length; x++){
		// }

		for(var x=0; x<typePropSets.length; x++){
		}

		for(var x=0; x<localPropSets.length; x++){
		}
		*/
		// dojo.profile.end("satisfyPropertySets");
		
		return args;
	},

	mixInProperties: function(args, frag){
		if((args["fastMixIn"])||(frag["fastMixIn"])){
			// dojo.profile.start("mixInProperties_fastMixIn");
			// fast mix in assumes case sensitivity, no type casting, etc...
			// dojo.lang.mixin(this, args);
			for(var x in args){
				this[x] = args[x];
			}
			// dojo.profile.end("mixInProperties_fastMixIn");
			return;
		}
		// dojo.profile.start("mixInProperties");
		/*
		 * the actual mix-in code attempts to do some type-assignment based on
		 * PRE-EXISTING properties of the "this" object. When a named property
		 * of a propset is located, it is first tested to make sure that the
		 * current object already "has one". Properties which are undefined in
		 * the base widget are NOT settable here. The next step is to try to
		 * determine type of the pre-existing property. If it's a string, the
		 * property value is simply assigned. If a function, the property is
		 * replaced with a "new Function()" declaration. If an Array, the
		 * system attempts to split the string value on ";" chars, and no
		 * further processing is attempted (conversion of array elements to a
		 * integers, for instance). If the property value is an Object
		 * (testObj.constructor === Object), the property is split first on ";"
		 * chars, secondly on ":" chars, and the resulting key/value pairs are
		 * assigned to an object in a map style. The onus is on the property
		 * user to ensure that all property values are converted to the
		 * expected type before usage.
		 */

		var undef;

		// NOTE: we cannot assume that the passed properties are case-correct
		// (esp due to some browser bugs). Therefore, we attempt to locate
		// properties for assignment regardless of case. This may cause
		// problematic assignments and bugs in the future and will need to be
		// documented with big bright neon lights.

		// FIXME: fails miserably if a mixin property has a default value of null in 
		// a widget

		// NOTE: caching lower-cased args in the prototype is only 
		// acceptable if the properties are invariant.
		// if we have a name-cache, get it
		var lcArgs = dojo.widget.lcArgsCache[this.widgetType];
		if ( lcArgs == null ){
			// build a lower-case property name cache if we don't have one
			lcArgs = {};
			for(var y in this){
				lcArgs[((new String(y)).toLowerCase())] = y;
			}
			dojo.widget.lcArgsCache[this.widgetType] = lcArgs;
		}
		var visited = {};
		for(var x in args){
			if(!this[x]){ // check the cache for properties
				var y = lcArgs[(new String(x)).toLowerCase()];
				if(y){
					args[y] = args[x];
					x = y; 
				}
			}
			if(visited[x]){ continue; }
			visited[x] = true;
			if((typeof this[x]) != (typeof undef)){
				if(typeof args[x] != "string"){
					this[x] = args[x];
				}else{
					if(dojo.lang.isString(this[x])){
						this[x] = args[x];
					}else if(dojo.lang.isNumber(this[x])){
						this[x] = new Number(args[x]); // FIXME: what if NaN is the result?
					}else if(dojo.lang.isBoolean(this[x])){
						this[x] = (args[x].toLowerCase()=="false") ? false : true;
					}else if(dojo.lang.isFunction(this[x])){

						// FIXME: need to determine if always over-writing instead
						// of attaching here is appropriate. I suspect that we
						// might want to only allow attaching w/ action items.
						
						// RAR, 1/19/05: I'm going to attach instead of
						// over-write here. Perhaps function objects could have
						// some sort of flag set on them? Or mixed-into objects
						// could have some list of non-mutable properties
						// (although I'm not sure how that would alleviate this
						// particular problem)? 

						// this[x] = new Function(args[x]);

						// after an IRC discussion last week, it was decided
						// that these event handlers should execute in the
						// context of the widget, so that the "this" pointer
						// takes correctly.
						
						// argument that contains no punctuation other than . is 
						// considered a function spec, not code
						if(args[x].search(/[^\w\.]+/i) == -1){
							this[x] = dojo.evalObjPath(args[x], false);
						}else{
							var tn = dojo.lang.nameAnonFunc(new Function(args[x]), this);
							dojo.event.connect(this, x, this, tn);
						}
					}else if(dojo.lang.isArray(this[x])){ // typeof [] == "object"
						this[x] = args[x].split(";");
					} else if (this[x] instanceof Date) {
						this[x] = new Date(Number(args[x])); // assume timestamp
					}else if(typeof this[x] == "object"){ 
						// FIXME: should we be allowing extension here to handle
						// other object types intelligently?

						// if we defined a URI, we probablt want to allow plain strings
						// to override it
						if (this[x] instanceof dojo.uri.Uri){

							this[x] = args[x];
						}else{

							// FIXME: unlike all other types, we do not replace the
							// object with a new one here. Should we change that?
							var pairs = args[x].split(";");
							for(var y=0; y<pairs.length; y++){
								var si = pairs[y].indexOf(":");
								if((si != -1)&&(pairs[y].length>si)){
									this[x][pairs[y].substr(0, si).replace(/^\s+|\s+$/g, "")] = pairs[y].substr(si+1);
								}
							}
						}
					}else{
						// the default is straight-up string assignment. When would
						// we ever hit this?
						this[x] = args[x];
					}
				}
			}else{
				// collect any extra 'non mixed in' args
				this.extraArgs[x.toLowerCase()] = args[x];
			}
		}
		// dojo.profile.end("mixInProperties");
	},
	
	postMixInProperties: function(){
	},

	initialize: function(args, frag){
		// dojo.unimplemented("dojo.widget.Widget.initialize");
		return false;
	},

	postInitialize: function(args, frag){
		return false;
	},

	postCreate: function(args, frag){
		return false;
	},

	uninitialize: function(){
		// dojo.unimplemented("dojo.widget.Widget.uninitialize");
		return false;
	},

	buildRendering: function(){
		// SUBCLASSES MUST IMPLEMENT
		dojo.unimplemented("dojo.widget.Widget.buildRendering, on "+this.toString()+", ");
		return false;
	},

	destroyRendering: function(){
		// SUBCLASSES MUST IMPLEMENT
		dojo.unimplemented("dojo.widget.Widget.destroyRendering");
		return false;
	},

	cleanUp: function(){
		// SUBCLASSES MUST IMPLEMENT
		dojo.unimplemented("dojo.widget.Widget.cleanUp");
		return false;
	},

	addedTo: function(parent){
		// this is just a signal that can be caught
	},

	addChild: function(child){
		// SUBCLASSES MUST IMPLEMENT
		dojo.unimplemented("dojo.widget.Widget.addChild");
		return false;
	},

	// Detach the given child widget from me, but don't destroy it
	removeChild: function(widget){
		for(var x=0; x<this.children.length; x++){
			if(this.children[x] === widget){
				this.children.splice(x, 1);
				break;
			}
		}
		return widget;
	},

	resize: function(width, height){
		// both width and height may be set as percentages. The setWidth and
		// setHeight  functions attempt to determine if the passed param is
		// specified in percentage or native units. Integers without a
		// measurement are assumed to be in the native unit of measure.
		this.setWidth(width);
		this.setHeight(height);
	},

	setWidth: function(width){
		if((typeof width == "string")&&(width.substr(-1) == "%")){
			this.setPercentageWidth(width);
		}else{
			this.setNativeWidth(width);
		}
	},

	setHeight: function(height){
		if((typeof height == "string")&&(height.substr(-1) == "%")){
			this.setPercentageHeight(height);
		}else{
			this.setNativeHeight(height);
		}
	},

	setPercentageHeight: function(height){
		// SUBCLASSES MUST IMPLEMENT
		return false;
	},

	setNativeHeight: function(height){
		// SUBCLASSES MUST IMPLEMENT
		return false;
	},

	setPercentageWidth: function(width){
		// SUBCLASSES MUST IMPLEMENT
		return false;
	},

	setNativeWidth: function(width){
		// SUBCLASSES MUST IMPLEMENT
		return false;
	},

	getPreviousSibling: function() {
		var idx = this.getParentIndex();
 
		 // first node is idx=0 not found is idx<0
		if (idx<=0) return null;
 
		return this.getSiblings()[idx-1];
	},
 
	getSiblings: function() {
		return this.parent.children;
	},
 
	getParentIndex: function() {
		return dojo.lang.indexOf( this.getSiblings(), this, true);
	},
 
	getNextSibling: function() {
 
		var idx = this.getParentIndex();
 
		if (idx == this.getSiblings().length-1) return null; // last node
		if (idx < 0) return null; // not found
 
		return this.getSiblings()[idx+1];
 
	}
});

// Lower case name cache: listing of the lower case elements in each widget.
// We can't store the lcArgs in the widget itself because if B subclasses A,
// then B.prototype.lcArgs might return A.prototype.lcArgs, which is not what we
// want
dojo.widget.lcArgsCache = {};

// TODO: should have a more general way to add tags or tag libraries?
// TODO: need a default tags class to inherit from for things like getting propertySets
// TODO: parse properties/propertySets into component attributes
// TODO: parse subcomponents
// TODO: copy/clone raw markup fragments/nodes as appropriate
dojo.widget.tags = {};
dojo.widget.tags.addParseTreeHandler = function(type){
	var ltype = type.toLowerCase();
	this[ltype] = function(fragment, widgetParser, parentComp, insertionIndex, localProps){ 
		return dojo.widget.buildWidgetFromParseTree(ltype, fragment, widgetParser, parentComp, insertionIndex, localProps);
	}
}
dojo.widget.tags.addParseTreeHandler("dojo:widget");

dojo.widget.tags["dojo:propertyset"] = function(fragment, widgetParser, parentComp){
	// FIXME: Is this needed?
	// FIXME: Not sure that this parses into the structure that I want it to parse into...
	// FIXME: add support for nested propertySets
	var properties = widgetParser.parseProperties(fragment["dojo:propertyset"]);
}

// FIXME: need to add the <dojo:connect />
dojo.widget.tags["dojo:connect"] = function(fragment, widgetParser, parentComp){
	var properties = widgetParser.parseProperties(fragment["dojo:connect"]);
}

// FIXME: if we know the insertion point (to a reasonable location), why then do we:
//	- create a template node
//	- clone the template node
//	- render the clone and set properties
//	- remove the clone from the render tree
//	- place the clone
// this is quite dumb
dojo.widget.buildWidgetFromParseTree = function(type, frag, 
												parser, parentComp, 
												insertionIndex, localProps){
	var stype = type.split(":");
	stype = (stype.length == 2) ? stype[1] : type;
	// FIXME: we don't seem to be doing anything with this!
	// var propertySets = parser.getPropertySets(frag);
	var localProperties = localProps || parser.parseProperties(frag["dojo:"+stype]);
	// var tic = new Date();
	var twidget = dojo.widget.manager.getImplementation(stype);
	if(!twidget){
		throw new Error("cannot find \"" + stype + "\" widget");
	}else if (!twidget.create){
		throw new Error("\"" + stype + "\" widget object does not appear to implement *Widget");
	}
	localProperties["dojoinsertionindex"] = insertionIndex;
	// FIXME: we loose no less than 5ms in construction!
	var ret = twidget.create(localProperties, frag, parentComp);
	// dojo.debug(new Date() - tic);
	return ret;
}

/*
 * it would be best to be able to call defineWidget for any widget namespace
 */
dojo.widget.defineWidget = function(widgetClass /*string*/, superclass /*function*/, props /*object*/, renderer /*string*/, ctor /*function*/){
	// widgetClass takes the form foo.bar.baz<.renderer>.WidgetName (e.g. foo.bar.baz.WidgetName or foo.bar.baz.html.WidgetName)
	var namespace = widgetClass.split(".");
	var type = namespace.pop(); // type <= WidgetName, namespace <= foo.bar.baz<.renderer>
	if(renderer){
		// FIXME: could just be namespace.pop(), unless there can be foo.bar.baz.html.zot.WidgetName
		while ((namespace.length)&&(namespace.pop() != renderer)); // namespace <= foo.bar.baz
	}
	namespace = namespace.join(".");

	dojo.widget.manager.registerWidgetPackage(namespace);
	dojo.widget.tags.addParseTreeHandler("dojo:"+type.toLowerCase());

	if(!props){ props = {}; }
	props.widgetType = type;

	if((!ctor)&&(props["classConstructor"])){
		ctor = props.classConstructor;
		delete props.classConstructor;
	}
	dojo.declare(widgetClass, superclass, props, ctor);
}

dojo.provide("dojo.widget.Parse");

dojo.require("dojo.widget.Manager");
dojo.require("dojo.dom");

dojo.widget.Parse = function(fragment) {
	this.propertySetsList = [];
	this.fragment = fragment;
	
	this.createComponents = function(frag, parentComp){
		var comps = [ ];
		var built = false;
		// if we have items to parse/create at this level, do it!
		try{
			if((frag)&&(frag["tagName"])&&(frag!=frag["nodeRef"])){
				var djTags = dojo.widget.tags;
				// we split so that you can declare multiple
				// non-destructive widgets from the same ctor node
				var tna = String(frag["tagName"]).split(";");
				for(var x=0; x<tna.length; x++){
					var ltn = (tna[x].replace(/^\s+|\s+$/g, "")).toLowerCase();
					if(djTags[ltn]){
						built = true;
						frag.tagName = ltn;
						var ret = djTags[ltn](frag, this, parentComp, frag["index"]);
						comps.push(ret);
					}else{
						if((dojo.lang.isString(ltn))&&(ltn.substr(0, 5)=="dojo:")){
							dojo.debug("no tag handler registed for type: ", ltn);
						}
					}
				}
			}
		}catch(e){
			dojo.debug("dojo.widget.Parse: error:", e);
			// throw(e);
			// IE is such a bitch sometimes
		}
		// if there's a sub-frag, build widgets from that too
		if(!built){
			comps = comps.concat(this.createSubComponents(frag, parentComp));
		}
		return comps;
	}

	/*	createSubComponents recurses over a raw JavaScript object structure,
			and calls the corresponding handler for its normalized tagName if it exists
	*/
	this.createSubComponents = function(fragment, parentComp){
		var frag, comps = [];
		for(var item in fragment){
			frag = fragment[item];
			if ((frag)&&(typeof frag == "object")&&(frag!=fragment.nodeRef)&&(frag!=fragment["tagName"])){
				comps = comps.concat(this.createComponents(frag, parentComp));
			}
		}
		return comps;
	}

	/*  parsePropertySets checks the top level of a raw JavaScript object
			structure for any propertySets.  It stores an array of references to 
			propertySets that it finds.
	*/
	this.parsePropertySets = function(fragment) {
		return [];
		var propertySets = [];
		for(var item in fragment){
			if(	(fragment[item]["tagName"] == "dojo:propertyset") ) {
				propertySets.push(fragment[item]);
			}
		}
		// FIXME: should we store these propertySets somewhere for later retrieval
		this.propertySetsList.push(propertySets);
		return propertySets;
	}
	
	/*  parseProperties checks a raw JavaScript object structure for
			properties, and returns an array of properties that it finds.
	*/
	this.parseProperties = function(fragment) {
		var properties = {};
		for(var item in fragment){
			// FIXME: need to check for undefined?
			// case: its a tagName or nodeRef
			if((fragment[item] == fragment["tagName"])||
				(fragment[item] == fragment.nodeRef)){
				// do nothing
			}else{
				if((fragment[item]["tagName"])&&
					(dojo.widget.tags[fragment[item].tagName.toLowerCase()])){
					// TODO: it isn't a property or property set, it's a fragment, 
					// so do something else
					// FIXME: needs to be a better/stricter check
					// TODO: handle xlink:href for external property sets
				}else if((fragment[item][0])&&(fragment[item][0].value!="")&&(fragment[item][0].value!=null)){
					try{
						// FIXME: need to allow more than one provider
						if(item.toLowerCase() == "dataprovider") {
							var _this = this;
							this.getDataProvider(_this, fragment[item][0].value);
							properties.dataProvider = this.dataProvider;
						}
						properties[item] = fragment[item][0].value;
						var nestedProperties = this.parseProperties(fragment[item]);
						// FIXME: this kind of copying is expensive and inefficient!
						for(var property in nestedProperties){
							properties[property] = nestedProperties[property];
						}
					}catch(e){ dojo.debug(e); }
				}
			}
		}
		return properties;
	}

	/* getPropertySetById returns the propertySet that matches the provided id
	*/
	
	this.getDataProvider = function(objRef, dataUrl) {
		// FIXME: this is currently sync.  To make this async, we made need to move 
		//this step into the widget ctor, so that it is loaded when it is needed 
		// to populate the widget
		dojo.io.bind({
			url: dataUrl,
			load: function(type, evaldObj){
				if(type=="load"){
					objRef.dataProvider = evaldObj;
				}
			},
			mimetype: "text/javascript",
			sync: true
		});
	}

	
	this.getPropertySetById = function(propertySetId){
		for(var x = 0; x < this.propertySetsList.length; x++){
			if(propertySetId == this.propertySetsList[x]["id"][0].value){
				return this.propertySetsList[x];
			}
		}
		return "";
	}
	
	/* getPropertySetsByType returns the propertySet(s) that match(es) the
	 * provided componentClass
	 */
	this.getPropertySetsByType = function(componentType){
		var propertySets = [];
		for(var x=0; x < this.propertySetsList.length; x++){
			var cpl = this.propertySetsList[x];
			var cpcc = cpl["componentClass"]||cpl["componentType"]||null;
			if((cpcc)&&(propertySetId == cpcc[0].value)){
				propertySets.push(cpl);
			}
		}
		return propertySets;
	}
	
	/* getPropertySets returns the propertySet for a given component fragment
	*/
	this.getPropertySets = function(fragment){
		var ppl = "dojo:propertyproviderlist";
		var propertySets = [];
		var tagname = fragment["tagName"];
		if(fragment[ppl]){ 
			var propertyProviderIds = fragment[ppl].value.split(" ");
			// FIXME: should the propertyProviderList attribute contain #
			// 		  syntax for reference to ids or not?
			// FIXME: need a better test to see if this is local or external
			// FIXME: doesn't handle nested propertySets, or propertySets that
			// 		  just contain information about css documents, etc.
			for(propertySetId in propertyProviderIds){
				if((propertySetId.indexOf("..")==-1)&&(propertySetId.indexOf("://")==-1)){
					// get a reference to a propertySet within the current parsed structure
					var propertySet = this.getPropertySetById(propertySetId);
					if(propertySet != ""){
						propertySets.push(propertySet);
					}
				}else{
					// FIXME: add code to parse and return a propertySet from
					// another document
					// alex: is this even necessaray? Do we care? If so, why?
				}
			}
		}
		// we put the typed ones first so that the parsed ones override when
		// iteration happens.
		return (this.getPropertySetsByType(tagname)).concat(propertySets);
	}
	
	/* 
		nodeRef is the node to be replaced... in the future, we might want to add 
		an alternative way to specify an insertion point

		componentName is the expected dojo widget name, i.e. Button of ContextMenu

		properties is an object of name value pairs
	*/
	this.createComponentFromScript = function(nodeRef, componentName, properties){
		var ltn = "dojo:" + componentName.toLowerCase();
		if(dojo.widget.tags[ltn]){
			properties.fastMixIn = true;
			return [dojo.widget.tags[ltn](properties, this, null, null, properties)];
		}else{
			if(ltn.substr(0, 5)=="dojo:"){
				dojo.debug("no tag handler registed for type: ", ltn);
			}
		}
	}
}


dojo.widget._parser_collection = {"dojo": new dojo.widget.Parse() };
dojo.widget.getParser = function(name){
	if(!name){ name = "dojo"; }
	if(!this._parser_collection[name]){
		this._parser_collection[name] = new dojo.widget.Parse();
	}
	return this._parser_collection[name];
}

/**
 * Creates widget.
 *
 * @param name     The name of the widget to create
 * @param props    Key-Value pairs of properties of the widget
 * @param refNode  If the last argument is specified this node is used as
 *                 a reference for inserting this node into a DOM tree else
 *                 it beomces the domNode
 * @param position The position to insert this widget's node relative to the
 *                 refNode argument
 * @return The new Widget object
 */
 
dojo.widget.createWidget = function(name, props, refNode, position){
	var lowerCaseName = name.toLowerCase();
	var namespacedName = "dojo:" + lowerCaseName;
	var isNode = ( dojo.byId(name) && (!dojo.widget.tags[namespacedName]) );

	// if we got a node or an unambiguious ID, build a widget out of it
	if(	(arguments.length==1) && ((typeof name != "string")||(isNode)) ){
		// we got a DOM node
		var xp = new dojo.xml.Parse();
		// FIXME: we should try to find the parent!
		var tn = (isNode) ? dojo.byId(name) : name;
		return dojo.widget.getParser().createComponents(xp.parseElement(tn, null, true))[0];
	}

	function fromScript (placeKeeperNode, name, props) {
		props[namespacedName] = { 
			dojotype: [{value: lowerCaseName}],
			nodeRef: placeKeeperNode,
			fastMixIn: true
		};
		return dojo.widget.getParser().createComponentFromScript(
			placeKeeperNode, name, props, true);
	}

	if (typeof name != "string" && typeof props == "string") {
		dojo.deprecated("dojo.widget.createWidget", 
			"argument order is now of the form " +
			"dojo.widget.createWidget(NAME, [PROPERTIES, [REFERENCENODE, [POSITION]]])");
		return fromScript(name, props, refNode);
	}
	
	props = props||{};
	var notRef = false;
	var tn = null;
	var h = dojo.render.html.capable;
	if(h){
		tn = document.createElement("span");
	}
	if(!refNode){
		notRef = true;
		refNode = tn;
		if(h){
			document.body.appendChild(refNode);
		}
	}else if(position){
		dojo.dom.insertAtPosition(tn, refNode, position);
	}else{ // otherwise don't replace, but build in-place
		tn = refNode;
	}
	var widgetArray = fromScript(tn, name, props);
	if (!widgetArray[0] || typeof widgetArray[0].widgetType == "undefined") {
		throw new Error("createWidget: Creation of \"" + name + "\" widget failed.");
	}
	if (notRef) {
		if (widgetArray[0].domNode.parentNode) {
			widgetArray[0].domNode.parentNode.removeChild(widgetArray[0].domNode);
		}
	}
	return widgetArray[0]; // just return the widget
}
 
dojo.widget.fromScript = function(name, props, refNode, position){
	dojo.deprecated("dojo.widget.fromScript", " use " +
		"dojo.widget.createWidget instead");
	return dojo.widget.createWidget(name, props, refNode, position);
}

dojo.provide("dojo.widget.DomWidget");

dojo.require("dojo.event.*");
dojo.require("dojo.widget.Widget");
dojo.require("dojo.dom");
dojo.require("dojo.xml.Parse");
dojo.require("dojo.uri.*");
dojo.require("dojo.lang.func");

dojo.widget._cssFiles = {};
dojo.widget._cssStrings = {};
dojo.widget._templateCache = {};

dojo.widget.defaultStrings = {
	dojoRoot: dojo.hostenv.getBaseScriptUri(),
	baseScriptUri: dojo.hostenv.getBaseScriptUri()
};

dojo.widget.buildFromTemplate = function() {
	dojo.lang.forward("fillFromTemplateCache");
}

// static method to build from a template w/ or w/o a real widget in place
dojo.widget.fillFromTemplateCache = function(obj, templatePath, templateCssPath, templateString, avoidCache){
	// dojo.debug("avoidCache:", avoidCache);
	var tpath = templatePath || obj.templatePath;
	var cpath = templateCssPath || obj.templateCssPath;

	// DEPRECATED: use Uri objects, not strings
	if (tpath && !(tpath instanceof dojo.uri.Uri)) {
		tpath = dojo.uri.dojoUri(tpath);
		dojo.deprecated("templatePath should be of type dojo.uri.Uri");
	}
	if (cpath && !(cpath instanceof dojo.uri.Uri)) {
		cpath = dojo.uri.dojoUri(cpath);
		dojo.deprecated("templateCssPath should be of type dojo.uri.Uri");
	}
	
	var tmplts = dojo.widget._templateCache;
	if(!obj["widgetType"]) { // don't have a real template here
		do {
			var dummyName = "__dummyTemplate__" + dojo.widget._templateCache.dummyCount++;
		} while(tmplts[dummyName]);
		obj.widgetType = dummyName;
	}
	var wt = obj.widgetType;

	if((!obj.templateCssString)&&(cpath)&&(!dojo.widget._cssFiles[cpath])){
		obj.templateCssString = dojo.hostenv.getText(cpath);
		obj.templateCssPath = null;
		dojo.widget._cssFiles[cpath] = true;
	}
	if((obj["templateCssString"])&&(!obj.templateCssString["loaded"])){
		dojo.style.insertCssText(obj.templateCssString, null, cpath);
		if(!obj.templateCssString){ obj.templateCssString = ""; }
		obj.templateCssString.loaded = true;
	}

	var ts = tmplts[wt];
	if(!ts){
		tmplts[wt] = { "string": null, "node": null };
		if(avoidCache){
			ts = {};
		}else{
			ts = tmplts[wt];
		}
	}
	if(!obj.templateString){
		obj.templateString = templateString || ts["string"];
	}
	if(!obj.templateNode){
		obj.templateNode = ts["node"];
	}
	if((!obj.templateNode)&&(!obj.templateString)&&(tpath)){
		// fetch a text fragment and assign it to templateString
		// NOTE: we rely on blocking IO here!
		var tstring = dojo.hostenv.getText(tpath);
		if(tstring){
			var matches = tstring.match(/<body[^>]*>\s*([\s\S]+)\s*<\/body>/im);
			if(matches){
				tstring = matches[1];
			}
		}else{
			tstring = "";
		}
		obj.templateString = tstring;
		if(!avoidCache){
			tmplts[wt]["string"] = tstring;
		}
	}
	if((!ts["string"])&&(!avoidCache)){
		ts.string = obj.templateString;
	}
}
dojo.widget._templateCache.dummyCount = 0;

dojo.widget.attachProperties = ["dojoAttachPoint", "id"];
dojo.widget.eventAttachProperty = "dojoAttachEvent";
dojo.widget.onBuildProperty = "dojoOnBuild";

dojo.widget.attachTemplateNodes = function(rootNode, targetObj, events){
	// FIXME: this method is still taking WAAAY too long. We need ways of optimizing:
	//	a.) what we are looking for on each node
	//	b.) the nodes that are subject to interrogation (use xpath instead?)
	//	c.) how expensive event assignment is (less eval(), more connect())
	// var start = new Date();
	var elementNodeType = dojo.dom.ELEMENT_NODE;

	function trim(str){
		return str.replace(/^\s+|\s+$/g, "");
	}

	if(!rootNode){ 
		rootNode = targetObj.domNode;
	}

	if(rootNode.nodeType != elementNodeType){
		return;
	}
	// alert(events.length);

	var nodes = rootNode.all || rootNode.getElementsByTagName("*");
	var _this = targetObj;
	for(var x=-1; x<nodes.length; x++){
		var baseNode = (x == -1) ? rootNode : nodes[x];
		// FIXME: is this going to have capitalization problems?  Could use getAttribute(name, 0); to get attributes case-insensitve
		var attachPoint = [];
		for(var y=0; y<this.attachProperties.length; y++){
			var tmpAttachPoint = baseNode.getAttribute(this.attachProperties[y]);
			if(tmpAttachPoint){
				attachPoint = tmpAttachPoint.split(";");
				for(var z=0; z<attachPoint.length; z++){
					if(dojo.lang.isArray(targetObj[attachPoint[z]])){
						targetObj[attachPoint[z]].push(baseNode);
					}else{
						targetObj[attachPoint[z]]=baseNode;
					}
				}
				break;
			}
		}
		// continue;

		// FIXME: we need to put this into some kind of lookup structure
		// instead of direct assignment
		var tmpltPoint = baseNode.getAttribute(this.templateProperty);
		if(tmpltPoint){
			targetObj[tmpltPoint]=baseNode;
		}

		var attachEvent = baseNode.getAttribute(this.eventAttachProperty);
		if(attachEvent){
			// NOTE: we want to support attributes that have the form
			// "domEvent: nativeEvent; ..."
			var evts = attachEvent.split(";");
			for(var y=0; y<evts.length; y++){
				if((!evts[y])||(!evts[y].length)){ continue; }
				var thisFunc = null;
				var tevt = trim(evts[y]);
				if(evts[y].indexOf(":") >= 0){
					// oh, if only JS had tuple assignment
					var funcNameArr = tevt.split(":");
					tevt = trim(funcNameArr[0]);
					thisFunc = trim(funcNameArr[1]);
				}
				if(!thisFunc){
					thisFunc = tevt;
				}

				var tf = function(){ 
					var ntf = new String(thisFunc);
					return function(evt){
						if(_this[ntf]){
							_this[ntf](dojo.event.browser.fixEvent(evt, this));
						}
					};
				}();
				dojo.event.browser.addListener(baseNode, tevt, tf, false, true);
				// dojo.event.browser.addListener(baseNode, tevt, dojo.lang.hitch(_this, thisFunc));
			}
		}

		for(var y=0; y<events.length; y++){
			//alert(events[x]);
			var evtVal = baseNode.getAttribute(events[y]);
			if((evtVal)&&(evtVal.length)){
				var thisFunc = null;
				var domEvt = events[y].substr(4); // clober the "dojo" prefix
				thisFunc = trim(evtVal);
				var funcs = [thisFunc];
				if(thisFunc.indexOf(";")>=0){
					funcs = dojo.lang.map(thisFunc.split(";"), trim);
				}
				for(var z=0; z<funcs.length; z++){
					if(!funcs[z].length){ continue; }
					var tf = function(){ 
						var ntf = new String(funcs[z]);
						return function(evt){
							if(_this[ntf]){
								_this[ntf](dojo.event.browser.fixEvent(evt, this));
							}
						}
					}();
					dojo.event.browser.addListener(baseNode, domEvt, tf, false, true);
					// dojo.event.browser.addListener(baseNode, domEvt, dojo.lang.hitch(_this, funcs[z]));
				}
			}
		}

		var onBuild = baseNode.getAttribute(this.onBuildProperty);
		if(onBuild){
			eval("var node = baseNode; var widget = targetObj; "+onBuild);
		}
	}

}

dojo.widget.getDojoEventsFromStr = function(str){
	// var lstr = str.toLowerCase();
	var re = /(dojoOn([a-z]+)(\s?))=/gi;
	var evts = str ? str.match(re)||[] : [];
	var ret = [];
	var lem = {};
	for(var x=0; x<evts.length; x++){
		if(evts[x].legth < 1){ continue; }
		var cm = evts[x].replace(/\s/, "");
		cm = (cm.slice(0, cm.length-1));
		if(!lem[cm]){
			lem[cm] = true;
			ret.push(cm);
		}
	}
	return ret;
}

/*
dojo.widget.buildAndAttachTemplate = function(obj, templatePath, templateCssPath, templateString, targetObj) {
	this.buildFromTemplate(obj, templatePath, templateCssPath, templateString);
	var node = dojo.dom.createNodesFromText(obj.templateString, true)[0];
	this.attachTemplateNodes(node, targetObj||obj, dojo.widget.getDojoEventsFromStr(templateString));
	return node;
}
*/

dojo.declare("dojo.widget.DomWidget", dojo.widget.Widget, {
	initializer: function() {
		if((arguments.length>0)&&(typeof arguments[0] == "object")){
			this.create(arguments[0]);
		}
	},
								 
	templateNode: null,
	templateString: null,
	templateCssString: null,
	preventClobber: false,
	domNode: null, // this is our visible representation of the widget!
	containerNode: null, // holds child elements

	// Process the given child widget, inserting it's dom node as a child of our dom node
	// FIXME: should we support addition at an index in the children arr and
	// order the display accordingly? Right now we always append.
	addChild: function(widget, overrideContainerNode, pos, ref, insertIndex){
		if(!this.isContainer){ // we aren't allowed to contain other widgets, it seems
			dojo.debug("dojo.widget.DomWidget.addChild() attempted on non-container widget");
			return null;
		}else{
			this.addWidgetAsDirectChild(widget, overrideContainerNode, pos, ref, insertIndex);
			this.registerChild(widget, insertIndex);
		}
		return widget;
	},
	
	addWidgetAsDirectChild: function(widget, overrideContainerNode, pos, ref, insertIndex){
		if((!this.containerNode)&&(!overrideContainerNode)){
			this.containerNode = this.domNode;
		}
		var cn = (overrideContainerNode) ? overrideContainerNode : this.containerNode;
		if(!pos){ pos = "after"; }
		if(!ref){ 
			// if(!cn){ cn = document.body; }
			if(!cn){ cn = document.body; }
			ref = cn.lastChild; 
		}
		if(!insertIndex) { insertIndex = 0; }
		widget.domNode.setAttribute("dojoinsertionindex", insertIndex);

		// insert the child widget domNode directly underneath my domNode, in the
		// specified position (by default, append to end)
		if(!ref){
			cn.appendChild(widget.domNode);
		}else{
			// FIXME: was this meant to be the (ugly hack) way to support insert @ index?
			//dojo.dom[pos](widget.domNode, ref, insertIndex);

			// CAL: this appears to be the intended way to insert a node at a given position...
			if (pos == 'insertAtIndex'){
				// dojo.debug("idx:", insertIndex, "isLast:", ref === cn.lastChild);
				dojo.dom.insertAtIndex(widget.domNode, ref.parentNode, insertIndex);
			}else{
				// dojo.debug("pos:", pos, "isLast:", ref === cn.lastChild);
				if((pos == "after")&&(ref === cn.lastChild)){
					cn.appendChild(widget.domNode);
				}else{
					dojo.dom.insertAtPosition(widget.domNode, cn, pos);
				}
			}
		}
	},

	// Record that given widget descends from me
	registerChild: function(widget, insertionIndex){

		// we need to insert the child at the right point in the parent's 
		// 'children' array, based on the insertionIndex

		widget.dojoInsertionIndex = insertionIndex;

		var idx = -1;
		for(var i=0; i<this.children.length; i++){
			if (this.children[i].dojoInsertionIndex < insertionIndex){
				idx = i;
			}
		}

		this.children.splice(idx+1, 0, widget);

		widget.parent = this;
		widget.addedTo(this);
		
		// If this widget was created programatically, then it was erroneously added
		// to dojo.widget.manager.topWidgets.  Fix that here.
		delete dojo.widget.manager.topWidgets[widget.widgetId];
	},

	removeChild: function(widget){
		// detach child domNode from parent domNode
		dojo.dom.removeNode(widget.domNode);

		// remove child widget from parent widget
		return dojo.widget.DomWidget.superclass.removeChild.call(this, widget);
	},

	getFragNodeRef: function(frag){
		if( !frag || !frag["dojo:"+this.widgetType.toLowerCase()] ){
			dojo.raise("Error: no frag for widget type " + this.widgetType +
				", id " + this.widgetId + " (maybe a widget has set it's type incorrectly)");
		}
		return (frag ? frag["dojo:"+this.widgetType.toLowerCase()]["nodeRef"] : null);
	},
	
	// Replace source domNode with generated dom structure, and register
	// widget with parent.
	postInitialize: function(args, frag, parentComp){
		var sourceNodeRef = this.getFragNodeRef(frag);
		// Stick my generated dom into the output tree
		//alert(this.widgetId + ": replacing " + sourceNodeRef + " with " + this.domNode.innerHTML);
		if (parentComp && (parentComp.snarfChildDomOutput || !sourceNodeRef)){
			// Add my generated dom as a direct child of my parent widget
			// This is important for generated widgets, and also cases where I am generating an
			// <li> node that can't be inserted back into the original DOM tree
			parentComp.addWidgetAsDirectChild(this, "", "insertAtIndex", "",  args["dojoinsertionindex"], sourceNodeRef);
		} else if (sourceNodeRef){
			// Do in-place replacement of the my source node with my generated dom
			if(this.domNode && (this.domNode !== sourceNodeRef)){
				var oldNode = sourceNodeRef.parentNode.replaceChild(this.domNode, sourceNodeRef);
			}
		}

		// Register myself with my parent, or with the widget manager if
		// I have no parent
		// TODO: the code below erroneously adds all programatically generated widgets
		// to topWidgets (since we don't know who the parent is until after creation finishes)
		if ( parentComp ) {
			parentComp.registerChild(this, args.dojoinsertionindex);
		} else {
			dojo.widget.manager.topWidgets[this.widgetId]=this;
		}

		// Expand my children widgets
		if(this.isContainer){
			//alert("recurse from " + this.widgetId);
			// build any sub-components with us as the parent
			var fragParser = dojo.widget.getParser();
			fragParser.createSubComponents(frag, this);
		}
	},

	// method over-ride
	buildRendering: function(args, frag){
		// DOM widgets construct themselves from a template
		var ts = dojo.widget._templateCache[this.widgetType];
		if(	
			(!this.preventClobber)&&(
				(this.templatePath)||
				(this.templateNode)||
				(
					(this["templateString"])&&(this.templateString.length) 
				)||
				(
					(typeof ts != "undefined")&&( (ts["string"])||(ts["node"]) )
				)
			)
		){
			// if it looks like we can build the thing from a template, do it!
			this.buildFromTemplate(args, frag);
		}else{
			// otherwise, assign the DOM node that was the source of the widget
			// parsing to be the root node
			this.domNode = this.getFragNodeRef(frag);
		}
		this.fillInTemplate(args, frag); 	// this is where individual widgets
											// will handle population of data
											// from properties, remote data
											// sets, etc.
	},

	buildFromTemplate: function(args, frag){
		// var start = new Date();
		// copy template properties if they're already set in the templates object
		// dojo.debug("buildFromTemplate:", this);
		var avoidCache = false;
		if(args["templatecsspath"]){
			args["templateCssPath"] = args["templatecsspath"];
		}
		if(args["templatepath"]){
			avoidCache = true;
			args["templatePath"] = args["templatepath"];
		}
		dojo.widget.fillFromTemplateCache(	this, 
											args["templatePath"], 
											args["templateCssPath"],
											null,
											avoidCache);
		var ts = dojo.widget._templateCache[this.widgetType];
		if((ts)&&(!avoidCache)){
			if(!this.templateString.length){
				this.templateString = ts["string"];
			}
			if(!this.templateNode){
				this.templateNode = ts["node"];
			}
		}
		var matches = false;
		var node = null;
		// var tstr = new String(this.templateString); 
		var tstr = this.templateString; 
		// attempt to clone a template node, if there is one
		if((!this.templateNode)&&(this.templateString)){
			matches = this.templateString.match(/\$\{([^\}]+)\}/g);
			if(matches) {
				// if we do property replacement, don't create a templateNode
				// to clone from.
				var hash = this.strings || {};
				// FIXME: should this hash of default replacements be cached in
				// templateString?
				for(var key in dojo.widget.defaultStrings) {
					if(dojo.lang.isUndefined(hash[key])) {
						hash[key] = dojo.widget.defaultStrings[key];
					}
				}
				// FIXME: this is a lot of string munging. Can we make it faster?
				for(var i = 0; i < matches.length; i++) {
					var key = matches[i];
					key = key.substring(2, key.length-1);
					var kval = (key.substring(0, 5) == "this.") ? this[key.substring(5)] : hash[key];
					var value;
					if((kval)||(dojo.lang.isString(kval))){
						value = (dojo.lang.isFunction(kval)) ? kval.call(this, key, this.templateString) : kval;
						tstr = tstr.replace(matches[i], value);
					}
				}
			}else{
				// otherwise, we are required to instantiate a copy of the template
				// string if one is provided.
				
				// FIXME: need to be able to distinguish here what should be done
				// or provide a generic interface across all DOM implementations
				// FIMXE: this breaks if the template has whitespace as its first 
				// characters
				// node = this.createNodesFromText(this.templateString, true);
				// this.templateNode = node[0].cloneNode(true); // we're optimistic here
				this.templateNode = this.createNodesFromText(this.templateString, true)[0];
				ts.node = this.templateNode;
			}
		}
		if((!this.templateNode)&&(!matches)){ 
			dojo.debug("weren't able to create template!");
			return false;
		}else if(!matches){
			node = this.templateNode.cloneNode(true);
			if(!node){ return false; }
		}else{
			node = this.createNodesFromText(tstr, true)[0];
		}

		// recurse through the node, looking for, and attaching to, our
		// attachment points which should be defined on the template node.

		this.domNode = node;
		// dojo.profile.start("attachTemplateNodes");
		this.attachTemplateNodes(this.domNode, this);
		// dojo.profile.end("attachTemplateNodes");
		
		// relocate source contents to templated container node
		// this.containerNode must be able to receive children, or exceptions will be thrown
		if (this.isContainer && this.containerNode){
			var src = this.getFragNodeRef(frag);
			if (src){
				dojo.dom.moveChildren(src, this.containerNode);
			}
		}
	},

	attachTemplateNodes: function(baseNode, targetObj){
		if(!targetObj){ targetObj = this; }
		return dojo.widget.attachTemplateNodes(baseNode, targetObj, 
					dojo.widget.getDojoEventsFromStr(this.templateString));
	},

	fillInTemplate: function(){
		// dojo.unimplemented("dojo.widget.DomWidget.fillInTemplate");
	},
	
	// method over-ride
	destroyRendering: function(){
		try{
			delete this.domNode;
		}catch(e){ /* squelch! */ }
	},

	// FIXME: method over-ride
	cleanUp: function(){},
	
	getContainerHeight: function(){
		dojo.unimplemented("dojo.widget.DomWidget.getContainerHeight");
	},

	getContainerWidth: function(){
		dojo.unimplemented("dojo.widget.DomWidget.getContainerWidth");
	},

	createNodesFromText: function(){
		dojo.unimplemented("dojo.widget.DomWidget.createNodesFromText");
	}
});

dojo.provide("dojo.lfx.toggle");
dojo.require("dojo.lfx.*");

dojo.lfx.toggle.plain = {
	show: function(node, duration, easing, callback){
		dojo.style.show(node);
		if(dojo.lang.isFunction(callback)){ callback(); }
	},
	
	hide: function(node, duration, easing, callback){
		dojo.style.hide(node);
		if(dojo.lang.isFunction(callback)){ callback(); }
	}
}

dojo.lfx.toggle.fade = {
	show: function(node, duration, easing, callback){
		dojo.lfx.fadeShow(node, duration, easing, callback).play();
	},

	hide: function(node, duration, easing, callback){
		dojo.lfx.fadeHide(node, duration, easing, callback).play();
	}
}

dojo.lfx.toggle.wipe = {
	show: function(node, duration, easing, callback){
		dojo.lfx.wipeIn(node, duration, easing, callback).play();
	},

	hide: function(node, duration, easing, callback){
		dojo.lfx.wipeOut(node, duration, easing, callback).play();
	}
}

dojo.lfx.toggle.explode = {
	show: function(node, duration, easing, callback, explodeSrc){
		dojo.lfx.explode(explodeSrc||[0,0,0,0], node, duration, easing, callback).play();
	},

	hide: function(node, duration, easing, callback, explodeSrc){
		dojo.lfx.implode(node, explodeSrc||[0,0,0,0], duration, easing, callback).play();
	}
}

dojo.provide("dojo.widget.HtmlWidget");
dojo.require("dojo.widget.DomWidget");
dojo.require("dojo.html");
dojo.require("dojo.html.extras");
dojo.require("dojo.lang.extras");
dojo.require("dojo.lang.func");
dojo.require("dojo.lfx.toggle");

dojo.declare("dojo.widget.HtmlWidget", dojo.widget.DomWidget, {								 
	widgetType: "HtmlWidget",

	templateCssPath: null,
	templatePath: null,

	// for displaying/hiding widget
	toggle: "plain",
	toggleDuration: 150,

	animationInProgress: false,

	initialize: function(args, frag){
	},

	postMixInProperties: function(args, frag){
		// now that we know the setting for toggle, get toggle object
		// (default to plain toggler if user specified toggler not present)
		this.toggleObj =
			dojo.lfx.toggle[this.toggle.toLowerCase()] || dojo.lfx.toggle.plain;
	},

	getContainerHeight: function(){
		// NOTE: container height must be returned as the INNER height
		dojo.unimplemented("dojo.widget.HtmlWidget.getContainerHeight");
	},

	getContainerWidth: function(){
		return this.parent.domNode.offsetWidth;
	},

	setNativeHeight: function(height){
		var ch = this.getContainerHeight();
	},

	createNodesFromText: function(txt, wrap){
		return dojo.html.createNodesFromText(txt, wrap);
	},

	destroyRendering: function(finalize){
		try{
			if(!finalize){
				dojo.event.browser.clean(this.domNode);
			}
			this.domNode.parentNode.removeChild(this.domNode);
			delete this.domNode;
		}catch(e){ /* squelch! */ }
	},

	/////////////////////////////////////////////////////////
	// Displaying/hiding the widget
	/////////////////////////////////////////////////////////
	isShowing: function(){
		return dojo.style.isShowing(this.domNode);
	},

	toggleShowing: function(){
		// dojo.style.toggleShowing(this.domNode);
		if(this.isHidden){
			this.show();
		}else{
			this.hide();
		}
	},

	show: function(){
		this.animationInProgress=true;
		this.isHidden = false;
		this.toggleObj.show(this.domNode, this.toggleDuration, null,
			dojo.lang.hitch(this, this.onShow), this.explodeSrc);
	},

	// called after the show() animation has completed
	onShow: function(){
		this.animationInProgress=false;
	},

	hide: function(){
		this.animationInProgress = true;
		this.isHidden = true;
		this.toggleObj.hide(this.domNode, this.toggleDuration, null,
			dojo.lang.hitch(this, this.onHide), this.explodeSrc);
	},

	// called after the hide() animation has completed
	onHide: function(){
		this.animationInProgress=false;
	},

	//////////////////////////////////////////////////////////////////////////////
	// Sizing related methods
	//  If the parent changes size then for each child it should call either
	//   - resizeTo(): size the child explicitly
	//   - or onParentResized(): notify the child the the parent has changed size
	//////////////////////////////////////////////////////////////////////////////

	// Test if my size has changed.
	// If width & height are specified then that's my new size; otherwise,
	// query outerWidth/outerHeight of my domNode
	_isResized: function(w, h){
		// If I'm not being displayed then disregard (show() must
		// check if the size has changed)
		if(!this.isShowing()){ return false; }

		// If my parent has been resized and I have style="height: 100%"
		// or something similar then my size has changed too.
		w=w||dojo.style.getOuterWidth(this.domNode);
		h=h||dojo.style.getOuterHeight(this.domNode);
		if(this.width == w && this.height == h){ return false; }

		this.width=w;
		this.height=h;
		return true;
	},

	// Called when my parent has changed size, but my parent won't call resizeTo().
	// This is useful if my size is height:100% or something similar
	onParentResized: function(){
		if(!this._isResized()){ return; }
		this.onResized();
	},

	// Explicitly set this widget's size (in pixels).
	resizeTo: function(w, h){
		if(!this._isResized(w,h)){ return; }
		dojo.style.setOuterWidth(this.domNode, w);
		dojo.style.setOuterHeight(this.domNode, h);
		this.onResized();
	},

	resizeSoon: function(){
		if(this.isShowing()){
			dojo.lang.setTimeout(this, this.onResized, 0);
		}
	},

	// Called when my size has changed.
	// Must notify children if their size has (possibly) changed
	onResized: function(){
		dojo.lang.forEach(this.children, function(child){ child.onParentResized(); });
	}
});

dojo.kwCompoundRequire({
	common: ["dojo.xml.Parse", 
			 "dojo.widget.Widget", 
			 "dojo.widget.Parse", 
			 "dojo.widget.Manager"],
	browser: ["dojo.widget.DomWidget",
			  "dojo.widget.HtmlWidget"],
	dashboard: ["dojo.widget.DomWidget",
			  "dojo.widget.HtmlWidget"],
	svg: 	 ["dojo.widget.SvgWidget"]
});
dojo.provide("dojo.widget.*");

dojo.provide("dojo.math");

dojo.math.degToRad = function (x) { return (x*Math.PI) / 180; }
dojo.math.radToDeg = function (x) { return (x*180) / Math.PI; }

dojo.math.factorial = function (n) {
	if(n<1){ return 0; }
	var retVal = 1;
	for(var i=1;i<=n;i++){ retVal *= i; }
	return retVal;
}

//The number of ways of obtaining an ordered subset of k elements from a set of n elements
dojo.math.permutations = function (n,k) {
	if(n==0 || k==0) return 1;
	return (dojo.math.factorial(n) / dojo.math.factorial(n-k));
}

//The number of ways of picking n unordered outcomes from r possibilities
dojo.math.combinations = function (n,r) {
	if(n==0 || r==0) return 1;
	return (dojo.math.factorial(n) / (dojo.math.factorial(n-r) * dojo.math.factorial(r)));
}

dojo.math.bernstein = function (t,n,i) {
	return (dojo.math.combinations(n,i) * Math.pow(t,i) * Math.pow(1-t,n-i));
}

/**
 * Returns random numbers with a Gaussian distribution, with the mean set at
 * 0 and the variance set at 1.
 *
 * @return A random number from a Gaussian distribution
 */
dojo.math.gaussianRandom = function () {
	var k = 2;
	do {
		var i = 2 * Math.random() - 1;
		var j = 2 * Math.random() - 1;
		k = i * i + j * j;
	} while (k >= 1);
	k = Math.sqrt((-2 * Math.log(k)) / k);
	return i * k;
}

/**
 * Calculates the mean of an Array of numbers.
 *
 * @return The mean of the numbers in the Array
 */
dojo.math.mean = function () {
	var array = dojo.lang.isArray(arguments[0]) ? arguments[0] : arguments;
	var mean = 0;
	for (var i = 0; i < array.length; i++) { mean += array[i]; }
	return mean / array.length;
}

/**
 * Extends Math.round by adding a second argument specifying the number of
 * decimal places to round to.
 *
 * @param number The number to round
 * @param places The number of decimal places to round to
 * @return The rounded number
 */
// TODO: add support for significant figures
dojo.math.round = function (number, places) {
	if (!places) { var shift = 1; }
	else { var shift = Math.pow(10, places); }
	return Math.round(number * shift) / shift;
}

/**
 * Calculates the standard deviation of an Array of numbers
 *
 * @return The standard deviation of the numbers
 */
dojo.math.sd = function () {
	var array = dojo.lang.isArray(arguments[0]) ? arguments[0] : arguments;
	return Math.sqrt(dojo.math.variance(array));
}

/**
 * Calculates the variance of an Array of numbers
 *
 * @return The variance of the numbers
 */
dojo.math.variance = function () {
	var array = dojo.lang.isArray(arguments[0]) ? arguments[0] : arguments;
	var mean = 0, squares = 0;
	for (var i = 0; i < array.length; i++) {
		mean += array[i];
		squares += Math.pow(array[i], 2);
	}
	return (squares / array.length)
		- Math.pow(mean / array.length, 2);
}

/**
 * Like range() in python
**/
dojo.math.range = function(a, b, step) {
    if(arguments.length < 2) {
        b = a;
        a = 0;
    }
    if(arguments.length < 3) {
        step = 1;
    }

    var range = [];
    if(step > 0) {
        for(var i = a; i < b; i += step) {
            range.push(i);
        }
    } else if(step < 0) {
        for(var i = a; i > b; i += step) {
            range.push(i);
        }
    } else {
        throw new Error("dojo.math.range: step must be non-zero");
    }
    return range;
}

dojo.provide("dojo.math.curves");

dojo.require("dojo.math");

/* Curves from Dan's 13th lib stuff.
 * See: http://pupius.co.uk/js/Toolkit.Drawing.js
 *      http://pupius.co.uk/dump/dojo/Dojo.Math.js
 */

dojo.math.curves = {
	//Creates a straight line object
	Line: function(start, end) {
		this.start = start;
		this.end = end;
		this.dimensions = start.length;

		for(var i = 0; i < start.length; i++) {
			start[i] = Number(start[i]);
		}

		for(var i = 0; i < end.length; i++) {
			end[i] = Number(end[i]);
		}

		//simple function to find point on an n-dimensional, straight line
		this.getValue = function(n) {
			var retVal = new Array(this.dimensions);
			for(var i=0;i<this.dimensions;i++)
				retVal[i] = ((this.end[i] - this.start[i]) * n) + this.start[i];
			return retVal;
		}

		return this;
	},


	//Takes an array of points, the first is the start point, the last is end point and the ones in
	//between are the Bezier control points.
	Bezier: function(pnts) {
		this.getValue = function(step) {
			if(step >= 1) return this.p[this.p.length-1];	// if step>=1 we must be at the end of the curve
			if(step <= 0) return this.p[0];					// if step<=0 we must be at the start of the curve
			var retVal = new Array(this.p[0].length);
			for(var k=0;j<this.p[0].length;k++) { retVal[k]=0; }
			for(var j=0;j<this.p[0].length;j++) {
				var C=0; var D=0;
				for(var i=0;i<this.p.length;i++) {
					C += this.p[i][j] * this.p[this.p.length-1][0]
						* dojo.math.bernstein(step,this.p.length,i);
				}
				for(var l=0;l<this.p.length;l++) {
					D += this.p[this.p.length-1][0] * dojo.math.bernstein(step,this.p.length,l);
				}
				retVal[j] = C/D;
			}
			return retVal;
		}
		this.p = pnts;
		return this;
	},


	//Catmull-Rom Spline - allows you to interpolate a smooth curve through a set of points in n-dimensional space
	CatmullRom : function(pnts,c) {
		this.getValue = function(step) {
			var percent = step * (this.p.length-1);
			var node = Math.floor(percent);
			var progress = percent - node;

			var i0 = node-1; if(i0 < 0) i0 = 0;
			var i = node;
			var i1 = node+1; if(i1 >= this.p.length) i1 = this.p.length-1;
			var i2 = node+2; if(i2 >= this.p.length) i2 = this.p.length-1;

			var u = progress;
			var u2 = progress*progress;
			var u3 = progress*progress*progress;

			var retVal = new Array(this.p[0].length);
			for(var k=0;k<this.p[0].length;k++) {
				var x1 = ( -this.c * this.p[i0][k] ) + ( (2 - this.c) * this.p[i][k] ) + ( (this.c-2) * this.p[i1][k] ) + ( this.c * this.p[i2][k] );
				var x2 = ( 2 * this.c * this.p[i0][k] ) + ( (this.c-3) * this.p[i][k] ) + ( (3 - 2 * this.c) * this.p[i1][k] ) + ( -this.c * this.p[i2][k] );
				var x3 = ( -this.c * this.p[i0][k] ) + ( this.c * this.p[i1][k] );
				var x4 = this.p[i][k];

				retVal[k] = x1*u3 + x2*u2 + x3*u + x4;
			}
			return retVal;

		}


		if(!c) this.c = 0.7;
		else this.c = c;
		this.p = pnts;

		return this;
	},

	// FIXME: This is the bad way to do a partial-arc with 2 points. We need to have the user
	// supply the radius, otherwise we always get a half-circle between the two points.
	Arc : function(start, end, ccw) {
		var center = dojo.math.points.midpoint(start, end);
		var sides = dojo.math.points.translate(dojo.math.points.invert(center), start);
		var rad = Math.sqrt(Math.pow(sides[0], 2) + Math.pow(sides[1], 2));
		var theta = dojo.math.radToDeg(Math.atan(sides[1]/sides[0]));
		if( sides[0] < 0 ) {
			theta -= 90;
		} else {
			theta += 90;
		}
		dojo.math.curves.CenteredArc.call(this, center, rad, theta, theta+(ccw?-180:180));
	},

	// Creates an arc object, with center and radius (Top of arc = 0 degrees, increments clockwise)
	//  center => 2D point for center of arc
	//  radius => scalar quantity for radius of arc
	//  start  => to define an arc specify start angle (default: 0)
	//  end    => to define an arc specify start angle
	CenteredArc : function(center, radius, start, end) {
		this.center = center;
		this.radius = radius;
		this.start = start || 0;
		this.end = end;

		this.getValue = function(n) {
			var retVal = new Array(2);
			var theta = dojo.math.degToRad(this.start+((this.end-this.start)*n));

			retVal[0] = this.center[0] + this.radius*Math.sin(theta);
			retVal[1] = this.center[1] - this.radius*Math.cos(theta);

			return retVal;
		}

		return this;
	},

	// Special case of Arc (start = 0, end = 360)
	Circle : function(center, radius) {
		dojo.math.curves.CenteredArc.call(this, center, radius, 0, 360);
		return this;
	},

	Path : function() {
		var curves = [];
		var weights = [];
		var ranges = [];
		var totalWeight = 0;

		this.add = function(curve, weight) {
			if( weight < 0 ) { dojo.raise("dojo.math.curves.Path.add: weight cannot be less than 0"); }
			curves.push(curve);
			weights.push(weight);
			totalWeight += weight;
			computeRanges();
		}

		this.remove = function(curve) {
			for(var i = 0; i < curves.length; i++) {
				if( curves[i] == curve ) {
					curves.splice(i, 1);
					totalWeight -= weights.splice(i, 1)[0];
					break;
				}
			}
			computeRanges();
		}

		this.removeAll = function() {
			curves = [];
			weights = [];
			totalWeight = 0;
		}

		this.getValue = function(n) {
			var found = false, value = 0;
			for(var i = 0; i < ranges.length; i++) {
				var r = ranges[i];
				//w(r.join(" ... "));
				if( n >= r[0] && n < r[1] ) {
					var subN = (n - r[0]) / r[2];
					value = curves[i].getValue(subN);
					found = true;
					break;
				}
			}

			// FIXME: Do we want to assume we're at the end?
			if( !found ) {
				value = curves[curves.length-1].getValue(1);
			}

			for(j = 0; j < i; j++) {
				value = dojo.math.points.translate(value, curves[j].getValue(1));
			}
			return value;
		}

		function computeRanges() {
			var start = 0;
			for(var i = 0; i < weights.length; i++) {
				var end = start + weights[i] / totalWeight;
				var len = end - start;
				ranges[i] = [start, end, len];
				start = end;
			}
		}

		return this;
	}
};

dojo.provide("dojo.math.points");
dojo.require("dojo.math");

// TODO: add a Point class?
dojo.math.points = {
	translate: function(a, b) {
		if( a.length != b.length ) {
			dojo.raise("dojo.math.translate: points not same size (a:[" + a + "], b:[" + b + "])");
		}
		var c = new Array(a.length);
		for(var i = 0; i < a.length; i++) {
			c[i] = a[i] + b[i];
		}
		return c;
	},

	midpoint: function(a, b) {
		if( a.length != b.length ) {
			dojo.raise("dojo.math.midpoint: points not same size (a:[" + a + "], b:[" + b + "])");
		}
		var c = new Array(a.length);
		for(var i = 0; i < a.length; i++) {
			c[i] = (a[i] + b[i]) / 2;
		}
		return c;
	},

	invert: function(a) {
		var b = new Array(a.length);
		for(var i = 0; i < a.length; i++) { b[i] = -a[i]; }
		return b;
	},

	distance: function(a, b) {
		return Math.sqrt(Math.pow(b[0]-a[0], 2) + Math.pow(b[1]-a[1], 2));
	}
};

dojo.kwCompoundRequire({
	common: [
		["dojo.math", false, false],
		["dojo.math.curves", false, false],
		["dojo.math.points", false, false]
	]
});
dojo.provide("dojo.math.*");

