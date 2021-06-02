# Contributing

Contributions are welcome, and they are greatly appreciated! Every little bit
helps, and credit will always be given.

You can contribute in many ways:

## Types of Contributions

### Report Bugs

Report bugs at
[bugzilla](https://bugzilla.mozilla.org/enter_bug.cgi?assigned_to=nobody%40mozilla.org&bug_ignored=0&bug_severity=normal&bug_status=NEW&cf_fission_milestone=---&cf_fx_iteration=---&cf_fx_points=---&cf_status_firefox65=---&cf_status_firefox66=---&cf_status_firefox67=---&cf_status_firefox_esr60=---&cf_status_thunderbird_esr60=---&cf_tracking_firefox65=---&cf_tracking_firefox66=---&cf_tracking_firefox67=---&cf_tracking_firefox_esr60=---&cf_tracking_firefox_relnote=---&cf_tracking_thunderbird_esr60=---&product=Data%20Platform%20and%20Tools&component=Glean%3A%20SDK&contenttypemethod=list&contenttypeselection=text%2Fplain&defined_groups=1&flag_type-203=X&flag_type-37=X&flag_type-41=X&flag_type-607=X&flag_type-721=X&flag_type-737=X&flag_type-787=X&flag_type-799=X&flag_type-800=X&flag_type-803=X&flag_type-835=X&flag_type-846=X&flag_type-855=X&flag_type-864=X&flag_type-916=X&flag_type-929=X&flag_type-930=X&flag_type-935=X&flag_type-936=X&flag_type-937=X&form_name=enter_bug&maketemplate=Remember%20values%20as%20bookmarkable%20template&op_sys=Unspecified&priority=P3&&rep_platform=Unspecified&status_whiteboard=%5Btelemetry%3Aglean-rs%3Am%3F%5D&target_milestone=---&version=unspecified).

If you are reporting a bug, please include:

- Your operating system name and version.
- Any details about your local setup that might be helpful in troubleshooting.
- Detailed steps to reproduce the bug.

### Fix Bugs

Look through the GitHub issues for bugs. Anything tagged with "bug" and "help
wanted" is open to whoever wants to implement it.

### Implement Features

Look through the GitHub issues for features. Anything tagged with "enhancement"
and "help wanted" is open to whoever wants to implement it.

### Write Documentation

`glean_parser` could always use more documentation, whether as part of the
official `glean_parser` docs, in docstrings, or even on the web in blog posts,
articles, and such.

### Submit Feedback

The best way to send feedback is to file an issue at TODO

If you are proposing a feature:

- Explain in detail how it would work.
- Keep the scope as narrow as possible, to make it easier to implement.
- Remember that this is a volunteer-driven project, and that contributions are
  welcome :)

## Get Started!

Ready to contribute? Here's how to set up `glean_parser` for local
development.

1. Fork the `glean_parser` repo on GitHub.

2. Clone your fork locally:

    ```sh
    $ git clone git@github.com:your_name_here/glean_parser.git
    ```

3. Install your local copy into a virtualenv. Assuming you have
   virtualenvwrapper installed, this is how you set up your fork for local
   development:

    ```sh
    $ mkvirtualenv glean_parser
    $ cd glean_parser/
    $ pip install --editable .
    ```

4. Create a branch for local development:

    ```sh
    $ git checkout -b name-of-your-bugfix-or-feature
    ```

    Now you can make your changes locally.

5. To test your changes to `glean_parser`:

   Install the testing dependencies:

    ```sh
    $ pip install -r requirements_dev.txt
    ```

   Optionally, if you want to ensure that the generated Kotlin code lints
   correctly, install a Java SDK, and then run:

    ```sh
    $ make install-kotlin-linters
    ```

   Then make sure that all lints and tests are passing:

    ```sh
    $ make lint
    $ make test
    ```

6. Commit your changes and push your branch to GitHub:

    ```sh
    $ git add .
    $ git commit -m "Your detailed description of your changes."
    $ git push origin name-of-your-bugfix-or-feature
    ```

7. Submit a pull request through the GitHub website.

## Pull Request Guidelines

Before you submit a pull request, check that it meets these guidelines:

1. The pull request should include tests.
2. If the pull request adds functionality, the docs should be updated. Put your
   new functionality into a function with a docstring, and describe
   public-facing features in the docs.
3. The pull request should work for Python 3.6, 3.7, 3.8 and 3.9 (The CI system
   will take care of testing all of these Python versions).
4. The pull request should update the changelog in `CHANGELOG.md`.

## Tips

To run a subset of tests:

```sh
$ py.test tests.test_glean_parser
```

## Deploying

A reminder for the maintainers on how to deploy.

Get a clean main branch with all of the changes from `upstream`:

```sh
$ git checkout main
$ git fetch upstream
$ git rebase upstream/main
```

- Update the header with the new version and date in `CHANGELOG.md`.

- (By using the setuptools-scm package, there is no need to update the
  version anywhere else).

- Make sure all your changes are committed.

- Push the changes upstream. (Normally pushing directly without review
  is frowned upon, but the `main` branch is protected from force
  pushes and release tagging requires the same permissions as pushing
  to `main`):

  ```sh
  $ git push upstream main
  ```

- Wait for [continuous integration to
  pass](https://circleci.com/gh/mozilla/glean_parser/tree/main) on main.

- Make the release on GitHub using [this
  link](https://github.com/mozilla/glean_parser/releases/new)

- Both the tag and the release title should be in the form `vX.Y.Z`.

- Copy and paste the relevant part of the `CHANGELOG.md` file into the
  description.

- Tagging the release will trigger a CI workflow which will build the
  distribution of `glean_parser` and publish it to PyPI.

The continuous integration system will then automatically deploy to
PyPI.

See also:

- The [instructions for updating the version of `glean_parser`
used by the Glean
SDK](https://mozilla.github.io/glean/dev/upgrading-glean-parser.html).
- The [instructions for updating the version of `glean_parser`
used by Glean.js](https://github.com/mozilla/glean.js/tree/main/docs/update_glean_parser.md).
