Problems:

Not all tests work on all platforms
Many tests work on N+1 platforms

Goals:

Tests and builds should be loosely coupled (you probably need a build
but you don't always need a build!)

Workflows:

1. Try: decide upon a set of builds and tests from a matrix of checkboxes

2. Branch: decide upon a set of builds based on in tree configuration
   (essentially a "fixed" version of try flags)

3. One off builds / One of tests (which require a build we created
   earlier)

## Build tasks

No special logic needed but convention of generating artifacts should be followed!

## Test Tasks

Always need a build (and likely also need the tests.zip). Should know
what potential builds they can run on.
