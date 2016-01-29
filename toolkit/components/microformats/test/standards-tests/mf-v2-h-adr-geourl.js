/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v2/h-adr/geourl
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-adr', function() {
   var htmlFragment = "<p class=\"h-adr\">\n    <a class=\"p-name u-geo\" href=\"geo:51.526421;-0.081067;crs=wgs84;u=40\">Bricklayer's Arms</a>, \n    <span class=\"p-locality\">London</span> \n</p>";
   var expected = {"items":[{"type":["h-adr"],"properties":{"name":["Bricklayer's Arms"],"geo":["geo:51.526421;-0.081067;crs=wgs84;u=40"],"locality":["London"],"url":["geo:51.526421;-0.081067;crs=wgs84;u=40"]}}],"rels":{},"rel-urls":{}};

   it('geourl', function(){
       var doc, dom, node, options, parser, found;
       dom = new DOMParser();
       doc = dom.parseFromString( htmlFragment, 'text/html' );
       options ={
       		'document': doc,
       		'node': doc,
       		'baseUrl': 'http://example.com',
       		'dateFormat': 'html5'
       };
       found = Microformats.get( options );
       assert.deepEqual(found, expected);
   });
});
