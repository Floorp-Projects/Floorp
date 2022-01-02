# Vertical and horizontal operations

In SIMD terminology, each vector has a certain "width" (number of lanes).
A vector processor is able to perform two kinds of operations on a vector:

- Vertical operations:
  operate on two vectors of the same width, result has same width

**Example**: vertical addition of two `f32x4` vectors

      %0     == | 2 | -3.5 |  0 | 7 |
                  +     +     +   +
      %1     == | 4 |  1.5 | -1 | 0 |
                  =     =     =   =
    %0 + %1  == | 6 |  -2  | -1 | 7 |

- Horizontal operations:
  reduce the elements of two vectors in some way,
  the result's elements combine information from the two original ones

**Example**: horizontal addition of two `u64x2` vectors

      %0     == | 1 |  3 |
                  └─+───┘
                    └───────┐
                            │
      %1     == | 4 | -1 |  │
                  └─+──┘    │
                    └───┐   │
                        │   │
                  ┌─────│───┘
                  ▼     ▼
    %0 + %1  == | 4 |   3 |

## Performance consideration of horizontal operations

The result of vertical operations, like vector negation: `-a`, for a given lane,
does not depend on the result of the operation for the other lanes. The result
of horizontal operations, like the vector `sum` reduction: `a.sum()`, depends on
the value of all vector lanes.

In virtually all architectures vertical operations are fast, while horizontal
operations are, by comparison, very slow.

Consider the following two functions for computing the sum of all `f32` values
in a slice:

```rust
fn fast_sum(x: &[f32]) -> f32 {
    assert!(x.len() % 4 == 0);
    let mut sum = f32x4::splat(0.); // [0., 0., 0., 0.]
    for i in (0..x.len()).step_by(4) {
        sum += f32x4::from_slice_unaligned(&x[i..]);
    }
    sum.sum()
}

fn slow_sum(x: &[f32]) -> f32 {
    assert!(x.len() % 4 == 0);
    let mut sum: f32 = 0.;
    for i in (0..x.len()).step_by(4) {
        sum += f32x4::from_slice_unaligned(&x[i..]).sum();
    }
    sum
}
```

The inner loop over the slice is where the bulk of the work actually happens.
There, the `fast_sum` function perform vertical operations into a vector, doing
a single horizontal reduction at the end, while the `slow_sum` function performs
horizontal vector operations inside of the loop.

On all widely-used architectures, `fast_sum` is a large constant factor faster
than `slow_sum`. You can run the [slice_sum]() example and see for yourself. On
the particular machine tested there the algorithm using the horizontal vector
addition is 2.7x slower than the one using vertical vector operations!
