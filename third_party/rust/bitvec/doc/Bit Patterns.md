# Bit Patterns

This table displays the *bit index*, in [base64], of each position in a
`BitSlice<Cursor, Fundamental>` on a little-endian machine.

```text
byte  | 00000000 11111111 22222222 33333333 44444444 55555555 66666666 77777777
bit   | 76543210 76543210 76543210 76543210 76543210 76543210 76543210 76543210
------+------------------------------------------------------------------------
LEu__ | HGFEDCBA PONMLKJI XWVUTSRQ fedcbaZY nmlkjihg vutsrqpo 3210zyxw /+987654
BEu64 | 456789+/ wxyz0123 opqrstuv ghijklmn YZabcdef QRSTUVWX IJKLMNOP ABCDEFGH
BEu32 | YZabcdef QRSTUVWX IJKLMNOP ABCDEFGH 456789+/ wxyz0123 opqrstuv ghijklmn
BEu16 | IJKLMNOP ABCDEFGH YZabcdef QRSTUVWX opqrstuv ghijklmn 456789+/ wxyz0123
BEu8  | ABCDEFGH IJKLMNOP QRSTUVWX YZabcdef ghijklmn opqrstuv wxyz0123 456789+/
```

This table displays the bit index in [base64] of each position in a
`BitSlice<Cursor, Fundamental>` on a big-endian machine.

```text
byte  | 00000000 11111111 22222222 33333333 44444444 55555555 66666666 77777777
bit   | 76543210 76543210 76543210 76543210 76543210 76543210 76543210 76543210
------+------------------------------------------------------------------------
BEu__ | ABCDEFGH IJKLMNOP QRSTUVWX YZabcdef ghijklmn opqrstuv wxyz0123 456789+/
LEu64 | /+987654 3210zyxw vutsrqpo nmlkjihg fedcbaZY XWVUTSRQ PONMLKJI HGFEDCBA
LEu32 | fedcbaZY XWVUTSRQ PONMLKJI HGFEDCBA /+987654 3210zyxw vutsrqpo nmlkjihg
LEu16 | PONMLKJI HGFEDCBA fedcbaZY XWVUTSRQ vutsrqpo nmlkjihg /+987654 3210zyxw
LEu8  | HGFEDCBA PONMLKJI XWVUTSRQ fedcbaZY nmlkjihg vutsrqpo 3210zyxw /+987654
```

`<BigEndian, u8>` and `<LittleEndian, u8>` will always have the same
representation in memory on all machines. The wider cursors will not.

# Pointer Representation

Currently, the bitslice pointer uses the `len` field to address an individual
bit in the slice. This means that all bitslices can address `usize::MAX` bits,
regardless of the underlying storage fundamental. The bottom `3 <= n <= 6` bits
of `len` address the bit in the fundamental, and the high bits address the
fundamental in the slice.

The next representation of bitslice pointer will permit the data pointer to
address any *byte*, regardless of fundamental type, and address any bit in that
byte by storing the bit position in `len`. This reduces the bit storage capacity
of bitslice from `usize::MAX` to `usize::MAX / 8`. 2<sup>29</sup> is still a
very large number, so I do not anticipate 32-bit machines being too limited by
this.

This means that bitslice pointers will have the following representation, in C++
because Rust lacks bitfield syntax.

```cpp
template<typename T>
struct WidePtr<T> {
  size_t ptr_byte : __builtin_ctzll(alignof(T)); // 0 ... 3
  size_t ptr_data : sizeof(T*) * 8
                  - __builtin_ctzll(alignof(T)); // 64 ... 61

  size_t len_head : 3;
  size_t len_tail : 3 + __builtin_ctzll(alignof(T)); // 3 ... 6
  size_t len_data : sizeof(size_t) * 8
                  - 6 - __builtin_ctzll(alignof(T)); // 58 ... 55
};
```

So, for any storage fundamental, its bitslice pointer representation has:

- the low `alignof` bits of the pointer for selecting a byte, and the rest of
  the pointer for selecting the fundamental. This is just a `*const u8` except
  the type remembers how to find the correctly aligned pointer.

- the lowest 3 bits of the length counter for selecting the bit under the head
  pointer
- the *next* (3 + log<sub>2</sub>(bit size)) bits of the length counter address
  the final bit within the final *storage fundamental* of the slice.
- the remaining high bits address the final *storage fundamental* of the slice,
  counting from the correctly aligned address in the pointer.

# Calculations

Given an arbitrary `WidePtr<T>` value,

- the initial `*const T` pointer is retrieved by masking away the low bits of
  the `ptr` value

- the number of `T` elements *between* the first and the last is found by taking
  the `len` value, masking away the low bits, and shifting right/down.

- the number of `T` elements in the slice is found by taking the above and
  adding one

- the address of the last `T` element in the slice is found by taking the
  initial pointer, and adding the `T`-element-count to it

- the slot number of the first live bit in the slice is found by masking away
  the high bits of `ptr` and shifting the result left/up by three, then adding
  the low three bits of `len`

# Values

## Uninhabited Domains

All pointers whose non-`data` members are fully zeroed are considered
uninhabited. When the `data` member is the null pointer, then the slice is
*empty*; when it is non-null, the slice points to a validly allocated region of
memory and is merely uninhabited. This distinction is important for vectors.

## Full Domains

The longest possible domain has `!0` as its `elts`, and `tail` values, and `0`
as its `head` value.

When `elts` and `tail` are both `!0`, then the `!0`th element has `!0 - 1` live
bits. The final bit in the final element is a tombstone that cannot be used.
This is a regrettable consequence of the need to distinguish between the nil and
uninhabited slices.

[base64]: https://en.wikipedia.org/wiki/Base64
