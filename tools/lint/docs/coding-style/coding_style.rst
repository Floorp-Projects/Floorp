============
Coding style
============


This document attempts to explain the basic styles and patterns used in
the Mozilla codebase. New code should try to conform to these standards,
so it is as easy to maintain as existing code. There are exceptions, but
it's still important to know the rules!

This article is particularly for those new to the Mozilla codebase, and
in the process of getting their code reviewed. Before requesting a
review, please read over this document, making sure that your code
conforms to recommendations.

.. container:: blockIndicator warning

   Firefox code base uses the `Google Coding style for C++
   code <https://google.github.io/styleguide/cppguide.html>`__

Article navigation
------------------

#. `Naming_and_formatting_code <#Naming_and_Formatting_code>`__.
#. `General practices <#General_C.2FC.2B.2B_Practices>`__.
#. `C/C++ practices <#CC_practices>`__.
#. `JavaScript practices <#JavaScript_practices>`__.
#. `Java practices <#Java_practices>`__.
#. `Makefile/moz.build practices <#Makefile_moz.build_practices>`__.
#. `Python practices <#Python_Practices>`__.
#. `SVG practices <#SVG_practices>`__.
#. `COM, pointers and strings <#COM_and_pointers>`__.
#. `IDL <#IDL>`__.
#. `Error handling <#Error_handling>`__.
#. `C++ strings <#Strings>`__.
#. `Usage of PR_(MAX|MIN|ABS|ROUNDUP) macro
   calls <#Usage_of_PR_(MAXMINABSROUNDUP)_macro_calls>`__


Naming and formatting code
--------------------------

*The following norms should be followed for new code. For existing code,
use the prevailing style in a file or module, ask the owner if you are
in another team's codebase or it's not clear what style to use.*

Whitespace
~~~~~~~~~~

No tabs. No whitespace at the end of a line.

Unix-style linebreaks ('\n'), not Windows-style ('\r\n'). You can
convert patches, with DOS newlines to Unix via the 'dos2unix' utility,
or your favorite text editor.

Line length
~~~~~~~~~~~

80 characters or less (for laptop side-by-side diffing and two-window
tiling; also for Bonsai / hgweb and hardcopy printing).

Following the Google coding style, we are using 100 characters line for
Objective-C/C++.


Indentation
~~~~~~~~~~~

Two spaces per logic level, or four spaces in Python code.

Note that class visibility and ``goto`` labels do not consume a logic
level, but ``switch`` ``case`` labels do. See examples below.


Control structures
~~~~~~~~~~~~~~~~~~

Use `K&R bracing
style <https://en.wikipedia.org/wiki/Indentation_style#K&R>`__: left
brace at end of first line, 'cuddled' else on both sides.

.. note::

   **Note:** Class and function definitions are not control structures;
   left brace goes by itself on the second line and without extra
   indentation, generally in C++.

Always brace controlled statements, even a single-line consequent of
``if else else``. This is redundant, typically, but it avoids dangling
else bugs, so it's safer at scale than fine-tuning.

Examples:

.. code-block:: cpp

   if (...) {
   } else if (...) {
   } else {
   }

   while (...) {
   }

   do {
   } while (...);

   for (...; ...; ...) {
   }

   switch (...) {
     case 1: {
       // When you need to declare a variable in a switch, put the block in braces.
       int var;
       break;
     }
     case 2:
       ...
       break;
     default:
       break;
   }

You can achieve the following ``switch`` statement indentation in emacs
by setting the "case-label" offset:

::

   (c-set-offset 'case-label '+)

| Control keywords ``if``, ``for``, ``while``, and ``switch`` are always
  followed by a space to distinguish them from function calls, which
  have no trailing space.
| ``else`` should only ever be followed by ``{`` or ``if``; i.e., other
  control keywords are not allowed and should be placed inside braces.


C++ namespaces
~~~~~~~~~~~~~~

Mozilla project C++ declarations should be in the "``mozilla``"
namespace. Modules should avoid adding nested namespaces under
"``mozilla``", unless they are meant to contain names which have a high
probability of colliding with other names in the code base. For example,
``Point``, ``Path``, etc. Such symbols can be put under
module-specific namespaces, under "``mozilla``", with short
all-lowercase names. Other global namespaces besides "``mozilla``" are
not allowed.

No "using" statements are allowed in header files, except inside class
definitions or functions. (We don't want to pollute the global scope of
compilation units that use the header file.)

``using namespace ...;`` is only allowed in ``.cpp`` files after all
``#include``\ s. Prefer to wrap code in ``namespace ... { ... };``
instead. if possible. ``using namespace ...;``\ should always specify
the fully qualified namespace. That is, to use ``Foo::Bar`` do not
write ``using namespace Foo;``\ ``using namespace Bar;``, write
``using namespace Foo::Bar;``

Don't indent code inside ``namespace ... { ... }``. You can prevent
this, inside emacs, by setting the "innamespace" offset:

::

   (c-set-offset 'innamespace 0)


Anonymous namespaces
~~~~~~~~~~~~~~~~~~~~

We prefer using "static", instead of anonymous C++ namespaces. This may
change once there is better debugger support (especially on Windows) for
placing breakpoints, etc. on code in anonymous namespaces. You may still
use anonymous namespaces for things that can't be hidden with 'static',
such as types, or certain objects which need to be passed to template
functions.


C++ classes
~~~~~~~~~~~~

.. code-block:: cpp

   namespace mozilla {

   class MyClass : public A
   {
     ...
   };

   class MyClass
     : public X  // When deriving from more than one class, put each on its own line.
     , public Y
   {
   public:
     MyClass(int aVar, int aVar2)
       : mVar(aVar)
       , mVar2(aVar2)
     {
        ...
     }

     // Tiny constructors and destructors can be written on a single line.
     MyClass() { ... }

     // Special member functions, like constructors, that have default bodies
     // should use '= default' annotation instead.
     MyClass() = default;

     // Unless it's a copy or move constructor or you have a specific reason to allow
     // implicit conversions, mark all single-argument constructors explicit.
     explicit MyClass(OtherClass aArg)
     {
       ...
     }

     // This constructor can also take a single argument, so it also needs to be marked
     // explicit.
     explicit MyClass(OtherClass aArg, AnotherClass aArg2 = AnotherClass())
     {
       ...
     }

     int TinyFunction() { return mVar; }  // Tiny functions can be written in a single line.

     int LargerFunction()
     {
       ...
       ...
     }

   private:
     int mVar;
   };

   } // namespace mozilla

Define classes using the style given above.

Existing classes in the global namespace are named with a short prefix
(For example, "ns") as a pseudo-namespace.

For small functions, constructors, or other braced constructs, it's okay
to collapse the definition to one line, as shown for ``TinyFunction``
above. For larger ones, use something similar to method declarations,
below.


Methods and functions
~~~~~~~~~~~~~~~~~~~~~


C/C++
^^^^^

In C/C++, method names should be capitalized and use camelCase.
Typenames, and the names of arguments, should be separated with a single
space character.

.. code-block:: cpp

   template<typename T>  // Templates on own line.
   static int            // Return type on own line for top-level functions.
   MyFunction(const nsACstring& aStr,
              mozilla::UniquePtr<const char*>&& aBuffer,
              nsISupports* aOptionalThing = nullptr)
   {
     ...
   }

   int
   MyClass::Method(const nsACstring& aStr,
                   mozilla::UniquePtr<const char*>&& aBuffer,
                   nsISupports* aOptionalThing = nullptr)
   {
     ...
   }

Getters that never fail, and never return null, are named ``Foo()``,
while all other getters use ``GetFoo()``. Getters can return an object
value, via a ``Foo** aResult`` outparam (typical for an XPCOM getter),
or as an ``already_AddRefed<Foo>`` (typical for a WebIDL getter,
possibly with an ``ErrorResult& rv`` parameter), or occasionally as a
``Foo*`` (typical for an internal getter for an object with a known
lifetime). See `the bug 223255 <https://bugzilla.mozilla.org/show_bug.cgi?id=223255>`_
for more information.

XPCOM getters always return primitive values via an outparam, while
other getters normally use a return value.

Method declarations must use, at most, one of the following keywords:
``virtual``, ``override``, or ``final``. Use ``virtual`` to declare
virtual methods, which do not override a base class method with the same
signature. Use ``override`` to declare virtual methods which do
override a base class method, with the same signature, but can be
further overridden in derived classes. Use ``final`` to declare virtual
methods which do override a base class method, with the same signature,
but can NOT be further overridden in the derived classes. This should
help the person reading the code fully understand what the declaration
is doing, without needing to further examine base classes.

JavaScript
^^^^^^^^^^

In JavaScript, functions should use camelCase, but should not capitalize
the first letter. Methods should not use the named function expression
syntax, because our tools understand method names:

.. code-block:: cpp

   doSomething: function (aFoo, aBar) {
     ...
   }

In-line functions should have spaces around braces, except before commas
or semicolons:

.. code-block:: cpp

   function valueObject(aValue) { return { value: aValue }; }


JavaScript objects
~~~~~~~~~~~~~~~~~~

.. code-block:: cpp

   var foo = { prop1: "value1" };

   var bar = {
     prop1: "value1",
     prop2: "value2"
   };

Constructors for objects should be capitalized and use Pascal Case:

.. code-block:: cpp

   function ObjectConstructor() {
     this.foo = "bar";
   }


Mode line
~~~~~~~~~

Files should have Emacs and vim mode line comments as the first two
lines of the file, which should set indent-tabs-mode to nil. For new
files, use the following, specifying two-space indentation:

.. code-block:: cpp

   /* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
   /* vim: set ts=8 sts=2 et sw=2 tw=80: */
   /* This Source Code Form is subject to the terms of the Mozilla Public
    * License, v. 2.0. If a copy of the MPL was not distributed with this
    * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

Be sure to use the correct "Mode" in the first line, don't use "C++" in
JavaScript files. The exception to this is in Python code, in which we
use four spaces for indentations.

Declarations
~~~~~~~~~~~~

In general, snuggle pointer stars with the type, not the variable name:

.. code-block:: cpp

   T* p; // GOOD
   T *p; // BAD
   T* p, q; // OOPS put these on separate lines

Some existing modules still use the ``T *p`` style.


Operators
~~~~~~~~~

In C++, when breaking lines containing overlong expressions, binary
operators must be left on their original lines if the line break happens
around the operator. The second line should start in the same column as
the start of the expression in the first line.

In JavaScript, overlong expressions not joined by ``&&`` and
``||`` should break so the operator starts on the second line and
starting in the same column as the beginning of the expression in the
first line. This applies to ``?:``, binary arithmetic operators
including ``+``, and member-of operators. Rationale: an operator at the
front of the continuation line makes for faster visual scanning, as
there is no need to read to the end of line. Also there exists a
context-sensitive keyword hazard in JavaScript; see {{bug(442099, "bug",
19)}}, which can be avoided by putting . at the start of a continuation
line, in long member expression.

In JavaScript, ``==`` is preferred to ``===``.

Unary keyword operators, such as ``typeof`` and ``sizeof``, should have
their operand parenthesized; e.g. ``typeof("foo") == "string"``.


Literals
~~~~~~~~

Double-quoted strings (e.g. ``"foo"``) are preferred to single-quoted
strings (e.g. ``'foo'``), in JavaScript, except to avoid escaping
embedded double quotes, or when assigning inline event handlers.

Use ``\uXXXX`` unicode escapes for non-ASCII characters. The character
set for XUL, DTD, script, and properties files is UTF-8, which is not easily
readable.


Prefixes
~~~~~~~~

Follow these naming prefix conventions:


Variable prefixes
^^^^^^^^^^^^^^^^^

-  k=constant (e.g. ``kNC_child``). Not all code uses this style; some
   uses ``ALL_CAPS`` for constants.
-  g=global (e.g. ``gPrefService``)
-  a=argument (e.g. ``aCount``)
-  C++ Specific Prefixes

   -  s=static member (e.g. ``sPrefChecked``)
   -  m=member (e.g. ``mLength``)
   -  e=enum variants (e.g. ``enum Foo { eBar, eBaz }``). Enum classes
      should use \`CamelCase\` instead (e.g.
      ``enum class Foo { Bar, Baz }``).

-  JavaScript Specific Prefixes

   -  \_=member (variable or function) (e.g. ``_length`` or
      ``_setType(aType)``)
   -  k=enumeration value (e.g. ``const kDisplayModeNormal = 0``)
   -  on=event handler (e.g. ``function onLoad()``)
   -  Convenience constants for interface names should be prefixed with
      ``nsI``:

      .. code-block:: javascript

         const nsISupports = Components.interfaces.nsISupports;
         const nsIWBN = Components.interfaces.nsIWebBrowserNavigation;


Global functions/macros/etc
^^^^^^^^^^^^^^^^^^^^^^^^^^^

-  Macros begin with ``MOZ_``, and are all caps (e.g.
   ``MOZ_WOW_GOODNESS``). Note that older code uses the ``NS_`` prefix;
   while these aren't being changed, you should only use ``MOZ_`` for
   new macros. The only exception is if you're creating a new macro,
   which is part of a set of related macros still using the old ``NS_``
   prefix. Then you should be consistent with the existing macros.


Error Variables
^^^^^^^^^^^^^^^

-  local nsresult result codes should be \`rv`. \`rv\` should not be
   used for bool or other result types.
-  local bool result codes should be \`ok\`


General practices
-----------------

-  Don't put an ``else`` right after a ``return`` (or a ``break``).
   Delete the ``else``, it's unnecessary and increases indentation
   level.
-  Don't leave debug ``printf``\ s or ``dump``\ s lying around.
-  Use `JavaDoc-style
   comments <https://www.oracle.com/technetwork/java/javase/documentation/index-137868.html>`__.
-  When fixing a problem, check to see if the problem occurs elsewhere
   in the same file, and fix it everywhere if possible.
-  End the file with a newline. Make sure your patches don't contain
   files with the text "no newline at end of file" in them.
-  Declare local variables as near to their use as possible.
-  For new files, be sure to use the right `license
   boilerplate <https://www.mozilla.org/MPL/headers/>`__, per our
   `license policy <https://www.mozilla.org/MPL/license-policy.html>`__.


C/C++ practices
---------------

-  **Have you checked for compiler warnings?** Warnings often point to
   real bugs.
-  In C++ code, use ``nullptr`` for pointers. In C code, using ``NULL``
   or ``0`` is allowed.
-  Don't use ``PRBool`` and ``PRPackedBool`` in C++, use ``bool``
   instead.
-  For checking if a ``std`` container has no items, don't use
   ``size()``, instead use ``empty()``.
-  When testing a pointer, use ``(``\ ``!myPtr``\ ``)`` or ``(myPtr)``;
   don't use ``myPtr != nullptr`` or ``myPtr == nullptr``.
-  Do not compare ``x == true`` or ``x == false``. Use ``(x)`` or
   ``(!x)`` instead. ``x == true``, is certainly different from if
   ``(x)``!
-  In general, initialize variables with ``nsFoo aFoo = bFoo,`` and not
   nsFoo aFoo(bFoo).

   -  For constructors, initialize member variables with : nsFoo
      aFoo(bFoo) syntax.

-  To avoid warnings created by variables used only in debug builds, use
   the
   ```DebugOnly<T>`` <https://developer.mozilla.org/docs/Mozilla/Debugging/DebugOnly%3CT%3E>`__
   helper when declaring them.
-  You should `use the static preference
   API <https://developer.mozilla.org/docs/Mozilla/Preferences/Using_preferences_from_application_code>`__ for
   working with preferences.
-  One-argument constructors, that are not copy or move constructors,
   should generally be marked explicit. Exceptions should be annotated
   with MOZ_IMPLICIT.
-  Use ``char32_t`` as the return type or argument type of a method that
   returns or takes as argument a single Unicode scalar value. (Don't
   use UTF-32 strings, though.)
-  Don't use functions from ``ctype.h`` (``isdigit()``, ``isalpha()``,
   etc.) or from ``strings.h`` (``strcasecmp()``, ``strncasecmp()``).
   These are locale-sensitive, which makes them inappropriate for
   processing protocol text. At the same time, they are too limited to
   work properly for processing natural-language text. Use the
   alternatives in ``mozilla/TextUtils.h`` and in ``nsUnicharUtils.h``
   in place of ``ctype.h``. In place of ``strings.h``, prefer the
   ``nsStringComparator`` facilities for comparing strings or if you
   have to work with zero-terminated strings, use ``nsCRT.h`` for
   ASCII-case-insensitive comparison.
-  Forward-declare classes in your header files, instead of including
   them, whenever possible. For example, if you have an interface with a
   ``void DoSomething(nsIContent* aContent)`` function, forward-declare
   with ``class nsIContent;`` instead of ``#include "nsIContent.h"``
-  Include guards are named per the Google coding style and should not
   include a leading ``MOZ_`` or ``MOZILLA_``. For example
   ``dom/media/foo.h`` would use the guard ``DOM_MEDIA_FOO_H_``.


JavaScript practices
--------------------

-  Make sure you are aware of the `JavaScript
   Tips <https://developer.mozilla.org/docs/Mozilla/JavaScript_Tips>`__.
-  Do not compare ``x == true`` or ``x == false``. Use ``(x)`` or
   ``(!x)`` instead. ``x == true``, is certainly different from if
   ``(x)``! Compare objects to ``null``, numbers to ``0`` or strings to
   ``""``, if there is chance for confusion.
-  Make sure that your code doesn't generate any strict JavaScript
   warnings, such as:

   -  Duplicate variable declaration.
   -  Mixing ``return;`` with ``return value;``
   -  Undeclared variables or members. If you are unsure if an array
      value exists, compare the index to the array's length. If you are
      unsure if an object member exists, use ``"name"`` in ``aObject``,
      or if you are expecting a particular type you may use
      ``typeof(aObject.name) == "function"`` (or whichever type you are
      expecting).

-  Use ``['value1, value2']`` to create a JavaScript array in preference
   to using
   ``new {{JSxRef("Array", "Array", "Syntax", 1)}}(value1, value2)``
   which can be confusing, as ``new Array(length)`` will actually create
   a physically empty array with the given logical length, while
   ``[value]`` will always create a 1-element array. You cannot actually
   guarantee to be able to preallocate memory for an array.
-  Use ``{ member: value, ... }`` to create a JavaScript object; a
   useful advantage over ``new {{JSxRef("Object", "Object", "", 1)}}()``
   is the ability to create initial properties and use extended
   JavaScript syntax, to define getters and setters.
-  If having defined a constructor you need to assign default
   properties, it is preferred to assign an object literal to the
   prototype property.
-  Use regular expressions, but use wisely. For instance, to check that
   ``aString`` is not completely whitespace use
   ``/\S/.{{JSxRef("RegExp.test", "test(aString)", "", 1)}}``. Only use
   {{JSxRef("String.search", "aString.search()")}} if you need to know
   the position of the result, or {{JSxRef("String.match",
   "aString.match()")}} if you need to collect matching substrings
   (delimited by parentheses in the regular expression). Regular
   expressions are less useful if the match is unknown in advance, or to
   extract substrings in known positions in the string. For instance,
   {{JSxRef("String.slice", "aString.slice(-1)")}} returns the last
   letter in ``aString``, or the empty string if ``aString`` is empty.


Java practices
--------------

-  We use the `Java Coding
   Style <https://www.oracle.com/technetwork/java/codeconvtoc-136057.html>`__.
   Quick summary:

   -  FirstLetterUpperCase for class names.
   -  camelCase for method and variable names.
   -  One declaration per line:

      .. code-block:: java

         int x, y; // this is BAD!
         int a;    // split it over
         int b;    // two lines

-  Braces should be placed like so (generally, opening braces on same
   line, closing braces on a new line):

   .. code-block:: java

      public void func(int arg) {
          if (arg != 0) {
              while (arg > 0) {
                  arg--;
              }
          } else {
              arg++;
          }
      }

-  Places we differ from the Java coding style:

   -  Start class variable names with 'm' prefix (e.g.
      mSomeClassVariable) and static variables with 's' prefix (e.g.
      sSomeStaticVariable)
   -  ``import`` statements:

      -  Do not use wildcard imports like \`import java.util.*;\`
      -  Organize imports by blocks separated by empty line:
         org.mozilla.*, android.*, com.*, net.*, org.*, then java.\*
         This is basically what Android Studio does by default, except
         that we place org.mozilla.\* at the front - please adjust
         Settings -> Editor -> Code Style -> Java -> Imports
         accordingly.
      -  Within each import block, alphabetize import names with
         uppercase before lowercase. For example, ``com.example.Foo`` is
         before ``com.example.bar``

   -  4-space indents.
   -  Spaces, not tabs.
   -  Don't restrict yourself to 80-character lines. Google's Android
      style guide suggests 100-character lines, which is also the
      default setting in Android Studio. Java code tends to be long
      horizontally, so use appropriate judgement when wrapping. Avoid
      deep indents on wrapping. Note that aligning the wrapped part of a
      line, with some previous part of the line (rather than just using
      a fixed indent), may require shifting the code every time the line
      changes, resulting in spurious whitespace changes.

-  For additional specifics on Firefox for Android, see the `Coding
   Style guide for Firefox on
   Android <https://wiki.mozilla.org/Mobile/Fennec/Android#Coding_Style>`__.
-  The `Android Coding
   Style <https://source.android.com/source/code-style.html>`__ has some
   useful guidelines too.


Makefile/moz.build practices
----------------------------

-  Changes to makefile and moz.build variables do not require
   build-config peer review. Any other build system changes, such as
   adding new scripts or rules, require review from the build-config
   team.
-  Suffix long ``if``/``endif`` conditionals with #{ & #}, so editors
   can display matched tokens enclosing a block of statements.

   ::

      ifdef CHECK_TYPE #{
        ifneq ($(flavor var_type),recursive) #{
          $(warning var should be expandable but detected var_type=$(flavor var_type))
        endif #}
      endif #}

-  moz.build are python and follow normal Python style.
-  List assignments should be written with one element per line. Align
   closing square brace with start of variable assignment. If ordering
   is not important, variables should be in alphabetical order.

   .. code-block:: python

      var += [
          'foo',
          'bar'
      ]

-  Use ``CONFIG['CPU_ARCH'] {=arm}`` to test for generic classes of
   architecture rather than ``CONFIG['OS_TEST'] {=armv7}`` (re: bug 886689).

Python practices
----------------

-  Install the
   `mozext <https://hg.mozilla.org/hgcustom/version-control-tools/file/default/hgext/mozext>`__
   Mercurial extension, and address every issue reported on commit,
   qrefresh, or the output of ``hg critic``.
-  Follow `PEP 8 <https://www.python.org/dev/peps/pep-0008/>`__.
-  Do not place statements on the same line as ``if/elif/else``
   conditionals to form a one-liner.
-  Global vars, please avoid them at all cost.
-  Exclude outer parenthesis from conditionals.Use
   ``if x > 5:,``\ rather than ``if (x > 5):``
-  Use string formatters, rather than var + str(val).
   ``var = 'Type %s value is %d'% ('int', 5).``
-  Utilize tools like
   `pylint <https://pypi.python.org/pypi/pylint>`__ or
   `pychecker <http://pychecker.sourceforge.net>`__ to screen
   sources for common problems.
-  Testing/Unit tests, please write them.


SVG practices
-------------

Check `SVG
Guidelines <https://developer.mozilla.org/docs/Mozilla/Developer_guide/SVG_Guidelines>`__ for
more details.


COM, pointers and strings
-------------------------

-  Use ``nsCOMPtr<>``
   If you don't know how to use it, start looking in the code for
   examples. The general rule, is that the very act of typing
   ``NS_RELEASE`` should be a signal to you to question your code:
   "Should I be using ``nsCOMPtr`` here?". Generally the only valid use
   of ``NS_RELEASE``, are when you are storing refcounted pointers in a
   long-lived datastructure.
-  Declare new XPCOM interfaces using `XPIDL <https://developer.mozilla.org/docs/Mozilla/Tech/XPIDL>`__, so they
   will be scriptable.
-  Use `nsCOMPtr <https://developer.mozilla.org/docs/Mozilla/Tech/XPCOM/Reference/Glue_classes/nsCOMPtr>`__ for strong references, and
   `nsWeakPtr <https://developer.mozilla.org/docs/Mozilla/Tech/XPCOM/Weak_reference>`__ for weak references.
-  String arguments to functions should be declared as ``nsAString``.
-  Use ``EmptyString()`` and ``EmptyCString()`` instead of
   ``NS_LITERAL_STRING("")`` or ``nsAutoString empty``;.
-  Use ``str.IsEmpty()`` instead of ``str.Length() == 0``.
-  Use ``str.Truncate()`` instead of ``str.SetLength(0)`` or
   ``str.Assign(EmptyString())``.
-  Don't use ``QueryInterface`` directly. Use ``CallQueryInterface`` or
   ``do_QueryInterface`` instead.
-  ``nsresult`` should be declared as ``rv``. Not res, not result, not
   foo.
-  For constant strings, use ``NS_LITERAL_STRING("...")`` instead of
   ``NS_ConvertASCIItoUCS2("...")``, ``AssignWithConversion("...")``,
   ``EqualsWithConversion("...")``, or ``nsAutoString()``
-  To compare a string with a literal, use .EqualsLiteral("...").
-  Use `Contract
   IDs <news://news.mozilla.org/3994AE3E.D96EF810@netscape.com>`__,
   instead of CIDs with do_CreateInstance/do_GetService.
-  Use pointers, instead of references for function out parameters, even
   for primitive types.


IDL
---


Use leading-lowercase, or "interCaps"
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When defining a method or attribute in IDL, the first letter should be
lowercase, and each following word should be capitalized. For example:

.. code-block:: cpp

   long updateStatusBar();


Use attributes wherever possible
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Whenever you are retrieving or setting a single value, without any
context, you should use attributes. Don't use two methods when you could
use an attribute. Using attributes logically connects the getting and
setting of a value, and makes scripted code look cleaner.

This example has too many methods:

.. code-block:: cpp

   interface nsIFoo : nsISupports
   {
       long getLength();
       void setLength(in long length);
       long getColor();
   };

The code below will generate the exact same C++ signature, but is more
script-friendly.

.. code-block:: cpp

   interface nsIFoo : nsISupports
   {
       attribute long length;
       readonly attribute long color;
   };


Use Java-style constants
~~~~~~~~~~~~~~~~~~~~~~~~

When defining scriptable constants in IDL, the name should be all
uppercase, with underscores between words:

.. code-block:: cpp

   const long ERROR_UNDEFINED_VARIABLE = 1;


See also
~~~~~~~~

For details on interface development, as well as more detailed style
guides, see the `Interface development
guide <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Interface_development_guide>`__.


Error handling
--------------


Check for errors early and often
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Every time you make a call into an XPCOM function, you should check for
an error condition. You need to do this even if you know that call will
never fail. Why?

-  Someone may change the callee in the future to return a failure
   condition.
-  The object in question may live on another thread, another process,
   or possibly even another machine. The proxy could have failed to make
   your call in the first place.

Also, when you make a new function which is failable (i.e. it will
return a nsresult or a bool that may indicate an error), you should
explicitly mark the return value should always be checked. For example:

::

   // for IDL.
   [must_use] nsISupports
   create();

   // for C++, add this in *declaration*, do not add it again in implementation.
   MOZ_MUST_USE nsresult
   DoSomething();

There are some exceptions:

-  Predicates or getters, which return bool or nsresult.
-  IPC method implementation (For example, bool RecvSomeMessage()).
-  Most callers will check the output parameter, see below.

.. code-block:: cpp

   nsresult
   SomeMap::GetValue(const nsString& key, nsString& value);

If most callers need to check the output value first, then adding
MOZ_MUST_USE might be too verbose. In this case, change the return value
to void might be a reasonable choice.

There is also a static analysis attribute MOZ_MUST_USE_TYPE, which can
be added to class declarations, to ensure that those declarations are
always used when they are returned.


Use the NS_WARN_IF macro when errors are unexpected.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The NS_WARN_IF macro can be used to issue a console warning, in debug
builds if the condition fails. This should only be used when the failure
is unexpected and cannot be caused by normal web content.

If you are writing code which wants to issue warnings when methods fail,
please either use NS_WARNING directly, or use the new NS_WARN_IF macro.

.. code-block:: cpp

   if (NS_WARN_IF(somethingthatshouldbefalse)) {
     return NS_ERROR_INVALID_ARG;
   }

   if (NS_WARN_IF(NS_FAILED(rv))) {
     return rv;
   }

Previously, the ``NS_ENSURE_*`` macros were used for this purpose, but
those macros hide return statements, and should not be used in new code.
(This coding style rule isn't generally agreed, so use of NS_ENSURE_*
can be valid.)


Return from errors immediately
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In most cases, your knee-jerk reaction should be to return from the
current function, when an error condition occurs. Don't do this:

.. code-block:: cpp

   rv = foo->Call1();
   if (NS_SUCCEEDED(rv)) {
     rv = foo->Call2();
     if (NS_SUCCEEDED(rv)) {
       rv = foo->Call3();
     }
   }
   return rv;

Instead, do this:

.. code-block:: cpp

   rv = foo->Call1();
   if (NS_FAILED(rv)) {
     return rv;
   }

   rv = foo->Call2();
   if (NS_FAILED(rv)) {
     return rv;
   }

   rv = foo->Call3();
   if (NS_FAILED(rv)) {
     return rv;
   }

Why? Error handling should not obfuscate the logic of the code. The
author's intent, in the first example, was to make 3 calls in
succession. Wrapping the calls in nested if() statements, instead
obscured the most likely behavior of the code.

Consider a more complicated example to hide a bug:

.. code-block:: cpp

   bool val;
   rv = foo->GetBooleanValue(&val);
   if (NS_SUCCEEDED(rv) && val) {
     foo->Call1();
   } else {
     foo->Call2();
   }

The intent of the author, may have been, that foo->Call2() would only
happen when val had a false value. In fact, foo->Call2() will also be
called, when foo->GetBooleanValue(&val) fails. This may, or may not,
have been the author's intent. It is not clear from this code. Here is
an updated version:

.. code-block:: cpp

   bool val;
   rv = foo->GetBooleanValue(&val);
   if (NS_FAILED(rv)) {
     return rv;
   }
   if (val) {
     foo->Call1();
   } else {
     foo->Call2();
   }

In this example, the author's intent is clear, and an error condition
avoids both calls to foo->Call1() and foo->Call2();

*Possible exceptions:* Sometimes it is not fatal if a call fails. For
instance, if you are notifying a series of observers that an event has
fired, it might be trivial that one of these notifications failed:

.. code-block:: cpp

   for (size_t i = 0; i < length; ++i) {
     // we don't care if any individual observer fails
     observers[i]->Observe(foo, bar, baz);
   }

Another possibility, is you are not sure if a component exists or is
installed, and you wish to continue normally, if the component is not
found.

.. code-block:: cpp

   nsCOMPtr<nsIMyService> service = do_CreateInstance(NS_MYSERVICE_CID, &rv);
   // if the service is installed, then we'll use it.
   if (NS_SUCCEEDED(rv)) {
     // non-fatal if this fails too, ignore this error.
     service->DoSomething();

     // this is important, handle this error!
     rv = service->DoSomethingImportant();
     if (NS_FAILED(rv)) {
       return rv;
     }
   }

   // continue normally whether or not the service exists.


C++ strings
-----------


Use the ``Auto`` form of strings for local values
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When declaring a local, short-lived ``nsString`` class, always use
``nsAutoString`` or ``nsAutoCString``. These pre-allocate a 64-byte
buffer on the stack, and avoid fragmenting the heap. Don't do this:

.. code-block:: cpp

   nsresult
   foo()
   {
     nsCString bar;
     ..
   }

instead:

.. code-block:: cpp

   nsresult
   foo()
   {
     nsAutoCString bar;
     ..
   }


Be wary of leaking values from non-XPCOM functions that return char\* or PRUnichar\*
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is an easy trap to return an allocated string, from an internal
helper function, and then using that function inline in your code,
without freeing the value. Consider this code:

.. code-block:: cpp

   static char*
   GetStringValue()
   {
     ..
     return resultString.ToNewCString();
   }

     ..
     WarnUser(GetStringValue());

In the above example, WarnUser will get the string allocated from
``resultString.ToNewCString(),`` and throw away the pointer. The
resulting value is never freed. Instead, either use the string classes,
to make sure your string is automatically freed when it goes out of
scope, or make sure that your string is freed.

Automatic cleanup:

.. code-block:: cpp

   static void
   GetStringValue(nsAWritableCString& aResult)
   {
     ..
     aResult.Assign("resulting string");
   }

     ..
     nsAutoCString warning;
     GetStringValue(warning);
     WarnUser(warning.get());

Free the string manually:

.. code-block:: cpp

   static char*
   GetStringValue()
   {
     ..
     return resultString.ToNewCString();
   }

     ..
     char* warning = GetStringValue();
     WarnUser(warning);
     nsMemory::Free(warning);


Use MOZ_UTF16() or NS_LITERAL_STRING() to avoid runtime string conversion
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

It is very common to need to assign the value of a literal string, such
as "Some String", into a unicode buffer. Instead of using ``nsString``'s
``AssignLiteral`` and ``AppendLiteral``, use ``NS_LITERAL_STRING(),``
instead. On most platforms, this will force the compiler to compile in a
raw unicode string, and assign it directly.

Incorrect:

.. code-block:: cpp

   nsAutoString warning;
   warning.AssignLiteral("danger will robinson!");
   ...
   foo->SetStringValue(warning);
   ...
   bar->SetUnicodeValue(warning.get());

Correct:

.. code-block:: cpp

   NS_NAMED_LITERAL_STRING(warning, "danger will robinson!");
   ...
   // if you'll be using the 'warning' string, you can still use it as before:
   foo->SetStringValue(warning);
   ...
   bar->SetUnicodeValue(warning.get());

   // alternatively, use the wide string directly:
   foo->SetStringValue(NS_LITERAL_STRING("danger will robinson!"));
   ...
   bar->SetUnicodeValue(MOZ_UTF16("danger will robinson!"));

.. note::

   Note: Named literal strings cannot yet be static.


Usage of PR_(MAX|MIN|ABS|ROUNDUP) macro calls
---------------------------------------------

Use the standard-library functions (std::max), instead of
PR_(MAX|MIN|ABS|ROUNDUP).

Use mozilla::Abs instead of PR_ABS. All PR_ABS calls in C++ code have
been replaced with mozilla::Abs calls, in `bug
847480 <https://bugzilla.mozilla.org/show_bug.cgi?id=847480>`__. All new
code in Firefox/core/toolkit needs to ``#include "nsAlgorithm.h",`` and
use the NS_foo variants instead of PR_foo, or
``#include "mozilla/MathAlgorithms.h"`` for ``mozilla::Abs``.
