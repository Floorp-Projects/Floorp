/* -*- Mode: js; js-indent-level: 2; indent-tabs-mode: nil; tab-width: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint esnext: true, moz: true */

'use strict';

this.EXPORTED_SYMBOLS = ['DNSRecord'];

const { utils: Cu } = Components;

Cu.import('resource://gre/modules/DataWriter.jsm');
Cu.import('resource://gre/modules/DNSTypes.jsm');

class DNSRecord {
  constructor(properties = {}) {
    this.name       = properties.name       || '';
    this.recordType = properties.recordType || DNS_RECORD_TYPES.ANY;
    this.classCode  = properties.classCode  || DNS_CLASS_CODES.IN;
    this.cacheFlush = properties.cacheFlush || false;
  }

  static parseFromPacketReader(reader) {
    let name       = reader.getLabel();
    let recordType = reader.getValue(2);
    let classCode  = reader.getValue(2);
    let cacheFlush = (classCode & 0x8000) ? true : false;
    classCode &= 0xff;

    return new this({
      name: name,
      recordType: recordType,
      classCode: classCode,
      cacheFlush: cacheFlush
    });
  }

  serialize() {
    let writer = new DataWriter();

    // Write `name` (ends with trailing 0x00 byte)
    writer.putLabel(this.name);

    // Write `recordType` (2 bytes)
    writer.putValue(this.recordType, 2);

    // Write `classCode` (2 bytes)
    let classCode = this.classCode;
    if (this.cacheFlush) {
      classCode |= 0x8000;
    }
    writer.putValue(classCode, 2);

    return writer.data;
  }

  toJSON() {
    return JSON.stringify(this.toJSONObject());
  }

  toJSONObject() {
    return {
      name: this.name,
      recordType: this.recordType,
      classCode: this.classCode,
      cacheFlush: this.cacheFlush
    };
  }
}
