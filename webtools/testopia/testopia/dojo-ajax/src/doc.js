/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.doc");
dojo.require("dojo.io.*");
dojo.require("dojo.event.topic");
dojo.require("dojo.rpc.JotService");
dojo.require("dojo.dom");

/*
 * TODO:
 *
 * Package summary needs to compensate for "is"
 * Handle host environments
 * Deal with dojo.widget weirdness
 * Parse parameters
 * Limit function parameters to only the valid ones (Involves packing parameters onto meta during rewriting)
 * Package display page
 *
 */

dojo.doc._count = 0;
dojo.doc._keys = {};
dojo.doc._myKeys = [];
dojo.doc._callbacks = {function_names: []};
dojo.doc._cache = {}; // Saves the JSON objects in cache
dojo.doc._rpc = new dojo.rpc.JotService;
dojo.doc._rpc.serviceUrl = "http://dojotoolkit.org/~pottedmeat/jsonrpc.php";

dojo.doc.functionNames = function(/*mixed*/ selectKey, /*Function*/ callback){
	// summary: Returns an ordered list of package and function names.
	dojo.debug("functionNames()");
	if(!selectKey){
		selectKey = ++dojo.doc._count;
	}
	dojo.doc._buildCache({
		type: "function_names",
		callbacks: [dojo.doc._functionNames, callback],
		selectKey: selectKey
	});
}

dojo.doc._functionNames = function(/*String*/ type, /*Array*/ data, /*Object*/ evt){
	var searchData = [];
	for(var key in data){
		// Add the package if it doesn't exist in its children
		if(!dojo.lang.inArray(data[key], key)){
			searchData.push([key, key]);
		}
		// Add the functions
		for(var pkg_key in data[key]){
			searchData.push([data[key][pkg_key], data[key][pkg_key]]);
		}
	}

	searchData = searchData.sort(dojo.doc._sort);

	if(evt.callbacks && evt.callbacks.length){
		var callback = evt.callbacks.shift();
		callback.call(null, type, searchData, evt);
	}
}

dojo.doc.getMeta = function(/*mixed*/ selectKey, /*Function*/ callback, /*Function*/ name, /*String?*/ id){
	// summary: Gets information about a function in regards to its meta data
	dojo.debug("getMeta(" + name + ")");
	if(!selectKey){
		selectKey = ++dojo.doc._count;
	}
	dojo.doc._buildCache({
		type: "meta",
		callbacks: [callback],
		name: name,
		id: id,
		selectKey: selectKey
	});
}

dojo.doc._getMeta = function(/*String*/ type, /*Object*/ data, /*Object*/ evt){
	dojo.debug("_getMeta(" + evt.name + ") has package: " + evt.pkg + " with: " + type);
	if("load" == type && evt.pkg){
		evt.type = "meta";
		dojo.doc._buildCache(evt);
	}else{
		if(evt.callbacks && evt.callbacks.length){
			var callback = evt.callbacks.shift();
			callback.call(null, "error", {}, evt);
		}
	}
}

dojo.doc.getSrc = function(/*mixed*/ selectKey, /*Function*/ callback, /*String*/ name, /*String?*/ id){
	// summary: Gets src file (created by the doc parser)
	dojo.debug("getSrc()");
	if(!selectKey){
		selectKey = ++dojo.doc._count;
	}	
	dojo.doc._buildCache({
		type: "src",
		callbacks: [callback],
		name: name,
		id: id,
		selectKey: selectKey
	});
}

dojo.doc._getSrc = function(/*String*/ type, /*Object*/ data, /*Object*/ evt){
	dojo.debug("_getSrc()");
	if(evt.pkg){	
		evt.type = "src";
		dojo.doc._buildCache(evt);
	}else{
		if(evt.callbacks && evt.callbacks.length){
			var callback =  evt.callbacks.shift();
			callback.call(null, "error", {}, evt);
		}
	}
}

dojo.doc.getDoc = function(/*mixed*/ selectKey, /*Function*/ callback, /*String*/ name, /*String?*/ id){
	// summary: Gets external documentation stored on jot
	dojo.debug("getDoc()");
	if(!selectKey){
		selectKey = ++dojo.doc._count;
	}
	var input = {
		type: "doc",
		callbacks: [callback],
		name: name,
		id: id,
		selectKey: selectKey
	}
	dojo.doc.functionPackage(dojo.doc._getDoc, input);
}

dojo.doc._getDoc = function(/*String*/ type, /*Object*/ data, /*Object*/ evt){
	dojo.debug("_getDoc(" + evt.pkg + "/" + evt.name + ")");
	
	dojo.doc._keys[evt.selectKey] = {count: 0};

	var search = {};
	search.forFormName = "DocFnForm";
	search.limit = 1;

	if(!evt.id){
		search.filter = "it/DocFnForm/require = '" + evt.pkg + "' and it/DocFnForm/name = '" + evt.name + "' and not(it/DocFnForm/id)";
	}else{
		search.filter = "it/DocFnForm/require = '" + evt.pkg + "' and it/DocFnForm/name = '" + evt.name + "' and it/DocFnForm/id = '" + evt.id + "'";
	}
	
	dojo.doc._rpc.callRemote("search", search).addCallbacks(function(data){ evt.type = "fn"; dojo.doc._gotDoc("load", data.list[0], evt); }, function(data){ evt.type = "fn"; dojo.doc._gotDoc("error", {}, evt); });
	
	search.forFormName = "DocParamForm";

	if(!evt.id){
		search.filter = "it/DocParamForm/fns = '" + evt.pkg + "=>" + evt.name + "'";
	}else{
		search.filter = "it/DocParamForm/fns = '" + evt.pkg + "=>" + evt.name + "=>" + evt.id + "'";
	}
	delete search.limit;

	dojo.doc._rpc.callRemote("search", search).addCallbacks(function(data){ evt.type = "param"; dojo.doc._gotDoc("load", data.list, evt); }, function(data){ evt.type = "param"; dojo.doc._gotDoc("error", {}, evt); });
}

dojo.doc._gotDoc = function(/*String*/ type, /*Array*/ data, /*Object*/ evt){
	dojo.debug("_gotDoc(" + evt.type + ") for " + evt.selectKey);
	dojo.doc._keys[evt.selectKey][evt.type] = data;
	if(++dojo.doc._keys[evt.selectKey].count == 2){
		dojo.debug("_gotDoc() finished");
		var keys = dojo.doc._keys[evt.selectKey];
		var description = '';
		if(!keys.fn){
			keys.fn = {}
		}
		if(keys.fn["main/text"]){
			description = dojo.dom.createDocumentFromText(keys.fn["main/text"]).childNodes[0].innerHTML;
			if(!description){
				description = keys.fn["main/text"];
			}			
		}
		data = {
			description: description,
			returns: keys.fn["DocFnForm/returns"],
			id: keys.fn["DocFnForm/id"],
			parameters: {},
			variables: []
		}
		for(var i = 0, param; param = keys["param"][i]; i++){
			data.parameters[param["DocParamForm/name"]] = {
				description: param["DocParamForm/desc"]
			};
		}

		delete dojo.doc._keys[evt.selectKey];
		
		if(evt.callbacks && evt.callbacks.length){
			var callback = evt.callbacks.shift();
			callback.call(null, "load", data, evt);
		}
	}
}

dojo.doc.getPkgMeta = function(/*mixed*/ selectKey, /*Function*/ callback, /*String*/ name){
	dojo.debug("getPkgMeta(" + name + ")");
	if(!selectKey){
		selectKey = ++dojo.doc._count;
	}
	dojo.doc._buildCache({
		type: "pkgmeta",
		callbacks: [callback],
		name: name,
		selectKey: selectKey
	});
}

dojo.doc._getPkgMeta = function(/*Object*/ input){
	dojo.debug("_getPkgMeta(" + input.name + ")");
	input.type = "pkgmeta";
	dojo.doc._buildCache(input);
}

dojo.doc._onDocSearch = function(/*Object*/ input){
	dojo.debug("_onDocSearch(" + input.name + ")");
	if(!input.name){
		return;
	}
	if(!input.selectKey){
		input.selectKey = ++dojo.doc._count;
	}
	input.callbacks = [dojo.doc._onDocSearchFn];
	input.name = input.name.toLowerCase();
	input.type = "function_names";

	dojo.doc._buildCache(input);
}

dojo.doc._onDocSearchFn = function(/*String*/ type, /*Array*/ data, /*Object*/ evt){
	dojo.debug("_onDocSearchFn(" + evt.name + ")");
	var packages = [];
	var size = 0;
	pkgLoop:
	for(var pkg in data){
		for(var i = 0, fn; fn = data[pkg][i]; i++){
			if(fn.toLowerCase().indexOf(evt.name) > -1){
				// Build a list of all packages that need to be loaded and their loaded state.
				++size;
				packages.push(pkg);
				continue pkgLoop;
			}
		}
	}
	dojo.doc._keys[evt.selectKey] = {};
	dojo.doc._keys[evt.selectKey].pkgs = packages;
	dojo.doc._keys[evt.selectKey].pkg = evt.name; // Remember what we were searching for
	dojo.doc._keys[evt.selectKey].loaded = 0;
	for(var i = 0, pkg; pkg = packages[i]; i++){
		setTimeout("dojo.doc.getPkgMeta(\"" + evt.selectKey + "\", dojo.doc._onDocResults, \"" + pkg + "\");", i*10);
	}
}

dojo.doc._onDocResults = function(/*String*/ type, /*Object*/ data, /*Object*/ evt){
	dojo.debug("_onDocResults(" + evt.name + "/" + dojo.doc._keys[evt.selectKey].pkg + ") " + type);
	++dojo.doc._keys[evt.selectKey].loaded;

	if(dojo.doc._keys[evt.selectKey].loaded == dojo.doc._keys[evt.selectKey].pkgs.length){
		var info = dojo.doc._keys[evt.selectKey];
		var pkgs = info.pkgs;
		var name = info.pkg;
		delete dojo.doc._keys[evt.selectKey];
		var results = {selectKey: evt.selectKey, docResults: []};
		data = dojo.doc._cache;

		for(var i = 0, pkg; pkg = pkgs[i]; i++){
			if(!data[pkg]){
				continue;
			}
			for(var fn in data[pkg]["meta"]){
				if(fn.toLowerCase().indexOf(name) == -1){
					continue;
				}
				if(fn != "requires"){
					for(var sig in data[pkg]["meta"][fn]){
						var result = {
							pkg: pkg,
							name: fn,
							summary: ""
						}
						if(data[pkg]["meta"][fn][sig]["summary"]){
							result.summary = data[pkg]["meta"][fn][sig]["summary"];
						}
						results.docResults.push(result);
					}
				}
			}
		}

		dojo.debug("Publishing docResults");
		dojo.event.topic.publish("docResults", results);
	}
}

// Get doc
// Get meta
// Get src
// Get function signature
dojo.doc._onDocSelectFunction = function(/*Object*/ input){
	dojo.debug("_onDocSelectFunction(" + input.name + ")");
	if(!input.name){
		return false;
	}
	if(!input.selectKey){
		input.selectKey = ++dojo.doc._count;
	}

	dojo.doc._keys[input.selectKey] = {size: 0};
	dojo.doc._myKeys[++dojo.doc._count] = {selectKey: input.selectKey, type: "meta"}
	dojo.doc.getMeta(dojo.doc._count, dojo.doc._onDocSelectResults, input.name);
	dojo.doc._myKeys[++dojo.doc._count] = {selectKey: input.selectKey, type: "src"}
	dojo.doc.getSrc(dojo.doc._count, dojo.doc._onDocSelectResults, input.name);
	dojo.doc._myKeys[++dojo.doc._count] = {selectKey: input.selectKey, type: "doc"}
	dojo.doc.getDoc(dojo.doc._count, dojo.doc._onDocSelectResults, input.name);
}

dojo.doc._onDocSelectResults = function(/*String*/ type, /*Object*/ data, /*Object*/ evt){
	dojo.debug("dojo.doc._onDocSelectResults(" + evt.type + ", " + evt.name + ")");
	var myKey = dojo.doc._myKeys[evt.selectKey];
	dojo.doc._keys[myKey.selectKey][myKey.type] = data;
	dojo.doc._keys[myKey.selectKey].size;
	if(++dojo.doc._keys[myKey.selectKey].size == 3){
		var key = dojo.lang.mixin(evt, dojo.doc._keys[myKey.selectKey]);
		delete key.size;
		dojo.debug(dojo.json.serialize(key));
		dojo.debug("Publishing docFunctionDetail");
		dojo.event.topic.publish("docFunctionDetail", key);
		delete dojo.doc._keys[myKey.selectKey];
		delete dojo.doc._myKeys[evt.selectKey];
	}
}

dojo.doc._buildCache = function(/*Object*/ input){
	dojo.debug("_buildCache() type: " + input.type);
	if(input.type == "function_names"){
		if(!dojo.doc._cache["function_names"]){
			dojo.debug("_buildCache() new cache");
			if(input.callbacks && input.callbacks.length){
				dojo.doc._callbacks.function_names.push([input, input.callbacks.shift()]);
			}
			dojo.doc._cache["function_names"] = {loading: true};
			dojo.io.bind({
				url: "json/function_names",
				mimetype: "text/json",
				error: function(type, data, evt){
					for(var i = 0, callback; callback = dojo.doc._callbacks.function_names[i]; i++){
						callback[1].call(null, "error", {}, callback[0]);
					}
				},
				load: function(type, data, evt){
					for(var key in data){
						// Packages starting with _ have a parent of "dojo"
						if(key.charAt(0) == "_"){
							var new_key = "dojo" + key.substring(1, key.length);
							data[new_key] = data[key];
							delete data[key];
							key = new_key;
						}
						// Function names starting with _ have a parent of their package name
						for(var pkg_key in data[key]){
							if(data[key][pkg_key].charAt(0) == "_"){
								var new_value = key + data[key][pkg_key].substring(1, data[key][pkg_key].length);
								data[key][pkg_key] = new_value;
							}
						}
						// Save data to the cache
					}
					dojo.doc._cache['function_names'] = data;
					for(var i = 0, callback; callback = dojo.doc._callbacks.function_names[i]; i++){
						callback[1].call(null, "load", data, callback[0]);
					}
				}
			});
		}else if(dojo.doc._cache["function_names"].loading){
			dojo.debug("_buildCache() loading cache");
			if(input.callbacks && input.callbacks.length){
				dojo.doc._callbacks.function_names.push([input, input.callbacks.shift()]);
			}
		}else{
			dojo.debug("_buildCache() from cache");
			if(input.callbacks && input.callbacks.length){
				var callback = input.callbacks.shift();
				callback.call(null, "load", dojo.doc._cache["function_names"], input);
			}
		}
	}else if(input.type == "meta" || input.type == "src"){
		if(!input.pkg){
			if(input.type == "meta"){
				dojo.doc.functionPackage(dojo.doc._getMeta, input);
			}else{
				dojo.doc.functionPackage(dojo.doc._getSrc, input);
			}
		}else{
			try{
				if(input.id){
					var cached = dojo.doc._cache[input.pkg][input.name][input.id][input.type];
				}else{
					var cached = dojo.doc._cache[input.pkg][input.name][input.type];
				}
			}catch(e){}

			if(cached){
				if(input.callbacks && input.callbacks.length){
					var callback = input.callbacks.shift();
					callback.call(null, "load", cached, input);
					return;
				}
			}

			dojo.debug("Finding " + input.type + " for: " + input.pkg + ", function: " + input.name + ", id: " + input.id);

			var name = input.name.replace(new RegExp('^' + input.pkg.replace(/\./g, "\\.")), "_");
			var pkg = input.pkg.replace(/^(dojo|\*)/g, "_");

			var mimetype = "text/json";
			if(input.type == "src"){
				mimetype = "text/plain"
			}

			var url;
			if(input.id){
				url = "json/" + pkg + "/" + name + "/" + input.id + "/" + input.type;
			}else{
				url = "json/" + pkg + "/" + name + "/" + input.type;		
			}

			dojo.io.bind({
				url: url,
				input: input,
				mimetype: mimetype,
				error: function(type, data, evt, args){
					if(args.input.callbacks && args.input.callbacks.length){
						if(!data){
							data = {};
						}
						if(!dojo.doc._cache[args.input.pkg]){
							dojo.doc._cache[args.input.pkg] = {};
						}
						if(!dojo.doc._cache[args.input.pkg][args.input.name]){
							dojo.doc._cache[args.input.pkg][args.input.name] = {};
						}
						if(args.input.type == "meta"){
							if(args.input.id){
								data.sig = dojo.doc._cache[args.input.pkg][args.input.name][args.input.id].sig;
								data.params = dojo.doc._cache[args.input.pkg][args.input.name][args.input.id].params;
							}else{
								data.sig = dojo.doc._cache[args.input.pkg][args.input.name].sig;								
								data.params = dojo.doc._cache[args.input.pkg][args.input.name].params;
							}
						}
						var callback = args.input.callbacks.shift();
						callback.call(null, "error", data, args.input);
					}
				},
				load: function(type, data, evt, args){
					if(!data){
						data = {};
					}
					if(!dojo.doc._cache[args.input.pkg]){
						dojo.doc._cache[args.input.pkg] = {};
					}
					if(!dojo.doc._cache[args.input.pkg][args.input.name]){
						dojo.doc._cache[args.input.pkg][args.input.name] = {};
					}
					if(args.input.id){
						dojo.doc._cache[args.input.pkg][args.input.name][args.input.id][args.input.type] = data;
						if(args.input.type == "meta"){
							data.sig = dojo.doc._cache[args.input.pkg][args.input.name][args.input.id].sig;
							data.params = dojo.doc._cache[args.input.pkg][args.input.name][args.input.id].params;
						}
					}else{
						dojo.doc._cache[args.input.pkg][args.input.name][args.input.type] = data;
						if(args.input.type == "meta"){
							data.sig = dojo.doc._cache[args.input.pkg][args.input.name].sig;
							data.params = dojo.doc._cache[args.input.pkg][args.input.name].params;
						}
					}
					if(args.input.callbacks && args.input.callbacks.length){
						var callback = input.callbacks.shift();
						callback.call(null, "load", data, args.input);
					}
				}
			});
		}
	}else if(input.type == "pkgmeta"){
		try{
			var cached = dojo.doc._cache[input.name]["meta"];
		}catch(e){}

		if(cached){
			if(input.callbacks && input.callbacks.length){
				var callback = input.callbacks.shift();
				callback.call(null, "load", cached, input);
				return;
			}
		}

		dojo.debug("Finding package meta for: " + input.name);

		var pkg = input.name.replace(/^(dojo|\*)/g, "_");

		dojo.io.bind({
			url: "json/" + pkg + "/meta",
			input: input,
			mimetype: "text/json",
			error: function(type, data, evt, args){
				if(args.input.callbacks && args.input.callbacks.length){
					var callback = args.input.callbacks.shift();
					callback.call(null, "error", {}, args.input);
				}
			},
			load: function(type, data, evt, args){
				if(!dojo.doc._cache[args.input.name]){
					dojo.doc._cache[args.input.name] = {};
				}
				
				for(var key in data){
					if(key != "requires"){
						if(key.charAt(0) == "_"){
							var new_key = args.input.name + key.substring(1, key.length);
							data[new_key] = data[key];
							delete data[key];
							for(var sig in data[new_key]){
								if(sig == "is"){
									continue;
								}
								var real_sig = sig;
								if(sig.charAt(0) == "("){
									real_sig = "undefined " + real_sig;
								}
								real_sig = real_sig.split("(");
								real_sig = real_sig[0] + new_key + "(" + real_sig[1];
								var parameters = {};
								var parRegExp = /(?:\(|,)([^,\)]+)/g;
								var result;
								while(result = parRegExp.exec(real_sig)){
									var parts = result[1].split(" ");
									if(parts.length > 1){
										var pName = parts.pop();
										var pType = parts.join(" ");
										var pOpt = false;
										if(pType.indexOf("?") != -1){
											pType = pType.replace("?", "");
											pOpt = true;
										}
										parameters[pName] = {type: pType, opt: pOpt};
									}else{
										parameters[parts[0]] = {type: "", opt: false};
									}
								}
								real_sig = real_sig.replace(/\?/g, "");
								if(data[new_key][sig].summary || dojo.lang.isArray(data[new_key][sig])){
									if(!dojo.doc._cache[args.input.name][new_key]){
										dojo.doc._cache[args.input.name][new_key] = {};
									}
									dojo.doc._cache[args.input.name][new_key].sig = real_sig;
									dojo.doc._cache[args.input.name][new_key].params = parameters;
								}else{
									// Polymorphic sigs
								}
							}
						}
					}
				}

				dojo.doc._cache[args.input.name]["meta"] = data;
				if(input.callbacks && input.callbacks.length){
					var callback = input.callbacks.shift();
					callback.call(null, "load", data, input);
				}
			}
		});
	}
}

dojo.doc.selectFunction = function(/*String*/ name, /*String?*/ id){
	// summary: The combined information
}



dojo.doc.savePackage = function(/*String*/ name, /*String*/ description){
	dojo.doc._rpc.callRemote(
		"saveForm",
		{
			form: "DocPkgForm",
			path: "/WikiHome/DojoDotDoc/id",
			pname1: "main/text",
			pvalue1: "Test"
		}
	).addCallbacks(dojo.doc._results, dojo.doc._results);
}

dojo.doc.functionPackage = function(/*Function*/ callback, /*Object*/ input){
	dojo.debug("functionPackage() name: " + input.name + " for type: " + input.type);
	input.type = "function_names";
	input.callbacks.unshift(callback);
	input.callbacks.unshift(dojo.doc._functionPackage);
	dojo.doc._buildCache(input);
}

dojo.doc._functionPackage = function(/*String*/ type, /*Array*/ data, /*Object*/ evt){
	dojo.debug("_functionPackage() name: " + evt.name + " for: " + evt.type + " with: " + type);
	evt.pkg = '';

	var data = dojo.doc._cache['function_names'];
	for(var key in data){
		if(dojo.lang.inArray(data[key], evt.name)){
			evt.pkg = key;
			break;
		}
	}

	if(evt.callbacks && evt.callbacks.length){
		var callback = evt.callbacks.shift();
		callback.call(null, type, data[key], evt);
	}
}

dojo.doc._sort = function(a, b){
	if(a[0] < b[0]){
		return -1;
	}
	if(a[0] > b[0]){
		return 1;
	}
  return 0;
}

dojo.event.topic.registerPublisher("docSearch");  	
dojo.event.topic.registerPublisher("docResults");  	
dojo.event.topic.registerPublisher("docSelectFunction");  	
dojo.event.topic.registerPublisher("docFunctionDetail");

dojo.event.topic.subscribe("docSearch", dojo.doc, "_onDocSearch");
dojo.event.topic.subscribe("docSelectFunction", dojo.doc, "_onDocSelectFunction");
