/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v2/h-feed/implied-title
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-feed', function() {
   var htmlFragment = "\n<html>\n\t<head>\n\t\t<title>microformats blog</title>\n\t</head>\n\t<body>\n\t<section class=\"h-feed\">\n\t\t\n\t\t<div class=\"h-entry\">\n\t\t    <h1><a class=\"p-name u-url\" href=\"http://microformats.org/2012/06/25/microformats-org-at-7\">microformats.org at 7</a></h1>\n\t\t    <div class=\"e-content\">\n\t\t        <p class=\"p-summary\">Last week the microformats.org community \n\t\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t\t            San Francisco and recognized accomplishments, challenges, and \n\t\t            opportunities.</p>\n\t\t\n\t\t        <p>The microformats tagline “humans first, machines second” \n\t\t            forms the basis of many of our \n\t\t            <a href=\"http://microformats.org/wiki/principles\">principles</a>, and \n\t\t            in that regard, we’d like to recognize a few people and \n\t\t            thank them for their years of volunteer service </p>\n\t\t    </div>  \n\t\t    <p>Updated \n\t\t        <time class=\"dt-updated\" datetime=\"2012-06-25T17:08:26\">June 25th, 2012</time>\n\t\t    </p>\n\t\t</div>\n\t\t\n\t</section>\n\t</body>\n</html>";
   var expected = {"items":[{"type":["h-feed"],"properties":{"name":["microformats blog"]},"children":[{"value":"microformats.org at 7\n\t\t    \n\t\t        Last week the microformats.org community \n\t\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t\t            San Francisco and recognized accomplishments, challenges, and \n\t\t            opportunities.\n\t\t\n\t\t        The microformats tagline “humans first, machines second” \n\t\t            forms the basis of many of our \n\t\t            principles, and \n\t\t            in that regard, we’d like to recognize a few people and \n\t\t            thank them for their years of volunteer service \n\t\t      \n\t\t    Updated \n\t\t        June 25th, 2012","type":["h-entry"],"properties":{"name":["microformats.org at 7"],"url":["http://microformats.org/2012/06/25/microformats-org-at-7"],"content":[{"value":"Last week the microformats.org community \n\t\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t\t            San Francisco and recognized accomplishments, challenges, and \n\t\t            opportunities.\n\t\t\n\t\t        The microformats tagline “humans first, machines second” \n\t\t            forms the basis of many of our \n\t\t            principles, and \n\t\t            in that regard, we’d like to recognize a few people and \n\t\t            thank them for their years of volunteer service","html":"\n\t\t        <p class=\"p-summary\">Last week the microformats.org community \n\t\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t\t            San Francisco and recognized accomplishments, challenges, and \n\t\t            opportunities.</p>\n\t\t\n\t\t        <p>The microformats tagline “humans first, machines second” \n\t\t            forms the basis of many of our \n\t\t            <a href=\"http://microformats.org/wiki/principles\">principles</a>, and \n\t\t            in that regard, we’d like to recognize a few people and \n\t\t            thank them for their years of volunteer service </p>\n\t\t    "}],"summary":["Last week the microformats.org community \n\t\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t\t            San Francisco and recognized accomplishments, challenges, and \n\t\t            opportunities."],"updated":["2012-06-25 17:08:26"]}}]}],"rels":{},"rel-urls":{}};

   it('implied-title', function(){
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
