/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// A minimal ASN.1 DER decoder. Supports input lengths up to 65539 (one byte for
// the outer tag, one byte for the 0x82 length-length indicator, two bytes
// indicating a contents length of 65535, and then the 65535 bytes of contents).
// Intended to be used like so:
//
// let bytes = <an array of bytes describing a SEQUENCE OF INTEGER>;
// let der = new DER.DERDecoder(bytes);
// let contents = new DER.DERDecoder(der.readTagAndGetContents(DER.SEQUENCE));
// while (!contents.atEnd()) {
//   let integerBytes = contents.readTagAndGetContents(DER.INTEGER);
//   <... do something with integerBytes ...>
// }
// der.assertAtEnd();
//
// For CHOICE, use readTLVChoice and pass an array of acceptable tags.
// The convenience function readBIT_STRING is provided to handle the unused bits
// aspect of BIT STRING. It returns an object that has the properties contents
// (an array of bytes consisting of the bytes making up the BIT STRING) and
// unusedBits (indicating the number of unused bits at the end).
// All other functions generally return an array of bytes or a single byte as
// appropriate.
// peekTag can be used to see if the next tag is an expected given tag.
// readTLV reads and returns an entire (tag, length, value) tuple (again
// returned as an array of bytes).
//
// NB: While DERDecoder must be given an array, it does not validate that each
// element in the array is an integer in the range [0, 255]. If the input to be
// decoded could conceivably violate this property, callers should perform this
// check before using DERDecoder.

const UNIVERSAL = 0 << 6;
const CONSTRUCTED = 1 << 5;
const CONTEXT_SPECIFIC = 2 << 6;

const INTEGER = UNIVERSAL | 0x02; // 0x02
const BIT_STRING = UNIVERSAL | 0x03; // 0x03
const NULL = UNIVERSAL | 0x05; // 0x05
const OBJECT_IDENTIFIER = UNIVERSAL | 0x06; // 0x06
const PrintableString = UNIVERSAL | 0x13; // 0x13
const TeletexString = UNIVERSAL | 0x14; // 0x14
const IA5String = UNIVERSAL | 0x16; // 0x16
const UTCTime = UNIVERSAL | 0x17; // 0x17
const GeneralizedTime = UNIVERSAL | 0x18; // 0x18
const UTF8String = UNIVERSAL | 0x0c; // 0x0c
const SEQUENCE = UNIVERSAL | CONSTRUCTED | 0x10; // 0x30
const SET = UNIVERSAL | CONSTRUCTED | 0x11; // 0x31

const ERROR_INVALID_INPUT = "invalid input";
const ERROR_DATA_TRUNCATED = "data truncated";
const ERROR_EXTRA_DATA = "extra data";
const ERROR_INVALID_LENGTH = "invalid length";
const ERROR_UNSUPPORTED_ASN1 = "unsupported asn.1";
const ERROR_UNSUPPORTED_LENGTH = "unsupported length";
const ERROR_INVALID_BIT_STRING = "invalid BIT STRING encoding";

/** Class representing a decoded BIT STRING. */
class BitString {
  /**
   * @param {number} unusedBits the number of unused bits
   * @param {number[]} contents an array of bytes comprising the BIT STRING
   */
  constructor(unusedBits, contents) {
    this._unusedBits = unusedBits;
    this._contents = contents;
  }

  /**
   * Get the number of unused bits in the BIT STRING
   *
   * @returns {number} the number of unused bits
   */
  get unusedBits() {
    return this._unusedBits;
  }

  /**
   * Get the contents of the BIT STRING
   *
   * @returns {number[]} an array of bytes representing the contents
   */
  get contents() {
    return this._contents;
  }
}

/** Class representing DER-encoded data. Provides methods for decoding it. */
class DERDecoder {
  /**
   * @param {number[]} bytes an array of bytes representing the encoded data
   */
  constructor(bytes) {
    // Reject non-array inputs.
    if (!Array.isArray(bytes)) {
      throw new Error(ERROR_INVALID_INPUT);
    }
    if (bytes.length > 65539) {
      throw new Error(ERROR_UNSUPPORTED_LENGTH);
    }
    this._bytes = bytes;
    this._cursor = 0;
  }

  /**
   * Asserts that the decoder is at the end of the given data. Throws an error
   * if this is not the case.
   */
  assertAtEnd() {
    if (!this.atEnd()) {
      throw new Error(ERROR_EXTRA_DATA);
    }
  }

  /**
   * Determines whether or not the decoder is at the end of the given data.
   *
   * @returns {boolean} true if the decoder is at the end and false otherwise
   */
  atEnd() {
    return this._cursor == this._bytes.length;
  }

  /**
   * Reads the next byte of data. Throws if no more data is available.
   *
   * @returns {number} the next byte of data
   */
  readByte() {
    if (this._cursor >= this._bytes.length) {
      throw new Error(ERROR_DATA_TRUNCATED);
    }
    let val = this._bytes[this._cursor];
    this._cursor++;
    return val;
  }

  /**
   * Given the next expected tag, reads and asserts that the next tag is in fact
   * the given tag.
   *
   * @param {number} expectedTag the expected next tag
   */
  _readExpectedTag(expectedTag) {
    let tag = this.readByte();
    if (tag != expectedTag) {
      throw new Error(`unexpected tag: found ${tag} instead of ${expectedTag}`);
    }
  }

  /**
   * Decodes and returns the length portion of an ASN.1 TLV tuple. Throws if the
   * length is incorrectly encoded or if it describes a length greater than
   * 65535 bytes. Indefinite-length encoding is not supported.
   *
   * @returns {number} the length of the value of the TLV tuple
   */
  _readLength() {
    let nextByte = this.readByte();
    if (nextByte < 0x80) {
      return nextByte;
    }
    if (nextByte == 0x80) {
      throw new Error(ERROR_UNSUPPORTED_ASN1);
    }
    if (nextByte == 0x81) {
      let length = this.readByte();
      if (length < 0x80) {
        throw new Error(ERROR_INVALID_LENGTH);
      }
      return length;
    }
    if (nextByte == 0x82) {
      let length1 = this.readByte();
      let length2 = this.readByte();
      let length = (length1 << 8) | length2;
      if (length < 256) {
        throw new Error(ERROR_INVALID_LENGTH);
      }
      return length;
    }
    throw new Error(ERROR_UNSUPPORTED_LENGTH);
  }

  /**
   * Reads <length> bytes of data if available. Throws if less than <length>
   * bytes are available.
   *
   * @param {number} length the number of bytes to read. Must be non-negative.
   * @returns {number[]} the next <length> bytes of data
   */
  readBytes(length) {
    if (length < 0) {
      throw new Error(ERROR_INVALID_LENGTH);
    }
    if (this._cursor + length > this._bytes.length) {
      throw new Error(ERROR_DATA_TRUNCATED);
    }
    let bytes = this._bytes.slice(this._cursor, this._cursor + length);
    this._cursor += length;
    return bytes;
  }

  /**
   * Given an expected next ASN.1 tag, ensures that that tag is next and returns
   * the contents of that tag. Throws if a different tag is encountered or if
   * the data is otherwise incorrectly encoded.
   *
   * @param {number} tag the next expected ASN.1 tag
   * @returns {number[]} the contents of the tag
   */
  readTagAndGetContents(tag) {
    this._readExpectedTag(tag);
    let length = this._readLength();
    return this.readBytes(length);
  }

  /**
   * Returns the next byte without advancing the decoder. Throws if no more data
   * is available.
   *
   * @returns {number} the next available byte
   */
  _peekByte() {
    if (this._cursor >= this._bytes.length) {
      throw new Error(ERROR_DATA_TRUNCATED);
    }
    return this._bytes[this._cursor];
  }

  /**
   * Given an expected tag, reads the next entire ASN.1 TLV tuple, asserting
   * that the tag matches.
   *
   * @param {number} tag the expected tag
   * @returns {number[]} an array of bytes representing the TLV tuple
   */
  _readExpectedTLV(tag) {
    let mark = this._cursor;
    this._readExpectedTag(tag);
    let length = this._readLength();
    // read the bytes so we know they're there (also to advance the cursor)
    this.readBytes(length);
    return this._bytes.slice(mark, this._cursor);
  }

  /**
   * Reads the next ASN.1 tag, length, and value and returns them as an array of
   * bytes.
   *
   * @returns {number[]} an array of bytes representing the next ASN.1 TLV
   */
  readTLV() {
    let nextTag = this._peekByte();
    return this._readExpectedTLV(nextTag);
  }

  /**
   * Convenience function for decoding a BIT STRING. Reads and returns the
   * contents of the expected next BIT STRING. Throws if the next TLV isn't a
   * BIT STRING or if the BIT STRING is incorrectly encoded.
   *
   * @returns {BitString} the next BIT STRING
   */
  readBIT_STRING() {
    let contents = this.readTagAndGetContents(BIT_STRING);
    if (contents.length < 1) {
      throw new Error(ERROR_INVALID_BIT_STRING);
    }
    let unusedBits = contents[0];
    if (unusedBits > 7) {
      throw new Error(ERROR_INVALID_BIT_STRING);
    }
    // Zero bytes of content but some amount of padding is invalid.
    if (contents.length == 1 && unusedBits != 0) {
      throw new Error(ERROR_INVALID_BIT_STRING);
    }
    return new BitString(unusedBits, contents.slice(1, contents.length));
  }

  /**
   * Looks to see if the next ASN.1 tag is the expected given tag.
   *
   * @param {number} tag the expected next ASN.1 tag
   * @returns {boolean} true if the next tag is the given one and false otherwise
   */
  peekTag(tag) {
    if (this._cursor >= this._bytes.length) {
      return false;
    }
    return this._bytes[this._cursor] == tag;
  }

  /**
   * Given a list of possible next ASN.1 tags, returns the next TLV if the next
   * tag is in the list. Throws if the next tag is not in the list or if the
   * data is incorrectly encoded.
   *
   * @param {number[]} tagList the list of potential next tags
   * @returns {number[]} the contents of the next TLV if the next tag is in
   *                    <tagList>
   */
  readTLVChoice(tagList) {
    let tag = this._peekByte();
    if (!tagList.includes(tag)) {
      throw new Error(
        `unexpected tag: found ${tag} instead of one of ${tagList}`
      );
    }
    return this._readExpectedTLV(tag);
  }
}

export const DER = {
  UNIVERSAL,
  CONSTRUCTED,
  CONTEXT_SPECIFIC,
  INTEGER,
  BIT_STRING,
  NULL,
  OBJECT_IDENTIFIER,
  PrintableString,
  TeletexString,
  IA5String,
  UTCTime,
  GeneralizedTime,
  UTF8String,
  SEQUENCE,
  SET,
  DERDecoder,
};
