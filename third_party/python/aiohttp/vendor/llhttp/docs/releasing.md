# How to release a new version of llhttp

## What does releasing involves?

These are the required steps to release a new version of llhttp:

1. Increase the version number.
2. Build it locally.
3. Create a new build and push it to GitHub.
4. Create a new release on GitHub release.

> Do not try to execute the commands in the Makefile manually. This is really error-prone!

## Which commands to run?

First of all, make sure you have [GitHub CLI](https://cli.github.com) installed and configured. While this is not strictly necessary, it will make your life way easier.

As a preliminary check, run the build command and execute the test suite locally:

```
npm run build
npm test
```

If all goes good, you are ready to go!

To release a new version of llhttp, first increase the version using `npm` and make sure it also execute the `postversion` script. Unless you have some very specific setup, this should happen automatically, which means the following command will suffice:

```
npm version [major|minor|patch]
```

The command will increase the version and then will create a new release branch on GitHub.

> Even thought there is a package on NPM, it is not updated anymore. NEVER RUN `npm publish`!

It's now time to create the release on GitHub. If you DON'T have GitHub CLI available, skip to the next section, otherwise run the following command:

```
npm run github-release
```

This command will create a draft release on GitHub and then show it in your browser so you can review and publish it.

Congratulation, you are all set!

## Create a GitHub release without GitHub CLI

> From now on, `$VERSION` will be the new version you are trying to create, including the leading letter, for instance `v6.0.9`.

If you don't want to or can't use GitHub CLI, you can still create the release on GitHub following this procedure.

1. Go on GitHub and start creating a new release which targets tag `$VERSION`. Generate the notes using the `Generate release notes` button.

2. At the bottom of the generated notes, make sure the previous and current version in the notes are correct.
   
   The last line should be something like this: `**Full Changelog**: https://github.com/nodejs/llhttp/compare/v6.0.8...v6.0.9`

   In this case it says we are creating release `v6.0.9` and we are showing the changes between `v6.0.8` and `v6.0.9`.

3. Change the target of the release to point to tag `release/$VERSION`.

4. Review and then publish the release.

Congratulation, you are all set!