This is a port of the WPF hardware rasterizer code to Rust. That
rasterizer is predecessor to the Direct2D rasterizer. Direct2D still
uses a similar technique when run on hardware that does not support
Target Independent Rasterization.

Design
======

The general algorithm used for rasterization is a vertical sweep of
the shape that maintains an active edge list.  The sweep is done
at a sub-scanline resolution and results in either:
   1. Sub-scanlines being combined in the coverage buffer and output
      as "complex scans". These are emitted as lines constructed out
      of triangle strips.
   2. Simple trapezoids being recognized in the active edge list
      and output using a faster simple trapezoid path.

Bezier flattening is done using an approach that uses forward differencing
of the error metric to compute a flattened version that would match a traditional
adaptive recursive flattening.


