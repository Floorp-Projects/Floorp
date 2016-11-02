/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["WebRequestUpload"];

/* exported WebRequestUpload */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

var WebRequestUpload;

function rewind(stream) {
  try {
    if (stream instanceof Ci.nsISeekableStream) {
      stream.seek(0, 0);
    }
  } catch (e) {
    // It might be already closed, e.g. because of a previous error.
  }
}

function parseFormData(stream, channel, lenient = false) {
  const BUFFER_SIZE = 8192; // Empirically it seemed a good compromise.

  let mimeStream = null;

  if (stream instanceof Ci.nsIMIMEInputStream && stream.data) {
    mimeStream = stream;
    stream = stream.data;
  }
  let multiplexStream = null;
  if (stream instanceof Ci.nsIMultiplexInputStream) {
    multiplexStream = stream;
  }

  let touchedStreams = new Set();

  function createTextStream(stream) {
    let textStream = Cc["@mozilla.org/intl/converter-input-stream;1"].createInstance(Ci.nsIConverterInputStream);
    textStream.init(stream, "UTF-8", 0, lenient ? textStream.DEFAULT_REPLACEMENT_CHARACTER : 0);
    if (stream instanceof Ci.nsISeekableStream) {
      touchedStreams.add(stream);
    }
    return textStream;
  }

  let streamIdx = 0;
  function nextTextStream() {
    for (; streamIdx < multiplexStream.count;) {
      let currentStream = multiplexStream.getStream(streamIdx++);
      if (currentStream instanceof Ci.nsIStringInputStream) {
        touchedStreams.add(multiplexStream);
        return createTextStream(currentStream);
      }
    }
    return null;
  }

  let textStream;
  if (multiplexStream) {
    textStream = nextTextStream();
  } else {
    textStream = createTextStream(mimeStream || stream);
  }

  if (!textStream) {
    return null;
  }

  function readString() {
    if (textStream) {
      let textBuffer = {};
      textStream.readString(BUFFER_SIZE, textBuffer);
      return textBuffer.value;
    }
    return "";
  }

  function multiplexRead() {
    let str = readString();
    if (!str) {
      textStream = nextTextStream();
      if (textStream) {
        str = multiplexRead();
      }
    }
    return str;
  }

  let readChunk;
  if (multiplexStream) {
    readChunk = multiplexRead;
  } else {
    readChunk = readString;
  }

  function appendFormData(formData, name, value) {
    if (name in formData) {
      formData[name].push(value);
    } else {
      formData[name] = [value];
    }
  }

  function parseMultiPart(firstChunk, boundary = "") {
    let formData = Object.create(null);

    if (!boundary) {
      let match = firstChunk.match(/^--\S+/);
      if (!match) {
        return null;
      }
      boundary = match[0];
    }

    let unslash = (s) => s.replace(/\\"/g, '"');
    let tail = "";
    for (let chunk = firstChunk;
         chunk || tail;
         chunk = readChunk()) {
      let parts;
      if (chunk) {
        chunk = tail + chunk;
        parts = chunk.split(boundary);
        tail = parts.pop();
      } else {
        parts = [tail];
        tail = "";
      }

      for (let part of parts) {
        let match = part.match(/^\r\nContent-Disposition: form-data; name="(.*)"\r\n(?:Content-Type: (\S+))?.*\r\n/i);
        if (!match) {
          continue;
        }
        let [header, name, contentType] = match;
        if (contentType) {
          let fileName;
          // Since escaping inside Content-Disposition subfields is still poorly defined and buggy (see Bug 136676),
          // currently we always consider backslash-prefixed quotes as escaped even if that's not generally true
          // (i.e. in a field whose value actually ends with a backslash).
          // Therefore in this edge case we may end coalescing name and filename, which is marginally better than
          // potentially truncating the name field at the wrong point, at least from a XSS filter POV.
          match = name.match(/^(.*[^\\])"; filename="(.*)/);
          if (match) {
            [, name, fileName] = match;
          }
          appendFormData(formData, unslash(name), fileName ? unslash(fileName) : "");
        } else {
          appendFormData(formData, unslash(name), part.slice(header.length, -2));
        }
      }
    }

    return formData;
  }

  function parseUrlEncoded(firstChunk) {
    let formData = Object.create(null);

    let tail = "";
    for (let chunk = firstChunk;
         chunk || tail;
         chunk = readChunk()) {
      let pairs;
      if (chunk) {
        chunk = tail + chunk.trim();
        pairs = chunk.split("&");
        tail = pairs.pop();
      } else {
        chunk = tail;
        tail = "";
        pairs = [chunk];
      }
      for (let pair of pairs) {
        let [name, value] = pair.replace(/\+/g, " ").split("=").map(decodeURIComponent);
        appendFormData(formData, name, value);
      }
    }

    return formData;
  }

  try {
    let chunk = readChunk();

    if (multiplexStream) {
      touchedStreams.add(multiplexStream);
      return parseMultiPart(chunk);
    }
    let contentType;
    if (/^Content-Type:/i.test(chunk)) {
      contentType = chunk.replace(/^Content-Type:\s*/i, "");
      chunk = chunk.slice(chunk.indexOf("\r\n\r\n") + 4);
    } else {
      try {
        contentType = channel.getRequestHeader("Content-Type");
      } catch (e) {
        Cu.reportError(e);
        return null;
      }
    }

    let match = contentType.match(/^(?:multipart\/form-data;\s*boundary=(\S*)|application\/x-www-form-urlencoded\s)/i);
    if (match) {
      let boundary = match[1];
      if (boundary) {
        return parseMultiPart(chunk, boundary);
      }
      return parseUrlEncoded(chunk);
    }
  } finally {
    for (let stream of touchedStreams) {
      rewind(stream);
    }
  }

  return null;
}

function createFormData(stream, channel) {
  try {
    rewind(stream);
    return parseFormData(stream, channel);
  } catch (e) {
    Cu.reportError(e);
  } finally {
    rewind(stream);
  }
  return null;
}

function convertRawData(outerStream) {
  let raw = [];
  let totalBytes = 0;

  // Here we read the stream up to WebRequestUpload.MAX_RAW_BYTES, returning false if we had to truncate the result.
  function readAll(stream) {
    let unbuffered = stream.unbufferedStream || stream;
    if (unbuffered instanceof Ci.nsIFileInputStream) {
      raw.push({file: "<file>"}); // Full paths not supported yet for naked files (follow up bug)
      return true;
    }
    rewind(stream);

    let binaryStream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(Ci.nsIBinaryInputStream);
    binaryStream.setInputStream(stream);
    const MAX_BYTES = WebRequestUpload.MAX_RAW_BYTES;
    try {
      for (let available; (available = binaryStream.available());) {
        let size = Math.min(MAX_BYTES - totalBytes, available);
        let bytes = new ArrayBuffer(size);
        binaryStream.readArrayBuffer(size, bytes);
        let chunk = {bytes};
        raw.push(chunk);
        totalBytes += size;

        if (totalBytes >= MAX_BYTES) {
          if (size < available) {
            chunk.truncated = true;
            chunk.originalSize = available;
            return false;
          }
          break;
        }
      }
    } finally {
      rewind(stream);
    }
    return true;
  }

  let unbuffered = outerStream;
  if (outerStream instanceof Ci.nsIStreamBufferAccess) {
    unbuffered = outerStream.unbufferedStream;
  }

  if (unbuffered instanceof Ci.nsIMultiplexInputStream) {
    for (let i = 0, count = unbuffered.count; i < count; i++) {
      if (!readAll(unbuffered.getStream(i))) {
        break;
      }
    }
  } else {
    readAll(outerStream);
  }

  return raw;
}

WebRequestUpload = {
  createRequestBody(channel) {
    let requestBody = null;
    if (channel instanceof Ci.nsIUploadChannel && channel.uploadStream) {
      try {
        let stream = channel.uploadStream.QueryInterface(Ci.nsISeekableStream);
        let formData = createFormData(stream, channel);
        if (formData) {
          requestBody = {formData};
        } else {
          requestBody = {raw: convertRawData(stream), lenientFormData: createFormData(stream, channel, true)};
        }
      } catch (e) {
        Cu.reportError(e);
        requestBody = {error: e.message || String(e)};
      }
      requestBody = Object.freeze(requestBody);
    }
    return requestBody;
  },
};

XPCOMUtils.defineLazyPreferenceGetter(WebRequestUpload, "MAX_RAW_BYTES", "webextensions.webRequest.requestBodyMaxRawBytes");
