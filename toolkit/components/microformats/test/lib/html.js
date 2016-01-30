/*
	html 
	Extracts a HTML string from DOM nodes. Was created to get around the issue of not being able to exclude the content 
	of nodes with the 'data-include' attribute. DO NOT replace with functions such as innerHTML as it will break a 
	number of microformat include patterns.
	
	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-node/master/license.txt
	Dependencies  utilities.js, domutils.js
*/


var Modules = (function (modules) {
	
	modules.html = {
		
		// elements which are self-closing
		selfClosingElt: ['area', 'base', 'br', 'col', 'hr', 'img', 'input', 'link', 'meta', 'param', 'command', 'keygen', 'source'],
	

		/**
		 * parse the html string from DOM Node
		 *
		 * @param  {DOM Node} node
		 * @return {String}
		 */ 
		parse: function( node ){
			var out = '',
				j = 0;
	
			// we do not want the outer container
			if(node.childNodes && node.childNodes.length > 0){
				for (j = 0; j < node.childNodes.length; j++) {
					var text = this.walkTreeForHtml( node.childNodes[j] );
					if(text !== undefined){
						out += text;
					}
				}
			}
	
			return out;
		},
	
  
		/**
		 * walks the DOM tree parsing the html string from the nodes
		 *
		 * @param  {DOM Document} doc
		 * @param  {DOM Node} node
		 * @return {String}
		 */ 
		walkTreeForHtml: function( node ) {
			var out = '',
				j = 0;
	
			// if node is a text node get its text
			if(node.nodeType && node.nodeType === 3){
				out += modules.domUtils.getElementText( node ); 
			}
	
		
			// exclude text which has been added with include pattern  - 
			if(node.nodeType && node.nodeType === 1 && modules.domUtils.hasAttribute(node, 'data-include') === false){
	
				// begin tag
				out += '<' + node.tagName.toLowerCase();  
	
				// add attributes
				var attrs = modules.domUtils.getOrderedAttributes(node);
				for (j = 0; j < attrs.length; j++) {
					out += ' ' + attrs[j].name +  '=' + '"' + attrs[j].value + '"';
				}
	
				if(this.selfClosingElt.indexOf(node.tagName.toLowerCase()) === -1){
					out += '>';
				}
	
				// get the text of the child nodes
				if(node.childNodes && node.childNodes.length > 0){
					
					for (j = 0; j < node.childNodes.length; j++) {
						var text = this.walkTreeForHtml( node.childNodes[j] );
						if(text !== undefined){
							out += text;
						}
					}
				}
	
				// end tag
				if(this.selfClosingElt.indexOf(node.tagName.toLowerCase()) > -1){
					out += ' />'; 
				}else{
					out += '</' + node.tagName.toLowerCase() + '>'; 
				}
			} 
			
			return (out === '')? undefined : out;
		}    
	
	
	};
	

	return modules;

} (Modules || {}));

