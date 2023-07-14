Markers
=======

Markers are packets of arbitrary data that are added to a profile by the Firefox code, usually to
indicate something important happening at a point in time, or during an interval of time.

Each marker has a name, a category, some common optional information (timing, backtrace, etc.),
and an optional payload of a specific type (containing arbitrary data relevant to that type).

.. note::
    This guide explains C++ markers in depth. To learn more about how to add a
    marker in JavaScript or Rust, please take a look at their documentation
    in :doc:`instrumenting-javascript` or :doc:`instrumenting-rust` respectively.

Example
-------

Short example, details below.

Note: Most marker-related identifiers are in the ``mozilla`` namespace, to be added where necessary.

.. code-block:: c++

    // Record a simple marker with the category of DOM.
    PROFILER_MARKER_UNTYPED("Marker Name", DOM);

    // Create a marker with some additional text information. (Be wary of printf!)
    PROFILER_MARKER_TEXT("Marker Name", JS, MarkerOptions{}, "Additional text information.");

    // Record a custom marker of type `ExampleNumberMarker` (see definition below).
    PROFILER_MARKER("Number", OTHER, MarkerOptions{}, ExampleNumberMarker, 42);

.. code-block:: c++

    // Marker type definition.
    struct ExampleNumberMarker {
      // Unique marker type name.
      static constexpr Span<const char> MarkerTypeName() { return MakeStringSpan("number"); }
      // Data specific to this marker type, serialized to JSON for profiler.firefox.com.
      static void StreamJSONMarkerData(SpliceableJSONWriter& aWriter, int a number) {
        aWriter.IntProperty("number", a number);
      }
      // Where and how to display the marker and its data.
      static MarkerSchema MarkerTypeDisplay() {
        using MS = MarkerSchema;
        MS schema(MS::Location::MarkerChart, MS::Location::MarkerTable);
        schema.SetChartLabel("Number: {marker.data.number}");
        schema.AddKeyLabelFormat("number", "Number", MS::Format::Number);
        return schema;
      }
    };


How to Record Markers
---------------------

Header to Include
^^^^^^^^^^^^^^^^^

If the compilation unit only defines and records untyped, text, and/or its own markers, include
`the main profiler markers header <https://searchfox.org/mozilla-central/source/tools/profiler/public/ProfilerMarkers.h>`_:

.. code-block:: c++

    #include "mozilla/ProfilerMarkers.h"

If it also records one of the other common markers defined in
`ProfilerMarkerTypes.h <https://searchfox.org/mozilla-central/source/tools/profiler/public/ProfilerMarkerTypes.h>`_,
include that one instead:

.. code-block:: c++

    #include "mozilla/ProfilerMarkerTypes.h"

And if it uses any other profiler functions (e.g., labels), use
`the main Gecko Profiler header <https://searchfox.org/mozilla-central/source/tools/profiler/public/GeckoProfiler.h>`_
instead:

.. code-block:: c++

    #include "GeckoProfiler.h"

The above works from source files that end up in libxul, which is true for the majority
of Firefox source code. But some files live outside of libxul, such as mfbt, in which
case the advice is the same but the equivalent headers are from the Base Profiler instead:

.. code-block:: c++

    #include "mozilla/BaseProfilerMarkers.h" // Only own/untyped/text markers
    #include "mozilla/BaseProfilerMarkerTypes.h" // Only common markers
    #include "BaseProfiler.h" // Markers and other profiler functions

Untyped Markers
^^^^^^^^^^^^^^^

Untyped markers don't carry any information apart from common marker data:
Name, category, options.

.. code-block:: c++

    PROFILER_MARKER_UNTYPED(
        // Name, and category pair.
        "Marker Name", OTHER,
        // Marker options, may be omitted if all defaults are acceptable.
        MarkerOptions(MarkerStack::Capture(), ...));

``PROFILER_MARKER_UNTYPED`` is a macro that simplifies the use of the main
``profiler_add_marker`` function, by adding the appropriate namespaces, and a surrounding
``#ifdef MOZ_GECKO_PROFILER`` guard.

1. Marker name
    The first argument is the name of this marker. This will be displayed in most places
    the marker is shown. It can be a literal C string, or any dynamic string object.
2. `Category pair name <https://searchfox.org/mozilla-central/source/__GENERATED__/mozglue/baseprofiler/public/ProfilingCategoryList.h>`_
    Choose a category + subcategory from the `the list of categories <https://searchfox.org/mozilla-central/source/mozglue/baseprofiler/build/profiling_categories.yaml>`_.
    This is the second parameter of each ``SUBCATEGORY`` line, for instance ``LAYOUT_Reflow``.
    (Internally, this is really a `MarkerCategory <https://searchfox.org/mozilla-central/define?q=T_mozilla%3A%3AMarkerCategory>`_
    object, in case you need to construct it elsewhere.)
3. `MarkerOptions <https://searchfox.org/mozilla-central/define?q=T_mozilla%3A%3AMarkerOptions>`_
    See the options below. It can be omitted if there are no other arguments, ``{}``, or
    ``MarkerOptions()`` (no specified options); only one of the following option types
    alone; or ``MarkerOptions(...)`` with one or more of the following options types:

    * `MarkerThreadId <https://searchfox.org/mozilla-central/define?q=T_mozilla%3A%3AMarkerThreadId>`_
        Rarely used, as it defaults to the current thread. Otherwise it specifies the target
        "thread id" (aka "track") where the marker should appear; This may be useful when
        referring to something that happened on another thread (use ``profiler_current_thread_id()``
        from the original thread to get its id); or for some important markers, they may be
        sent to the "main thread", which can be specified with ``MarkerThreadId::MainThread()``.
    * `MarkerTiming <https://searchfox.org/mozilla-central/define?q=T_mozilla%3A%3AMarkerTiming>`_
        This specifies an instant or interval of time. It defaults to the current instant if
        left unspecified. Otherwise use ``MarkerTiming::InstantAt(timestamp)`` or
        ``MarkerTiming::Interval(ts1, ts2)``; timestamps are usually captured with
        ``TimeStamp::Now()``. It is also possible to record only the start or the end of an
        interval, pairs of start/end markers will be matched by their name. *Note: The
        upcoming "marker sets" feature will make this pairing more reliable, and also
        allow more than two markers to be connected*.
    * `MarkerStack <https://searchfox.org/mozilla-central/define?q=T_mozilla%3A%3AMarkerStack>`_
        By default, markers do not record a "stack" (or "backtrace"). To record a stack at
        this point, in the most efficient manner, specify ``MarkerStack::Capture()``. To
        record a previously captured stack, first store a stack into a
        ``UniquePtr<ProfileChunkedBuffer>`` with ``profiler_capture_backtrace()``, then pass
        it to the marker with ``MarkerStack::TakeBacktrace(std::move(stack))``.
    * `MarkerInnerWindowId <https://searchfox.org/mozilla-central/define?q=T_mozilla%3A%3AMarkerInnerWindowId>`_
        If you have access to an "inner window id", consider specifying it as an option, to
        help profiler.firefox.com to classify them by tab.

Text Markers
^^^^^^^^^^^^

Text markers are very common, they carry an extra text as a fourth argument, in addition to
the marker name. Use the following macro:

.. code-block:: c++

    PROFILER_MARKER_TEXT(
        // Name, category pair, options.
        "Marker Name", OTHER, {},
        // Text string.
        "Here are some more details."
    );

As useful as it is, using an expensive ``printf`` operation to generate a complex text
comes with a variety of issues string. It can leak potentially sensitive information
such as URLs can be leaked during the profile sharing step. profiler.firefox.com cannot
access the information programmatically. It won't get the formatting benefits of the
built-in marker schema. Please consider using a custom marker type to separate and
better present the data.

Other Typed Markers
^^^^^^^^^^^^^^^^^^^

From C++ code, a marker of some type ``YourMarker`` (details about type definition follow) can be
recorded like this:

.. code-block:: c++

    PROFILER_MARKER(
        "YourMarker name", OTHER,
        MarkerOptions(MarkerTiming::IntervalUntilNowFrom(someStartTimestamp),
                      MarkerInnerWindowId(innerWindowId))),
        YourMarker, "some string", 12345, "http://example.com", someTimeStamp);

After the first three common arguments (like in ``PROFILER_MARKER_UNTYPED``), there are:

4. The marker type, which is the name of the C++ ``struct`` that defines that type.
5. A variadic list of type-specific argument. They must match the number of, and must
   be convertible to, ``StreamJSONMarkerData`` parameters as specified in the marker type definition.

"Auto" Scoped Interval Markers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To capture time intervals around some important operations, it is common to store a timestamp, do the work,
and then record a marker, e.g.:

.. code-block:: c++

    void DoTimedWork() {
      TimeStamp start = TimeStamp::Now();
      DoWork();
      PROFILER_MARKER_TEXT("Timed work", OTHER, MarkerTiming::IntervalUntilNowFrom(start), "Details");
    }

`RAII <https://en.cppreference.com/w/cpp/language/raii>`_ objects automate this, by recording the time
when the object is constructed, and later recording the marker when the object is destroyed at the end
of its C++ scope.
This is especially useful if there are multiple scope exit points.

``AUTO_PROFILER_MARKER_TEXT`` is `the only one implemented <https://searchfox.org/mozilla-central/search?q=id%3AAUTO_PROFILER_MARKER_TEXT`_ at this time.

.. code-block:: c++

    void MaybeDoTimedWork(bool aDoIt) {
      AUTO_PROFILER_MARKER_TEXT("Timed work", OTHER, "Details");
      if (!aDoIt) { /* Marker recorded here... */ return; }
      DoWork();
      /* ... or here. */
    }

Note that these RAII objects only record one marker. In some situation, a very long
operation could be missed if it hasn't completed by the end of the profiling session.
In this case, consider recording two distinct markers, using
``MarkerTiming::IntervalStart()`` and ``MarkerTiming::IntervalEnd()``.

Where to Define New Marker Types
--------------------------------

The first step is to determine the location of the marker type definition:

* If this type is only used in one function, or a component, it can be defined in a
  local common place relative to its use.
* For a more common type that could be used from multiple locations:

  * If there is no dependency on XUL, it can be defined in the Base Profiler, which can
    be used in most locations in the codebase:
    `mozglue/baseprofiler/public/BaseProfilerMarkerTypes.h <https://searchfox.org/mozilla-central/source/mozglue/baseprofiler/public/BaseProfilerMarkerTypes.h>`__

  * However, if there is a XUL dependency, then it needs to be defined in the Gecko Profiler:
    `tools/profiler/public/ProfilerMarkerTypes.h <https://searchfox.org/mozilla-central/source/tools/profiler/public/ProfilerMarkerTypes.h>`__

.. _how-to-define-new-marker-types:

How to Define New Marker Types
------------------------------

Each marker type must be defined once and only once.
The definition is a C++ ``struct``, its identifier is used when recording
markers of that type in C++.
By convention, the suffix "Marker" is recommended to better distinguish them
from non-profiler entities in the source.

.. code-block:: c++

    struct YourMarker {

Marker Type Name
^^^^^^^^^^^^^^^^

A marker type must have a unique name, it is used to keep track of the type of
markers in the profiler storage, and to identify them uniquely on profiler.firefox.com.
(It does not need to be the same as the ``struct``'s name.)

This name is defined in a special static member function ``MarkerTypeName``:

.. code-block:: c++

    // …
      static constexpr Span<const char> MarkerTypeName() {
        return MakeStringSpan("YourMarker");
      }

Marker Type Data
^^^^^^^^^^^^^^^^

All markers of any type have some common data: A name, a category, options like
timing, etc. as previously explained.

In addition, a certain marker type may carry zero of more arbitrary pieces of
information, and they are always the same for all markers of that type.

These are defined in a special static member function ``StreamJSONMarkerData``.

The first function parameters is always ``SpliceableJSONWriter& aWriter``,
it will be used to stream the data as JSON, to later be read by
profiler.firefox.com.

.. code-block:: c++

    // …
      static void StreamJSONMarkerData(SpliceableJSONWriter& aWriter,

The following function parameters is how the data is received as C++ objects
from the call sites.

* Most C/C++ `POD (Plain Old Data) <https://en.cppreference.com/w/cpp/named_req/PODType>`_
  and `trivially-copyable <https://en.cppreference.com/w/cpp/named_req/TriviallyCopyable>`_
  types should work as-is, including ``TimeStamp``.
* Character strings should be passed using ``const ProfilerString8View&`` (this handles
  literal strings, and various ``std::string`` and ``nsCString`` types, and spans with or
  without null terminator). Use ``const ProfilerString16View&`` for 16-bit strings such as
  ``nsString``.
* Other types can be used if they define specializations for ``ProfileBufferEntryWriter::Serializer``
  and ``ProfileBufferEntryReader::Deserializer``. You should rarely need to define new
  ones, but if needed see how existing specializations are written, or contact the
  `perf-tools team for help <https://chat.mozilla.org/#/room/#profiler:mozilla.org>`_.

Passing by value or by reference-to-const is recommended, because arguments are serialized
in binary form (i.e., there are no optimizable ``move`` operations).

For example, here's how to handle a string, a 64-bit number, another string, and
a timestamp:

.. code-block:: c++

    // …
                                       const ProfilerString8View& aString,
                                       const int64_t aBytes,
                                       const ProfilerString8View& aURL,
                                       const TimeStamp& aTime) {

Then the body of the function turns these parameters into a JSON stream.

When this function is called, the writer has just started a JSON object, so
everything that is written should be a named object property. Use
``SpliceableJSONWriter`` functions, in most cases ``...Property`` functions
from its parent class ``JSONWriter``: ``NullProperty``, ``BoolProperty``,
``IntProperty``, ``DoubleProperty``, ``StringProperty``. (Other nested JSON
types like arrays or objects are not supported by the profiler.)

As a special case, ``TimeStamps`` must be streamed using ``aWriter.TimeProperty(timestamp)``.

The property names will be used to identify where each piece of data is stored and
how it should be displayed on profiler.firefox.com (see next section).

Here's how the above functions parameters could be streamed:

.. code-block:: c++

    // …
        aWriter.StringProperty("myString", aString);
        aWriter.IntProperty("myBytes", aBytes);
        aWriter.StringProperty("myURL", aURL);
        aWriter.TimeProperty("myTime", aTime);
      }

.. _marker-type-display-schema:

Marker Type Display Schema
^^^^^^^^^^^^^^^^^^^^^^^^^^

Now that we have defined how to stream type-specific data (from Firefox to
profiler.firefox.com), we need to describe where and how this data will be
displayed on profiler.firefox.com.

The static member function ``MarkerTypeDisplay`` returns an opaque ``MarkerSchema``
object, which will be forwarded to profiler.firefox.com.

.. code-block:: c++

    // …
      static MarkerSchema MarkerTypeDisplay() {

The ``MarkerSchema`` type will be used repeatedly, so for convenience we can define
a local type alias:

.. code-block:: c++

    // …
        using MS = MarkerSchema;

First, we construct the ``MarkerSchema`` object to be returned at the end.

One or more constructor arguments determine where this marker will be displayed in
the profiler.firefox.com UI. See the `MarkerSchema::Location enumeration for the
full list <https://searchfox.org/mozilla-central/define?q=T_mozilla%3A%3AMarkerSchema%3A%3ALocation>`_.

Here is the most common set of locations, showing markers of that type in both the
Marker Chart and the Marker Table panels:

.. code-block:: c++

    // …
        MS schema(MS::Location::MarkerChart, MS::Location::MarkerTable);

Some labels can optionally be specified, to display certain information in different
locations: ``SetChartLabel``, ``SetTooltipLabel``, and ``SetTableLabel``; or
``SetAllLabels`` to define all of them the same way.

The arguments is a string that may refer to marker data within braces:

* ``{marker.name}``: Marker name.
* ``{marker.data.X}``: Type-specific data, as streamed with property name "X" from ``StreamJSONMarkerData`` (e.g., ``aWriter.IntProperty("X", a number);``

For example, here's how to set the Marker Chart label to show the marker name and the
``myBytes`` number of bytes:

.. code-block:: c++

    // …
        schema.SetChartLabel("{marker.name} – {marker.data.myBytes}");

profiler.firefox.com will apply the label with the data in a consistent manner. For
example, with this label definition, it could display marker information like the
following in the Firefox Profiler's Marker Chart:

 * "Marker Name – 10B"
 * "Marker Name – 25.204KB"
 * "Marker Name – 512.54MB"

For implementation details on this processing, see `src/profiler-logic/marker-schema.js <https://github.com/firefox-devtools/profiler/blob/main/src/profile-logic/marker-schema.js>`_
in the profiler's front-end.

Next, define the main display of marker data, which will appear in the Marker
Chart tooltips and the Marker Table sidebar.

Each row may either be:

* A dynamic key-value pair, using one of the ``MarkerSchema::AddKey...`` functions. Each function is given:

  * Key: Element property name as streamed in ``StreamJSONMarkerData``.
  * Label: Optional prefix. Defaults to the key name.
  * Format: How to format the data element value, see `MarkerSchema::Format for details <https://searchfox.org/mozilla-central/define?q=T_mozilla%3A%3AMarkerSchema%3A%3AFormat>`_.
  * Searchable: Optional boolean, indicates if the value is used in searches, defaults to false.

* Or a fixed label and value strings, using ``MarkerSchema::AddStaticLabelValue``.

.. code-block:: c++

    // …
        schema.AddKeyLabelFormatSearchable(
            "myString", "My String", MS::Format::String, true);
        schema.AddKeyLabelFormat(
            "myBytes", "My Bytes", MS::Format::Bytes);
        schema.AddKeyLabelFormat(
            "myUrl", "My URL", MS::Format::Url);
        schema.AddKeyLabelFormat(
            "myTime", "Event time", MS::Format::Time);

Finally the ``schema`` object is returned from the function:

.. code-block:: c++

    // …
        return schema;
      }

Any other ``struct`` member function is ignored. There could be utility functions used by the above
compulsory functions, to make the code clearer.

And that is the end of the marker definition ``struct``.

.. code-block:: c++

    // …
    };

Performance Considerations
--------------------------

During profiling, it is best to reduce the amount of work spent doing profiler
operations, as they can influence the performance of the code that you want to profile.

Whenever possible, consider passing simple types to marker functions, such that
``StreamJSONMarkerData`` will do the minimum amount of work necessary to serialize
the marker type-specific arguments to its internal buffer representation. POD types
(numbers) and strings are the easiest and cheapest to serialize. Look at the
corresponding ``ProfileBufferEntryWriter::Serializer`` specializations if you
want to better understand the work done.

Avoid doing expensive operations when recording markers. E.g.: ``printf`` of
different things into a string, or complex computations; instead pass the
``printf``/computation arguments straight through to the marker function, so that
``StreamJSONMarkerData`` can do the expensive work at the end of the profiling session.

Marker Architecture Description
-------------------------------

The above sections should give all the information needed for adding your own marker
types. However, if you are wanting to work on the marker architecture itself, this
section will describe how the system works.

TODO:
 * Briefly describe the buffer and serialization.
 * Describe the template strategy for generating marker types
 * Describe the serialization and link to profiler front-end docs on marker processing (if they exist)
