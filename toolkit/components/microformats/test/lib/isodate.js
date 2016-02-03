/*!
	iso date
	This module was built for the exact needs of parsing ISO dates to the microformats standard.

	* Parses and builds ISO dates to the W3C note, HTML5 or RFC3339 profiles.
	* Also allows for profile detection using 'auto'
	* Outputs to the same level of specificity of date and time that was input

	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
 */



var Modules = (function (modules) {


	/**
	 * constructor
	 * parses text to find just the date element of an ISO date/time string i.e. 2008-05-01
	 *
	 * @param  {String} dateString
	 * @param  {String} format
	 * @return {String}
	 */
	modules.ISODate = function ( dateString, format ) {
		this.clear();

		this.format = (format)? format : 'auto'; // auto or W3C or RFC3339 or HTML5
		this.setFormatSep();

		// optional should be full iso date/time string
		if(arguments[0]) {
			this.parse(dateString, format);
		}
	};


	modules.ISODate.prototype = {


		/**
		 * clear all states
		 *
		 */
		clear: function(){
			this.clearDate();
			this.clearTime();
			this.clearTimeZone();
			this.setAutoProfileState();
		},


		/**
		 * clear date states
		 *
		 */
		clearDate: function(){
			this.dY = -1;
			this.dM = -1;
			this.dD = -1;
			this.dDDD = -1;
		},


		/**
		 * clear time states
		 *
		 */
		clearTime: function(){
			this.tH = -1;
			this.tM = -1;
			this.tS = -1;
			this.tD = -1;
		},


		/**
		 * clear timezone states
		 *
		 */
		clearTimeZone: function(){
			this.tzH = -1;
			this.tzM = -1;
			this.tzPN = '+';
			this.z = false;
		},


		/**
		 * resets the auto profile state
		 *
		 */
		setAutoProfileState: function(){
			this.autoProfile = {
			   sep: 'T',
			   dsep: '-',
			   tsep: ':',
			   tzsep: ':',
			   tzZulu: 'Z'
			};
		},


		/**
		 * parses text to find ISO date/time string i.e. 2008-05-01T15:45:19Z
		 *
		 * @param  {String} dateString
		 * @param  {String} format
		 * @return {String}
		 */
		parse: function( dateString, format ) {
			this.clear();

			var parts = [],
				tzArray = [],
				position = 0,
				datePart = '',
				timePart = '',
				timeZonePart = '';

			if(format){
				this.format = format;
			}



			// discover date time separtor for auto profile
			// Set to 'T' by default
			if(dateString.indexOf('t') > -1) {
				this.autoProfile.sep = 't';
			}
			if(dateString.indexOf('z') > -1) {
				this.autoProfile.tzZulu = 'z';
			}
			if(dateString.indexOf('Z') > -1) {
				this.autoProfile.tzZulu = 'Z';
			}
			if(dateString.toUpperCase().indexOf('T') === -1) {
				this.autoProfile.sep = ' ';
			}


			dateString = dateString.toUpperCase().replace(' ','T');

			// break on 'T' divider or space
			if(dateString.indexOf('T') > -1) {
				parts = dateString.split('T');
				datePart = parts[0];
				timePart = parts[1];

				// zulu UTC
				if(timePart.indexOf( 'Z' ) > -1) {
					this.z = true;
				}

				// timezone
				if(timePart.indexOf( '+' ) > -1 || timePart.indexOf( '-' ) > -1) {
					tzArray = timePart.split( 'Z' ); // incase of incorrect use of Z
					timePart = tzArray[0];
					timeZonePart = tzArray[1];

					// timezone
					if(timePart.indexOf( '+' ) > -1 || timePart.indexOf( '-' ) > -1) {
						position = 0;

						if(timePart.indexOf( '+' ) > -1) {
							position = timePart.indexOf( '+' );
						} else {
							position = timePart.indexOf( '-' );
						}

						timeZonePart = timePart.substring( position, timePart.length );
						timePart = timePart.substring( 0, position );
					}
				}

			} else {
				datePart = dateString;
			}

			if(datePart !== '') {
				this.parseDate( datePart );
				if(timePart !== '') {
					this.parseTime( timePart );
					if(timeZonePart !== '') {
						this.parseTimeZone( timeZonePart );
					}
				}
			}
			return this.toString( format );
		},


		/**
		 * parses text to find just the date element of an ISO date/time string i.e. 2008-05-01
		 *
		 * @param  {String} dateString
		 * @param  {String} format
		 * @return {String}
		 */
		parseDate: function( dateString, format ) {
			this.clearDate();

			var parts = [];

			// discover timezone separtor for auto profile // default is ':'
			if(dateString.indexOf('-') === -1) {
				this.autoProfile.tsep = '';
			}

			// YYYY-DDD
			parts = dateString.match( /(\d\d\d\d)-(\d\d\d)/ );
			if(parts) {
				if(parts[1]) {
					this.dY = parts[1];
				}
				if(parts[2]) {
					this.dDDD = parts[2];
				}
			}

			if(this.dDDD === -1) {
				// YYYY-MM-DD ie 2008-05-01 and YYYYMMDD ie 20080501
				parts = dateString.match( /(\d\d\d\d)?-?(\d\d)?-?(\d\d)?/ );
				if(parts[1]) {
					this.dY = parts[1];
				}
				if(parts[2]) {
					this.dM = parts[2];
				}
				if(parts[3]) {
					this.dD = parts[3];
				}
			}
			return this.toString(format);
		},


		/**
		 * parses text to find just the time element of an ISO date/time string i.e. 13:30:45
		 *
		 * @param  {String} timeString
		 * @param  {String} format
		 * @return {String}
		 */
		parseTime: function( timeString, format ) {
			this.clearTime();
			var parts = [];

			// discover date separtor for auto profile // default is ':'
			if(timeString.indexOf(':') === -1) {
				this.autoProfile.tsep = '';
			}

			// finds timezone HH:MM:SS and HHMMSS  ie 13:30:45, 133045 and 13:30:45.0135
			parts = timeString.match( /(\d\d)?:?(\d\d)?:?(\d\d)?.?([0-9]+)?/ );
			if(parts[1]) {
				this.tH = parts[1];
			}
			if(parts[2]) {
				this.tM = parts[2];
			}
			if(parts[3]) {
				this.tS = parts[3];
			}
			if(parts[4]) {
				this.tD = parts[4];
			}
			return this.toTimeString(format);
		},


		/**
		 * parses text to find just the time element of an ISO date/time string i.e. +08:00
		 *
		 * @param  {String} timeString
		 * @param  {String} format
		 * @return {String}
		 */
		parseTimeZone: function( timeString, format ) {
			this.clearTimeZone();
			var parts = [];

			if(timeString.toLowerCase() === 'z'){
				this.z = true;
				// set case for z
				this.autoProfile.tzZulu = (timeString === 'z')? 'z' : 'Z';
			}else{

				// discover timezone separtor for auto profile // default is ':'
				if(timeString.indexOf(':') === -1) {
					this.autoProfile.tzsep = '';
				}

				// finds timezone +HH:MM and +HHMM  ie +13:30 and +1330
				parts = timeString.match( /([\-\+]{1})?(\d\d)?:?(\d\d)?/ );
				if(parts[1]) {
					this.tzPN = parts[1];
				}
				if(parts[2]) {
					this.tzH = parts[2];
				}
				if(parts[3]) {
					this.tzM = parts[3];
				}


			}
			this.tzZulu = 'z';
			return this.toTimeString( format );
		},


		/**
		 * returns ISO date/time string in W3C Note, RFC 3339, HTML5, or auto profile
		 *
		 * @param  {String} format
		 * @return {String}
		 */
		toString: function( format ) {
			var output = '';

			if(format){
				this.format = format;
			}
			this.setFormatSep();

			if(this.dY  > -1) {
				output = this.dY;
				if(this.dM > 0 && this.dM < 13) {
					output += this.dsep + this.dM;
					if(this.dD > 0 && this.dD < 32) {
						output += this.dsep + this.dD;
						if(this.tH > -1 && this.tH < 25) {
							output += this.sep + this.toTimeString( format );
						}
					}
				}
				if(this.dDDD > -1) {
					output += this.dsep + this.dDDD;
				}
			} else if(this.tH > -1) {
				output += this.toTimeString( format );
			}

			return output;
		},


		/**
		 * returns just the time string element of an ISO date/time
		 * in W3C Note, RFC 3339, HTML5, or auto profile
		 *
		 * @param  {String} format
		 * @return {String}
		 */
		toTimeString: function( format ) {
			var out = '';

			if(format){
				this.format = format;
			}
			this.setFormatSep();

			// time can only be created with a full date
			if(this.tH) {
				if(this.tH > -1 && this.tH < 25) {
					out += this.tH;
					if(this.tM > -1 && this.tM < 61){
						out += this.tsep + this.tM;
						if(this.tS > -1 && this.tS < 61){
							out += this.tsep + this.tS;
							if(this.tD > -1){
								out += '.' + this.tD;
							}
						}
					}



					// time zone offset
					if(this.z) {
						out += this.tzZulu;
					} else {
						if(this.tzH && this.tzH > -1 && this.tzH < 25) {
							out += this.tzPN + this.tzH;
							if(this.tzM > -1 && this.tzM < 61){
								out += this.tzsep + this.tzM;
							}
						}
					}
				}
			}
			return out;
		},


		/**
		 * set the current profile to W3C Note, RFC 3339, HTML5, or auto profile
		 *
		 */
		setFormatSep: function() {
			switch( this.format.toLowerCase() ) {
				case 'rfc3339':
					this.sep = 'T';
					this.dsep = '';
					this.tsep = '';
					this.tzsep = '';
					this.tzZulu = 'Z';
					break;
				case 'w3c':
					this.sep = 'T';
					this.dsep = '-';
					this.tsep = ':';
					this.tzsep = ':';
					this.tzZulu = 'Z';
					break;
				case 'html5':
					this.sep = ' ';
					this.dsep = '-';
					this.tsep = ':';
					this.tzsep = ':';
					this.tzZulu = 'Z';
					break;
				default:
					// auto - defined by format of input string
					this.sep = this.autoProfile.sep;
					this.dsep = this.autoProfile.dsep;
					this.tsep = this.autoProfile.tsep;
					this.tzsep = this.autoProfile.tzsep;
					this.tzZulu = this.autoProfile.tzZulu;
			}
		},


		/**
		 * does current data contain a full date i.e. 2015-03-23
		 *
		 * @return {Boolean}
		 */
		hasFullDate: function() {
			return(this.dY !== -1 && this.dM !== -1 && this.dD !== -1);
		},


		/**
		 * does current data contain a minimum date which is just a year number i.e. 2015
		 *
		 * @return {Boolean}
		 */
		hasDate: function() {
			return(this.dY !== -1);
		},


		/**
		 * does current data contain a minimum time which is just a hour number i.e. 13
		 *
		 * @return {Boolean}
		 */
		hasTime: function() {
			return(this.tH !== -1);
		},

		/**
		 * does current data contain a minimum timezone i.e. -1 || +1 || z
		 *
		 * @return {Boolean}
		 */
		hasTimeZone: function() {
			return(this.tzH !== -1);
		}

	};

	modules.ISODate.prototype.constructor = modules.ISODate;

	return modules;

} (Modules || {}));
