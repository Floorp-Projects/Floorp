---
title: RustObject.<init> - 
---

[mozilla.components.service.fxa](../index.html) / [RustObject](index.html) / [&lt;init&gt;](./-init-.html)

# &lt;init&gt;

`RustObject()`

Base class that wraps an non-optional [Pointer](#) representing a pointer to a Rust object.
This class implements [Closeable](http://docs.oracle.com/javase/6/docs/api/java/io/Closeable.html) but does not provide an implementation, forcing all
subclasses to implement it. This ensures that all classes that inherit from RustObject
will have their [Pointer](#) destroyed when the Java wrapper is destroyed.

