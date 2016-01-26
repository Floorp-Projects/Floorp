/*
	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt  
*/

var Modules = (function (modules) {
	
	modules.maps = (modules.maps)? modules.maps : {};

	modules.maps['h-feed'] = {
		root: 'hfeed',
		name: 'h-feed',
		properties: {
			'category': {
				'map': 'p-category',
				'relAlt': ['tag']
			},
			'summary': {
				'map': 'p-summary'
			},
			'author': { 
				'uf': ['h-card']
			},
			'url': {
				'map': 'u-url'
			},
			'photo': {
				'map': 'u-photo'
			},
		}
	};

	return modules;

} (Modules || {}));

