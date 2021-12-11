Selenium atoms
==============

Marionette uses a small list of [Selenium atoms] to interact with
web elements.  Initially those have been added to ensure a better
reliability due to a wider usage inside the Selenium project. But
by adding full support for the [WebDriver specification] they will
be removed step by step.

Currently the following atoms are in use:

- `getElementText`
- `isDisplayed`

To use one of those atoms Javascript modules will have to import
[atom.js].

[Selenium atoms]: https://github.com/SeleniumHQ/selenium/tree/master/javascript/webdriver/atoms
[WebDriver specification]: https://w3c.github.io/webdriver/webdriver-spec.html
[atom.js]: https://searchfox.org/mozilla-central/source/remote/marionette/atom.js


Update required Selenium atoms
------------------------------

In regular intervals the atoms, which are still in use, have to
be updated.  Therefore they have to be exported from the Selenium
repository first, and then updated in [atom.js].


### Export Selenium Atoms

The canonical GitHub repository for Selenium is

	https://github.com/SeleniumHQ/selenium.git

so make sure to have a local copy of it. For the cloning process
it is recommended to specify the `--depth=1` argument, so only the
last changeset is getting downloaded (which itself will already be
more than 100 MB). Once the clone is ready the export of the atoms
can be triggered by running the following commands:

	% cd selenium
	% ./go
	% python buck-out/crazy-fun/%changeset%/buck.pex build --show-output %atom%

Hereby `%changeset%` corresponds to the currently used version of
buck, and `%atom%` to the atom to export. The following targets
for exporting are available:

  - `//javascript/webdriver/atoms:clear-element-firefox`
  - `//javascript/webdriver/atoms:get-text-firefox`
  - `//javascript/webdriver/atoms:is-displayed-firefox`
  - `//javascript/webdriver/atoms:is-enabled-firefox`
  - `//javascript/webdriver/atoms:is-selected-firefox`

For each of the exported atoms a file can now be found in the folder
`buck-out/gen/javascript/webdriver/atoms/`.  They contain all the
code including dependencies for the atom wrapped into a single function.


### Update atom.js

To update the atoms for Marionette the `atoms.js` file has to be edited. For
each atom to be updated the steps as laid out below have to be performed:

1. Open the Javascript file of the exported atom. See above for
   its location.

2. Remove the contained license header, which can be found somewhere
   in the middle of the file.

3. Update the parameters of the wrapper function (at the very top)
   so that those are equal with the used parameters in `atom.js`.

4. Copy the whole content of the file, and replace the existing
   code for the atom in `atom.js`.


### Test the changes

To ensure that the update of the atoms doesn't cause a regression
a try build should be run including Marionette unit tests, Firefox
ui tests, and all the web-platform-tests.
