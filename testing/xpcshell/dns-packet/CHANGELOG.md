# Version 5.2.0 - 2019-02-21

- Feature: Added support for de/encoding certain OPT options.

# Version 5.1.0 - 2019-01-22

- Feature: Added support for the RP record type.

# Version 5.0.0 - 2018-06-01

- Breaking: Node.js 6.0.0 or greater is now required.
- Feature: Added support for DNSSEC record types.

# Version 4.1.0 - 2018-02-11

- Feature: Added support for the MX record type.

# Version 4.0.0 - 2018-02-04

- Feature: Added `streamEncode` and `streamDecode` methods for encoding TCP packets.
- Breaking: Changed the decoded value of TXT records to an array of Buffers. This is to accomodate DNS-SD records which rely on the individual strings record being separated.
- Breaking: Renamed the `flag_trunc` and `flag_auth` to `flag_tc` and `flag_aa` to match the names of these in the dns standards.

# Version 3.0.0 - 2018-01-12

- Breaking: The `class` option has been changed from integer to string.

# Version 2.0.0 - 2018-01-11

- Breaking: Converted module to ES2015, now requires Node.js 4.0 or greater
