:mod:`mozgeckoprofiler.rst` --- Gecko Profiler utilities
========================================================

This module contains various utilities to work with the Firefox Profiler, Gecko's
built-in performance profiler. Gecko itself records the profiles, and can dump them
out to file once the browser shuts down. This package takes those files, symbolicates
them (turns raw memory addresses into function or symbol names), and provides utilities
like opening up a locally stored profile in the Firefox Profiler interface. This
is done by serving the profiles locally, and opening a custom url in profiler.firefox.com.

:mod:`mozgeckoprofiler.rst` --- File origins in mozgeckoprofiler
----------------------------------------------------------------
The symbolication files were originally imported from the following repos,
with permission from their respective authors. However, since then the code has
been updated for usage within mozbase.

https://github.com/vdjeric/Snappy-Symbolication-Server/
https://github.com/mstange/analyze-tryserver-profiles/

The dump_syms_mac binary was copied from the objdir of a Firefox build on Mac. It's a
byproduct of the regular Firefox build process and gets generated in objdir/dist/host/bin/.
