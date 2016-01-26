/*
	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt  
*/

var Modules = (function (modules) {
	
	modules.maps = (modules.maps)? modules.maps : {};

	modules.maps['h-org'] = {
		root: 'h-x-org',  // drop this from v1 as it causes issue with fn org hcard pattern
		name: 'h-org',
		childStructure: true,
		properties: {
			'organization-name': {},
			'organization-unit': {}
		}
	};

	return modules;

} (Modules || {}));


