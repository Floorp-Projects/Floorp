============
Use Counters
============

Use counters are used to report Telemetry statistics on whether individual documents
use a given WebIDL method or attribute (getters and setters are reported separately), CSS
property, or deprecated DOM operation.  Custom use counters can also be
defined to test frequency of things that don't fall into one of those
categories.

As of Firefox 65 the collection of Use Counters is enabled on all channels.

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

CSS Properties
~~~~~~~~~~~~~~

Use counters for CSS properties are generated for every property Gecko supports automatically, and are counted via StyleUseCounters (`Rust code <https://searchfox.org/mozilla-central/rev/7ed8e2d3d1d7a1464ba42763a33fd2e60efcaedc/servo/components/style/use_counters/mod.rs>`_, `C++ code <https://searchfox.org/mozilla-central/rev/7ed8e2d3d1d7a1464ba42763a33fd2e60efcaedc/dom/base/Document.h#5077>`_).

The UseCounters registry
------------------------
Use counters for WebIDL methods/attributes are registered in the `UseCounters.conf <https://dxr.mozilla.org/mozilla-central/source/dom/base/UseCounters.conf>`_ file.  The format of this file is very strict. Each line can be:

1. a blank line
2. a comment, which is a line that begins with ``//``
3. one of four possible use counter declarations:

  * ``method <IDL interface name>.<IDL operation name>``
  * ``attribute <IDL interface name>.<IDL attribute name>``
  * ``custom <any valid identifier> <description>``

Custom use counters
~~~~~~~~~~~~~~~~~~~
The <description> for custom counters will be appended to "When a document " or "When a page ", so phrase it appropriately.  For instance, "constructs a Foo object" or "calls Document.bar('some value')".  It may contain any character (including whitespace).  Custom counters are incremented when SetUseCounter(eUseCounter_custom_MyName) is called on a Document object.

WebIDL methods and attributes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Additionally to having a new entry added to the `UseCounters.conf <https://dxr.mozilla.org/mozilla-central/source/dom/base/UseCounters.conf>`_ file, WebIDL methods and attributes must have a ``[UseCounter]`` extended attribute in the Web IDL file in order for the counters to be incremented.

Both additions are required because generating things from bindings codegen and ensuring all the dependencies are correct would have been rather difficult.

The processor script
====================
The definition files are processed twice:

- once to generate two C++ headers files, included by the web platform components (e.g. DOM) that own the features to be tracked;
- the other time by the Telemetry component, to generate the histogram definitions that make the collection system work.

.. note::

    The histograms that are generated out of use counters are set to *never* expire and are collected from Firefox release. Note that before Firefox 65 they were only collected on pre-release.

gen-usecounters.py
------------------
This script is called by the build system to generate:

- the ``UseCounterList.h`` header for the WebIDL, out of the definition files.

Interpreting the data
=====================
The histogram as accumulated on the client only puts values into the 1 bucket, meaning that
the use counter directly reports if a feature was used but it does not directly report if
it isn't used.
The values accumulated within a use counter should be considered proportional to
``CONTENT_DOCUMENTS_DESTROYED`` and ``TOP_LEVEL_CONTENT_DOCUMENTS_DESTROYED`` (see
`here <https://dxr.mozilla.org/mozilla-central/rev/b056526be38e96b3e381b7e90cd8254ad1d96d9d/dom/base/nsDocument.cpp#13209-13231>`__). The difference between the values of these two histograms
and the related use counters below tell us how many pages did *not* use the feature in question.
For instance, if we see that a given session has destroyed 30 content documents, but a
particular use counter shows only a count of 5, we can infer that the use counter was *not*
used in 25 of those 30 documents.

Things are done this way, rather than accumulating a boolean flag for each use counter,
to avoid sending histograms for features that don't get widely used. Doing things in this
fashion means smaller telemetry payloads and faster processing on the server side.

Version History
---------------

- Firefox 65:

  - Enable Use Counters on release channel (`bug 1477433 <https://bugzilla.mozilla.org/show_bug.cgi?id=1477433>`_)
