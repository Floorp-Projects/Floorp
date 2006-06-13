/*
	Copyright (c) 2004-2006, The Dojo Foundation
	All Rights Reserved.

	Licensed under the Academic Free License version 2.1 or above OR the
	modified BSD license. For more information on Dojo licensing, see:

		http://dojotoolkit.org/community/licensing.shtml
*/

dojo.provide("dojo.validate");
dojo.provide("dojo.validate.us");
dojo.require("dojo.regexp");

// *** Validation Functions ****

/**
  Checks if a string has non whitespace characters. 
  Parameters allow you to constrain the length.

  @param value  A string.
  @param flags  An object.
    flags.length  If set, checks if there are exactly flags.length number of characters.
    flags.minlength  If set, checks if there are at least flags.minlength number of characters.
    flags.maxlength  If set, checks if there are at most flags.maxlength number of characters.
  @return  true or false.
*/
dojo.validate.isText = function(value, flags) {
	flags = (typeof flags == "object") ? flags : {};

	// test for text
	if ( /^\s*$/.test(value) ) { return false; }

	// length tests
	if ( typeof flags.length == "number" && flags.length != value.length ) { return false; }
	if ( typeof flags.minlength == "number" && flags.minlength > value.length ) { return false; }
	if ( typeof flags.maxlength == "number" && flags.maxlength < value.length ) { return false; }

	return true;
}

/**
  Validates an IP address.
  Supports 5 formats for IPv4: dotted decimal, dotted hex, dotted octal, decimal and hexadecimal.
  Supports 2 formats for Ipv6.

  @param value  A string.
  @param flags  An object.  All flags are boolean with default = true.
    flags.allowDottedDecimal  Example, 207.142.131.235.  No zero padding.
    flags.allowDottedHex  Example, 0x18.0x11.0x9b.0x28.  Case insensitive.  Zero padding allowed.
    flags.allowDottedOctal  Example, 0030.0021.0233.0050.  Zero padding allowed.
    flags.allowDecimal  Example, 3482223595.  A decimal number between 0-4294967295.
    flags.allowHex  Example, 0xCF8E83EB.  Hexadecimal number between 0x0-0xFFFFFFFF.
      Case insensitive.  Zero padding allowed.
    flags.allowIPv6   IPv6 address written as eight groups of four hexadecimal digits.
    flags.allowHybrid   IPv6 address written as six groups of four hexadecimal digits
      followed by the usual 4 dotted decimal digit notation of IPv4. x:x:x:x:x:x:d.d.d.d
  @return  true or false
*/
dojo.validate.isIpAddress = function(value, flags) {
	var re = new RegExp("^" + dojo.regexp.ipAddress(flags) + "$", "i");
	return re.test(value);
}

/**
  Checks if a string could be a valid URL.

  @param value  A string.
  @param flags  An object.
    flags.scheme  Can be true, false, or [true, false]. 
      This means: required, not allowed, or either.
    flags in regexp.host can be applied.
    flags in regexp.ipAddress can be applied.
    flags in regexp.tld can be applied.
  @return  true or false
*/
dojo.validate.isUrl = function(value, flags) {
	var re = new RegExp("^" + dojo.regexp.url(flags) + "$", "i");
	return re.test(value);
}

/**
  Checks if a string could be a valid email address.

  @param value  A string.
  @param flags  An object.
    flags.allowCruft  Allow address like <mailto:foo@yahoo.com>.  Default is false.
    flags in regexp.host can be applied.
    flags in regexp.ipAddress can be applied.
    flags in regexp.tld can be applied.
  @return  true or false.
*/
dojo.validate.isEmailAddress = function(value, flags) {
	var re = new RegExp("^" + dojo.regexp.emailAddress(flags) + "$", "i");
	return re.test(value);
}

/**
  Checks if a string could be a valid email address list.

  @param value  A string.
  @param flags  An object.
    flags.listSeparator  The character used to separate email addresses.  Default is ";", ",", "\n" or " ".
    flags in regexp.emailAddress can be applied.
    flags in regexp.host can be applied.
    flags in regexp.ipAddress can be applied.
    flags in regexp.tld can be applied.
  @return  true or false.
*/
dojo.validate.isEmailAddressList = function(value, flags) {
	var re = new RegExp("^" + dojo.regexp.emailAddressList(flags) + "$", "i");
	return re.test(value);
}

/**
  Check if value is an email address list. If an empty list
  is returned, the value didn't pass the test or it was empty.

  @param value	A string
  @param flags	An object (same as isEmailAddressList)
  @return array of emails
*/
dojo.validate.getEmailAddressList = function(value, flags) {
	if(!flags) { flags = {}; }
	if(!flags.listSeparator) { flags.listSeparator = "\\s;,"; }

	if ( dojo.validate.isEmailAddressList(value, flags) ) {
		return value.split(new RegExp("\\s*[" + flags.listSeparator + "]\\s*"));
	}
	return [];
}

/**
  Validates whether a string is in an integer format. 

  @param value  A string.
  @param flags  An object.
    flags.signed  The leading plus-or-minus sign.  Can be true, false, or [true, false].
      Default is [true, false], (i.e. sign is optional).
    flags.separator  The character used as the thousands separator.  Default is no separator.
      For more than one symbol use an array, e.g. [",", ""], makes ',' optional.
  @return  true or false.
*/
dojo.validate.isInteger = function(value, flags) {
	var re = new RegExp("^" + dojo.regexp.integer(flags) + "$");
	return re.test(value);
}

/**
  Validates whether a string is a real valued number. 
  Format is the usual exponential notation.

  @param value  A string.
  @param flags  An object.
    flags.places  The integer number of decimal places.
      If not given, the decimal part is optional and the number of places is unlimited.
    flags.decimal  The character used for the decimal point.  Default is ".".
    flags.exponent  Express in exponential notation.  Can be true, false, or [true, false].
      Default is [true, false], (i.e. the exponential part is optional).
    flags.eSigned  The leading plus-or-minus sign on the exponent.  Can be true, false, 
      or [true, false].  Default is [true, false], (i.e. sign is optional).
    flags in regexp.integer can be applied.
  @return  true or false.
*/
dojo.validate.isRealNumber = function(value, flags) {
	var re = new RegExp("^" + dojo.regexp.realNumber(flags) + "$");
	return re.test(value);
}

/**
  Validates whether a string denotes a monetary value. 

  @param value  A string.
  @param flags  An object.
    flags.signed  The leading plus-or-minus sign.  Can be true, false, or [true, false].
      Default is [true, false], (i.e. sign is optional).
    flags.symbol  A currency symbol such as Yen "�", Pound "�", or the Euro sign "�".  
      Default is "$".  For more than one symbol use an array, e.g. ["$", ""], makes $ optional.
    flags.placement  The symbol can come "before" the number or "after".  Default is "before".
    flags.separator  The character used as the thousands separator. The default is ",".
    flags.cents  The two decimal places for cents.  Can be true, false, or [true, false].
      Default is [true, false], (i.e. cents are optional).
    flags.decimal  The character used for the decimal point.  Default is ".".
  @return  true or false.
*/
dojo.validate.isCurrency = function(value, flags) {
	var re = new RegExp("^" + dojo.regexp.currency(flags) + "$");
	return re.test(value);
}

/**
  Validates U.S. currency.

  @param value  A string.
  @param flags  An object.
    flags in validate.isCurrency can be applied.
  @return  true or false.
*/
dojo.validate.us.isCurrency = function(value, flags) {
	return dojo.validate.isCurrency(value, flags);
}

/**
  Validates German currency.

  @param value  A string.
  @return  true or false.
*/
dojo.validate.isGermanCurrency = function(value) {
	flags = {
		symbol: "�",
		placement: "after",
		decimal: ",",
		separator: "."
	};
	return dojo.validate.isCurrency(value, flags);
}

/**
  Validates Japanese currency.

  @param value  A string.
  @return  true or false.
*/
dojo.validate.isJapaneseCurrency = function(value) {
	flags = {
		symbol: "�",
		cents: false
	};
	return dojo.validate.isCurrency(value, flags);
}

/**
  Validates whether a string denoting an integer, 
  real number, or monetary value is between a max and min. 

  @param value  A string.
  @param flags  An object.
    flags.max  A number, which the value must be less than or equal to for the validation to be true.
    flags.min  A number, which the value must be greater than or equal to for the validation to be true.
    flags.decimal  The character used for the decimal point.  Default is ".".
  @return  true or false.
*/
dojo.validate.isInRange = function(value, flags) {
	// assign default values to missing paramters
	flags = (typeof flags == "object") ? flags : {};
	var max = (typeof flags.max == "number") ? flags.max : Infinity;
	var min = (typeof flags.min == "number") ? flags.min : -Infinity;
	var dec = (typeof flags.decimal == "string") ? flags.decimal : ".";
	
	// splice out anything not part of a number
	var pattern = "[^" + dec + "\\deE+-]";
	value = value.replace(RegExp(pattern, "g"), "");

	// trim ends of things like e, E, or the decimal character
	value = value.replace(/^([+-]?)(\D*)/, "$1");
	value = value.replace(/(\D*)$/, "");

	// replace decimal with ".". The minus sign '-' could be the decimal!
	pattern = "(\\d)[" + dec + "](\\d)";
	value = value.replace(RegExp(pattern, "g"), "$1.$2");

	value = Number(value);
	if ( value < min || value > max ) { return false; }

	return true;
}

/**
  Validates a time value in any International format.
  The value can be validated against one format or one of multiple formats.

  Format
  h        12 hour, no zero padding.
  hh       12 hour, has leading zero.
  H        24 hour, no zero padding.
  HH       24 hour, has leading zero.
  m        minutes, no zero padding.
  mm       minutes, has leading zero.
  s        seconds, no zero padding.
  ss       seconds, has leading zero.
  All other characters must appear literally in the expression.

  Example
    "h:m:s t"  ->   2:5:33 PM
    "HH:mm:ss" ->  14:05:33

  @param value  A string.
  @param flags  An object.
    flags.format  A string or an array of strings.  Default is "h:mm:ss t".
    flags.amSymbol  The symbol used for AM.  Default is "AM".
    flags.pmSymbol  The symbol used for PM.  Default is "PM".
  @return  true or false
*/
dojo.validate.isValidTime = function(value, flags) {
	var re = new RegExp("^" + dojo.regexp.time(flags) + "$", "i");
	return re.test(value);
}

/**
  Validates 12-hour time format.
  Zero-padding is not allowed for hours, required for minutes and seconds.
  Seconds are optional.

  @param value  A string.
  @return  true or false
*/
dojo.validate.is12HourTime = function(value) {
	return dojo.validate.isValidTime(value, {format: ["h:mm:ss t", "h:mm t"]});
}

/**
  Validates 24-hour military time format.
  Zero-padding is required for hours, minutes, and seconds.
  Seconds are optional.

  @param value  A string.
  @return  true or false
*/
dojo.validate.is24HourTime = function(value) {
	return dojo.validate.isValidTime(value, {format: ["HH:mm:ss", "HH:mm"]} );
}

/**
  Returns true if the date conforms to the format given and is a valid date. Otherwise returns false.

  @param dateValue  A string for the date.
  @param format  A string, default is  "MM/DD/YYYY".
  @return  true or false

  Accepts any type of format, including ISO8601.
  All characters in the format string are treated literally except the following tokens:

  YYYY - matches a 4 digit year
  M - matches a non zero-padded month
  MM - matches a zero-padded month
  D -  matches a non zero-padded date
  DD -  matches a zero-padded date
  DDD -  matches an ordinal date, 001-365, and 366 on leapyear
  ww - matches week of year, 01-53
  d - matches day of week, 1-7

  Examples: These are all today's date.

  Date          Format
  2005-W42-3    YYYY-Www-d
  2005-292      YYYY-DDD
  20051019      YYYYMMDD
  10/19/2005    M/D/YYYY
  19.10.2005    D.M.YYYY
*/
dojo.validate.isValidDate = function(dateValue, format) {
	// Default is the American format
	if (typeof format != "string") { format = "MM/DD/YYYY"; }

	// Create a literal regular expression based on format
	var reLiteral = format.replace(/([$^.*+?=!:|\/\\\(\)\[\]\{\}])/g, "\\$1");

	// Convert all the tokens to RE elements
	reLiteral = reLiteral.replace( "YYYY", "([0-9]{4})" );
	reLiteral = reLiteral.replace( "MM", "(0[1-9]|10|11|12)" );
	reLiteral = reLiteral.replace( "M", "([1-9]|10|11|12)" );
	reLiteral = reLiteral.replace( "DDD", "(00[1-9]|0[1-9][0-9]|[12][0-9][0-9]|3[0-5][0-9]|36[0-6])" );
	reLiteral = reLiteral.replace( "DD", "(0[1-9]|[12][0-9]|30|31)" );
	reLiteral = reLiteral.replace( "D", "([1-9]|[12][0-9]|30|31)" );
	reLiteral = reLiteral.replace( "ww", "(0[1-9]|[1-4][0-9]|5[0-3])" );
	reLiteral = reLiteral.replace( "d", "([1-7])" );

	// Anchor pattern to begining and end of string
	reLiteral = "^" + reLiteral + "$";

	// Dynamic RE that parses the original format given
	var re = new RegExp(reLiteral);
	
	// Test if date is in a valid format
	if (!re.test(dateValue))  return false;

	// Parse date to get elements and check if date is valid
	// Assume valid values for date elements not given.
	var year = 0, month = 1, date = 1, dayofyear = 1, week = 1, day = 1;

	// Capture tokens
	var tokens = format.match( /(YYYY|MM|M|DDD|DD|D|ww|d)/g );

	// Capture date values
	var values = re.exec(dateValue);

	// Match up tokens with date values
	for (var i = 0; i < tokens.length; i++) {
		switch (tokens[i]) {
		case "YYYY":
			year = Number(values[i+1]); break;
		case "M":
		case "MM":
			month = Number(values[i+1]); break;
		case "D":
		case "DD":
			date = Number(values[i+1]); break;
		case "DDD":
			dayofyear = Number(values[i+1]); break;
		case "ww":
			week = Number(values[i+1]); break;
		case "d":
			day = Number(values[i+1]); break;
		}
	}

	// Leap years are divisible by 4, but not by 100, unless by 400
	var leapyear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));

	// 31st of a month with 30 days
	if (date == 31 && (month == 4 || month == 6 || month == 9 || month == 11)) return false; 

	// February 30th or 31st
	if (date >= 30 && month == 2) return false; 

	// February 29th outside a leap year
	if (date == 29 && month == 2 && !leapyear) return false; 
	if (dayofyear == 366 && !leapyear)  return false;

	return true;
}

/**
  Validates US state and territory abbreviations.

	@param value  A two character string.
  @param flags  An object.
    flags.allowTerritories  Allow Guam, Puerto Rico, etc.  Default is true.
    flags.allowMilitary  Allow military 'states', e.g. Armed Forces Europe (AE).  Default is true.
  @return  true or false
*/
dojo.validate.us.isState = function(value, flags) {
	var re = new RegExp("^" + dojo.regexp.us.state(flags) + "$", "i");
	return re.test(value);
}

/**
  Validates any sort of number based format.
  Use it for phone numbers, social security numbers, zip-codes, etc.
  The value can be validated against one format or one of multiple formats.

  Format
    #        Stands for a digit, 0-9.
    ?        Stands for an optional digit, 0-9 or nothing.
    All other characters must appear literally in the expression.

  Example   
    "(###) ###-####"       ->   (510) 542-9742
    "(###) ###-#### x#???" ->   (510) 542-9742 x153
    "###-##-####"          ->   506-82-1089       i.e. social security number
    "#####-####"           ->   98225-1649        i.e. zip code

  @param value  A string.
  @param flags  An object.
    flags.format  A string or an Array of strings for multiple formats.
  @return  true or false
*/
dojo.validate.isNumberFormat = function(value, flags) {
	var re = new RegExp("^" + dojo.regexp.numberFormat(flags) + "$", "i");
	return re.test(value);
}

/**
  Validates 10 US digit phone number for several common formats:

  @param value The telephone number string
  @return true or false
*/
dojo.validate.us.isPhoneNumber = function(value) {
	flags = {
		format: [
			"###-###-####",
			"(###) ###-####",
			"(###) ### ####",
			"###.###.####",
			"###/###-####",
			"### ### ####",
			"###-###-#### x#???",
			"(###) ###-#### x#???",
			"(###) ### #### x#???",
			"###.###.#### x#???",
			"###/###-#### x#???",
			"### ### #### x#???",
			"##########"
		]
	};

	return dojo.validate.isNumberFormat(value, flags);
}

// Validates social security number
dojo.validate.us.isSocialSecurityNumber = function(value) {
	flags = {
		format: [
			"###-##-####",
			"### ## ####",
			"#########"
		]
	};

	return dojo.validate.isNumberFormat(value, flags);
}

// Validates U.S. zip-code
dojo.validate.us.isZipCode = function(value) {
	flags = {
		format: [
			"#####-####",
			"##### ####",
			"#########",
			"#####"
		]
	};

	return dojo.validate.isNumberFormat(value, flags);
}


/**
	Procedural API Description

		The main aim is to make input validation expressible in a simple format.
		You define profiles which declare the required and optional fields and any constraints they might have.
		The results are provided as an object that makes it easy to handle missing and invalid input.

	Usage

		var results = dojo.validate.check(form, profile);

	Profile Object

		var profile = {
			// filters change the field value and are applied before validation.
			trim: ["tx1", "tx2"],
			uppercase: ["tx9"],
			lowercase: ["tx5", "tx6", "tx7"],
			ucfirst: ["tx10"],
			digit: ["tx11"],

			// required input fields that are blank will be reported missing.
			// required radio button groups and drop-down lists with no selection will be reported missing.
			// checkbox groups and selectboxes can be required to have more than one value selected.
			// List required fields by name and use this notation to require more than one value: {checkboxgroup: 2}, {selectboxname: 3}.
			required: ["tx7", "tx8", "pw1", "ta1", "rb1", "rb2", "cb3", "s1", {"doubledip":2}, {"tripledip":3}],

			// dependant/conditional fields are required if the target field is present and not blank.
			// At present only textbox, password, and textarea fields are supported.
			dependancies:	{
				cc_exp: "cc_no",	
				cc_type: "cc_no",	
			},

			// Fields can be validated using any boolean valued function.  
			// Use arrays to specify parameters in addition to the field value.
			constraints: {
				field_name1: myValidationFunction,
				field_name2: dojo.validate.isInteger,
				field_name3: [myValidationFunction, additional parameters],
				field_name4: [dojo.validate.isValidDate, "YYYY.MM.DD"],
				field_name5: [dojo.validate.isEmailAddress, false, true],
			},

			// Confirm is a sort of conditional validation.
			// It associates each field in its property list with another field whose value should be equal.
			// If the values are not equal, the field in the property list is reported as Invalid. Unless the target field is blank.
			confirm: {
				email_confirm: "email",	
				pw2: "pw1",	
			}
		};

	Results Object

		isSuccessful(): Returns true if there were no invalid or missing fields, else it returns false.
		hasMissing():  Returns true if the results contain any missing fields.
		getMissing():  Returns a list of required fields that have values missing.
		isMissing(field):  Returns true if the field is required and the value is missing.
		hasInvalid():  Returns true if the results contain fields with invalid data.
		getInvalid():  Returns a list of fields that have invalid values.
		isInvalid(field):  Returns true if the field has an invalid value.

*/

/**
  Validates user input of an HTML form based on input profile.

	@param form  The form object to be validated.
	@param profile  The input profile that specifies how the form fields are to be validated.
	@return results  An object that contains several methods summarizing the results of the validation.
*/
dojo.validate.check = function(form, profile) {
	// Essentially private properties of results object
	var missing = [];
	var invalid = [];

	// results object summarizes the validation
	var results = {
		isSuccessful: function() {return ( !this.hasInvalid() && !this.hasMissing() );},
		hasMissing: function() {return ( missing.length > 0 );},
		getMissing: function() {return missing;},
		isMissing: function(elemname) {
			for (var i = 0; i < missing.length; i++) {
				if ( elemname == missing[i] ) { return true; }
			}
			return false;
		},
		hasInvalid: function() {return ( invalid.length > 0 );},
		getInvalid: function() {return invalid;},
		isInvalid: function(elemname) {
			for (var i = 0; i < invalid.length; i++) {
				if ( elemname == invalid[i] ) { return true; }
			}
			return false;
		}
	};

	// Filters are applied before fields are validated.
	// Trim removes white space at the front and end of the fields.
	if ( profile.trim instanceof Array ) {
		for (var i = 0; i < profile.trim.length; i++) {
			var elem = form[profile.trim[i]];
			if ( elem.type != "text" && elem.type != "textarea" && elem.type != "password" ) { continue; }
			elem.value = elem.value.replace(/(^\s*|\s*$)/g, "");
		}
	}
	// Convert to uppercase
	if ( profile.uppercase instanceof Array ) {
		for (var i = 0; i < profile.uppercase.length; i++) {
			var elem = form[profile.uppercase[i]];
			if ( elem.type != "text" && elem.type != "textarea" && elem.type != "password" ) { continue; }
			elem.value = elem.value.toUpperCase();
		}
	}
	// Convert to lowercase
	if ( profile.lowercase instanceof Array ) {
		for (var i = 0; i < profile.lowercase.length; i++) {
			var elem = form[profile.lowercase[i]];
			if ( elem.type != "text" && elem.type != "textarea" && elem.type != "password" ) { continue; }
			elem.value = elem.value.toLowerCase();
		}
	}
	// Uppercase first letter
	if ( profile.ucfirst instanceof Array ) {
		for (var i = 0; i < profile.ucfirst.length; i++) {
			var elem = form[profile.ucfirst[i]];
			if ( elem.type != "text" && elem.type != "textarea" && elem.type != "password" ) { continue; }
			elem.value = elem.value.replace(/\b\w+\b/g, function(word) { return word.substring(0,1).toUpperCase() + word.substring(1).toLowerCase(); });
		}
	}
	// Remove non digits characters from the input.
	if ( profile.digit instanceof Array ) {
		for (var i = 0; i < profile.digit.length; i++) {
			var elem = form[profile.digit[i]];
			if ( elem.type != "text" && elem.type != "textarea" && elem.type != "password" ) { continue; }
			elem.value = elem.value.replace(/\D/g, "");
		}
	}

	// See if required input fields have values missing.
	if ( profile.required instanceof Array ) {
		for (var i = 0; i < profile.required.length; i++) { 
			if ( typeof profile.required[i] != "string" ) { continue; }
			var elem = form[profile.required[i]];
			// Are textbox, textarea, or password fields blank.
			if ( (elem.type == "text" || elem.type == "textarea" || elem.type == "password") && /^\s*$/.test(elem.value) ) {	
				missing[missing.length] = elem.name;
			}
			// Does drop-down box have option selected.
			else if ( (elem.type == "select-one" || elem.type == "select-multiple") && elem.selectedIndex == -1 ) {
				missing[missing.length] = elem.name;
			}
			// Does radio button group (or check box group) have option checked.
			else if ( elem instanceof Array )  {
				var checked = false;
				for (var j = 0; j < elem.length; j++) {
					if (elem[j].checked) { checked = true; }
				}
				if ( !checked ) {	
					missing[missing.length] = elem[0].name;
				}
			}
		}
	}

	// See if checkbox groups and select boxes have x number of required values.
	if ( profile.required instanceof Array ) {
		for (var i = 0; i < profile.required.length; i++) { 
			if ( typeof profile.required[i] != "object" ) { continue; }
			var elem, numRequired;
			for (var name in profile.required[i]) { 
				elem = form[name]; 
				numRequired = profile.required[i][name];
			}
			// case 1: elem is a check box group
			if ( elem instanceof Array )  {
				var checked = 0;
				for (var j = 0; j < elem.length; j++) {
					if (elem[j].checked) { checked++; }
				}
				if ( checked < numRequired ) {	
					missing[missing.length] = elem[0].name;
				}
			}
			// case 2: elem is a select box
			else if ( elem.type == "select-multiple" ) {
				var selected = 0;
				for (var j = 0; j < elem.options.length; j++) {
					if (elem.options[j].selected) { selected++; }
				}
				if ( selected < numRequired ) {	
					missing[missing.length] = elem.name;
				}
			}
		}
	}

	// Dependant fields are required when the target field is present (not blank).
	// Todo: Support dependant and target fields that are radio button groups, or select drop-down lists.
	// Todo: Make the dependancy based on a specific value of the target field.
	// Todo: allow dependant fields to have several required values, like {checkboxgroup: 3}.
	if ( typeof profile.dependancies == "object" ) {
		// properties of dependancies object are the names of dependant fields to be checked
		for (name in profile.dependancies) {
			var elem = form[name];	// the dependant element
			if ( elem.type != "text" && elem.type != "textarea" && elem.type != "password" ) { continue; } // limited support
			if ( /\S+/.test(elem.value) ) { continue; }	// has a value already
			if ( results.isMissing(elem.name) ) { continue; }	// already listed as missing
			var target = form[profile.dependancies[name]];
			if ( target.type != "text" && target.type != "textarea" && target.type != "password" ) { continue; }	// limited support
			if ( /^\s*$/.test(target.value) ) { continue; }	// skip if blank
			missing[missing.length] = elem.name;	// ok the dependant field is missing
		}
	}

	// Find invalid input fields.
	if ( typeof profile.constraints == "object" ) {
		// constraint properties are the names of fields to be validated
		for (name in profile.constraints) {
			var elem = form[name];
			if ( elem.type != "text" && elem.type != "textarea" && elem.type != "password" ) { continue; }
			// skip if blank - its optional unless required, in which case it is already listed as missing.
			if ( /^\s*$/.test(elem.value) ) { continue; }

			var isValid = true;
			// case 1: constraint value is validation function
			if ( typeof profile.constraints[name] == "function" ) {
				isValid = profile.constraints[name](elem.value);
			}
			// case 2: constraint value is array, first elem is function, tail is parameters
			else if ( profile.constraints[name] instanceof Array ) {
				var isValidSomething = profile.constraints[name][0];
				var params = profile.constraints[name].slice(1);
				params.unshift(elem.value);
				isValid = isValidSomething.apply(null, params);
			}

			if ( !isValid ) {	
				invalid[invalid.length] = elem.name;
			}
		}
	}

	// Find unequal confirm fields and report them as Invalid.
	if ( typeof profile.confirm == "object" ) {
		for (name in profile.confirm) {
			var elem = form[name];	// the confirm element
			var target = form[profile.confirm[name]];
			if ( (elem.type != "text" && elem.type != "textarea" && elem.type != "password") 
				|| target.type != elem.type 
				|| target.value == elem.value		// it's valid
				|| results.isInvalid(elem.name)	// already listed as invalid
				|| /^\s*$/.test(target.value)	)	// skip if blank - only confirm if target has a value
			{
				continue; 
			}	
			invalid[invalid.length] = elem.name;
		}
	}

	return results;
}
