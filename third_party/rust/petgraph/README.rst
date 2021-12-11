
petgraph
========

Graph data structure library. Supports Rust 1.41 and later.

Please read the `API documentation here`__

__ https://docs.rs/petgraph/

|build_status|_ |crates|_ |gitter|_

.. |build_status| image:: https://github.com/petgraph/petgraph/workflows/Continuous%20integration/badge.svg?branch=master
.. _build_status: https://github.com/petgraph/petgraph/actions

.. |crates| image:: http://meritbadge.herokuapp.com/petgraph
.. _crates: https://crates.io/crates/petgraph

.. |gitter| image:: https://badges.gitter.im/petgraph-rs/community.svg
.. _gitter: https://gitter.im/petgraph-rs/community?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge

Crate feature flags:

- ``graphmap`` (default) enable ``GraphMap``.
- ``stable_graph`` (default) enable ``StableGraph``.
- ``matrix_graph`` (default) enable ``MatrixGraph``.
- ``serde-1`` (optional) enable serialization for ``Graph, StableGraph`` using
  serde 1.0. Requires Rust version as required by serde.

Recent Changes
--------------

See RELEASES_ for a list of changes.
The minimum supported rust version will only change on major releases.

.. _RELEASES: RELEASES.rst

License
-------

Dual-licensed to be compatible with the Rust project.

Licensed under the Apache License, Version 2.0
http://www.apache.org/licenses/LICENSE-2.0 or the MIT license
http://opensource.org/licenses/MIT, at your
option. This file may not be copied, modified, or distributed
except according to those terms.


