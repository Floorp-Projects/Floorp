/*
	Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
	MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
*/

var Modules = (function (modules) {

	modules.maps = (modules.maps)? modules.maps : {};

	modules.maps['h-entry'] = {
		root: 'hentry',
		name: 'h-entry',
		properties: {
			'entry-title': {
				'map': 'p-name'
			},
			'entry-summary': {
				'map': 'p-summary'
			},
			'entry-content': {
				'map': 'e-content'
			},
			'published': {
				'map': 'dt-published'
			},
			'updated': {
				'map': 'dt-updated'
			},
			'author': {
				'uf': ['h-card']
			},
			'category': {
				'map': 'p-category',
				'relAlt': ['tag']
			},
			'geo': {
				'map': 'p-geo',
				'uf': ['h-geo']
			},
			'latitude': {},
			'longitude': {},
			'url': {
				'map': 'u-url',
				'relAlt': ['bookmark']
			}
		}
	};

	return modules;

} (Modules || {}));

