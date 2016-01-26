/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24 
Mocha integration test from: microformats-v2/h-event/time
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-event', function() {
   var htmlFragment = "<span class=\"h-event\">\n    <span class=\"p-name\">The 4th Microformat party</span> will be on \n    <ul>\n        <li class=\"dt-start\">\n            <time class=\"value\" datetime=\"2009-06-26\">26 July</time>, from\n            <time class=\"value\">19:00:00-08:00</time> \n        </li>\n        <li class=\"dt-start\">\n            <time class=\"value\" datetime=\"2009-06-26\">26 July</time>, from\n            <time class=\"value\">19:00:00-0800</time> \n        </li>\n        <li class=\"dt-start\">\n            <time class=\"value\" datetime=\"2009-06-26\">26 July</time>, from\n            <time class=\"value\">19:00:00+0800</time> \n        </li> \n        <li class=\"dt-start\">\n            <time class=\"value\" datetime=\"2009-06-26\">26 July</time>, from\n            <time class=\"value\">19:00:00Z</time> \n        </li>\n        <li class=\"dt-start\">\n            <time class=\"value\" datetime=\"2009-06-26\">26 July</time>, from\n            <time class=\"value\">19:00:00</time> \n        </li>\n        <li class=\"dt-start\">\n            <time class=\"value\" datetime=\"2009-06-26\">26 July</time>, from\n            <time class=\"value\">19:00-08:00</time> \n        </li> \n        <li class=\"dt-start\">\n            <time class=\"value\" datetime=\"2009-06-26\">26 July</time>, from\n            <time class=\"value\">19:00+08:00</time> \n        </li>\n        <li class=\"dt-start\">\n            <time class=\"value\" datetime=\"2009-06-26\">26 July</time>, from\n            <time class=\"value\">19:00Z</time> \n        </li>\n        <li class=\"dt-start\">\n            <time class=\"value\" datetime=\"2009-06-26\">26 July</time>, from\n            <time class=\"value\">19:00</time> \n        </li>  \n        <li>\n            <time class=\"dt-end\" datetime=\"2013-034\">3 February 2013</time>\n        </li>\n        <li>\n            <time class=\"dt-end\" datetime=\"2013-06-27 15:34\">26 July 2013</time>\n        </li>              \n    </ul>\n</span>";
   var expected = {"items":[{"type":["h-event"],"properties":{"name":["The 4th Microformat party"],"start":["2009-06-26 19:00:00-08:00","2009-06-26 19:00:00-08:00","2009-06-26 19:00:00+08:00","2009-06-26 19:00:00Z","2009-06-26 19:00:00","2009-06-26 19:00-08:00","2009-06-26 19:00+08:00","2009-06-26 19:00Z","2009-06-26 19:00"],"end":["2013-034","2013-06-27 15:34"]}}],"rels":{},"rel-urls":{}};

   it('time', function(){
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
