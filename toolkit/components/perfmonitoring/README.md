This directory is part of the implementation of the Performance Monitoring API

# What is the Performance Monitoring API?

The Performance Monitoring API is a set of interfaces designed to let front-end code find out if the application or a specific process is currently janky, quantify this jank and its evolution, and investigate what is causing jank (system code? a webpage? an add-on? CPOW?). In other words, this is a form of minimal profiler, designed to be lightweight enough to be enabled at all times in production code.

In Firefox Nightly, the Performance Monitoring API is used to:
- inform users if their machine janks because of an add-on;
- upload add-on performance to Telemetry for the benefit of AMO maintainers and add-on developers;
- let users inspect the performance of their browser through about:performance.

# How can I use the API?

The API is designed mainly to be used from JavaScript client code, using PerformanceStats.jsm.  If you really need to use it from C++ code, you should use the performance stats service defined in nsIPerformanceStats.idl. Note that PerformanceStats.jsm contains support for entire e10s-enabled applications, while nsIPerformanceStats.idl only supports one process at a time.


# How does the Performance Monitoring API work?

At the time of this writing, the implementation of this API monitors only performance information related to the execution of JavaScript code, and only in the main thread. This is performed by an instrumentation of js/, orchestrated by toolkit/.

At low-level, the unit of code used for monitoring is the JS Compartment: one .jsm module, one XPCOM component, one sandbox, one script in an iframe, ... When executing code in a compartment, it is possible to inspect either the compartment or the script itself to find out who this compartment belongs to: a `<xul:browser>`, an add-on, etc.

At higher-level, the unit of code used for monitoring is the Performance Group. One Performance Group represents one or more JS Compartments, grouped together because we are interested in their performance. The current implementation uses Performance Groups to represent individual JS Compartments, entire add-ons, entire webpages including iframes and entire threads. Other applications have been discussed to represent entire eTLD+1 domains (e.g. to monitor the cost of ads), etc.

A choice was made to represent the CPU cost in *clock cycles* at low-level, as extracting a number of clock cycles has a very low latency (typically a few dozen cycles on recent architectures) and is much more precise than `getrusage`-style CPU clocks (which are often limited to a precision of 16ms). The drawback of this choice is that distinct CPUs/cores may, depending on the architecture, have entirely unrelated clock cycles count. We assume that the risk of false positives is reasonably low, and bound the uncertainty by renormalizing the result with the actual CPU clocks once per event.

## SpiderMonkey-level

The instrumentation of SpiderMonkey lives in `js/src/vm/Stopwatch.*`. As SpiderMonkey does not know about the Gecko event loop, or DOM events, or windows, so any such information must be provided by the embedding. To communicate with higher levels, SpiderMonkey exposes a virtual class `js::PerformanceGroup` designed to be subclassed and instantiated by the embedding based on its interests.

An instance of `js::PerformanceGroup` may be acquired (to mark that it is currently being monitored) and released (once monitoring is complete or cancelled) by SpiderMonkey. Furthermore, a `js::PerformanceGroup` can be marked as active (to mark that the embedding is currently interested in its performance) or inactive (otherwise) by the embedding.

Each `js::Performance` holds a total CPU cost measured in *clock cycles* and a total CPOW cost measured in *microseconds*. Both costs can only increase while measuring data, and can be reset to 0 by the embedding, once we have finished execution of the event loop.

### Upon starting to execute code in a JS Compartment `cx`
1. If global monitoring is deactivated, bailout;
2. If XPConnect has informed us that we are entering a nested event loop, cancel any ongoing measure on the outer event loop and proceed with the current measure;
3. If we do not know to which performance groups `cx` is associated, request the information from the embedding;
4. For each performance group `group` to which `cx` belongs *and* that is not acquired *and* for which monitoring is active, acquire the group;
5. If no group was acquired, bailout;
6. Capture a timestamp for the CPU cost of `cx`, in *clock cycles*. This value is provided directly by the CPU;
7. Capture a timestamp for the CPOW cost of `cx`, in *CPOW microseconds*. This value is provided by the CPOW-level embedding.

### Upon stopping execution of the code in the JS compartment `cx`
1. If global monitoring is deactivated, bailout;
2. If the measure has been canceled, bailout;
3. If no group was acquired, bailout;
4. Capture a timestamp for the CPU cost of `cx`, use it to update the total CPU cost of each of the groups acquired;
5. Capture a timestamp for the CPOW cost of `cx`, use it to update the total CPOW cost of each of the groups acquired;
6. Mark acquired groups as executed recently;
7. Release groups.

### When informed by the embedding that the iteration of the event loop is complete
1. Commit all the groups executed recently to the embedding;
2. Release all groups;
3. Reset all CPU/CPOW costs to 0.

## Cross-Process Object Wrapper-level

The instrumentation of CPOW lives in `js/ipc/src`. It maintains a CPOW clock that increases whenever the process is blocked by a CPOW call.

## XPConnect-level

The instrumentation of XPConnect lives in `js/xpconnect/src/XPCJSRuntime.cpp`.

### When we enter a nested event loop

1. Inform the SpiderMonkey-level instrumentation, to let it cancel any ongoing measure.

### When we finish executing an iteration of the event loop, including microtasks:

1. Inform the SpiderMonkey-level instrumentation, to let it commit its recent data.

## nsIPerformanceStatsService-level

This code lives in `toolkit/components/perfmonitoring/ns*`. Its role is to orchestrate the information provided by SpiderMonkey at the scale of a single thread of a single process. At the time of this writing, this instrumentation is only activated on the main thread, for all Gecko processes.

The service defines a class `nsPerformanceGroup`, designed to be the sole concrete implementation of `js::PerformanceGroup`.  `nsPerformanceGroup` extends `js::PerformanceGroup` with the global performance information gathered for the group since the start of the service. The information is:
- total CPU time measured;
- total CPOW time measured;
- number of times CPU time exceeded 1ms;
- number of times CPU time exceeded 2ms;
- number of times CPU time exceeded 4ms;
- ...
- number of times CPU time exceeded 2^9ms.

Also, `nsPerformanceGroup` extends `js::PerformanceGroup` with high-level identification:
- id of the window that executed the code, if any;
- id of the add-on that provided the code, if any.

### When the SpiderMonkey-level instrumentation requests a list of PerformanceGroup for a compartment

Return a list with the following groups:
* all compartments are associated with the "top group", which represents the entire thread;
* find out if the compartment is code from a window, if so add a group shared by all compartments for this specific window;
* find out if the compartment is code from an add-on, if so add a group shared by all compartments for this add-on;
* add a group representing this specific compartment.

For performance reasons, groups representing a single compartment are inactive by default, while all other groups are active by default.

Performance groups are refcounted and destroyed with the implementation of `delete` used by toolkit/.

### When the SpiderMonkey-level instrumentation commits a list of PerformanceGroups

For each group in the list:
1. transfer recent CPU time and recent CPOW time to total CPU time, total CPOW time, number of times CPU time exceeded *n* ms;
2. reset group.

Future versions are expected to trigger low-performance alerts at this stage.

### Snapshotting

(to be documented)

## PerformanceStats.jsm-level

PerformanceStats provides a JS-friendly API on top of nsIPerformanceStatsService. The main differences are:
- utilities for subtracting snapshots;
- tracking clients that need specific measures;
- synchronization between e10s processes.
