/*
	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
*/

var Modules = (function (modules) {

	modules.maps = (modules.maps)? modules.maps : {};

	modules.maps['h-geo'] = {
		root: 'geo',
		name: 'h-geo',
		properties: {
			'latitude': {},
			'longitude': {}
		}
	};

	return modules;

} (Modules || {}));

