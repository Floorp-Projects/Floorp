/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * A Class to parse CSV files
 */

const QUOTATION_MARK = '"';
const LINE_BREAKS = ["\r", "\n"];
const EOL = {};

class ParsingFailedException extends Error {
  constructor(message) {
    super(message ? message : `Stopped parsing because of wrong csv format`);
  }
}

export class CSV {
  /**
   * Parses a csv formated string into rows split into [headerLine, parsedLines].
   * The csv string format has to follow RFC 4180, otherwise the parsing process is stopped and a ParsingFailedException is thrown, e.g.:
   * (wrong format => right format):
   * 'abc"def' => 'abc""def'
   * abc,def => "abc,def"
   *
   * @param {string} text
   * @param {string} delimiter a comma for CSV files and a tab for TSV files
   * @returns {Array[]} headerLine: column names (first line of text), parsedLines: Array of Login Objects with column name as properties and login data as values.
   */
  static parse(text, delimiter) {
    let headerline = [];
    let parsedLines = [];

    for (let row of this.mapValuesToRows(this.readCSV(text, delimiter))) {
      if (!headerline.length) {
        headerline = row;
      } else {
        let login = {};
        row.forEach((attr, i) => (login[headerline[i]] = attr));
        parsedLines.push(login);
      }
    }
    return [headerline, parsedLines];
  }
  static *readCSV(text, delimiter) {
    function maySkipMultipleLineBreaks() {
      while (LINE_BREAKS.includes(text[current])) {
        current++;
      }
    }
    function readUntilSingleQuote() {
      const start = ++current;
      while (current < text.length) {
        if (text[current] === QUOTATION_MARK) {
          if (text[current + 1] !== QUOTATION_MARK) {
            const result = text.slice(start, current).replaceAll('""', '"');
            current++;
            return result;
          }
          current++;
        }
        current++;
      }
      throw new ParsingFailedException();
    }
    function readUntilDelimiterOrNewLine() {
      const start = current;
      while (current < text.length) {
        if (text[current] === delimiter) {
          const result = text.slice(start, current);
          current++;
          return result;
        } else if (LINE_BREAKS.includes(text[current])) {
          const result = text.slice(start, current);
          return result;
        }
        current++;
      }
      return text.slice(start);
    }
    let current = 0;
    maySkipMultipleLineBreaks();

    while (current < text.length) {
      if (LINE_BREAKS.includes(text[current])) {
        maySkipMultipleLineBreaks();
        yield EOL;
      }

      let quotedValue = "";
      let value = "";

      if (text[current] === QUOTATION_MARK) {
        quotedValue = readUntilSingleQuote();
      }

      value = readUntilDelimiterOrNewLine();

      if (quotedValue && value) {
        throw new ParsingFailedException();
      }

      yield quotedValue ? quotedValue : value;
    }
  }

  static *mapValuesToRows(values) {
    let row = [];
    for (const value of values) {
      if (value === EOL) {
        yield row;
        row = [];
      } else {
        row.push(value);
      }
    }
    if (!(row.length === 1 && row[0] === "") && row.length) {
      yield row;
    }
  }
}
