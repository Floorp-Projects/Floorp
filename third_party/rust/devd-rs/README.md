[![crates.io](https://img.shields.io/crates/v/systemstat.svg)](https://crates.io/crates/systemstat)
[![unlicense](https://img.shields.io/badge/un-license-green.svg?style=flat)](http://unlicense.org)

# devd-rs

A Rust library for listening to FreeBSD (also DragonFlyBSD) [devd](https://www.freebsd.org/cgi/man.cgi?devd)'s device attach-detach notifications.

Listens on `/var/run/devd.seqpacket.pipe` and parses messages using [nom](https://github.com/Geal/nom).

## Usage

See [examples/main.rs](https://github.com/myfreeweb/devd-rs/blob/master/examples/main.rs).

## Contributing

Please feel free to submit pull requests!

By participating in this project you agree to follow the [Contributor Code of Conduct](http://contributor-covenant.org/version/1/4/).

[The list of contributors is available on GitHub](https://github.com/myfreeweb/devd-rs/graphs/contributors).

## License

This is free and unencumbered software released into the public domain.  
For more information, please refer to the `UNLICENSE` file or [unlicense.org](http://unlicense.org).
