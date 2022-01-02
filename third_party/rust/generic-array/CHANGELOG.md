* **`0.12.4`**
    * Fix unsoundness in the `arr!` macro.

* **`0.12.0`**
    * Allow trailing commas in `arr!` macro.
    * **BREAKING**: Serialize `GenericArray` using `serde` tuples, instead of variable-length sequences. This may not be compatible with old serialized data.

* **`0.11.0`**
    * **BREAKING** Redesign `GenericSequence` with an emphasis on use in generic type parameters.
    * Add `MappedGenericSequence` and `FunctionalSequence`
        * Implements optimized `map`, `zip` and `fold` for `GenericArray`, `&GenericArray` and `&mut GenericArray`
    * **BREAKING** Remove `map_ref`, `zip_ref` and `map_slice`
        * `map_slice` is now equivalent to `GenericArray::from_iter(slice.iter().map(...))`
* **`0.10.0`**
    * Add `GenericSequence`, `Lengthen`, `Shorten`, `Split` and `Concat` traits.
    * Redefine `transmute` to avert errors.
* **`0.9.0`**
    * Rewrite construction methods to be well-defined in panic situations, correctly dropping elements.
    * `NoDrop` crate replaced by `ManuallyDrop` as it became stable in Rust core.
    * Add optimized `map`/`map_ref` and `zip`/`zip_ref` methods to `GenericArray`
* **`0.8.0`**
    * Implement `AsRef`, `AsMut`, `Borrow`, `BorrowMut`, `Hash` for `GenericArray`
    * Update `serde` to `1.0`
    * Update `typenum`
    * Make macro `arr!` non-cloning
    * Implement `From<[T; N]>` up to `N=32`
    * Fix #45
* **`0.7.0`**
    * Upgrade `serde` to `0.9`
    * Make `serde` with `no_std`
    * Implement `PartialOrd`/`Ord` for `GenericArray`
* **`0.6.0`**
    * Fixed #30
    * Implement `Default` for `GenericArray`
    * Implement `LowerHex` and `UpperHex` for `GenericArray<u8, N>`
    * Use `precision` formatting field in hex representation
    * Add `as_slice`, `as_mut_slice`
    * Remove `GenericArray::new` in favor of `Default` trait
    * Add `from_slice` and `from_mut_slice`
    * `no_std` and `core` for crate.
* **`0.5.0`**
    * Update `serde`
    * remove `no_std` feature, fixed #19
* **`0.4.0`**
    * Re-export `typenum`
* **`0.3.0`**
    * Implement `IntoIter` for `GenericArray`
    * Add `map` method
    * Add optional `serde` (de)serialization support feature.
* **`< 0.3.0`**
    * Initial implementation in late 2015
