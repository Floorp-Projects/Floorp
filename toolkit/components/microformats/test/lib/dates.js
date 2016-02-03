/*!
	dates
	These functions are based on microformats implied rules for parsing date fragments from text.
	They are not generalist date utilities and should only be used with the isodate.js module of this library.

	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
	Dependencies  utilities.js, isodate.js
*/


var Modules = (function (modules) {

	modules.dates = {


		/**
		 * does text contain am
		 *
		 * @param  {String} text
		 * @return {Boolean}
		 */
		hasAM: function( text ) {
			text = text.toLowerCase();
			return(text.indexOf('am') > -1 || text.indexOf('a.m.') > -1);
		},


		/**
		 * does text contain pm
		 *
		 * @param  {String} text
		 * @return {Boolean}
		 */
		hasPM: function( text ) {
			text = text.toLowerCase();
			return(text.indexOf('pm') > -1 || text.indexOf('p.m.') > -1);
		},


		/**
		 * remove am and pm from text and return it
		 *
		 * @param  {String} text
		 * @return {String}
		 */
		removeAMPM: function( text ) {
			return text.replace('pm', '').replace('p.m.', '').replace('am', '').replace('a.m.', '');
		},


	   /**
		 * simple test of whether ISO date string is a duration  i.e.  PY17M or PW12
		 *
		 * @param  {String} text
		 * @return {Boolean}
		 */
		isDuration: function( text ) {
			if(modules.utils.isString( text )){
				text = text.toLowerCase();
				if(modules.utils.startWith(text, 'p') ){
					return true;
				}
			}
			return false;
		},


	   /**
		 * is text a time or timezone
		 * i.e. HH-MM-SS or z+-HH-MM-SS 08:43 | 15:23:00:0567 | 10:34pm | 10:34 p.m. | +01:00:00 | -02:00 | z15:00 | 0843
		 *
		 * @param  {String} text
		 * @return {Boolean}
		 */
		isTime: function( text ) {
			if(modules.utils.isString(text)){
				text = text.toLowerCase();
				text = modules.utils.trim( text );
				// start with timezone char
				if( text.match(':') && ( modules.utils.startWith(text, 'z') || modules.utils.startWith(text, '-')  || modules.utils.startWith(text, '+') )) {
					return true;
				}
				// has ante meridiem or post meridiem
				if( text.match(/^[0-9]/) &&
					( this.hasAM(text) || this.hasPM(text) )) {
					return true;
				}
				// contains time delimiter but not datetime delimiter
				if( text.match(':') && !text.match(/t|\s/) ) {
					return true;
				}

				// if it's a number of 2, 4 or 6 chars
				if(modules.utils.isNumber(text)){
					if(text.length === 2 || text.length === 4 || text.length === 6){
						return true;
					}
				}
			}
			return false;
		},


		/**
		 * parses a time from text and returns 24hr time string
		 * i.e. 5:34am = 05:34:00 and 1:52:04p.m. = 13:52:04
		 *
		 * @param  {String} text
		 * @return {String}
		 */
		parseAmPmTime: function( text ) {
			var out = text,
				times = [];

			// if the string has a text : or am or pm
			if(modules.utils.isString(out)) {
				//text = text.toLowerCase();
				text = text.replace(/[ ]+/g, '');

				if(text.match(':') || this.hasAM(text) || this.hasPM(text)) {

					if(text.match(':')) {
						times = text.split(':');
					} else {
						// single number text i.e. 5pm
						times[0] = text;
						times[0] = this.removeAMPM(times[0]);
					}

					// change pm hours to 24hr number
					if(this.hasPM(text)) {
						if(times[0] < 12) {
							times[0] = parseInt(times[0], 10) + 12;
						}
					}

					// add leading zero's where needed
					if(times[0] && times[0].length === 1) {
						times[0] = '0' + times[0];
					}

					// rejoin text elements together
					if(times[0]) {
						text = times.join(':');
					}
				}
			}

			// remove am/pm strings
			return this.removeAMPM(text);
		},


	   /**
		 * overlays a time on a date to return the union of the two
		 *
		 * @param  {String} date
		 * @param  {String} time
		 * @param  {String} format ( Modules.ISODate profile format )
		 * @return {Object} Modules.ISODate
		 */
		dateTimeUnion: function(date, time, format) {
			var isodate = new modules.ISODate(date, format),
				isotime = new modules.ISODate();

			isotime.parseTime(this.parseAmPmTime(time), format);
			if(isodate.hasFullDate() && isotime.hasTime()) {
				isodate.tH = isotime.tH;
				isodate.tM = isotime.tM;
				isodate.tS = isotime.tS;
				isodate.tD = isotime.tD;
				return isodate;
			} else {
				if(isodate.hasFullDate()){
					return isodate;
				}
				return new modules.ISODate();
			}
		},


	   /**
		 * concatenate an array of date and time text fragments to create an ISODate object
		 * used for microformat value and value-title rules
		 *
		 * @param  {Array} arr ( Array of Strings )
		 * @param  {String} format ( Modules.ISODate profile format )
		 * @return {Object} Modules.ISODate
		 */
		concatFragments: function (arr, format) {
			var out = new modules.ISODate(),
				i = 0,
				value = '';

			// if the fragment already contains a full date just return it once
			if(arr[0].toUpperCase().match('T')) {
				return new modules.ISODate(arr[0], format);
			}else{
				for(i = 0; i < arr.length; i++) {
				value = arr[i];

				// date pattern
				if( value.charAt(4) === '-' && out.hasFullDate() === false ){
					out.parseDate(value);
				}

				// time pattern
				if( (value.indexOf(':') > -1 || modules.utils.isNumber( this.parseAmPmTime(value) )) && out.hasTime() === false ) {
					// split time and timezone
					var items = this.splitTimeAndZone(value);
					value = items[0];

					// parse any use of am/pm
					value = this.parseAmPmTime(value);
					out.parseTime(value);

					// parse any timezone
					if(items.length > 1){
						 out.parseTimeZone(items[1], format);
					}
				}

				// timezone pattern
				if(value.charAt(0) === '-' || value.charAt(0) === '+' || value.toUpperCase() === 'Z') {
					if( out.hasTimeZone() === false ){
						out.parseTimeZone(value);
					}
				}

			}
			return out;

			}
		},


	   /**
		 * parses text by splitting it into an array of time and timezone strings
		 *
		 * @param  {String} text
		 * @return {Array} Modules.ISODate
		 */
		splitTimeAndZone: function ( text ){
		   var out = [text],
			   chars = ['-','+','z','Z'],
			   i = chars.length;

			while (i--) {
			  if(text.indexOf(chars[i]) > -1){
				  out[0] = text.slice( 0, text.indexOf(chars[i]) );
				  out.push( text.slice( text.indexOf(chars[i]) ) );
				  break;
			   }
			}
		   return out;
		}

	};


	return modules;

} (Modules || {}));




