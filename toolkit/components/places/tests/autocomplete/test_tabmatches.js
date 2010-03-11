let gTabRestrictChar = prefs.getCharPref("browser.urlbar.restrict.openpage");

let kSearchParam = "enable-actions";

let kURIs = [
  "http://abc.com/",
  "moz-action:switchtab,http://abc.com/",
  "http://xyz.net/",
  "moz-action:switchtab,http://xyz.net/"
];

let kTitles = [
  "ABC rocks",
  "xyz.net - we're better than ABC"
];

addPageBook(0, 0);
gPages[1] = [1, 0];
addPageBook(2, 1);
gPages[3] = [3, 1];

addOpenPages(0, 1);


let gTests = [
  ["0: single result, that is also a tab match",
   "abc.com", [0,1]],
  ["1: two results, one tab match",
   "abc", [0,1,2]],
  ["2: two results, both tab matches",
   "abc", [0,1,2,3],
   function() {
     addOpenPages(2, 1);
   }],
  ["3: two results, both tab matches, one has multiple tabs",
   "abc", [0,1,2,3],
   function() {
     addOpenPages(2, 5);
   }],
  ["4: two results, no tab matches",
   "abc", [0,2],
   function() {
     removeOpenPages(0, 1);
     removeOpenPages(2, 6);
   }],
  ["5: tab match search with restriction character",
   gTabRestrictChar + " abc", [1],
   function() {
    addOpenPages(0, 1);
   }]
];


function addOpenPages(aUri, aCount) {
  let num = aCount || 1;
  for (let i = 0; i < num; i++)
    bhist.registerOpenPage(toURI(kURIs[aUri]));
}

function removeOpenPages(aUri, aCount) {
  let num = aCount || 1;
  for (let i = 0; i < num; i++)
    bhist.unregisterOpenPage(toURI(kURIs[aUri]));
}
