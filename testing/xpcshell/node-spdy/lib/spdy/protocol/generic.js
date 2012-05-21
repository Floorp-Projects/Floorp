/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//
// ### function parseHeader (data)
// ### @data {Buffer} incoming data
// Returns parsed SPDY frame header
//
exports.parseHeader = function parseHeader(data) {
  var header = {
    control: (data.readUInt8(0) & 0x80) === 0x80 ? true : false,
    version: null,
    type: null,
    id: null,
    flags: data.readUInt8(4),
    length: data.readUInt32BE(4) & 0x00ffffff
  };

  if (header.control) {
    header.version = data.readUInt16BE(0) & 0x7fff;
    header.type = data.readUInt16BE(2);
  } else {
    header.id = data.readUInt32BE(0) & 0x7fffffff;
  }

  return header;
};
