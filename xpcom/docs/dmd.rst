Dark Matter Detector (DMD)
==========================

.. note::

    This section was copied with minimal modification from
    https://developer.mozilla.org/en-US/docs/Mozilla/Performance/DMD. Some
    parts of the documentation may be out-of-date.

DMD (short for "dark matter detector") is a heap profiler within Firefox. It
has four modes.

* "Dark Matter" mode. In this mode, DMD tracks the contents of the heap,
  including which heap blocks have been reported by memory reporters. It helps
  us reduce the "heap-unclassified" value in Firefox's about:memory page, and
  also detects if any heap blocks are reported twice. Originally, this was the
  only mode that DMD had, which explains DMD's name. This is the default mode.

* "Live" mode. In this mode, DMD tracks the current contents of the heap. You
  can dump that information to file, giving a profile of the live heap blocks
  at that point in time. This is good for understanding how memory is used at
  an interesting point in time, such as peak memory usage.

* "Cumulative" mode. In this mode, DMD tracks both the past and current
  contents of the heap. You can dump that information to file, giving a profile
  of the heap usage for the entire session. This is good for finding parts of
  the code that cause high heap churn, e.g. by allocating many short-lived
  allocations.

* "Heap scanning" mode. This mode is like live mode, but it also records the
  contents of every live block in the log. This can be used to investigate
  leaks by figuring out which objects might be holding references to other
  objects.


Building and Running
--------------------

Nightly Firefox
~~~~~~~~~~~~~~~

The easiest way to use DMD is with the normal Nightly Firefox build, which
has DMD already enabled in the build. To have DMD active while running it,
you just need to set the environment variable ``DMD=1`` when running. For
instance, on OSX, you can run something like:

.. code-block: bash

    DMD=1 /Applications/Firefox\ Nightly.app/Contents/MacOS/firefox

You can tell it is working by going to ``about:memory`` and looking for "Save
DMD Output". If DMD has been properly enabled, the "Save" button won't be
grayed out. Look at the "Trigger" section below to see the full list of ways
to get a DMD report once you have it activated. Note that stack information
you get will likely be less detailed, due to being unable to symbolicate. You
will be able to get function names, but not line numbers.

Processing the output
---------------------

DMD outputs one gzipped JSON file per process that contains a description of
that process's heap. You can analyze these files (either gzipped or not)
using ``dmd.py``. On Nightly Firefox, ``dmd.py`` is included in the
distribution. For instance on OS X, it is located in the directory
``/Applications/Firefox Nightly.app/Contents/Resources/``. For Nightly,
symbolication will fail, but you can at least get some information. In a
local build, ``dmd.py`` will be located in the directory
``$OBJDIR/dist/bin/``.

Some platforms (Linux, Mac, Android) require stack fixing, which adds missing
filename, function name and line number information. This will occur
automatically the first time you run ``dmd.py`` on the output file. This can
take 10s of seconds or more to complete. (This will fail if your build does
not contain symbols. However, if you have crash reporter symbols for your
build (as tryserver builds do) you can use this script instead: clone the
whole repo, edit the paths at the top of ``resymbolicate_dmd.py`` and run
it.) The simplest way to do this is to just run the ``dmd.py`` script on your
DMD report while your working directory is ``$OBJDIR/dist/bin``. This will
allow the local libraries to be found and used.

If you invoke ``dmd.py`` without arguments you will get output appropriate
for the mode in which DMD was invoked.

"Dark matter" mode output
~~~~~~~~~~~~~~~~~~~~~~~~~

For "dark matter" mode, ``dmd.py``'s output describes how the live heap blocks
are covered by memory reports. This output is broken into multiple sections.

1. "Invocation". This tells you how DMD was invoked, i.e. what options were used.

2. "Twice-reported stack trace records". This tells you which heap blocks
   were reported twice or more. The presence of any such records indicates bugs
   in one or more memory reporters.

3. "Unreported stack trace records". This tells you which heap blocks were
   not reported, which indicate where additional memory reporters would be most
   helpful.

4. "Once-reported stack trace records": like the "Unreported stack trace
   records" section, but for blocks reported once.

5. "Summary": gives measurements of the total heap, and the
   unreported/once-reported/twice-reported portions of it.

The "Twice-reported stack trace records" and "Unreported stack trace records"
sections are the most important, because they indicate ways in which the
memory reporters can be improved.

Here's an example stack trace record from the "Unreported stack trace
records" section.

.. code-block::

    Unreported {
      150 blocks in heap block record 283 of 5,495
      21,600 bytes (20,400 requested / 1,200 slop)
      Individual block sizes: 144 x 150
      0.00% of the heap (16.85% cumulative)
      0.02% of unreported (94.68% cumulative)
      Allocated at {
        #01: replace_malloc (/home/njn/moz/mi5/go64dmd/memory/replace/dmd/../../../../memory/replace/dmd/DMD.cpp:1286)
        #02: malloc (/home/njn/moz/mi5/go64dmd/memory/build/../../../memory/build/replace_malloc.c:153)
        #03: moz_xmalloc (/home/njn/moz/mi5/memory/mozalloc/mozalloc.cpp:84)
        #04: nsCycleCollectingAutoRefCnt::incr(void*, nsCycleCollectionParticipant*) (/home/njn/moz/mi5/go64dmd/dom/xul/../../dist/include/nsISupportsImpl.h:250)
        #05: nsXULElement::Create(nsXULPrototypeElement*, nsIDocument*, bool, bool,mozilla::dom::Element**) (/home/njn/moz/mi5/dom/xul/nsXULElement.cpp:287)
        #06: nsXBLContentSink::CreateElement(char16_t const**, unsigned int, mozilla::dom::NodeInfo*, unsigned int, nsIContent**, bool*, mozilla::dom::FromParser) (/home/njn/moz/mi5/dom/xbl/nsXBLContentSink.cpp:874)
        #07: nsCOMPtr<nsIContent>::StartAssignment() (/home/njn/moz/mi5/go64dmd/dom/xml/../../dist/include/nsCOMPtr.h:753)
        #08: nsXMLContentSink::HandleStartElement(char16_t const*, char16_t const**, unsigned int, unsigned int, bool) (/home/njn/moz/mi5/dom/xml/nsXMLContentSink.cpp:1007)
      }
    }

It tells you that there were 150 heap blocks that were allocated from the
program point indicated by the "Allocated at" stack trace, that these blocks
took up 21,600 bytes, that all 150 blocks had a size of 144 bytes, and that
1,200 of those bytes were "slop" (wasted space caused by the heap allocator
rounding up request sizes). It also indicates what percentage of the total
heap size and the unreported portion of the heap these blocks represent.

Within each section, records are listed from largest to smallest.

Once-reported and twice-reported stack trace records also have stack traces for the report point(s). For example:

.. code-block::

    Reported at {
      #01: mozilla::dmd::Report(void const*) (/home/njn/moz/mi2/memory/replace/dmd/DMD.cpp:1740) 0x7f68652581ca
      #02: CycleCollectorMallocSizeOf(void const*) (/home/njn/moz/mi2/xpcom/base/nsCycleCollector.cpp:3008) 0x7f6860fdfe02
      #03: nsPurpleBuffer::SizeOfExcludingThis(unsigned long (*)(void const*)) const (/home/njn/moz/mi2/xpcom/base/nsCycleCollector.cpp:933) 0x7f6860fdb7af
      #04: nsCycleCollector::SizeOfIncludingThis(unsigned long (*)(void const*), unsigned long*, unsigned long*, unsigned long*, unsigned long*, unsigned long*) const (/home/njn/moz/mi2/xpcom/base/nsCycleCollector.cpp:3029) 0x7f6860fdb6b1
      #05: CycleCollectorMultiReporter::CollectReports(nsIMemoryMultiReporterCallback*, nsISupports*) (/home/njn/moz/mi2/xpcom/base/nsCycleCollector.cpp:3075) 0x7f6860fde432
      #06: nsMemoryInfoDumper::DumpMemoryReportsToFileImpl(nsAString_internal const&) (/home/njn/moz/mi2/xpcom/base/nsMemoryInfoDumper.cpp:626) 0x7f6860fece79
      #07: nsMemoryInfoDumper::DumpMemoryReportsToFile(nsAString_internal const&, bool, bool) (/home/njn/moz/mi2/xpcom/base/nsMemoryInfoDumper.cpp:344) 0x7f6860febaf9
      #08: mozilla::(anonymous namespace)::DumpMemoryReportsRunnable::Run() (/home/njn/moz/mi2/xpcom/base/nsMemoryInfoDumper.cpp:58) 0x7f6860fefe03
    }

You can tell which memory reporter made the report by the name of the
``MallocSizeOf`` function near the top of the stack trace. In this case it
was the cycle collector's reporter.

By default, DMD does not record an allocation stack trace for most blocks, to
make it run faster. The decision on whether to record is done
probabilistically, and larger blocks are more likely to have an allocation
stack trace recorded. All unreported blocks that lack an allocation stack
trace will end up in a single record. For example:

.. code-block::

    Unreported {
      420,010 blocks in heap block record 2 of 5,495
      29,203,408 bytes (27,777,288 requested / 1,426,120 slop)
      Individual block sizes: 2,048 x 3; 1,024 x 103; 512 x 147; 496 x 7; 480 x 31; 464 x 6; 448 x 50; 432 x 41; 416 x 28; 400 x 53; 384 x 43; 368 x 216; 352 x 141; 336 x 58; 320 x 104; 304 x 5,130; 288 x 150; 272 x 591; 256 x 6,017; 240 x 1,372; 224 x 93; 208 x 488; 192 x 1,919; 176 x 18,903; 160 x 1,754; 144 x 5,041; 128 x 36,709; 112 x 5,571; 96 x 6,280; 80 x 40,738; 64 x 37,925; 48 x 78,392; 32 x 136,199; 16 x 31,001; 8 x 4,706
      3.78% of the heap (10.24% cumulative)
      21.24% of unreported (57.53% cumulative)
      Allocated at {
        #01: (no stack trace recorded due to --stacks=partial)
      }
    }

In contrast, stack traces are always recorded when a block is reported, which
means you can end up with records like this where the allocation point is
unknown but the reporting point is known:

.. code-block::

    Once-reported {
      104,491 blocks in heap block record 13 of 4,689
      10,392,000 bytes (10,392,000 requested / 0 slop)
      Individual block sizes: 512 x 124; 256 x 242; 192 x 813; 128 x 54,664; 64 x 48,648
      1.35% of the heap (48.65% cumulative)
      1.64% of once-reported (59.18% cumulative)
      Allocated at {
        #01: (no stack trace recorded due to --stacks=partial)
      }
      Reported at {
        #01: mozilla::dmd::DMDFuncs::Report(void const*) (/home/njn/moz/mi5/go64dmd/memory/replace/dmd/../../../../memory/replace/dmd/DMD.cpp:1646)
        #02: WindowsMallocSizeOf(void const*) (/home/njn/moz/mi5/dom/base/nsWindowMemoryReporter.cpp:189)
        #03: nsAttrAndChildArray::SizeOfExcludingThis(unsigned long (*)(void const*)) const (/home/njn/moz/mi5/dom/base/nsAttrAndChildArray.cpp:880)
        #04: mozilla::dom::FragmentOrElement::SizeOfExcludingThis(unsigned long (*)(void const*)) const (/home/njn/moz/mi5/dom/base/FragmentOrElement.cpp:2337)
        #05: nsINode::SizeOfIncludingThis(unsigned long (*)(void const*)) const (/home/njn/moz/mi5/go64dmd/parser/html/../../../dom/base/nsINode.h:307)
        #06: mozilla::dom::NodeInfo::NodeType() const (/home/njn/moz/mi5/go64dmd/dom/base/../../dist/include/mozilla/dom/NodeInfo.h:127)
        #07: nsHTMLDocument::DocAddSizeOfExcludingThis(nsWindowSizes*) const (/home/njn/moz/mi5/dom/html/nsHTMLDocument.cpp:3710)
        #08: nsIDocument::DocAddSizeOfIncludingThis(nsWindowSizes*) const (/home/njn/moz/mi5/dom/base/nsDocument.cpp:12820)
      }
    }

The choice of whether to record an allocation stack trace for all blocks is controlled by an option (see below).

"Live" mode output
~~~~~~~~~~~~~~~~~~

For "live" mode, dmd.py's output describes what live heap blocks are present.
This output is broken into multiple sections.

1. "Invocation". This tells you how DMD was invoked, i.e. what options were used.
2. "Live stack trace records". This tells you which heap blocks were present.
3. "Summary": gives measurements of the total heap.

The individual records are similar to those output in "dark matter" mode.


"Cumulative" mode output
~~~~~~~~~~~~~~~~~~~~~~~~

For "cumulative" mode, dmd.py's output describes how the live heap blocks are
covered by memory reports. This output is broken into multiple sections.

1. "Invocation". This tells you how DMD was invoked, i.e. what options were used.
2. "Cumulative stack trace records". This tells you which heap blocks were allocated during the session.
3. "Summary": gives measurements of the total (cumulative) heap.

The individual records are similar to those output in "dark matter" mode.


"Scan" mode output
~~~~~~~~~~~~~~~~~~

For "scan" mode, the output of ``dmd.py`` is the same as "live" mode. A
separate script, ``block_analyzer.py``, can be used to find out information
about which blocks refer to a particular block. ``dmd.py --clamp-contents``
needs to be run on the log first. See this other page for an overview of how
to use heap scan mode to fix a leak involving refcounted objects.

Options
-------

Runtime
~~~~~~~

When you run ``mach run --dmd`` you can specify additional options to control
how DMD runs. Run ``mach help run`` for documentation on these.

The most interesting one is ``--mode``. Acceptable values are ``dark-matter``
(the default), ``live``, ``cumulative``, and ``scan``.

Another interesting one is ``--stacks``. Acceptable values are ``partial``
(the default) and ``full``. In the former case most blocks will not have an
allocation stack trace recorded. However, because larger blocks are more
likely to have one recorded, most allocated bytes should have an allocation
stack trace even though most allocated blocks do not. Use ``--stacks=full``
if you want complete information, but note that DMD will run substantially
slower in that case.

The options may also be put in the environment variable DMD, or set DMD to 1
to enable DMD with default options (dark-matter and partial stacks).

The ``MOZ_DMD_SHUTDOWN_LOG`` environment variable, if set, triggers a DMD run
at shutdown; its value must be a directory where the logs will be placed.
Which processes get logged is controlled by the ``MOZ_DMD_LOG_PROCESS``
environment variable, which can take the following values.

* Unset: log all processes.
* "default": log the parent process only.
* "tab": log content processes only.

For example, if you have

.. code-block::

    MOZ_DMD_SHUTDOWN_LOG=~/dmdlogs/ MOZ_DMD_LOG_PROCESS=tab

then DMD logs for content processes will be saved to ~/dmdlogs/.

.. note::

    To dump DMD data from Content processes, you'll need to disable the sandbox

.. note::

    ``MOZ_DMD_SHUTDOWN_LOG`` must (currently) include the trailing separator (''/")


Post-processing
~~~~~~~~~~~~~~~

``dmd.py`` also takes options that control how it works. Run ``dmd.py -h``
for documentation. The following options are the most interesting ones.

* ``-f`` / ``--max-frames``. By default, records show up to 8 stack frames. You
  can choose a smaller number, in which case more allocations will be
  aggregated into each record, but you'll have less context. Or you can choose
  a larger number, in which cases allocations will be split across more
  records, but you will have more context. There is no single best value, but
  values in the range 2..10 are often good. The maximum is 24.

* ``-a`` / ``--ignore-alloc-frames``. Many allocation stack traces start with
  multiple frames that mention allocation wrapper functions, e.g.
  ``js_calloc()`` calls replace_calloc(). This option filters these out. It
  often helps improve the quality of the output when using a small
  ``--max-frames`` value.

* ``-s`` / ``--sort-by``. This controls how records are sorted. Acceptable
  values are usable (the default), ``req``, ``slop`` and ``num-blocks``.

* ``--clamp-contents``. For a heap scan log, this performs a conservative
  pointer analysis on the contents of each block, changing any value that is a
  pointer into the middle of a live block into a pointer to the start of that
  block. All other values are changes to null. In addition, all trailing nulls
  are removed from the block contents.

As an example that combines multiple options, if you apply the following
command to a profile obtained in "live" mode:

.. code-block::

    dmd.py -r -f 2 -a -s slop

it will give you a good idea of where the major sources of slop are.

``dmd.py`` can also compute the difference between two DMD output files, so
long as those files were produced in the same mode. Simply pass it two
filenames instead of one to get the difference.


Which heap blocks are reported?
-------------------------------

At this stage you might wonder how DMD knows, in "dark matter" mode, which
allocations have been reported and which haven't. DMD only knows about heap
blocks that are measured via a function created with one of the following two
macros:

.. code-block::

    MOZ_DEFINE_MALLOC_SIZE_OF
    MOZ_DEFINE_MALLOC_SIZE_OF_ON_ALLOC

Fortunately, most of the existing memory reporters do this.
