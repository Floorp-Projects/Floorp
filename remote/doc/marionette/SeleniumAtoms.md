# Selenium atoms

Marionette uses a small list of [Selenium atoms] to interact with
web elements.  Initially those have been added to ensure a better
reliability due to a wider usage inside the Selenium project. But
by adding full support for the [WebDriver specification] they will
be removed step by step.

Currently the following atoms are in use:

- `getElementText`
- `isElementDisplayed`
- `isElementEnabled`

To use one of those atoms Javascript modules will have to import
[atom.sys.mjs].

[Selenium atoms]: https://github.com/SeleniumHQ/selenium/tree/master/javascript/webdriver/atoms
[WebDriver specification]: https://w3c.github.io/webdriver/webdriver-spec.html
[atom.sys.mjs]: https://searchfox.org/mozilla-central/source/remote/marionette/atom.sys.mjs

## Update required Selenium atoms

In regular intervals the atoms, which are still in use, have to
be updated.  Therefore they have to be exported from the Selenium
repository first, and then updated in [atom.sys.mjs].

### Export Selenium Atoms

The canonical GitHub repository for Selenium is

  <https://github.com/SeleniumHQ/selenium.git>

so make sure to have an up-to-date local copy of it. If you have to clone
it first, it is recommended to specify the `--depth=1` argument, so only the
last changeset is getting downloaded (which itself might already be
more than 100 MB).

```bash
git clone --depth=1 https://github.com/SeleniumHQ/selenium.git
```

To export the correct version of the atoms identify the changeset id (SHA1) of
the Selenium repository in the [index section] of the WebDriver specification.

Fetch that changeset and check it out:

```bash
git fetch --depth=1 origin SHA1
git checkout SHA1
```

Now you can export all the required atoms by running the following
commands. Make sure to [install bazelisk] first.

```bash
bazel build //javascript/atoms/fragments:get-text
bazel build //javascript/atoms/fragments:is-displayed
bazel build //javascript/atoms/fragments:is-enabled
```

For each of the exported atoms a file can now be found in the folder
`bazel-bin/javascript/atoms/fragments/`.  They contain all the
code including dependencies for the atom wrapped into a single function.

[index section]: <https://w3c.github.io/webdriver/#index>
[install bazelisk]: <https://github.com/bazelbuild/bazelisk#installation>

### Update atom.sys.mjs

To update the atoms for Marionette the `atoms.js` file has to be edited. For
each atom to be updated the steps as laid out below have to be performed:

1. Open the Javascript file of the exported atom. See above for
   its location.

2. Add the related function name and `element` as parameters to the wrapper
   function, which can be found at the very beginning of the file so that it
   is equal to the parameters in `atom.sys.mjs`.

3. Copy and paste the whole contents of the file into the left textarea on
   <https://jsonformatter.org/json-stringify-online> to get a stringified
   version of all the required functions.

4. Copy and paste the whole contents of the right textarea, and replace the
   existing code for the atom in `atom.sys.mjs`.

### Test the changes

To ensure that the update of the atoms doesn't cause a regression
a try build should be run including Marionette unit tests, Firefox
ui tests, and all the web-platform-tests.
