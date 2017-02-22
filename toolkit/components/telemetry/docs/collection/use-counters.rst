============
Use Counters
============

Use counters are used to report Telemetry statistics on whether individual documents
use a given WebIDL method or attribute (getters and setters are reported separately), CSS
property and deprecated DOM operations.

The API
=======
The process to add a new use counter is different depending on the type feature that needs
to be measured. In general, for each defined use counter, two separate boolean histograms are generated:

- one describes the use of the tracked feature for individual documents and has the ``_DOCUMENT`` suffix;
- the other describes the use of the same thing for top-level pages (basically what we think of as a *web page*) and has the ``_PAGE`` suffix.

Using two histograms is particularly suited to measure how many sites would be affected by
removing the tracked feature.

Example scenarios:

- Site *X* triggers use counter *Y*.  We report "used" (true) in both the ``_DOCUMENT`` and ``_PAGE`` histograms.
- Site *X* does not trigger use counter *Y*.  We report "unused" (false) in both the ``_DOCUMENT`` and ``_PAGE`` histograms.
- Site *X* has an iframe for site *W*.  Site *W* triggers use counter *Y*, but site *X* does not.  We report one "used" and one "unused" in the individual ``_DOCUMENT`` histogram and one "used" in the top-level ``_PAGE`` histogram.

Deprecated DOM operations
-------------------------
Use counters for deprecated DOM operations are declared in the `nsDeprecatedOperationList.h <https://dxr.mozilla.org/mozilla-central/source/dom/base/nsDeprecatedOperationList.h>`_ file. The counters are
registered through the ``DEPRECATED_OPERATION(DeprecationReference)`` macro. The provided
parameter must have the same value of the deprecation note added to the *IDL* file.

See this `changeset <https://hg.mozilla.org/mozilla-central/rev/e30a357b25f1>`_ for a sample
deprecated operation.

The UseCounters registry
------------------------
Use counters for WebIDL methods/attributes and CSS properties are registered in the `UseCounters.conf <https://dxr.mozilla.org/mozilla-central/source/dom/base/UseCounters.conf>`_ file.  The format of this file is very strict. Each line can be:

1. a blank line
2. a comment, which is a line that begins with ``//``
3. one of three possible use counter declarations:

  * ``method <IDL interface name>.<IDL operation name>``
  * ``attribute <IDL interface name>.<IDL attribute name>``
  * ``property <CSS property method name>``

CSS properties
~~~~~~~~~~~~~~
The CSS property method name should be identical to the ``method`` argument of ``CSS_PROP()`` and related macros. The only differences are that all hyphens are removed and CamelCase naming is used.  See `nsCSSPropList.h <https://dxr.mozilla.org/mozilla-central/source/layout/style/nsCSSPropList.h>`_ for further details.

WebIDL methods and attributes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Additionally to having a new entry added to the `UseCounters.conf <https://dxr.mozilla.org/mozilla-central/source/dom/base/UseCounters.conf>`_ file, WebIDL methods and attributes must have a ``[UseCounter]`` extended attribute in the Web IDL file in order for the counters to be incremented.

Both additions are required because generating things from bindings codegen and ensuring all the dependencies are correct would have been rather difficult, and annotating the WebIDL files does nothing for identifying CSS property usage, which we would also like to track.

The processor script
====================
The definition files are processed twice:

- once to generate two C++ headers files, included by the web platform components (e.g. DOM) that own the features to be tracked;
- the other time by the Telemetry component, to generate the histogram definitions that make the collection system work.

.. note::

    The histograms that are generated out of use counters are set to *never* expire and are *opt-in*.

gen-usecounters.py
------------------
This script is called by the build system to generate:

- the ``PropertyUseCounterMap.inc`` C++ header for the CSS properties;
- the ``UseCounterList.h`` header for the WebIDL, out of the definition files.

