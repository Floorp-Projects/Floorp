# 0.1.12 (August 8, 2018)

* Fix initial send window size (#301).
* Fix panic when calling `reserve_capacity` after connection has been closed (#302).
* Fix handling of incoming `SETTINGS_INITIAL_WINDOW_SIZE`. (#299)

# 0.1.11 (July 31, 2018)

* Add `stream_id` accessors to public API types (#292).
* Fix potential panic when dropping clients (#295).
* Fix busy loop when shutting down server (#296).

# 0.1.10 (June 15, 2018)

* Fix potential panic in `SendRequest::poll_ready()` (#281).
* Fix infinite loop on reset connection during prefix (#285).

# 0.1.9 (May 31, 2018)

* Add `poll_reset` to `SendResponse` and `SendStream` (#279).

# 0.1.8 (May 23, 2018)

* Fix client bug when max streams is reached. (#277)

# 0.1.7 (May 14, 2018)

* Misc bug fixes (#266, #273, #261, #275).

# 0.1.6 (April 24, 2018)

* Misc bug fixes related to stream management (#258, #260, #262).

# 0.1.5 (April 6, 2018)

* Fix the `last_stream_id` sent during graceful GOAWAY (#254).

# 0.1.4 (April 5, 2018)

* Add `initial_connection_window_size` to client and server `Builder`s (#249).
* Add `graceful_shutdown` and `abrupt_shutdown` to `server::Connection`,
  deprecating `close_connection` (#250).

# 0.1.3 (March 28, 2018)

* Allow configuring max streams before the peer's settings frame is
  received (#242).
* Fix HPACK decoding bug with regards to large literals (#244).
* Fix state transition bug triggered by receiving a RST_STREAM frame (#247).

# 0.1.2 (March 13, 2018)

* Fix another bug relating to resetting connections and reaching
  max concurrency (#238).

# 0.1.1 (March 8, 2018)

* When streams are dropped, close the connection (#222).
* Notify send tasks on connection error (#231).
* Fix bug relating to resetting connections and reaching max concurrency (#235).
* Normalize HTTP request path to satisfy HTTP/2.0 specification (#228).
* Update internal dependencies.

# 0.1.0 (Jan 12, 2018)

* Initial release
