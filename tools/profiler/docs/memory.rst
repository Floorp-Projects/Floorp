Profiling Memory
================

Sampling stacks from native allocations
---------------------------------------

The profiler can sample allocations and de-allocations from malloc using the
"Native Allocations" feature. This can be enabled by going to `about:profiling` and
enabling the "Native Allocations" checkbox. It is only available in Nightly, as it
uses a technique of hooking into malloc that could be a little more risky to apply to
the broader population of Firefox users.

This implementation is located in: `tools/profiler/core/memory_hooks.cpp
<https://searchfox.org/mozilla-central/source/tools/profiler/core/memory_hooks.cpp>`_

It works by hooking into all of the malloc calls. When the profiler is running, it
performs a `Bernoulli trial`_, that will pass for a given probability of per-byte
allocated. What this means is that larger allocations have a higher chance of being
recorded compared to smaller allocations. Currently, there is no way to configure
the per-byte probability. This means that sampled allocation sizes will be closer
to the actual allocated bytes.

This infrastructure is quite similar to DMD, but with the additional motiviations of
making it easy to turn on and use with the profiler. The overhead is quite high,
especially on systems with more expensive stack walking, like Linux. Turning off
thee "Native Stacks" feature can help lower overhead, but will give less information.

For more information on analyzing these profiles, see the `Firefox Profiler docs`_.

Memory counters
---------------

Similar to the Native Allocations feature, memory counters use the malloc memory hook
that is only available in Nightly. When it's available, the memory counters are always
turned on. This is a lightweight way to count in a very granular fashion how much
memory is being allocated and deallocated during the profiling session.

This information is then visualized in the `Firefox Profiler memory track`_.

This feature uses the `Profiler Counters`_, which can be used to create other types
of cheap counting instrumentation.

.. _Bernoulli trial: https://en.wikipedia.org/wiki/Bernoulli_trial
.. _Firefox Profiler docs: https://profiler.firefox.com/docs/#/./memory-allocations
.. _Firefox Profiler memory track: https://profiler.firefox.com/docs/#/./memory-allocations?id=memory-track
.. _Profiler Counters: https://searchfox.org/mozilla-central/source/tools/profiler/public/ProfilerCounts.h
