# Style guide

Like other projects, we also have some guidelines to keep to the code.
For the overall Remote Agent project, a few rough rules are:

* Make your code readable and sensible, and don’t try to be clever.
  Prefer simple and easy solutions over more convoluted and foreign syntax.

* Fixing style violations whilst working on a real change as a preparatory
  clean-up step is good, but otherwise avoid useless code churn for the sake
  of conforming to the style guide.

* Code is mutable and not written in stone.  Nothing that is checked in is
  sacred and we encourage change to make this a pleasant ecosystem to work in.

* We never land any code that is unnecessary or unused.

## Documentation

We keep our documentation (what you are reading right now!) in-tree
under [remote/doc].  Updates and minor changes to documentation should
ideally not be scrutinized to the same degree as code changes to
encourage frequent updates so that documentation does not go stale.
To that end, documentation changes with `r=me a=doc` from anyone
with commit access level 3 are permitted.

Use fmt(1) or an equivalent editor specific mechanism (such as
`Meta-Q` in Emacs) to format paragraphs at a maximum of
75 columns with a goal of roughly 65.  This is equivalent to `fmt
-w75 -g65`, which happens to the default on BSD and macOS.

The documentation can be built locally this way:

```shell
% ./mach doc remote
```

[remote/doc]: https://searchfox.org/mozilla-central/source/remote/doc

## Linting

The Remote Agent consists mostly of JavaScript code, and we lint that
using [mozlint], which is harmonizes different linters including [eslint].

To run the linter and get sensible output for modified files:

```shell
% ./mach lint --outgoing --warning
```

For certain classes of style violations, eslint has an automatic
mode for fixing and formatting code.  This is particularly useful
to keep to whitespace and indentation rules:

```shell
% ./mach lint --outgoing --warning --fix
```

The linter is also run as a try job (shorthand `ES`) which means
any style violations will automatically block a patch from landing
(if using autoland) or cause your changeset to be backed out (if
landing directly on inbound).

If you use git(1) you can [enable automatic linting] before
you push to a remote through a pre-push (or pre-commit) hook.
This will run the linters on the changed files before a push and
abort if there are any problems.  This is convenient for avoiding
a try run failing due to a simple linting issue.

[mozlint]: /code-quality/lint/mozlint.rst
[eslint]: /code-quality/lint/linters/eslint.rst
[enable automatic linting]: /code-quality/lint/usage.rst#using-a-vcs-hook
