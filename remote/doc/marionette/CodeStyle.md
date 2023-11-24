# Style guide

Like other projects, we also have some guidelines to keep to the code.
For the overall Marionette project, a few rough rules are:

* Make your code readable and sensible, and don’t try to be
  clever.  Prefer simple and easy solutions over more convoluted
  and foreign syntax.

* Fixing style violations whilst working on a real change as a
  preparatory clean-up step is good, but otherwise avoid useless
  code churn for the sake of conforming to the style guide.

* Code is mutable and not written in stone.  Nothing that
  is checked in is sacred and we encourage change to make
  remote/marionette a pleasant ecosystem to work in.

## JavaScript

Marionette is written in JavaScript and ships
as part of Firefox.  We have access to all the latest ECMAScript
features currently in development, usually before it ships in the
wild and we try to make use of new features when appropriate,
especially when they move us off legacy internal replacements
(such as Promise.jsm and Task.jsm).

One of the peculiarities of working on JavaScript code that ships as
part of a runtime platform is, that unlike in a regular web document,
we share a single global state with the rest of Firefox.  This means
we have to be responsible and not leak resources unnecessarily.

JS code in Gecko is organised into _modules_ carrying _.js_ or _.jsm_
file extensions.  Depending on the area of Gecko you’re working on,
you may find they have different techniques for exporting symbols,
varying indentation and code style, as well as varying linting
requirements.

To export symbols to other Marionette modules, remember to assign
your exported symbols to the shared global `this`:

```javascript
const EXPORTED_SYMBOLS = ["PollPromise", "TimedPromise"];
```

When importing symbols in Marionette code, try to be specific about
what you need:

```javascript
const { TimedPromise } = ChromeUtils.import(
  "chrome://remote/content/marionette/sync.js"
);
```

We prefer object assignment shorthands when redefining names,
for example when you use functionality from the `Components` global:

```javascript
const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
```

When using symbols by their own name, the assignment name can be
omitted:

```javascript
const {TYPE_ONE_SHOT, TYPE_REPEATING_SLACK} = Ci.nsITimer;
```

In addition to the default [Mozilla eslint rules], we have [our
own specialisations] that are stricter and enforce more security.
A few notable examples are that we disallow fallthrough `case`
statements unless they are explicitly grouped together:

```javascript
switch (x) {
  case "foo":
    doSomething();

  case "bar":  // <-- disallowed!
    doSomethingElse();
    break;

  case "baz":
  case "bah":  // <-- allowed (-:
    doCrazyThings();
}
```

We disallow the use of `var`, for which we always prefer `let` and
`const` as replacements.  Do be aware that `const` does not mean
that the variable is immutable: just that it cannot be reassigned.
We require all lines to end with semicolons, disallow construction
of plain `new Object()`, require variable names to be camel-cased,
and complain about unused variables.

For purely aesthetic reasons we indent our code with two spaces,
which includes switch-statement `case`s, and limit the maximum
line length to 78 columns.  When you need to wrap a statement to
the next line, the second line is indented with four spaces, like this:

```javascript
throw new TypeError(pprint`Expected an element or WindowProxy, got: ${el}`);
```

This is not normally something you have to think to deeply about as
it is enforced by the [linter].  The linter also has an automatic
mode that fixes and formats certain classes of style violations.

If you find yourself struggling to fit a long statement on one line,
this is usually an indication that it is too long and should be
split into multiple lines.  This is also a helpful tip to make the
code easier to read.  Assigning transitive values to descriptive
variable names can serve as self-documentation:

```javascript
let location = event.target.documentURI || event.target.location.href;
log.debug(`Received DOM event ${event.type} for ${location}`);
```

On the topic of variable naming the opinions are as many as programmers
writing code, but it is often helpful to keep the input and output
arguments to functions descriptive (longer), and let transitive
internal values to be described more succinctly:

```javascript
/** Prettifies instance of Error and its stacktrace to a string. */
function stringify(error) {
  try {
    let s = error.toString();
    if ("stack" in error) {
      s += "\n" + error.stack;
    }
    return s;
  } catch (e) {
    return "<unprintable error>";
  }
}
```

When we can, we try to extract the relevant object properties in
the arguments to an event handler or a function:

```javascript
const responseListener = ({name, target, json, data}) => { … };
```

Instead of:

```javascript
const responseListener = msg => {
  let name = msg.name;
  let target = msg.target;
  let json = msg.json;
  let data = msg.data;
  …
};
```

All source files should have `"use strict";` as the first directive
so that the file is parsed in [strict mode].

Every source code file that ships as part of the Firefox bundle
must also have a [copying header], such as this:

```javascript
    /* This Source Code Form is subject to the terms of the Mozilla Public
     * License, v. 2.0. If a copy of the MPL was not distributed with this file,
     * You can obtain one at http://mozilla.org/MPL/2.0/. */
```

New xpcshell test files _should not_ have a license header as all
new Mozilla tests should be in the [public domain] so that they can
easily be shared with other browser vendors.  We want to re-license
existing tests covered by the [MPL] so that they can be shared.
We very much welcome your help in doing version control archeology
to make this happen!

The practical details of working on the Marionette code is outlined
in [Contributing.md], but generally you do not have to re-build
Firefox when changing code.  Any change to remote/marionette/*.js
will be picked up on restarting Firefox.  The only notable exception
is remote/components/Marionette.jsm, which does require
a re-build.

[strict mode]: https://developer.mozilla.org/docs/Web/JavaScript/Reference/Strict_mode
[Mozilla eslint rules]: https://searchfox.org/mozilla-central/source/.eslintrc.js
[our own specialisations]: https://searchfox.org/mozilla-central/source/remote/marionette/.eslintrc.js
[linter]: #linting
[copying header]: https://www.mozilla.org/en-US/MPL/headers/
[public domain]: https://creativecommons.org/publicdomain/zero/1.0/
[MPL]: https://www.mozilla.org/en-US/MPL/2.0/
[Contributing.md]: ./Contributing.md

## Python

TODO

## Documentation

We keep our documentation in-tree under [remote/doc/marionette]
and [testing/geckodriver/doc].  Updates and minor changes to
documentation should ideally not be scrutinised to the same degree
as code changes to encourage frequent updates so that the documentation
does not go stale.  To that end, documentation changes with `r=me`
from module peers are permitted.

Use fmt(1) or an equivalent editor specific mechanism (such as Meta-Q
in Emacs) to format paragraphs at a maximum width of 75 columns
with a goal of roughly 65.  This is equivalent to `fmt -w 75 -g 65`,
which happens to be the default on BSD and macOS.

We endeavour to document all _public APIs_ of the Marionette component.
These include public functions—or command implementations—on
the `GeckoDriver` class, as well as all exported symbols from
other modules.  Documentation for non-exported symbols is not required.

[remote/doc/marionette]: https://searchfox.org/mozilla-central/source/remote/marionette/doc
[testing/geckodriver/doc]: https://searchfox.org/mozilla-central/source/testing/geckodriver/doc

## Linting

Marionette consists mostly of JavaScript (server) and Python (client,
harness, test runner) code.  We lint our code with [mozlint],
which harmonises the output from [eslint] and [ruff].

To run the linter with a sensible output:

```shell
% ./mach lint -funix remote/marionette
```

For certain classes of style violations the eslint linter has
an automatic mode for fixing and formatting your code.  This is
particularly useful to keep to whitespace and indentation rules:

```shell
% ./mach eslint --fix remote/marionette
```

The linter is also run as a try job (shorthand `ES`) which means
any style violations will automatically block a patch from landing
(if using Autoland) or cause your changeset to be backed out (if
landing directly on mozilla-inbound).

If you use git(1) you can [enable automatic linting] before you push
to a remote through a pre-push (or pre-commit) hook.  This will
run the linters on the changed files before a push and abort if
there are any problems.  This is convenient for avoiding a try run
failing due to a stupid linting issue.

[mozlint]: /code-quality/lint/mozlint.rst
[eslint]: /code-quality/lint/linters/eslint.rst
[ruff]: /code-quality/lint/linters/ruff.rst
[enable automatic linting]: /code-quality/lint/usage.rst#using-a-vcs-hook
