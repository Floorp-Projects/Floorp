/*
 * Copyright 2017, Leanplum, Inc. All rights reserved.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *        http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

package com.leanplum.internal;

/**
 * LeanplumManifestParser class for get AndroidManifest.xml. http://stackoverflow.com/questions/2097813/how-to-parse-the-androidmanifest-xml-file-inside-an-apk-package
 *
 * @author Anna Orlova
 */
class LeanplumManifestParser {
  // XML tags and attributes:
  // Every XML start and end tag consists of 6 32 bit words:
  //   0th word: 02011000 for START_TAG and 03011000 for END_TAG
  //   1st word: a flag?, like 38000000
  //   2nd word: Line of where this tag appeared in the original source file
  //   3rd word: FFFFFFFF ??
  //   4th word: StringIndex of NameSpace name, or FFFFFFFF for default NS
  //   5th word: StringIndex of Element Name
  //   (Note: 01011000 in 0th word means end of XML document, END_DOC_TAG).

  // Start tags (not end tags) contain 3 more words:
  //   6th word: 14001400 meaning??
  //   7th word: Number of Attributes that follow this tag(follow word 8th)
  //   8th word: 00000000 meaning??

  // Attributes consist of 5 words:
  //   0th word: StringIndex of Attribute Name's Namespace, or FFFFFFFF
  //   1st word: StringIndex of Attribute Name
  //   2nd word: StringIndex of Attribute Value, or FFFFFFF if ResourceId used
  //   3rd word: Flags?
  //   4th word: str ind of attr value again, or ResourceId of value.
  // END_DOC_TAG = 0x00100101;
  private static final int START_TAG = 0x00100102;
  private static final int END_TAG = 0x00100103;
  private static final String SPACES = "                                             ";

  /**
   * Parse the 'compressed' binary form of Android XML docs such as for AndroidManifest.xml in .apk
   * files.
   *
   * @param xml byte array of AndroidManifest.xml.
   * @return String with data of AndroidManifest.xml.
   */
  static String decompressXml(byte[] xml) {
    String out = "";
    // Compressed XML file/bytes starts with 24x bytes of data,
    // 9 32 bit words in little endian order (LSB first):
    //   0th word is 03 00 08 00
    //   3rd word SEEMS TO BE:  Offset at then of StringTable
    //   4th word is: Number of strings in string table
    // WARNING: Sometime I indiscriminately display or refer to word in
    //   little endian storage format, or in integer format (ie MSB first).
    int numbStrings = littleEndianValue(xml, 4 * 4);
    // StringIndexTable starts at offset 24x, an array of 32 bit LE offsets
    // of the length/string data in the StringTable.
    int sitOff = 0x24;  // Offset of start of StringIndexTable.
    // StringTable, each string is represented with a 16 bit little endian
    // character count, followed by that number of 16 bit (LE) (Unicode) chars.
    int stOff = sitOff + numbStrings * 4;  // StringTable follows StrIndexTable.
    // Step through the XML tree element tags and attributes.
    int off = scanForFirstStartTag(xml);
    int indent = 0;

    while (off < xml.length) {
      int tag0 = littleEndianValue(xml, off);
      int nameSi = littleEndianValue(xml, off + 5 * 4);
      if (tag0 == START_TAG) {
        int numbAttrs = littleEndianValue(xml, off + 7 * 4);  // Number of Attributes to follow.
        off += 9 * 4;  // Skip over 6+3 words of START_TAG data
        String name = compXmlString(xml, sitOff, stOff, nameSi);
        // Look for the Attributes
        StringBuilder sb = new StringBuilder();
        for (int ii = 0; ii < numbAttrs; ii++) {
          int attrNameSi = littleEndianValue(xml, off + 4);  // AttrName String Index.
          int attrValueSi = littleEndianValue(xml, off + 2 * 4); // AttrValue Str Ind, or FFFFFFFF.
          int attrResId = littleEndianValue(xml, off + 4 * 4);  // AttrValue ResourceId or dup.
          // AttrValue StrInd.
          off += 5 * 4;  // Skip over the 5 words of an attribute.
          String attrName = compXmlString(xml, sitOff, stOff, attrNameSi);
          String attrValue = attrValueSi != -1
              ? compXmlString(xml, sitOff, stOff, attrValueSi)
              : "resourceID 0x" + Integer.toHexString(attrResId);
          sb.append(" ").append(attrName).append("=\"").append(attrValue).append("\"");
        }
        out += SPACES.substring(0, Math.min(indent * 2, SPACES.length())) + "<" + name + sb + ">";
        indent++;
      } else if (tag0 == END_TAG) {
        indent--;
        off += 6 * 4;  // Skip over 6 words of END_TAG data
        String name = compXmlString(xml, sitOff, stOff, nameSi);
        out += SPACES.substring(0, Math.min(indent * 2, SPACES.length())) + "</" + name + ">";

      } else {
        break;
      }
    }
    return out;
  }

  private static String compXmlString(byte[] xml, int sitOff, int stOff, int strInd) {
    if (strInd < 0) return null;
    int strOff = stOff + littleEndianValue(xml, sitOff + strInd * 4);
    return compXmlStringAt(xml, strOff);
  }

  /**
   * @return Return the string stored in StringTable format at offset strOff.  This offset points to
   * the 16 bit string length, which is followed by that number of 16 bit (Unicode) chars.
   */
  private static String compXmlStringAt(byte[] arr, int strOff) {
    int strLen = arr[strOff + 1] << 8 & 0xff00 | arr[strOff] & 0xff;
    byte[] chars = new byte[strLen];
    for (int ii = 0; ii < strLen; ii++) {
      chars[ii] = arr[strOff + 2 + ii * 2];
    }
    return new String(chars);  // Hack, just use 8 byte chars.
  }

  /**
   * @return Return value of a Little Endian 32 bit word from the byte array at offset off.
   */
  private static int littleEndianValue(byte[] arr, int off) {
    return arr[off + 3] << 24 & 0xff000000 | arr[off + 2] << 16 & 0xff0000
        | arr[off + 1] << 8 & 0xff00 | arr[off] & 0xFF;
  }

  private static int scanForFirstStartTag(byte[] xml) {
    // XMLTags, The XML tag tree starts after some unknown content after the
    // StringTable.  There is some unknown data after the StringTable, scan forward
    // from this point to the flag for the start of an XML start tag.
    int xmlTagOff = littleEndianValue(xml, 3 * 4);  // Start from the offset in the 3rd word.
    // Scan forward until we find the bytes: 0x02011000(x00100102 in normal int).
    for (int ii = xmlTagOff; ii < xml.length - 4; ii += 4) {
      if (littleEndianValue(xml, ii) == START_TAG) {
        xmlTagOff = ii;
        break;
      }
    }
    return xmlTagOff;
  }
}
