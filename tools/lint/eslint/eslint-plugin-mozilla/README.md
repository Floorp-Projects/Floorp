# eslint-plugin-mozilla

A collection of rules that help enforce JavaScript coding standard in the Mozilla project.

These are primarily developed and used within the Firefox build system ([mozilla-central](https://hg.mozilla.org/mozilla-central/)), but are made available for other
related projects to use as well.

## Installation

### Within mozilla-central:

```
$ ./mach eslint --setup
```

### Outside mozilla-central:

Install ESLint [ESLint](http://eslint.org):

```
$ npm i eslint --save-dev
```

Next, install `eslint-plugin-mozilla`:

```
$ npm install eslint-plugin-mozilla --save-dev
```

## Documentation

For details about the rules, please see the [gecko documentation page](http://gecko.readthedocs.io/en/latest/tools/lint/linters/eslint-plugin-mozilla.html).

## Source Code

The sources can be found at:

* Code: https://dxr.mozilla.org/mozilla-central/source/tools/lint/eslint/eslint-plugin-mozilla
* Documentation: https://dxr.mozilla.org/mozilla-central/source/tools/lint/docs/linters

## Bugs

Please file bugs in Bugzilla in the Lint component of the Testing product.

* [Existing bugs](https://bugzilla.mozilla.org/buglist.cgi?resolution=---&query_format=advanced&component=Lint&product=Testing)
* [New bugs](https://bugzilla.mozilla.org/enter_bug.cgi?product=Testing&component=Lint)

## Tests

The tests can only be run from within mozilla-central. To run the tests:

```
./mach eslint --setup
cd tools/lint/eslint/eslint-plugin-mozilla
npm run test
```
