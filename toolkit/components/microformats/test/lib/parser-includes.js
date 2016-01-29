/*!
	Parser includes
	All the functions that deal with microformats v1 include rules
	
	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
	Dependencies  dates.js, domutils.js, html.js, isodate,js, text.js, utilities.js
*/


var Modules = (function (modules) {
	
	// check parser module is loaded
	if(modules.Parser){
	
		
		/**
		 * appends clones of include Nodes into the DOM structure
		 *
		 * @param  {DOM node} rootNode
		 */	
		modules.Parser.prototype.addIncludes = function(rootNode) {
			this.addAttributeIncludes(rootNode, 'itemref');
			this.addAttributeIncludes(rootNode, 'headers');
			this.addClassIncludes(rootNode);
		};
	
		
		/**
		 * appends clones of include Nodes into the DOM structure for attribute based includes
		 *
		 * @param  {DOM node} rootNode
		 * @param  {String} attributeName
		 */
		modules.Parser.prototype.addAttributeIncludes = function(rootNode, attributeName) {
			var arr,
				idList,
				i,
				x,
				z,
				y;
	
			arr = modules.domUtils.getNodesByAttribute(rootNode, attributeName);
			x = 0;
			i = arr.length;
			while(x < i) {
				idList = modules.domUtils.getAttributeList(arr[x], attributeName);
				if(idList) {
					z = 0;
					y = idList.length;
					while(z < y) {
						this.apppendInclude(arr[x], idList[z]);
						z++;
					}
				}
				x++;
			}
		};
	
		
		/**
		 * appends clones of include Nodes into the DOM structure for class based includes
		 *
		 * @param  {DOM node} rootNode
		 */
		modules.Parser.prototype.addClassIncludes = function(rootNode) {
			var id,
				arr,
				x = 0,
				i;
	
			arr = modules.domUtils.getNodesByAttributeValue(rootNode, 'class', 'include');
			i = arr.length;
			while(x < i) {
				id = modules.domUtils.getAttrValFromTagList(arr[x], ['a'], 'href');
				if(!id) {
					id = modules.domUtils.getAttrValFromTagList(arr[x], ['object'], 'data');
				}
				this.apppendInclude(arr[x], id);
				x++;
			}
		};
	
	
		/**
		 * appends a clone of an include into another Node using Id
		 *
		 * @param  {DOM node} rootNode
		 * @param  {Stringe} id
		 */
		modules.Parser.prototype.apppendInclude = function(node, id){
			var include,
				clone;
	
			id = modules.utils.trim(id.replace('#', ''));
			include = modules.domUtils.getElementById(this.document, id);
			if(include) {
				clone = modules.domUtils.clone(include);
				this.markIncludeChildren(clone);
				modules.domUtils.appendChild(node, clone);
			}
		};
	
		
		/**
		 * adds an attribute marker to all the child microformat roots 
		 *
		 * @param  {DOM node} rootNode
		 */ 
		modules.Parser.prototype.markIncludeChildren = function(rootNode) {
			var arr,
				x,
				i;
	
			// loop the array and add the attribute
			arr = this.findRootNodes(rootNode);
			x = 0;
			i = arr.length;
			modules.domUtils.setAttribute(rootNode, 'data-include', 'true');
			modules.domUtils.setAttribute(rootNode, 'style', 'display:none');
			while(x < i) {
				modules.domUtils.setAttribute(arr[x], 'data-include', 'true');
				x++;
			}
		};
		
		
		/**
		 * removes all appended include clones from DOM 
		 *
		 * @param  {DOM node} rootNode
		 */ 
		modules.Parser.prototype.removeIncludes = function(rootNode){
			var arr,
				i;
	
			// remove all the items that were added as includes
			arr = modules.domUtils.getNodesByAttribute(rootNode, 'data-include');
			i = arr.length;
			while(i--) {
				modules.domUtils.removeChild(rootNode,arr[i]);
			}
		};
	
		
	}

	return modules;

} (Modules || {}));