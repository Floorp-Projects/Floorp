/* -*- Mode: js; js-indent-level: 2; indent-tabs-mode: nil; tab-width: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
/* jshint esnext: true, moz: true */

'use strict';

this.EXPORTED_SYMBOLS = ['MulticastDNS'];

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import('resource://gre/modules/Services.jsm');
Cu.import('resource://gre/modules/Timer.jsm');
Cu.import('resource://gre/modules/XPCOMUtils.jsm');

Cu.import('resource://gre/modules/DNSPacket.jsm');
Cu.import('resource://gre/modules/DNSRecord.jsm');
Cu.import('resource://gre/modules/DNSResourceRecord.jsm');
Cu.import('resource://gre/modules/DNSTypes.jsm');

const NS_NETWORK_LINK_TOPIC = 'network:link-status-changed';

let observerService     = Cc["@mozilla.org/observer-service;1"]
                            .getService(Components.interfaces.nsIObserverService);
let networkInfoService  = Cc['@mozilla.org/network-info-service;1']
                            .createInstance(Ci.nsINetworkInfoService);

const DEBUG = true;

const MDNS_MULTICAST_GROUP = '224.0.0.251';
const MDNS_PORT            = 5353;
const DEFAULT_TTL          = 120;

function debug(msg) {
  dump('MulticastDNS: ' + msg + '\n');
}

function ServiceKey(svc) {
  return "" + svc.serviceType.length + "/" + svc.serviceType + "|" +
              svc.serviceName.length + "/" + svc.serviceName + "|" +
              svc.port;
}

function TryGet(obj, name) {
  try {
    return obj[name];
  } catch (err) {
    return undefined;
  }
}

function IsIpv4Address(addr) {
  let parts = addr.split('.');
  if (parts.length != 4) {
    return false;
  }
  for (let part of parts) {
    let partInt = Number.parseInt(part, 10);
    if (partInt.toString() != part) {
      return false;
    }
    if (partInt < 0 || partInt >= 256) {
      return false;
    }
  }
  return true;
}

class PublishedService {
  constructor(attrs) {
    this.serviceType = attrs.serviceType.replace(/\.$/, '');
    this.serviceName = attrs.serviceName;
    this.domainName = TryGet(attrs, 'domainName') || "local";
    this.address = TryGet(attrs, 'address') || "0.0.0.0";
    this.port = attrs.port;
    this.serviceAttrs = _propertyBagToObject(TryGet(attrs, 'attributes') || {});
    this.host = TryGet(attrs, 'host');
    this.key = this.generateKey();
    this.lastAdvertised = undefined;
    this.advertiseTimer = undefined;
  }

  equals(svc) {
    return (this.port == svc.port) &&
           (this.serviceName == svc.serviceName) &&
           (this.serviceType == svc.serviceType);
  }

  generateKey() {
    return ServiceKey(this);
  }

  ptrMatch(name) {
    return name == (this.serviceType + "." + this.domainName);
  }

  clearAdvertiseTimer() {
    if (!this.advertiseTimer) {
      return;
    }
    clearTimeout(this.advertiseTimer);
    this.advertiseTimer = undefined;
  }
}

class MulticastDNS {
  constructor() {
    this._listeners       = new Map();
    this._sockets         = new Map();
    this._services        = new Map();
    this._discovered      = new Map();
    this._querySocket     = undefined;
    this._broadcastReceiverSocket = undefined;
    this._broadcastTimer  = undefined;

    this._networkLinkObserver = {
      observe: (subject, topic, data) => {
        DEBUG && debug(NS_NETWORK_LINK_TOPIC + '(' + data + '); Clearing list of previously discovered services');
        this._discovered.clear();
      }
    };
  }

  _attachNetworkLinkObserver() {
    if (this._networkLinkObserverTimeout) {
      clearTimeout(this._networkLinkObserverTimeout);
    }

    if (!this._isNetworkLinkObserverAttached) {
      DEBUG && debug('Attaching observer ' + NS_NETWORK_LINK_TOPIC);
      observerService.addObserver(this._networkLinkObserver, NS_NETWORK_LINK_TOPIC, false);
      this._isNetworkLinkObserverAttached = true;
    }
  }

  _detachNetworkLinkObserver() {
    if (this._isNetworkLinkObserverAttached) {
      if (this._networkLinkObserverTimeout) {
        clearTimeout(this._networkLinkObserverTimeout);
      }

      this._networkLinkObserverTimeout = setTimeout(() => {
        DEBUG && debug('Detaching observer ' + NS_NETWORK_LINK_TOPIC);
        observerService.removeObserver(this._networkLinkObserver, NS_NETWORK_LINK_TOPIC);
        this._isNetworkLinkObserverAttached = false;
        this._networkLinkObserverTimeout = null;
      }, 5000);
    }
  }

  startDiscovery(aServiceType, aListener) {
    DEBUG && debug('startDiscovery("' + aServiceType + '")');
    let { serviceType } = _parseServiceDomainName(aServiceType);

    this._attachNetworkLinkObserver();
    this._addServiceListener(serviceType, aListener);

    try {
      this._query(serviceType + '.local');
      aListener.onDiscoveryStarted(serviceType);
    } catch (e) {
      DEBUG && debug('startDiscovery("' + serviceType + '") FAILED: ' + e);
      this._removeServiceListener(serviceType, aListener);
      aListener.onStartDiscoveryFailed(serviceType, Cr.NS_ERROR_FAILURE);
    }
  }

  stopDiscovery(aServiceType, aListener) {
    DEBUG && debug('stopDiscovery("' + aServiceType + '")');
    let { serviceType } = _parseServiceDomainName(aServiceType);

    this._detachNetworkLinkObserver();
    this._removeServiceListener(serviceType, aListener);

    aListener.onDiscoveryStopped(serviceType);

    this._checkCloseSockets();
  }

  resolveService(aServiceInfo, aListener) {
    DEBUG && debug('resolveService(): ' + aServiceInfo.serviceName);

    // Address info is already resolved during discovery
    setTimeout(() => aListener.onServiceResolved(aServiceInfo));
  }

  registerService(aServiceInfo, aListener) {
    DEBUG && debug('registerService(): ' + aServiceInfo.serviceName);

    // Initialize the broadcast receiver socket in case it
    // hasn't already been started so we can listen for
    // multicast queries/announcements on all interfaces.
    this._getBroadcastReceiverSocket();

    for (let name of ['port', 'serviceName', 'serviceType']) {
      if (!TryGet(aServiceInfo, name)) {
        aListener.onRegistrationFailed(aServiceInfo, Cr.NS_ERROR_FAILURE);
        throw new Error('Invalid nsIDNSServiceInfo; Missing "' + name + '"');
      }
    }

    let publishedService;
    try {
      publishedService = new PublishedService(aServiceInfo);
    } catch (e) {
      DEBUG && debug("Error constructing PublishedService: " + e + " - " + e.stack);
      setTimeout(() => aListener.onRegistrationFailed(aServiceInfo, Cr.NS_ERROR_FAILURE));
      return;
    }

    // Ensure such a service does not already exist.
    if (this._services.get(publishedService.key)) {
      setTimeout(() => aListener.onRegistrationFailed(aServiceInfo, Cr.NS_ERROR_FAILURE));
      return;
    }

    // Make sure that the service addr is '0.0.0.0', or there is at least one
    // socket open on the address the service is open on.
    this._getSockets().then((sockets) => {
      if (publishedService.address != '0.0.0.0' && !sockets.get(publishedService.address)) {
        setTimeout(() => aListener.onRegistrationFailed(aServiceInfo, Cr.NS_ERROR_FAILURE));
        return;
      }

      this._services.set(publishedService.key, publishedService);

      // Service registered.. call onServiceRegistered on next tick.
      setTimeout(() => aListener.onServiceRegistered(aServiceInfo));

      // Set a timeout to start advertising the service too.
      publishedService.advertiseTimer = setTimeout(() => {
        this._advertiseService(publishedService.key, /* firstAdv = */ true);
      });
    });
  }

  unregisterService(aServiceInfo, aListener) {
    DEBUG && debug('unregisterService(): ' + aServiceInfo.serviceName);

    let serviceKey;
    try {
      serviceKey = ServiceKey(aServiceInfo);
    } catch (e) {
      setTimeout(() => aListener.onUnregistrationFailed(aServiceInfo, Cr.NS_ERROR_FAILURE));
      return;
    }

    let publishedService = this._services.get(serviceKey);
    if (!publishedService) {
      setTimeout(() => aListener.onUnregistrationFailed(aServiceInfo, Cr.NS_ERROR_FAILURE));
      return;
    }

    // Clear any advertise timeout for this published service.
    publishedService.clearAdvertiseTimer();

    // Delete the service from the service map.
    if (!this._services.delete(serviceKey)) {
      setTimeout(() => aListener.onUnregistrationFailed(aServiceInfo, Cr.NS_ERROR_FAILURE));
      return;
    }

    // Check the broadcast timer again to rejig when it should run next.
    this._checkStartBroadcastTimer();

    // Check to see if sockets should be closed, and if so close them.
    this._checkCloseSockets();

    aListener.onServiceUnregistered(aServiceInfo);
  }

  _respondToQuery(serviceKey, message) {
    let address = message.fromAddr.address;
    let port = message.fromAddr.port;
    DEBUG && debug('_respondToQuery(): key=' + serviceKey + ', fromAddr='
                        + address + ":" + port);

    let publishedService = this._services.get(serviceKey);
    if (!publishedService) {
      debug("_respondToQuery Could not find service (key=" + serviceKey + ")");
      return;
    }

    DEBUG && debug('_respondToQuery(): key=' + serviceKey + ': SENDING RESPONSE');
    this._advertiseServiceHelper(publishedService, {address,port});
  }

  _advertiseService(serviceKey, firstAdv) {
    DEBUG && debug('_advertiseService(): key=' + serviceKey);
    let publishedService = this._services.get(serviceKey);
    if (!publishedService) {
      debug("_advertiseService Could not find service to advertise (key=" + serviceKey + ")");
      return;
    }

    publishedService.advertiseTimer = undefined;

    this._advertiseServiceHelper(publishedService, null).then(() => {
      // If first advertisement, re-advertise in 1 second.
      // Otherwise, set the lastAdvertised time.
      if (firstAdv) {
        publishedService.advertiseTimer = setTimeout(() => {
          this._advertiseService(serviceKey)
        }, 1000);
      } else {
        publishedService.lastAdvertised = Date.now();
        this._checkStartBroadcastTimer();
      }
    });
  }

  _advertiseServiceHelper(svc, target) {
    if (!target) {
      target = {address:MDNS_MULTICAST_GROUP, port:MDNS_PORT};
    }

    return this._getSockets().then((sockets) => {
      sockets.forEach((socket, address) => {
        if (svc.address == "0.0.0.0" || address == svc.address)
        {
          let packet = this._makeServicePacket(svc, [address]);
          let data = packet.serialize();
          try {
            socket.send(target.address, target.port, data, data.length);
          } catch (err) {
            DEBUG && debug("Failed to send packet to "
                            + target.address + ":" + target.port);
          }
        }
      });
    });
  }

  _cancelBroadcastTimer() {
    if (!this._broadcastTimer) {
      return;
    }
    clearTimeout(this._broadcastTimer);
    this._broadcastTimer = undefined;
  }

  _checkStartBroadcastTimer() {
    DEBUG && debug("_checkStartBroadcastTimer()");
    // Cancel any existing broadcasting timer.
    this._cancelBroadcastTimer();

    let now = Date.now();

    // Go through services and find services to broadcast.
    let bcastServices = [];
    let nextBcastWait = undefined;
    for (let [serviceKey, publishedService] of this._services) {
      // if lastAdvertised is undefined, service hasn't finished it's initial
      // two broadcasts.
      if (publishedService.lastAdvertised === undefined) {
        continue;
      }

      // Otherwise, check lastAdvertised against now.
      let msSinceAdv = now - publishedService.lastAdvertised;

      // If msSinceAdv is more than 90% of the way to the TTL, advertise now.
      if (msSinceAdv > (DEFAULT_TTL * 1000 * 0.9)) {
        bcastServices.push(publishedService);
        continue;
      }

      // Otherwise, calculate the next time to advertise for this service.
      // We set that at 95% of the time to the TTL expiry.
      let nextAdvWait = (DEFAULT_TTL * 1000 * 0.95) - msSinceAdv;
      if (nextBcastWait === undefined || nextBcastWait > nextAdvWait) {
        nextBcastWait = nextAdvWait;
      }
    }

    // Schedule an immediate advertisement of all services to be advertised now.
    for (let svc of bcastServices) {
        svc.advertiseTimer = setTimeout(() => this._advertiseService(svc.key));
    }

    // Schedule next broadcast check for the next bcast time.
    if (nextBcastWait !== undefined) {
      DEBUG && debug("_checkStartBroadcastTimer(): Scheduling next check in " + nextBcastWait + "ms");
      this._broadcastTimer = setTimeout(() => this._checkStartBroadcastTimer(), nextBcastWait);
    }
  }

  _query(name) {
    DEBUG && debug('query("' + name + '")');
    let packet = new DNSPacket();
    packet.setFlag('QR', DNS_QUERY_RESPONSE_CODES.QUERY);

    // PTR Record
    packet.addRecord('QD', new DNSRecord({
      name: name,
      recordType: DNS_RECORD_TYPES.PTR,
      classCode: DNS_CLASS_CODES.IN,
      cacheFlush: true
    }));

    let data = packet.serialize();

    // Initialize the broadcast receiver socket in case it
    // hasn't already been started so we can listen for
    // multicast queries/announcements on all interfaces.
    this._getBroadcastReceiverSocket();

    this._getQuerySocket().then((querySocket) => {
      DEBUG && debug('sending query on query socket ("' + name + '")');
      querySocket.send(MDNS_MULTICAST_GROUP, MDNS_PORT, data, data.length);
    });

    // Automatically announce previously-discovered
    // services that match and haven't expired yet.
    setTimeout(() => {
      DEBUG && debug('announcing previously discovered services ("' + name + '")');
      let { serviceType } = _parseServiceDomainName(name);

      this._clearExpiredDiscoveries();
      this._discovered.forEach((discovery, key) => {
        let serviceInfo = discovery.serviceInfo;
        if (serviceInfo.serviceType !== serviceType) {
          return;
        }

        let listeners = this._listeners.get(serviceInfo.serviceType) || [];
        listeners.forEach((listener) => {
          listener.onServiceFound(serviceInfo);
        });
      });
    });
  }

  _clearExpiredDiscoveries() {
    this._discovered.forEach((discovery, key) => {
      if (discovery.expireTime < Date.now()) {
        this._discovered.delete(key);
        return;
      }
    });
  }

  _handleQueryPacket(packet, message) {
    packet.getRecords(['QD']).forEach((record) => {
      // Don't respond if the query's class code is not IN or ANY.
      if (record.classCode !== DNS_CLASS_CODES.IN &&
          record.classCode !== DNS_CLASS_CODES.ANY) {
        return;
      }

      // Don't respond if the query's record type is not PTR or ANY.
      if (record.recordType !== DNS_RECORD_TYPES.PTR &&
          record.recordType !== DNS_RECORD_TYPES.ANY) {
        return;
      }

      for (let [serviceKey, publishedService] of this._services) {
        DEBUG && debug("_handleQueryPacket: " + packet.toJSON());
        if (publishedService.ptrMatch(record.name)) {
          this._respondToQuery(serviceKey, message);
        }
      }
    });
  }

  _makeServicePacket(service, addresses) {
    let packet = new DNSPacket();
    packet.setFlag('QR', DNS_QUERY_RESPONSE_CODES.RESPONSE);
    packet.setFlag('AA', DNS_AUTHORITATIVE_ANSWER_CODES.YES);

    let host = service.host || _hostname;

    // e.g.: foo-bar-service._http._tcp.local
    let serviceDomainName = service.serviceName + '.' + service.serviceType + '.local';

    // PTR Record
    packet.addRecord('AN', new DNSResourceRecord({
      name: service.serviceType + '.local', // e.g.: _http._tcp.local
      recordType: DNS_RECORD_TYPES.PTR,
      data: serviceDomainName
    }));

    // SRV Record
    packet.addRecord('AR', new DNSResourceRecord({
      name: serviceDomainName,
      recordType: DNS_RECORD_TYPES.SRV,
      classCode: DNS_CLASS_CODES.IN,
      cacheFlush: true,
      data: {
        priority: 0,
        weight: 0,
        port: service.port,
        target: host // e.g.: My-Android-Phone.local
      }
    }));

    // A Records
    for (let address of addresses) {
        packet.addRecord('AR', new DNSResourceRecord({
          name: host,
          recordType: DNS_RECORD_TYPES.A,
          data: address
        }));
    }

    // TXT Record
    packet.addRecord('AR', new DNSResourceRecord({
      name: serviceDomainName,
      recordType: DNS_RECORD_TYPES.TXT,
      classCode: DNS_CLASS_CODES.IN,
      cacheFlush: true,
      data: service.serviceAttrs || {}
    }));

    return packet;
  }

  _handleResponsePacket(packet, message) {
    let services = {};
    let hosts = {};

    let srvRecords = packet.getRecords(['AN', 'AR'], DNS_RECORD_TYPES.SRV);
    let txtRecords = packet.getRecords(['AN', 'AR'], DNS_RECORD_TYPES.TXT);
    let ptrRecords = packet.getRecords(['AN', 'AR'], DNS_RECORD_TYPES.PTR);
    let aRecords = packet.getRecords(['AN', 'AR'], DNS_RECORD_TYPES.A);

    srvRecords.forEach((record) => {
      let data = record.data || {};

      services[record.name] = {
        host: data.target,
        port: data.port,
        ttl: record.ttl
      };
    });

    txtRecords.forEach((record) => {
      if (!services[record.name]) {
        return;
      }

      services[record.name].attributes = record.data;
    });

    aRecords.forEach((record) => {
      if (IsIpv4Address(record.data)) {
        hosts[record.name] = record.data;
      }
    });

    ptrRecords.forEach((record) => {
      let name = record.data;
      if (!services[name]) {
        return;
      }

      let {host, port} = services[name];
      if (!host || !port) {
        return;
      }

      let { serviceName, serviceType, domainName } = _parseServiceDomainName(name);
      if (!serviceName || !serviceType || !domainName) {
        return;
      }

      let address = hosts[host];
      if (!address) {
        return;
      }

      let ttl = services[name].ttl || 0;
      let serviceInfo = {
        serviceName: serviceName,
        serviceType: serviceType,
        host: host,
        address: address,
        port: port,
        domainName: domainName,
        attributes: services[name].attributes || {}
      };

      this._onServiceFound(serviceInfo, ttl);
    });
  }

  _onServiceFound(serviceInfo, ttl = 0) {
    let expireTime = Date.now() + (ttl * 1000);
    let key = serviceInfo.serviceName + '.' +
              serviceInfo.serviceType + '.' +
              serviceInfo.domainName + ' @' +
              serviceInfo.address + ':' +
              serviceInfo.port;

    // If this service was already discovered, just update
    // its expiration time and don't re-emit it.
    if (this._discovered.has(key)) {
      this._discovered.get(key).expireTime = expireTime;
      return;
    }

    this._discovered.set(key, {
      serviceInfo: serviceInfo,
      expireTime: expireTime
    });

    let listeners = this._listeners.get(serviceInfo.serviceType) || [];
    listeners.forEach((listener) => {
      listener.onServiceFound(serviceInfo);
    });

    DEBUG && debug('_onServiceFound()' + serviceInfo.serviceName);
  }

  /**
   * Gets a non-exclusive socket on 0.0.0.0:{random} to send
   * multicast queries on all interfaces. This socket does
   * not need to join a multicast group since it is still
   * able to *send* multicast queries, but it does not need
   * to *listen* for multicast queries/announcements since
   * the `_broadcastReceiverSocket` is already handling them.
   */
  _getQuerySocket() {
    return new Promise((resolve, reject) => {
      if (!this._querySocket) {
        this._querySocket = _openSocket('0.0.0.0', 0, {
          onPacketReceived: this._onPacketReceived.bind(this),
          onStopListening: this._onStopListening.bind(this)
        });
      }
      resolve(this._querySocket);
    });
  }

  /**
   * Gets a non-exclusive socket on 0.0.0.0:5353 to listen
   * for multicast queries/announcements on all interfaces.
   * Since this socket needs to listen for multicast queries
   * and announcements, this socket joins the multicast
   * group on *all* interfaces (0.0.0.0).
   */
  _getBroadcastReceiverSocket() {
    return new Promise((resolve, reject) => {
      if (!this._broadcastReceiverSocket) {
        this._broadcastReceiverSocket = _openSocket('0.0.0.0', MDNS_PORT, {
          onPacketReceived: this._onPacketReceived.bind(this),
          onStopListening: this._onStopListening.bind(this)
        }, /* multicastInterface = */ '0.0.0.0');
      }
      resolve(this._broadcastReceiverSocket);
    });
  }

  /**
   * Gets a non-exclusive socket for each interface on
   * {iface-ip}:5353 for sending query responses as
   * well as for listening for unicast queries. These
   * sockets do not need to join a multicast group
   * since they are still able to *send* multicast
   * query responses, but they do not need to *listen*
   * for multicast queries since the `_querySocket` is
   * already handling them.
   */
  _getSockets() {
    return new Promise((resolve) => {
      if (this._sockets.size > 0) {
        resolve(this._sockets);
        return;
      }

      Promise.all([getAddresses(), getHostname()]).then(() => {
        _addresses.forEach((address) => {
          let socket = _openSocket(address, MDNS_PORT, null);
          this._sockets.set(address, socket);
        });

        resolve(this._sockets);
      });
    });
  }

  _checkCloseSockets() {
    // Nothing to do if no sockets to close.
    if (this._sockets.size == 0)
      return;

    // Don't close sockets if discovery listeners are still present.
    if (this._listeners.size > 0)
      return;

    // Don't close sockets if advertised services are present.
    // Since we need to listen for service queries and respond to them.
    if (this._services.size > 0)
      return;

    this._closeSockets();
  }

  _closeSockets() {
    this._sockets.forEach(socket => socket.close());
    this._sockets.clear();
  }

  _onPacketReceived(socket, message) {
    let packet = DNSPacket.parse(message.rawData);

    switch (packet.getFlag('QR')) {
      case DNS_QUERY_RESPONSE_CODES.QUERY:
        this._handleQueryPacket(packet, message);
        break;
      case DNS_QUERY_RESPONSE_CODES.RESPONSE:
        this._handleResponsePacket(packet, message);
        break;
      default:
        break;
    }
  }

  _onStopListening(socket, status) {
    DEBUG && debug('_onStopListening() ' + status);
  }

  _addServiceListener(serviceType, listener) {
    let listeners = this._listeners.get(serviceType);
    if (!listeners) {
      listeners = [];
      this._listeners.set(serviceType, listeners);
    }

    if (!listeners.find(l => l === listener)) {
      listeners.push(listener);
    }
  }

  _removeServiceListener(serviceType, listener) {
    let listeners = this._listeners.get(serviceType);
    if (!listeners) {
      return;
    }

    let index = listeners.findIndex(l => l === listener);
    if (index >= 0) {
      listeners.splice(index, 1);
    }

    if (listeners.length === 0) {
      this._listeners.delete(serviceType);
    }
  }
}

let _addresses;

/**
 * @private
 */
function getAddresses() {
  return new Promise((resolve, reject) => {
    if (_addresses) {
      resolve(_addresses);
      return;
    }

    networkInfoService.listNetworkAddresses({
      onListedNetworkAddresses(aAddressArray) {
        _addresses = aAddressArray.filter((address) => {
          return address.indexOf('%p2p') === -1 &&  // No WiFi Direct interfaces
                 address.indexOf(':')    === -1 &&  // XXX: No IPv6 for now
                 address != "127.0.0.1"             // No ipv4 loopback addresses.
        });

        DEBUG && debug('getAddresses(): ' + _addresses);
        resolve(_addresses);
      },

      onListNetworkAddressesFailed() {
        DEBUG && debug('getAddresses() FAILED!');
        resolve([]);
      }
    });
  });
}

let _hostname;

/**
 * @private
 */
function getHostname() {
  return new Promise((resolve) => {
    if (_hostname) {
      resolve(_hostname);
      return;
    }

    networkInfoService.getHostname({
      onGotHostname(aHostname) {
        _hostname = aHostname.replace(/\s/g, '-') + '.local';

        DEBUG && debug('getHostname(): ' + _hostname);
        resolve(_hostname);
      },

      onGetHostnameFailed() {
        DEBUG && debug('getHostname() FAILED');
        resolve('localhost');
      }
    });
  });
}

/**
 * Parse fully qualified domain name to service name, instance name,
 * and domain name. See https://tools.ietf.org/html/rfc6763#section-7.
 *
 * Example: 'foo-bar-service._http._tcp.local' -> {
 *   serviceName: 'foo-bar-service',
 *   serviceType: '_http._tcp',
 *   domainName: 'local'
 * }
 *
 * @private
 */
function _parseServiceDomainName(serviceDomainName) {
  let parts = serviceDomainName.split('.');
  let index = Math.max(parts.lastIndexOf('_tcp'), parts.lastIndexOf('_udp'));

  return {
    serviceName: parts.splice(0, index - 1).join('.'),
    serviceType: parts.splice(0, 2).join('.'),
    domainName: parts.join('.')
  };
}

/**
 * @private
 */
function _propertyBagToObject(propBag) {
  let result = {};
  if (propBag.QueryInterface) {
    propBag.QueryInterface(Ci.nsIPropertyBag2);
    let propEnum = propBag.enumerator;
    while (propEnum.hasMoreElements()) {
      let prop = propEnum.getNext().QueryInterface(Ci.nsIProperty);
      result[prop.name] = prop.value.toString();
    }
  } else {
    for (let name in propBag) {
      result[name] = propBag[name].toString();
    }
  }
  return result;
}

/**
 * @private
 */
function _openSocket(addr, port, handler, multicastInterface) {
  let socket = Cc['@mozilla.org/network/udp-socket;1'].createInstance(Ci.nsIUDPSocket);
  socket.init2(addr, port, Services.scriptSecurityManager.getSystemPrincipal(), true);

  if (handler) {
    socket.asyncListen({
      onPacketReceived: handler.onPacketReceived,
      onStopListening: handler.onStopListening
    });
  }

  if (multicastInterface) {
    socket.joinMulticast(MDNS_MULTICAST_GROUP, multicastInterface);
  }

  return socket;
}
