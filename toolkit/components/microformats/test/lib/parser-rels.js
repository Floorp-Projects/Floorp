/*!
	Parser rels
	All the functions that deal with microformats v2 rel structures
	
	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
	Dependencies  dates.js, domutils.js, html.js, isodate,js, text.js,  utilities.js, url.js
*/


var Modules = (function (modules) {
	
	// check parser module is loaded
	if(modules.Parser){
	
		/**
		 * finds rel=* structures
		 *
		 * @param  {DOM node} rootNode
		 * @return {Object}
		 */
		modules.Parser.prototype.findRels = function(rootNode) {
			var out = {
					'items': [],
					'rels': {},
					'rel-urls': {}
				},
				x,
				i,
				y,
				z,
				relList,
				items,
				item,
				value,
				arr;
	
			arr = modules.domUtils.getNodesByAttribute(rootNode, 'rel');
			x = 0;
			i = arr.length;
			while(x < i) {
				relList = modules.domUtils.getAttribute(arr[x], 'rel');
	
				if(relList) {
					items = relList.split(' ');
					
					
					// add rels
					z = 0;
					y = items.length;
					while(z < y) {
						item = modules.utils.trim(items[z]);
	
						// get rel value
						value = modules.domUtils.getAttrValFromTagList(arr[x], ['a', 'area'], 'href');
						if(!value) {
							value = modules.domUtils.getAttrValFromTagList(arr[x], ['link'], 'href');
						}
	
						// create the key
						if(!out.rels[item]) {
							out.rels[item] = [];
						}
	
						if(typeof this.options.baseUrl === 'string' && typeof value === 'string') {
					
							var resolved = modules.url.resolve(value, this.options.baseUrl);
							// do not add duplicate rels - based on resolved URLs
							if(out.rels[item].indexOf(resolved) === -1){
								out.rels[item].push( resolved );
							}
						}
						z++;
					}
					
					
					var url = null;
					if(modules.domUtils.hasAttribute(arr[x], 'href')){
						url = modules.domUtils.getAttribute(arr[x], 'href');
						if(url){
							url = modules.url.resolve(url, this.options.baseUrl );
						}
					}
	
					
					// add to rel-urls
					var relUrl = this.getRelProperties(arr[x]);
					relUrl.rels = items;
					// // do not add duplicate rel-urls - based on resolved URLs
					if(url && out['rel-urls'][url] === undefined){
						out['rel-urls'][url] = relUrl;
					}
	
			
				}
				x++;
			}
			return out;
		};
		
		
		/**
		 * gets the properties of a rel=*
		 *
		 * @param  {DOM node} node
		 * @return {Object}
		 */
		modules.Parser.prototype.getRelProperties = function(node){
			var obj = {};
			
			if(modules.domUtils.hasAttribute(node, 'media')){
				obj.media = modules.domUtils.getAttribute(node, 'media');
			}
			if(modules.domUtils.hasAttribute(node, 'type')){
				obj.type = modules.domUtils.getAttribute(node, 'type');
			}
			if(modules.domUtils.hasAttribute(node, 'hreflang')){
				obj.hreflang = modules.domUtils.getAttribute(node, 'hreflang');
			}
			if(modules.domUtils.hasAttribute(node, 'title')){
				obj.title = modules.domUtils.getAttribute(node, 'title');
			}
			if(modules.utils.trim(this.getPValue(node, false)) !== ''){
				obj.text = this.getPValue(node, false);
			}	
			
			return obj;
		};
		
		
		/**
		 * finds any alt rel=* mappings for a given node/microformat
		 *
		 * @param  {DOM node} node
		 * @param  {String} ufName
		 * @return {String || undefined}
		 */
		modules.Parser.prototype.findRelImpied = function(node, ufName) {
			var out,
				map,
				i;
	
			map = this.getMapping(ufName);
			if(map) {
				for(var key in map.properties) {
					if (map.properties.hasOwnProperty(key)) {
						var prop = map.properties[key],
							propName = (prop.map) ? prop.map : 'p-' + key,
							relCount = 0;
		
						// is property an alt rel=* mapping 
						if(prop.relAlt && modules.domUtils.hasAttribute(node, 'rel')) {
							i = prop.relAlt.length;
							while(i--) {
								if(modules.domUtils.hasAttributeValue(node, 'rel', prop.relAlt[i])) {
									relCount++;
								}
							}
							if(relCount === prop.relAlt.length) {
								out = propName;
							}
						}
					}
				}
			}
			return out;
		};
		
		
		/**
		 * returns whether a node or its children has rel=* microformat
		 *
		 * @param  {DOM node} node
		 * @return {Boolean}
		 */
		modules.Parser.prototype.hasRel = function(node) {
			return (this.countRels(node) > 0);
		};
		
		
		/**
		 * returns the number of rel=* microformats
		 *
		 * @param  {DOM node} node
		 * @return {Int}
		 */
		modules.Parser.prototype.countRels = function(node) {
			if(node){
				return modules.domUtils.getNodesByAttribute(node, 'rel').length;
			}
			return 0;
		};
	
	
		
	}

	return modules;

} (Modules || {}));