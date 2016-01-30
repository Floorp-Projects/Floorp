/*
Unit test for get
*/

assert = chai.assert;


describe('experimental', function() {

   it('h-geo - geo data writen as lat;lon', function(){
       
        var expected = {
				'items': [{
					'type': ['h-geo'],
					'properties': {
						'name': ['30.267991;-97.739568'],
						'latitude': [30.267991],
						'longitude': [-97.739568]
					}
				}],
				'rels': {},
				'rel-urls': {}
			},   
			options = {
				'html': '<div class="h-geo">30.267991;-97.739568</div>',
				'baseUrl': 'http://example.com',
				'dateFormat': 'html5',
				'parseLatLonGeo': true
			};

        var result = Microformats.get(options);
        assert.deepEqual( result, expected );
        
   });
   
  
});