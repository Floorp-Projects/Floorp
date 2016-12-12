marionette-harness
==================

Marionette is an automation driver for Mozilla's Gecko engine. It can remotely
control either the UI or the internal JavaScript of a Gecko platform, such as
Firefox. It can control both the chrome (i.e. menus and functions) or the
content (the webpage loaded inside the browsing context), giving a high level
of control and ability to replicate user actions. In addition to performing
actions on the browser, Marionette can also read the properties and attributes
of the DOM.

The marionette_harness package contains the test runner for Marionette, and
allows you to run automated tests written in Python for Gecko based
applications. Therefore it offers the necessary testcase classes, which are
based on the unittest framework.

For more information and the repository please checkout:

- home and docs: https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette


Example
-------

The following command will run the tests as specified via a manifest file, or
test path, or test folder in Firefox:

    marionette --binary %path_to_firefox% [manifest_file | test_file | test_folder]

To get an overview about all possible option run `marionette --help`.
