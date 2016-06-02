/* -*- Mode: js; js-indent-level: 2; indent-tabs-mode: nil; tab-width: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint esnext: true, moz: true */

'use strict';

this.EXPORTED_SYMBOLS = ['DNSPacket'];

const { utils: Cu } = Components;

Cu.import('resource://gre/modules/Services.jsm');

Cu.import('resource://gre/modules/DataReader.jsm');
Cu.import('resource://gre/modules/DataWriter.jsm');
Cu.import('resource://gre/modules/DNSRecord.jsm');
Cu.import('resource://gre/modules/DNSResourceRecord.jsm');

const DEBUG = true;

function debug(msg) {
  Services.console.logStringMessage('DNSPacket: ' + msg);
}

let DNS_PACKET_SECTION_TYPES = [
  'QD', // Question
  'AN', // Answer
  'NS', // Authority
  'AR'  // Additional
];

/**
 * DNS Packet Structure
 * *************************************************
 *
 * Header
 * ======
 *
 * 00                   2-Bytes                   15
 * -------------------------------------------------
 * |00|01|02|03|04|05|06|07|08|09|10|11|12|13|14|15|
 * -------------------------------------------------
 * |<==================== ID =====================>|
 * |QR|<== OP ===>|AA|TC|RD|RA|UN|AD|CD|<== RC ===>|
 * |<================== QDCOUNT ==================>|
 * |<================== ANCOUNT ==================>|
 * |<================== NSCOUNT ==================>|
 * |<================== ARCOUNT ==================>|
 * -------------------------------------------------
 *
 * ID:        2-Bytes
 * FLAGS:     2-Bytes
 *  - QR:     1-Bit
 *  - OP:     4-Bits
 *  - AA:     1-Bit
 *  - TC:     1-Bit
 *  - RD:     1-Bit
 *  - RA:     1-Bit
 *  - UN:     1-Bit
 *  - AD:     1-Bit
 *  - CD:     1-Bit
 *  - RC:     4-Bits
 * QDCOUNT:   2-Bytes
 * ANCOUNT:   2-Bytes
 * NSCOUNT:   2-Bytes
 * ARCOUNT:   2-Bytes
 *
 *
 * Data
 * ====
 *
 * 00                   2-Bytes                   15
 * -------------------------------------------------
 * |00|01|02|03|04|05|06|07|08|09|10|11|12|13|14|15|
 * -------------------------------------------------
 * |<???=============== QD[...] ===============???>|
 * |<???=============== AN[...] ===============???>|
 * |<???=============== NS[...] ===============???>|
 * |<???=============== AR[...] ===============???>|
 * -------------------------------------------------
 *
 * QD:        ??-Bytes
 * AN:        ??-Bytes
 * NS:        ??-Bytes
 * AR:        ??-Bytes
 *
 *
 * Question Record
 * ===============
 *
 * 00                   2-Bytes                   15
 * -------------------------------------------------
 * |00|01|02|03|04|05|06|07|08|09|10|11|12|13|14|15|
 * -------------------------------------------------
 * |<???================ NAME =================???>|
 * |<=================== TYPE ====================>|
 * |<=================== CLASS ===================>|
 * -------------------------------------------------
 *
 * NAME:      ??-Bytes
 * TYPE:      2-Bytes
 * CLASS:     2-Bytes
 *
 *
 * Resource Record
 * ===============
 *
 * 00                   4-Bytes                   31
 * -------------------------------------------------
 * |00|02|04|06|08|10|12|14|16|18|20|22|24|26|28|30|
 * -------------------------------------------------
 * |<???================ NAME =================???>|
 * |<======= TYPE ========>|<======= CLASS =======>|
 * |<==================== TTL ====================>|
 * |<====== DATALEN ======>|<???==== DATA =====???>|
 * -------------------------------------------------
 *
 * NAME:      ??-Bytes
 * TYPE:      2-Bytes
 * CLASS:     2-Bytes
 * DATALEN:   2-Bytes
 * DATA:      ??-Bytes (Specified By DATALEN)
 */
class DNSPacket {
  constructor() {
    this._flags = _valueToFlags(0x0000);
    this._records = {};

    DNS_PACKET_SECTION_TYPES.forEach((sectionType) => {
      this._records[sectionType] = [];
    });
  }

  static parse(data) {
    let reader = new DataReader(data);
    if (reader.getValue(2) !== 0x0000) {
      throw new Error('Packet must start with 0x0000');
    }

    let packet = new DNSPacket();
    packet._flags = _valueToFlags(reader.getValue(2));

    let recordCounts = {};

    // Parse the record counts.
    DNS_PACKET_SECTION_TYPES.forEach((sectionType) => {
      recordCounts[sectionType] = reader.getValue(2);
    });

    // Parse the actual records.
    DNS_PACKET_SECTION_TYPES.forEach((sectionType) => {
      let recordCount = recordCounts[sectionType];
      for (let i = 0; i < recordCount; i++) {
        if (sectionType === 'QD') {
          packet.addRecord(sectionType,
              DNSRecord.parseFromPacketReader(reader));
        }

        else {
          packet.addRecord(sectionType,
              DNSResourceRecord.parseFromPacketReader(reader));
        }
      }
    });

    if (!reader.eof) {
      DEBUG && debug('Did not complete parsing packet data');
    }

    return packet;
  }

  getFlag(flag) {
    return this._flags[flag];
  }

  setFlag(flag, value) {
    this._flags[flag] = value;
  }

  addRecord(sectionType, record) {
    this._records[sectionType].push(record);
  }

  getRecords(sectionTypes, recordType) {
    let records = [];

    sectionTypes.forEach((sectionType) => {
      records = records.concat(this._records[sectionType]);
    });

    if (!recordType) {
      return records;
    }

    return records.filter(r => r.recordType === recordType);
  }

  serialize() {
    let writer = new DataWriter();

    // Write leading 0x0000 (2 bytes)
    writer.putValue(0x0000, 2);

    // Write `flags` (2 bytes)
    writer.putValue(_flagsToValue(this._flags), 2);

    // Write lengths of record sections (2 bytes each)
    DNS_PACKET_SECTION_TYPES.forEach((sectionType) => {
      writer.putValue(this._records[sectionType].length, 2);
    });

    // Write records
    DNS_PACKET_SECTION_TYPES.forEach((sectionType) => {
      this._records[sectionType].forEach((record) => {
        writer.putBytes(record.serialize());
      });
    });

    return writer.data;
  }

  toJSON() {
    return JSON.stringify(this.toJSONObject());
  }

  toJSONObject() {
    let result = {flags: this._flags};
    DNS_PACKET_SECTION_TYPES.forEach((sectionType) => {
      result[sectionType] = [];

      let records = this._records[sectionType];
      records.forEach((record) => {
        result[sectionType].push(record.toJSONObject());
      });
    });

    return result;
  }
}

/**
 * @private
 */
function _valueToFlags(value) {
  return {
    QR: (value & 0x8000) >> 15,
    OP: (value & 0x7800) >> 11,
    AA: (value & 0x0400) >> 10,
    TC: (value & 0x0200) >>  9,
    RD: (value & 0x0100) >>  8,
    RA: (value & 0x0080) >>  7,
    UN: (value & 0x0040) >>  6,
    AD: (value & 0x0020) >>  5,
    CD: (value & 0x0010) >>  4,
    RC: (value & 0x000f) >>  0
  };
}

/**
 * @private
 */
function _flagsToValue(flags) {
  let value = 0x0000;

  value += flags.QR & 0x01;

  value <<= 4;
  value += flags.OP & 0x0f;

  value <<= 1;
  value += flags.AA & 0x01;

  value <<= 1;
  value += flags.TC & 0x01;

  value <<= 1;
  value += flags.RD & 0x01;

  value <<= 1;
  value += flags.RA & 0x01;

  value <<= 1;
  value += flags.UN & 0x01;

  value <<= 1;
  value += flags.AD & 0x01;

  value <<= 1;
  value += flags.CD & 0x01;

  value <<= 4;
  value += flags.RC & 0x0f;

  return value;
}
