Adjust SDK integration
======================

The *Adjust install tracking SDK* is a pure-Java library that is conditionally
compiled into Fennec.  It's not trivial to integrate such conditional feature
libraries into Fennec without pre-processing.  To minimize such pre-processing,
we define a trivial ``AdjustHelperInterface`` and define two implementations:
the real ``AdjustHelper``, which requires the Adjust SDK, and a no-op
``StubAdjustHelper``, which has no additional requirements.  We use the existing
pre-processed ``AppConstants.java.in`` to switch, at build-time, between the two
implementations.

An alternative approach would be to build three jars -- one interface jar and
two implementation jars -- and include one of the implementation jars at
build-time.  The implementation jars could either define a common symbol, or the
appropriate symbol could be determined at build-time.  That's a rather heavier
approach than the one chosen.  If the helper class were to grow to multiple
classes, with a non-trivial exposed API, this approach could be better.  It
would also be easier to integrate into the parallel Gradle build system.
