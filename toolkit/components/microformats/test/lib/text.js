/*
	text
	Extracts text string from DOM nodes. Was created to extract text in a whitespace-normalized form. 
	It works like a none-CSS aware version of IE's innerText function. DO NOT replace this module 
	with functions such as textContent as it will reduce the quality of data provided to the API user.
	
	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
	Dependencies  utilities.js, domutils.js
*/


var Modules = (function (modules) {
	
	
	modules.text = {
		
		// normalised or whitespace or whitespacetrimmed
		textFormat: 'whitespacetrimmed', 
		
		// block level tags, used to add line returns
		blockLevelTags: ['h1', 'h2', 'h3', 'h4', 'h5', 'h6', 'p', 'hr', 'pre', 'table',
			'address', 'article', 'aside', 'blockquote', 'caption', 'col', 'colgroup', 'dd', 'div', 
			'dt', 'dir', 'fieldset', 'figcaption', 'figure', 'footer', 'form',  'header', 'hgroup', 'hr', 
			'li', 'map', 'menu', 'nav', 'optgroup', 'option', 'section', 'tbody', 'testarea', 
			'tfoot', 'th', 'thead', 'tr', 'td', 'ul', 'ol', 'dl', 'details'],

		// tags to exclude 
		excludeTags: ['noframe', 'noscript', 'template', 'script', 'style', 'frames', 'frameset'],
 
	
		/**
		 * parses the text from the DOM Node 
		 *
		 * @param  {DOM Node} node
		 * @param  {String} textFormat
		 * @return {String}
		 */
		parse: function(doc, node, textFormat){
			var out;
			this.textFormat = (textFormat)? textFormat : this.textFormat;
			if(this.textFormat === 'normalised'){
				out = this.walkTreeForText( node );
				if(out !== undefined){
					return this.normalise( doc, out );
				}else{
					return '';
				}
			}else{
			   return this.formatText( doc, modules.domUtils.textContent(node), this.textFormat );
			}
		},
		
		
		/**
		 * parses the text from a html string 
		 *
		 * @param  {DOM Document} doc
		 * @param  {String} text
		 * @param  {String} textFormat
		 * @return {String}
		 */  
		parseText: function( doc, text, textFormat ){
		   var node = modules.domUtils.createNodeWithText( 'div', text );
		   return this.parse( doc, node, textFormat );
		},
		
		
		/**
		 * parses the text from a html string - only for whitespace or whitespacetrimmed formats
		 *
		 * @param  {String} text
		 * @param  {String} textFormat
		 * @return {String}
		 */  
		formatText: function( doc, text, textFormat ){
		   this.textFormat = (textFormat)? textFormat : this.textFormat;
		   if(text){
			  var out = '',
				  regex = /(<([^>]+)>)/ig;
				
			  out = text.replace(regex, '');   
			  if(this.textFormat === 'whitespacetrimmed') {    
				 out = modules.utils.trimWhitespace( out );
			  }
			  
			  //return entities.decode( out, 2 );
			  return modules.domUtils.decodeEntities( doc, out );
		   }else{
			  return ''; 
		   }
		},
		
		
		/**
		 * normalises whitespace in given text 
		 *
		 * @param  {String} text
		 * @return {String}
		 */ 
		normalise: function( doc, text ){
			text = text.replace( /&nbsp;/g, ' ') ;    // exchanges html entity for space into space char
			text = modules.utils.collapseWhiteSpace( text );     // removes linefeeds, tabs and addtional spaces
			text = modules.domUtils.decodeEntities( doc, text );  // decode HTML entities
			text = text.replace( '–', '-' );          // correct dash decoding
			return modules.utils.trim( text );
		},
		
	 
		/**
		 * walks DOM tree parsing the text from DOM Nodes
		 *
		 * @param  {DOM Node} node
		 * @return {String}
		 */ 
		walkTreeForText: function( node ) {
			var out = '',
				j = 0;
	
			if(node.tagName && this.excludeTags.indexOf( node.tagName.toLowerCase() ) > -1){
				return out;
			}
	
			// if node is a text node get its text
			if(node.nodeType && node.nodeType === 3){
				out += modules.domUtils.getElementText( node ); 
			}
	
			// get the text of the child nodes
			if(node.childNodes && node.childNodes.length > 0){
				for (j = 0; j < node.childNodes.length; j++) {
					var text = this.walkTreeForText( node.childNodes[j] );
					if(text !== undefined){
						out += text;
					}
				}
			}
	
			// if it's a block level tag add an additional space at the end
			if(node.tagName && this.blockLevelTags.indexOf( node.tagName.toLowerCase() ) !== -1){
				out += ' ';
			} 
			
			return (out === '')? undefined : out ;
		}
		
	};
   
	return modules;

} (Modules || {}));