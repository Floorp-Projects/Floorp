<a name="v0.4.0"></a>
##  (2016-08-17)


#### Breaking Changes

* **LazyCell:**  return Err(value) on full cell ([68f3415d](https://github.com/indiv0/lazycell/commit/68f3415dd5d6a66ba047a133b7028ebe4f1c5070), breaks [#](https://github.com/indiv0/lazycell/issues/))

#### Improvements

* **LazyCell:**  return Err(value) on full cell ([68f3415d](https://github.com/indiv0/lazycell/commit/68f3415dd5d6a66ba047a133b7028ebe4f1c5070), breaks [#](https://github.com/indiv0/lazycell/issues/))



<a name="v0.3.0"></a>
##  (2016-08-16)


#### Features

*   add AtomicLazyCell which is thread-safe ([85afbd36](https://github.com/indiv0/lazycell/commit/85afbd36d8a148e14cc53654b39ddb523980124d))

#### Improvements

*   Use UnsafeCell instead of RefCell ([3347a8e9](https://github.com/indiv0/lazycell/commit/3347a8e97d2215a47e25c1e2fc953e8052ad8eb6))



<a name="v0.2.1"></a>
##  (2016-04-18)


#### Documentation

*   put types in between backticks ([607cf939](https://github.com/indiv0/lazycell/commit/607cf939b05e35001ba3070ec7a0b17b064e7be1))



<a name="v0.2.0"></a>
## v0.2.0 (2016-03-28)


#### Features

* **lazycell:**
  *  add tests for `LazyCell` struct ([38f1313d](https://github.com/indiv0/lazycell/commit/38f1313d98542ca8c98b424edfa9ba9c3975f99e), closes [#30](https://github.com/indiv0/lazycell/issues/30))
  *  remove unnecessary `Default` impl ([68c16d2d](https://github.com/indiv0/lazycell/commit/68c16d2df4e9d13d5298162c06edf918246fd758))

#### Documentation

* **CHANGELOG:**  removed unnecessary sections ([1cc0555d](https://github.com/indiv0/lazycell/commit/1cc0555d875898a01b0832ff967aed6b40e720eb))
* **README:**  add link to documentation ([c8dc33f0](https://github.com/indiv0/lazycell/commit/c8dc33f01f2c0dc187f59ee53a2b73081053012b), closes [#13](https://github.com/indiv0/lazycell/issues/13))



<a name="v0.1.0"></a>
## v0.1.0 (2016-03-16)


#### Features

* **lib.rs:**  implement Default trait for LazyCell ([150a6304](https://github.com/indiv0/LazyCell/commit/150a6304a230ee1de8424e49c447ec1b2d6578ce))



<a name="v0.0.1"></a>
## v0.0.1 (2016-03-16)


#### Bug Fixes

* **Cargo.toml:**  loosen restrictions on Clippy version ([84dd8f96](https://github.com/indiv0/LazyCell/commit/84dd8f960000294f9dad47d776a41b98ed812981))

#### Features

*   add initial implementation ([4b39764a](https://github.com/indiv0/LazyCell/commit/4b39764a575bcb701dbd8047b966f72720fd18a4))
*   add initial commit ([a80407a9](https://github.com/indiv0/LazyCell/commit/a80407a907ef7c9401f120104663172f6965521a))



