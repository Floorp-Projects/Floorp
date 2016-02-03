/*
   Utilities

   Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
   MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
*/

var Modules = (function (modules) {

	modules.utils = {

		/**
		 * is the object a string
		 *
		 * @param  {Object} obj
		 * @return {Boolean}
		 */
		isString: function( obj ) {
			return typeof( obj ) === 'string';
		},

		/**
		 * is the object a number
		 *
		 * @param  {Object} obj
		 * @return {Boolean}
		 */
		isNumber: function( obj ) {
			return !isNaN(parseFloat( obj )) && isFinite( obj );
		},


		/**
		 * is the object an array
		 *
		 * @param  {Object} obj
		 * @return {Boolean}
		 */
		isArray: function( obj ) {
			return obj && !( obj.propertyIsEnumerable( 'length' ) ) && typeof obj === 'object' && typeof obj.length === 'number';
		},


		/**
		 * is the object a function
		 *
		 * @param  {Object} obj
		 * @return {Boolean}
		 */
		isFunction: function(obj) {
			return !!(obj && obj.constructor && obj.call && obj.apply);
		},


		/**
		 * does the text start with a test string
		 *
		 * @param  {String} text
		 * @param  {String} test
		 * @return {Boolean}
		 */
		startWith: function( text, test ) {
			return(text.indexOf(test) === 0);
		},


		/**
		 * removes spaces at front and back of text
		 *
		 * @param  {String} text
		 * @return {String}
		 */
		trim: function( text ) {
			if(text && this.isString(text)){
				return (text.trim())? text.trim() : text.replace(/^\s+|\s+$/g, '');
			}else{
				return '';
			}
		},


		/**
		 * replaces a character in text
		 *
		 * @param  {String} text
		 * @param  {Int} index
		 * @param  {String} character
		 * @return {String}
		 */
		replaceCharAt: function( text, index, character ) {
			if(text && text.length > index){
			   return text.substr(0, index) + character + text.substr(index+character.length);
			}else{
				return text;
			}
		},


		/**
		 * removes whitespace, tabs and returns from start and end of text
		 *
		 * @param  {String} text
		 * @return {String}
		 */
		trimWhitespace: function( text ){
			if(text && text.length){
				var i = text.length,
					x = 0;

				// turn all whitespace chars at end into spaces
				while (i--) {
					if(this.isOnlyWhiteSpace(text[i])){
						text = this.replaceCharAt( text, i, ' ' );
					}else{
						break;
					}
				}

				// turn all whitespace chars at start into spaces
				i = text.length;
				while (x < i) {
					if(this.isOnlyWhiteSpace(text[x])){
						text = this.replaceCharAt( text, i, ' ' );
					}else{
						break;
					}
					x++;
				}
			}
			return this.trim(text);
		},


		/**
		 * does text only contain whitespace characters
		 *
		 * @param  {String} text
		 * @return {Boolean}
		 */
		isOnlyWhiteSpace: function( text ){
			return !(/[^\t\n\r ]/.test( text ));
		},


		/**
		 * removes whitespace from text (leaves a single space)
		 *
		 * @param  {String} text
		 * @return {Sring}
		 */
		collapseWhiteSpace: function( text ){
			return text.replace(/[\t\n\r ]+/g, ' ');
		},


		/**
		 * does an object have any of its own properties
		 *
		 * @param  {Object} obj
		 * @return {Boolean}
		 */
		hasProperties: function( obj ) {
			var key;
			for(key in obj) {
				if( obj.hasOwnProperty( key ) ) {
					return true;
				}
			}
			return false;
		},


		/**
		 * a sort function - to sort objects in an array by a given property
		 *
		 * @param  {String} property
		 * @param  {Boolean} reverse
		 * @return {Int}
		 */
		sortObjects: function(property, reverse) {
			reverse = (reverse) ? -1 : 1;
			return function (a, b) {
				a = a[property];
				b = b[property];
				if (a < b) {
					return reverse * -1;
				}
				if (a > b) {
					return reverse * 1;
				}
				return 0;
			};
		}

	};

	return modules;

} (Modules || {}));







