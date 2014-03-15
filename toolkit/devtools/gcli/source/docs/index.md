
# About GCLI

## GCLI is a Graphical Command Line Interpreter.

GCLI is a command line for modern computers. When command lines were invented,
computers were resource-limited, disconnected systems with slow multi-tasking
and poor displays. The design of the Unix CLI made sense in 1970, but over 40
years on, considering the pace of change, there are many improvements we can
make.

CLIs generally suffer from poor discoverability; It's hard when faced with a
blank command line to work out what to do. As a result the majority of programs
today use purely graphical user interfaces, however in doing so, they lose some
of the benefits of CLIs. CLIs are still used because generally, in the hands of
a skilled user they are faster, and have a wider range of available options.

GCLI attempts to get the best of the GUI world and the CLI world to produce
something that is both easy to use and learn as well as fast and powerful.

GCLI has a type system to help ensure that users are inputting valid commands
and to enable us to provide sensible context sensitive help. GCLI provides
integration with JavaScript rather than being an alternative (like CoffeeScript).


## History

GCLI was born as part of the
[Bespin](http://ajaxian.com/archives/canvas-for-a-text-editor) project and was
[discussed at the time](http://j.mp/bespin-cli). The command line component
survived the rename of Bepsin to Skywriter and the merger with Ace, got a name
of it's own (Cockpit) which didn't last long before the project was named GCLI.
It is now being used in the Firefox's web console where it doesn't have a
separate identity but it's still called GCLI outside of Firefox. It is also
used in [Eclipse Orion](http://www.eclipse.org/orion/).


## Environments

GCLI is designed to work in a number of environments:

1. As a component of Firefox developer tools.
2. As an adjunct to Orion/Ace and other online editors.
3. As a plugin to any web-page wishing to provide its own set of commands.
4. As part of a standalone web browser extension with it's own set of commands.


## Related Pages

Other sources of GCLI documentation:

- [Writing Commands](writing-commands.md)
- [Writing Types](writing-types.md)
- [Developing GCLI](developing-gcli.md)
- [Writing Tests](writing-tests.md) / [Running Tests](running-tests.md)
- [The Design of GCLI](design.md)
- Source
  - The most up-to-date source is in [this Github repository](https://github.com/joewalker/gcli/).
  - When a feature is 'done' it's merged into the [Mozilla clone](https://github.com/mozilla/gcli/).
  - From which it flows into [Mozilla Central](https://hg.mozilla.org/mozilla-central/file/tip/browser/devtools/commandline).
- [Demo of GCLI](http://mozilla.github.com/gcli/) with an arbitrary set of demo
  commands
- Other Documentation
  - [Embedding docs](https://github.com/mozilla/gcli/blob/master/docs/index.md)
  - [Status page](http://mozilla.github.com/devtools/2011/status.html#gcli)


## Accessibility

GCLI uses ARIA roles to guide a screen-reader as to the important sections to
voice. We welcome [feedback on how these roles are implemented](https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox&component=Developer+Tools:+Graphic+Commandline+and+Toolbar&rep_platform=All&op_sys=All&short_desc=GCLI).

The command line uses TAB as a method of completing current input, this
prevents use of TAB for keyboard navigation. Instead of using TAB to move to
the next field you can use F6. In addition to F6, ALT+TAB, CTRL+TAB, META+TAB
make an attempt to move the focus on. How well this works depends on your
OS/browser combination.


## Embedding GCLI

There are 3 basic steps in using GCLI in your system.

1. Import a GCLI JavaScript file.
   For serious use of GCLI you are likely to be creating a custom build (see
   below) however if you just want to have a quick play, you can use
   ``gcli-uncompressed.js`` from [the gh-pages branch of GCLI]
   (https://github.com/mozilla/gcli/tree/gh-pages)
   Just place the following wherever you place your script files.

        <script src="path/to/gcli-uncompressed.js" type="text/javascript"></script>

2. Having imported GCLI, we need to tell it where to display. The simplest
   method is to include an elements with the id of ``gcli-input`` and
   ``gcli-display``.

        <input id="gcli-input" type="text"/>
        <div id="gcli-display"></div>

3. Tell GCLI what commands to make available. See the sections on Writing
   Commands, Writing Types and Writing Fields for more information.

   GCLI uses the CommonJS AMD format for it's files, so a 'require' statement
   is needed to get started.

        require([ 'gcli/index' ], function(gcli) {
          gcli.addCommand(...); // Register custom commands
          gcli.createTerminal(); // Create a user interface
        });

   The createTerminal() function takes an ``options`` objects which allows
   customization. At the current time the documentation of these object is left
   to the source.


## Backwards Compatibility

The goals of the GCLI project are:

- Aim for very good backwards compatibility with code required from an
  'index' module. This means we will not break code without a cycle of
  deprecation warnings.

  There are currently 3 'index' modules:
  - gcli/index (all you need to get started with GCLI)
  - demo/index (a number of demo commands)
  - gclitest/index (GCLI test suite)

  Code from these modules uses the module pattern to prevent access to internal
  functions, so in essence, if you can get to it from an index module, you
  should be ok.

- We try to avoid needless change to other modules, however we don't make any
  promises, and don't provide a deprecation cycle.

  Code from other modules uses classes rather than modules, so member variables
  are exposed. Many classes mark private members using the `_underscorePrefix`
  pattern. Particular care should be taken if access is needed to a private
  member.


## Creating Custom Builds

GCLI uses [DryIce](https://github.com/mozilla/dryice) to create custom builds.
If dryice is installed (``npm install .``) then you can create a built
version of GCLI simply using ``node gcli.js standard``. DryIce supplies a custom
module loader to replace RequireJS for built applications.

The build will be output to the ``built`` directory. The directory will be
created if it doesn't exist.
