Takes a path and produces a triangle mesh that corresponds to the antialiased stroked path.

The approach here is naive and only works for opaquely filled paths. Overlaping areas can 
end up with seams or otherwise incorrect coverage values.

Transforms with uniform scale can be supported by scaling the input points and the stroke width
before passing them to the stroker. Other transforms are not currently (or ever?) supported.

### TODO
- using triangle strips instead of triangle lists
- handle curves more efficiently than just flattening to lines
- handle cusps of curves more correctly
