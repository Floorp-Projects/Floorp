module.exports = {
    "name": "escodegen",
    "description": "ECMAScript code generator",
    "homepage": "http://github.com/Constellation/escodegen.html",
    "main": "escodegen.js",
    "bin": {
        "esgenerate": "./bin/esgenerate.js",
        "escodegen": "./bin/escodegen.js"
    },
    "version": "0.0.26",
    "engines": {
        "node": ">=0.4.0"
    },
    "maintainers": [
        {
            "name": "Yusuke Suzuki",
            "email": "utatane.tea@gmail.com",
            "web": "http://github.com/Constellation"
        }
    ],
    "repository": {
        "type": "git",
        "url": "http://github.com/Constellation/escodegen.git"
    },
    "dependencies": {
        "esprima": "~1.0.2",
        "estraverse": "~1.3.0"
    },
    "optionalDependencies": {
        "source-map": ">= 0.1.2"
    },
    "devDependencies": {
        "esprima-moz": "*",
        "commonjs-everywhere": "~0.8.0",
        "q": "*",
        "bower": "*",
        "semver": "*",
        "chai": "~1.7.2",
        "grunt-contrib-jshint": "~0.5.0",
        "grunt-cli": "~0.1.9",
        "grunt": "~0.4.1",
        "grunt-mocha-test": "~0.6.2"
    },
    "licenses": [
        {
            "type": "BSD",
            "url": "http://github.com/Constellation/escodegen/raw/master/LICENSE.BSD"
        }
    ],
    "scripts": {
        "test": "grunt travis",
        "unit-test": "grunt test",
        "lint": "grunt lint",
        "release": "node tools/release.js",
        "build": "./node_modules/.bin/cjsify -ma path: tools/entry-point.js > escodegen.browser.js"
    }
};
