/*
Microformats Test Suite - Downloaded from github repo: microformats/tests version v0.1.24
Mocha integration test from: microformats-v2/h-feed/simple
The test was built on Fri Sep 25 2015 13:26:26 GMT+0100 (BST)
*/

assert = chai.assert;


describe('h-feed', function() {
   var htmlFragment = "<section class=\"h-feed\">\n\t<h1 class=\"p-name\">Microformats blog</h1>\n\t<a class=\"p-author h-card\" href=\"http://tantek.com/\">Tantek</a>\n\t<a class=\"u-url\" href=\"http://microformats.org/blog\">permlink</a>\n\t<img class=\"u-photo\" src=\"photo.jpeg\"/>\n\t\n\t<div class=\"h-entry\">\n\t    <h1><a class=\"p-name u-url\" href=\"http://microformats.org/2012/06/25/microformats-org-at-7\">microformats.org at 7</a></h1>\n\t    <div class=\"e-content\">\n\t        <p class=\"p-summary\">Last week the microformats.org community \n\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t            San Francisco and recognized accomplishments, challenges, and \n\t            opportunities.</p>\n\t\n\t        <p>The microformats tagline “humans first, machines second” \n\t            forms the basis of many of our \n\t            <a href=\"http://microformats.org/wiki/principles\">principles</a>, and \n\t            in that regard, we’d like to recognize a few people and \n\t            thank them for their years of volunteer service </p>\n\t    </div>  \n\t    <p>Updated \n\t        <time class=\"dt-updated\" datetime=\"2012-06-25T17:08:26\">June 25th, 2012</time>\n\t    </p>\n\t</div>\n\t\n</section>";
   var expected = {"items":[{"type":["h-feed"],"properties":{"name":["Microformats blog"],"author":[{"value":"Tantek","type":["h-card"],"properties":{"name":["Tantek"],"url":["http://tantek.com/"]}}],"url":["http://microformats.org/blog"],"photo":["http://example.com/photo.jpeg"]},"children":[{"value":"microformats.org at 7\n\t    \n\t        Last week the microformats.org community \n\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t            San Francisco and recognized accomplishments, challenges, and \n\t            opportunities.\n\t\n\t        The microformats tagline “humans first, machines second” \n\t            forms the basis of many of our \n\t            principles, and \n\t            in that regard, we’d like to recognize a few people and \n\t            thank them for their years of volunteer service \n\t      \n\t    Updated \n\t        June 25th, 2012","type":["h-entry"],"properties":{"name":["microformats.org at 7"],"url":["http://microformats.org/2012/06/25/microformats-org-at-7"],"content":[{"value":"Last week the microformats.org community \n\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t            San Francisco and recognized accomplishments, challenges, and \n\t            opportunities.\n\t\n\t        The microformats tagline “humans first, machines second” \n\t            forms the basis of many of our \n\t            principles, and \n\t            in that regard, we’d like to recognize a few people and \n\t            thank them for their years of volunteer service","html":"\n\t        <p class=\"p-summary\">Last week the microformats.org community \n\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t            San Francisco and recognized accomplishments, challenges, and \n\t            opportunities.</p>\n\t\n\t        <p>The microformats tagline “humans first, machines second” \n\t            forms the basis of many of our \n\t            <a href=\"http://microformats.org/wiki/principles\">principles</a>, and \n\t            in that regard, we’d like to recognize a few people and \n\t            thank them for their years of volunteer service </p>\n\t    "}],"summary":["Last week the microformats.org community \n\t            celebrated its 7th birthday at a gathering hosted by Mozilla in \n\t            San Francisco and recognized accomplishments, challenges, and \n\t            opportunities."],"updated":["2012-06-25 17:08:26"]}}]}],"rels":{},"rel-urls":{}};

   it('simple', function(){
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
