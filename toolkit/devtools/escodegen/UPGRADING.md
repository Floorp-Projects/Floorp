Assuming that escodegen's dependencies have not changed, to upgrade our tree's
escodegen to a new version:

1. Clone the escodegen repository, and check out the version you want to upgrade
to:

       $ git clone https://github.com/Constellation/escodegen.git
       $ cd escodegen
       $ git checkout <version>

2. Make sure that all tests pass:

       $ npm install .
       $ npm test

   If there are any test failures, do not upgrade to that version of escodegen!

3. Copy escodegen.js to our tree:

       $ cp escodegen.js /path/to/mozilla-central/toolkit/devtools/escodegen/escodegen.js

4. Copy the package.json to our tree, and append ".js" to make it work with our
loader:

       $ cp package.json /path/to/mozilla-central/toolkit/devtools/escodegen/package.json.js

5. Prepend `module.exports = ` to the package.json file contents, so that the
JSON data is exported, and we can load package.json as a module.

6. Copy the estraverse.js that escodegen depends on into our tree:

       $ cp node_modules/estraverse/estraverse.js /path/to/mozilla-central/devtools/escodegen/estraverse.js

7. Build the version of the escodegen that we can use in workers:

   First we need to alias `self` as `window`:

       $ echo 'let window = self;' >> /path/to/mozilla-central/toolkit/devtools/escodegen/escodegen.worker.js

   Then we need to add the browser build of the source map library:

       $ git clone https://github.com/mozilla/source-map
       $ cd source-map
       $ git co <latest release tag compatible with escodegen>
       $ npm run-script build
       $ cat dist/source-map.js >> /path/to/mozilla-central/toolkit/devtools/escodegen/escodegen.worker.js

   Then we need to build the browser version of escodegen:

       $ cd /path/to/escodegen
       $ npm run-script build
       $ cat escodegen.browser.js >> /path/to/mozilla-central/toolkit/devtools/escodegen/escodegen.worker.js
