/*
		Copyright (C) 2010 - 2015 Glenn Jones. All Rights Reserved.
		MIT License: https://raw.github.com/glennjones/microformat-shiv/master/license.txt
*/

var Modules = (function (modules) {

	modules.maps = (modules.maps)? modules.maps : {};

		modules.maps['h-listing'] = {
			root: 'hlisting',
			name: 'h-listing',
			properties: {
				'version': {},
				'lister': {
					'uf': ['h-card']
				},
				'dtlisted': {
					'map': 'dt-listed'
				},
				'dtexpired': {
					'map': 'dt-expired'
				},
				'location': {},
				'price': {},
				'item': {
					'uf': ['h-card','a-adr','h-geo']
				},
				'summary': {
					'map': 'p-name'
				},
				'description': {
					'map': 'e-description'
				},
				'listing': {}
			}
		};

		return modules;

} (Modules || {}));
