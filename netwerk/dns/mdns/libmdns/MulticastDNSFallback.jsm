/* -*- Mode: js; js-indent-level: 2; indent-tabs-mode: nil; tab-width: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint esnext: true, moz: true */

'use strict';

this.EXPORTED_SYMBOLS = ['MulticastDNS'];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

const MDNS_PORT = 5353;
const MDNS_ADDRESS = '224.0.0.251';

const DNS_REC_TYPE_PTR = 12;
const DNS_REC_TYPE_TXT = 16;
const DNS_REC_TYPE_SRV = 33;
const DNS_REC_TYPE_A   = 1;
const DNS_REC_TYPE_NSEC= 47;

const DNS_CLASS_QU = 0x8000;
const DNS_CLASS_IN = 0x0001;

const DNS_SECTION_QD = 'qd';
const DNS_SECTION_AN = 'an';
const DNS_SECTION_NS = 'ns';
const DNS_SECTION_AR = 'ar';

const DEBUG = false;

function debug(msg) {
  Services.console.logStringMessage('MulticastDNSFallback: ' + msg);
}

/* The following was taken from https://raw.githubusercontent.com/GoogleChrome/chrome-app-samples/master/mdns-browser/dns.js */

/**
 * DataWriter writes data to an ArrayBuffer, presenting it as the instance
 * variable 'buffer'.
 *
 * @constructor
 */
let DataWriter = function(opt_size) {
  let loc = 0;
  let view = new Uint8Array(new ArrayBuffer(opt_size || 512));

  this.byte_ = function(v) {
    view[loc] = v;
    ++loc;
    this.buffer = view.buffer.slice(0, loc);
  }.bind(this);
};

DataWriter.prototype.byte = function(v) {
  this.byte_(v);
  return this;
};

DataWriter.prototype.short = function(v) {
  return this.byte((v >> 8) & 0xff).byte(v & 0xff);
};

DataWriter.prototype.long = function(v) {
  return this.short((v >> 16) & 0xffff).short(v & 0xffff);
};

/**
 * Writes a DNS name. If opt_ref is specified, will finish this name with a
 * suffix reference (i.e., 0xc0 <ref>). If not, then will terminate with a NULL
 * byte.
 */
DataWriter.prototype.name = function(v, opt_ref) {
  let parts = v.split('.');
  parts.forEach(function(part) {
    this.byte(part.length);
    for (let i = 0; i < part.length; ++i) {
      this.byte(part.charCodeAt(i));
    }
  }.bind(this));
  if (opt_ref) {
    this.byte(0xc0).byte(opt_ref);
  } else {
    this.byte(0);
  }
  return this;
};

/**
 * DataConsumer consumes data from an ArrayBuffer.
 *
 * @constructor
 */
let DataConsumer = function(arg) {
  if (arg instanceof Uint8Array) {
    this.view_ = arg;
  } else {
    this.view_ = new Uint8Array(arg);
  }
  this.loc_ = 0;
};

/**
 * @return whether this DataConsumer has consumed all its data
 */
DataConsumer.prototype.isEOF = function() {
  return this.loc_ >= this.view_.byteLength;
};

/**
 * @param length {integer} number of bytes to return from the front of the view
 * @return a Uint8Array
 */
DataConsumer.prototype.slice = function(length) {
  let view = this.view_.subarray(this.loc_, this.loc_ + length);
  this.loc_ += length;
  return view;
};

DataConsumer.prototype.byte = function() {
  this.loc_ += 1;
  return this.view_[this.loc_ - 1];
};

DataConsumer.prototype.short = function() {
  return (this.byte() << 8) + this.byte();
};

DataConsumer.prototype.long = function() {
  return (this.short() << 16) + this.short();
};

/**
 * Consumes a DNS name, which will finish with a NULL byte.
 */
DataConsumer.prototype.name = function() {
  let parts = [];
  for (;;) {
    let len = this.byte();
    if (!len) {
      break;
    } else if (len == 0xc0) {
      // concat suffix reference (i.e., 0xc0 <ref>).
      let ref = this.byte();
      let refConsumer = new DataConsumer(new Uint8Array(this.view_.buffer, ref));
      parts.push(refConsumer.name());
      break;
    }

    // Otherwise, consume a string!
    let v = '';
    while (len-- > 0) {
      v += String.fromCharCode(this.byte());
    }
    parts.push(v);
  }
  return parts.join('.');
};

/**
 * Consumes a string according to length.
 */
DataConsumer.prototype.string = function() {
  let len = this.byte();
  if (!len) {
    return;
  }

  let v = '';
  while (len-- > 0) {
    v += String.fromCharCode(this.byte());
  }
  return v;
};

/**
 * DNSPacket holds the state of a DNS packet. It can be modified or serialized
 * in-place.
 *
 * @constructor
 */
let DNSPacket = function(opt_flags) {
  this.flags_ = opt_flags || 0; /* uint16 */
  this.data_ = {};
  this.data_[DNS_SECTION_QD] = [];
  this.data_[DNS_SECTION_AN] = [];
  this.data_[DNS_SECTION_NS] = [];
  this.data_[DNS_SECTION_AR] = [];
};

/**
 * Parse a DNSPacket from an ArrayBuffer (or Uint8Array).
 */
DNSPacket.parse = function(buffer) {
  let consumer = new DataConsumer(buffer);
  if (consumer.short()) {
    throw new Error('DNS packet must start with 00 00');
  }
  let flags = consumer.short();
  let count = {};
  count[DNS_SECTION_QD] = consumer.short();
  count[DNS_SECTION_AN] = consumer.short();
  count[DNS_SECTION_NS] = consumer.short();
  count[DNS_SECTION_AR] = consumer.short();

  let packet = new DNSPacket(flags);

  // Parse the QUESTION section.
  for (let i = 0; i < count[DNS_SECTION_QD]; ++i) {
    let part = new DNSRecord(
      consumer.name(),
      consumer.short(),  // type
      consumer.short()); // class
    packet.push(DNS_SECTION_QD, part);
  }

  // Parse the ANSWER, AUTHORITY and ADDITIONAL sections.
  [DNS_SECTION_AN, DNS_SECTION_NS, DNS_SECTION_AR].forEach(function(section) {
    for (let i = 0; i < count[section]; ++i) {
      let part = new DNSRecord(
        consumer.name(),
        consumer.short(),
        consumer.short(), // class
        consumer.long(),  // ttl
        consumer.slice(consumer.short()));
      packet.push(section, part);
    }
  });

  if (consumer.isEOF()) {
    DEBUG && debug('was not EOF on incoming packet');
  }
  return packet;
};

DNSPacket.prototype.push = function(section, record) {
  this.data_[section].push(record);
};

DNSPacket.prototype.each = function(section) {
  let filter = false;
  let call;
  if (arguments.length == 2) {
    call = arguments[1];
  } else {
    filter = arguments[1];
    call = arguments[2];
  }
  this.data_[section].forEach(function(rec) {
    if (!filter || rec.type == filter) {
      call(rec);
    }
  });
};

/**
 * Serialize this DNSPacket into an ArrayBuffer for sending over UDP.
 */
DNSPacket.prototype.serialize = function() {
  let out = new DataWriter();
  let s = [DNS_SECTION_QD, DNS_SECTION_AN, DNS_SECTION_NS, DNS_SECTION_AR];

  out.short(0).short(this.flags_);

  s.forEach(function(section) {
    out.short(this.data_[section].length);
  }.bind(this));

  s.forEach(function(section) {
    this.data_[section].forEach(function(rec) {
      out.name(rec.name).short(rec.type).short(rec.cl);

      if (section != DNS_SECTION_QD) {
        // TODO: implement .bytes()
        throw new Error('can\'t yet serialize non-QD records');
        //        out.long(rec.ttl).bytes(rec.data_);
      }
    });
  }.bind(this));

  return out.buffer;
};

/**
 * DNSRecord is a record inside a DNS packet; e.g. a QUESTION, or an ANSWER,
 * AUTHORITY, or ADDITIONAL record. Note that QUESTION records are special,
 * and do not have ttl or data.
 */
let DNSRecord = function(name, type, cl, opt_ttl, opt_data) {
  this.name = name;
  this.type = type;
  this.cl = cl;

  this.isQD = (arguments.length == 3);
  if (!this.isQD) {
    this.ttl = opt_ttl;
    this.data_ = opt_data;
  }
};

DNSRecord.prototype.asName = function() {
  return new DataConsumer(this.data_).name();
};

DNSRecord.prototype.asSRV = function() {
  if (this.type !== DNS_REC_TYPE_SRV) {
    return null;
  }

  let consumer = new DataConsumer(this.data_);
  let data_length = this.data_.length;
  return {
    priority: consumer.short(),
    weight: consumer.short(),
    port: consumer.short(),
    target: consumer.name() + consumer.name(),
  };
};

DNSRecord.prototype.asTXT = function() {
  if (this.type !== DNS_REC_TYPE_TXT) {
    return null;
  }

  let consumer = new DataConsumer(this.data_);
  let attributes = [];
  while(!consumer.isEOF()) {
    attributes.push(consumer.string());
  }

  return attributes;
};

/* end https://raw.githubusercontent.com/GoogleChrome/chrome-app-samples/master/mdns-browser/dns.js */

/**
 * Parse fully qualified domain name to service name, instance name,
 * and domain name. See https://tools.ietf.org/html/rfc6763#section-7.
 *
 * example: The Server._http._tcp.example.com
 *   instance name = "The Server"
 *   service type = "_http._tcp"
 *   domain = "example.com"
 * @private
 */
function _parseDomainName(str) {
  let items = str.split('.');
  let idx = items.findIndex(function(element) {
    return element === '_tcp' || element === '_udp';
  });

  return {
    instanceName: items.splice(0, idx - 1).join('.'),
    serviceType: items.splice(0, 2). join('.'),
    domainName: items.join('.')
  };
}

function _createPropertyBag(map) {
  let bag = Cc['@mozilla.org/hash-property-bag;1']
              .createInstance(Ci.nsIWritablePropertyBag);

  for (let entry of map.entries()) {
    bag.setProperty(entry[0], entry[1]);
  }

  return bag;
}

let MulticastDNS = function() {
  this._targets = new Map();
};

MulticastDNS.prototype = {
  socket: null,
  //public API
  startDiscovery: function(aServiceType, aListener) {
    DEBUG && debug('startDiscovery for ' + aServiceType);
    let { serviceType } = _parseDomainName(aServiceType);
    this._addServiceListener(serviceType, aListener);

    try {
      this._ensureSocket();
      this._query(serviceType + '.local', DNS_REC_TYPE_PTR);
      aListener.onDiscoveryStarted(serviceType);
    } catch (e) {
      DEBUG && debug('onStartDiscoveryFailed: ' + serviceType + ' (' + e + ')');
      this._removeServiceListener(serviceType, aListener);
      aListener.onStartDiscoveryFailed(serviceType, Cr.NS_ERROR_FAILURE);
    }
  },

  stopDiscovery: function(aServiceType, aListener) {
    DEBUG && debug('stopDiscovery for ' + aServiceType);
    let { serviceType } = _parseDomainName(aServiceType);
    this._removeServiceListener(serviceType, aListener);

    aListener.onDiscoveryStopped(serviceType);
    if (this._targets.size === 0) {
      DEBUG && debug('close current socket');
      this.socket.close();
      delete this.socket;
    }
  },

  registerService: function(aServiceInfo, aListener) {
    DEBUG && debug('service registration is not supported');
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  unregisterService: function(aServiceInfo, aListener) {
    DEBUG && debug('service registration is not supported');
    throw Cr.NS_ERROR_NOT_IMPLEMENTED;
  },

  resolveService: function(aServiceInfo, aListener) {
    DEBUG && debug('address info is already resolve during discovery phase');
    aListener.onServiceResolved(aServiceInfo);
  },

  //private API
  onReceive: function(info) {
    let packet = DNSPacket.parse(info.rawData);
    let serviceRecords = {};

    packet.each(DNS_SECTION_AR, DNS_REC_TYPE_SRV, (rec) => {
      DEBUG && debug('recieve SRV: ' + rec.name);
      let srv = rec.asSRV();
      serviceRecords[rec.name] = {
        port: srv.port,
        host: srv.target,
      };
    });

    packet.each(DNS_SECTION_AR, DNS_REC_TYPE_TXT, (rec) => {
      DEBUG && debug('recieve TXT: ' + rec.name);
      if (!serviceRecords[rec.name]) {
        return;
      }

      let txt =  rec.asTXT();
      let attributes = new Map();
      for(let x in txt) {
        let idx = x.indexOf('=');
        if (idx < 0) {
          attributes.set(txt[x], true);
          continue;
        }

        let key = txt[x].substring(0, idx);
        let value = txt[x].substring(idx + 1);
        attributes.set(key, value);
      }
      serviceRecords[rec.name].attributes = attributes;
    });

    packet.each(DNS_SECTION_AN, DNS_REC_TYPE_PTR, (rec) => {
      DEBUG && debug('recieve PTR: ' + rec.name);
      let { serviceType: answerType } = _parseDomainName(rec.name);
      if (this._targets.has(answerType)) {
        let name = rec.asName();
        DEBUG && debug('>> for ' + name);
        let {instanceName, serviceType, domainName} = _parseDomainName(name);
        let serviceInfo = {
          host: serviceRecords[name].host,
          address: info.fromAddr.address,
          port: serviceRecords[name].port,
          serviceType: serviceType,
          serviceName: instanceName,
          domainName: domainName,
          attributes: _createPropertyBag(serviceRecords[name].attributes),
          QueryInterface: XPCOMUtils.generateQI([Ci.nsIDNSServiceInfo]),
        };
        this._targets.get(serviceType).forEach(function(listener) {
          listener.onServiceFound(serviceInfo);
        });
      }
    });
  },

  /**
   * Handles network error occured while waiting for data.
   * @private
   */
  onReceiveError: function(socket, status) {
    DEBUG && debug('receiver socket error' + status);
    return true;
  },

  /**
   * Broadcasts for services on the given socket/address.
   * @private
   */
  _query: function(search, type) {
    DEBUG && debug('query for service: ' + search);
    let packet = new DNSPacket();
    packet.push(DNS_SECTION_QD, new DNSRecord(search, type, DNS_CLASS_IN | DNS_CLASS_QU));

    this._broadcast(this.socket, packet);
  },

  /**
   * Broadcasts a MDNS packet on the given socket/address.
   * @private
   */
  _broadcast: function(sock, packet) {
    let raw = new DataView(packet.serialize());
    let length =  raw.byteLength;
    let buf = [];
    for (let x = 0; x < length; x++) {
      let charcode = raw.getUint8(x);
      buf[x] = charcode;
    }
    sock.send(MDNS_ADDRESS, MDNS_PORT, buf, buf.length);
  },

  _addServiceListener: function(serviceType, listener) {
    let listeners = this._targets.get(serviceType);
    if (!listeners) {
      listeners = [];
      this._targets.set(serviceType, listeners);
    }

    if (!listeners.find((element) => {
          return element === listener;
        })) {
      DEBUG && debug('insert new listener');
      listeners.push(listener);
    }
  },

  _removeServiceListener: function(serviceType, listener) {
    if (!this._targets.has(serviceType)) {
      DEBUG && debug('listener doesnt exist');
      return;
    }

    let listeners = this._targets.get(serviceType);
    let idx = listeners.findIndex(function(element) {
      return element === listener;
    });

    if (idx >= 0) {
      listeners.splice(idx, 1);
    }

    if (listeners.length === 0) {
      this._targets.delete(serviceType);
    }
  },

  _ensureSocket: function() {
    if (this.socket) {
      DEBUG && debug('reuse current socket');
      return;
    }

    this.socket = Cc['@mozilla.org/network/udp-socket;1']
                    .createInstance(Ci.nsIUDPSocket);
    let self = this;
    this.socket.init(MDNS_PORT, false,
                     Services.scriptSecurityManager.getSystemPrincipal());
    this.socket.asyncListen({
      onPacketReceived: function(aSocket, aMessage) {
        self.onReceive(aMessage);
      },

      onStopListening: function(aSocket, aStatus) {
        self.onReceiveError(aSocket, aStatus);
      },
    });
    this.socket.joinMulticast(MDNS_ADDRESS);
  },
};
