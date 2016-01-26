/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v2/rel/xfn-elsewhere
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('rel', function() {
   var htmlFragment = "<ul>\n    <li><a rel=\"me\" href=\"http://twitter.com/glennjones\">twitter</a></li>\n    <li><a rel=\"me\" href=\"http://delicious.com/glennjonesnet/\">delicious</a></li>\n    <li><a rel=\"me\" href=\"https://plus.google.com/u/0/105161464208920272734/about\">google+</a></li>\n    <li><a rel=\"me\" href=\"http://lanyrd.com/people/glennjones/\">lanyrd</a></li>\n    <li><a rel=\"me\" href=\"http://github.com/glennjones\">github</a></li>\n    <li><a rel=\"me\" href=\"http://www.flickr.com/photos/glennjonesnet/\">flickr</a></li>\n    <li><a rel=\"me\" href=\"http://www.linkedin.com/in/glennjones\">linkedin</a></li>\n    <li><a rel=\"me\" href=\"http://www.slideshare.net/glennjones/presentations\">slideshare</a></li>\n</ul>";
   var expected = {"items":[],"rels":{"me":["http://twitter.com/glennjones","http://delicious.com/glennjonesnet/","https://plus.google.com/u/0/105161464208920272734/about","http://lanyrd.com/people/glennjones/","http://github.com/glennjones","http://www.flickr.com/photos/glennjonesnet/","http://www.linkedin.com/in/glennjones","http://www.slideshare.net/glennjones/presentations"]},"rel-urls":{"http://twitter.com/glennjones":{"text":"twitter","rels":["me"]},"http://delicious.com/glennjonesnet/":{"text":"delicious","rels":["me"]},"https://plus.google.com/u/0/105161464208920272734/about":{"text":"google+","rels":["me"]},"http://lanyrd.com/people/glennjones/":{"text":"lanyrd","rels":["me"]},"http://github.com/glennjones":{"text":"github","rels":["me"]},"http://www.flickr.com/photos/glennjonesnet/":{"text":"flickr","rels":["me"]},"http://www.linkedin.com/in/glennjones":{"text":"linkedin","rels":["me"]},"http://www.slideshare.net/glennjones/presentations":{"text":"slideshare","rels":["me"]}}};

   it('xfn-elsewhere', function(){
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
