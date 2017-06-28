'use strict';

// This polyfill library implements the WebUSB Test API as specified here:
// https://wicg.github.io/webusb/test/

(() => {

// These variables are logically members of the USBTest class but are defined
// here to hide them from being visible as fields of navigator.usb.test.
let internal = {
  intialized: false,

  deviceManager: null,
  deviceManagerInterceptor: null,
  deviceManagerCrossFrameProxy: null,

  chooser: null,
  chooserInterceptor: null,
  chooserCrossFrameProxy: null,
};

function fakeDeviceInitToDeviceInfo(guid, init) {
  let deviceInfo = {
    guid: guid + "",
    usbVersionMajor: init.usbVersionMajor,
    usbVersionMinor: init.usbVersionMinor,
    usbVersionSubminor: init.usbVersionSubminor,
    classCode: init.deviceClass,
    subclassCode: init.deviceSubclass,
    protocolCode: init.deviceProtocol,
    vendorId: init.vendorId,
    productId: init.productId,
    deviceVersionMajor: init.deviceVersionMajor,
    deviceVersionMinor: init.deviceVersionMinor,
    deviceVersionSubminor: init.deviceVersionSubminor,
    manufacturerName: init.manufacturerName,
    productName: init.productName,
    serialNumber: init.serialNumber,
    activeConfiguration: init.activeConfigurationValue,
    configurations: []
  };
  init.configurations.forEach(config => {
    var configInfo = {
      configurationValue: config.configurationValue,
      configurationName: config.configurationName,
      interfaces: []
    };
    config.interfaces.forEach(iface => {
      var interfaceInfo = {
        interfaceNumber: iface.interfaceNumber,
        alternates: []
      };
      iface.alternates.forEach(alternate => {
        var alternateInfo = {
          alternateSetting: alternate.alternateSetting,
          classCode: alternate.interfaceClass,
          subclassCode: alternate.interfaceSubclass,
          protocolCode: alternate.interfaceProtocol,
          interfaceName: alternate.interfaceName,
          endpoints: []
        };
        alternate.endpoints.forEach(endpoint => {
          var endpointInfo = {
            endpointNumber: endpoint.endpointNumber,
            packetSize: endpoint.packetSize,
          };
          switch (endpoint.direction) {
          case "in":
            endpointInfo.direction = device.mojom.UsbTransferDirection.INBOUND;
            break;
          case "out":
            endpointInfo.direction = device.mojom.UsbTransferDirection.OUTBOUND;
            break;
          }
          switch (endpoint.type) {
          case "bulk":
            endpointInfo.type = device.mojom.UsbTransferType.BULK;
            break;
          case "interrupt":
            endpointInfo.type = device.mojom.UsbTransferType.INTERRUPT;
            break;
          case "isochronous":
            endpointInfo.type = device.mojom.UsbTransferType.ISOCHRONOUS;
            break;
          }
          alternateInfo.endpoints.push(endpointInfo);
        });
        interfaceInfo.alternates.push(alternateInfo);
      });
      configInfo.interfaces.push(interfaceInfo);
    });
    deviceInfo.configurations.push(configInfo);
  });
  return deviceInfo;
}

function convertMojoDeviceFilters(input) {
  let output = [];
  input.forEach(filter => {
    output.push(convertMojoDeviceFilter(filter));
  });
  return output;
}

function convertMojoDeviceFilter(input) {
  let output = {};
  if (input.hasVendorId)
    output.vendorId = input.vendorId;
  if (input.hasProductId)
    output.productId = input.productId;
  if (input.hasClassCode)
    output.classCode = input.classCode;
  if (input.hasSubclassCode)
    output.subclassCode = input.subclassCode;
  if (input.hasProtocolCode)
    output.protocolCode = input.protocolCode;
  if (input.serialNumber)
    output.serialNumber = input.serialNumber;
  return output;
}

class FakeDevice {
  constructor(deviceInit) {
    this.info_ = deviceInit;
    this.opened_ = false;
    this.currentConfiguration_ = null;
    this.claimedInterfaces_ = new Map();
  }

  getConfiguration() {
    if (this.currentConfiguration_) {
      return Promise.resolve({
          value: this.currentConfiguration_.configurationValue });
    } else {
      return Promise.resolve({ value: 0 });
    }
  }

  open() {
    assert_false(this.opened_);
    this.opened_ = true;
    return Promise.resolve({ error: device.mojom.UsbOpenDeviceError.OK });
  }

  close() {
    assert_true(this.opened_);
    this.opened_ = false;
    return Promise.resolve();
  }

  setConfiguration(value) {
    assert_true(this.opened_);

    let selectedConfiguration = this.info_.configurations.find(
        configuration => configuration.configurationValue == value);
    // Blink should never request an invalid configuration.
    assert_not_equals(selectedConfiguration, undefined);
    this.currentConfiguration_ = selectedConfiguration;
    return Promise.resolve({ success: true });
  }

  claimInterface(interfaceNumber) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    assert_false(this.claimedInterfaces_.has(interfaceNumber),
                 'interface already claimed');

    // Blink should never request an invalid interface.
    assert_true(this.currentConfiguration_.interfaces.some(
            iface => iface.interfaceNumber == interfaceNumber));
    this.claimedInterfaces_.set(interfaceNumber, 0);
    return Promise.resolve({ success: true });
  }

  releaseInterface(interfaceNumber) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    assert_true(this.claimedInterfaces_.has(interfaceNumber));
    this.claimedInterfaces_.delete(interfaceNumber);
    return Promise.resolve({ success: true });
  }

  setInterfaceAlternateSetting(interfaceNumber, alternateSetting) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    assert_true(this.claimedInterfaces_.has(interfaceNumber));

    let iface = this.currentConfiguration_.interfaces.find(
        iface => iface.interfaceNumber == interfaceNumber);
    // Blink should never request an invalid interface or alternate.
    assert_false(iface == undefined);
    assert_true(iface.alternates.some(
        x => x.alternateSetting == alternateSetting));
    this.claimedInterfaces_.set(interfaceNumber, alternateSetting);
    return Promise.resolve({ success: true });
  }

  reset() {
    assert_true(this.opened_);
    return Promise.resolve({ success: true });
  }

  clearHalt(endpoint) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    // TODO(reillyg): Assert that endpoint is valid.
    return Promise.resolve({ success: true });
  }

  controlTransferIn(params, length, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    return Promise.resolve({
      status: device.mojom.UsbTransferStatus.OK,
      data: [length >> 8, length & 0xff, params.request, params.value >> 8,
             params.value & 0xff, params.index >> 8, params.index & 0xff]
    });
  }

  controlTransferOut(params, data, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    return Promise.resolve({
      status: device.mojom.UsbTransferStatus.OK,
      bytesWritten: data.byteLength
    });
  }

  genericTransferIn(endpointNumber, length, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    // TODO(reillyg): Assert that endpoint is valid.
    let data = new Array(length);
    for (let i = 0; i < length; ++i)
      data[i] = i & 0xff;
    return Promise.resolve({
      status: device.mojom.UsbTransferStatus.OK,
      data: data
    });
  }

  genericTransferOut(endpointNumber, data, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    // TODO(reillyg): Assert that endpoint is valid.
    return Promise.resolve({
      status: device.mojom.UsbTransferStatus.OK,
      bytesWritten: data.byteLength
    });
  }

  isochronousTransferIn(endpointNumber, packetLengths, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    // TODO(reillyg): Assert that endpoint is valid.
    let data = new Array(packetLengths.reduce((a, b) => a + b, 0));
    let dataOffset = 0;
    let packets = new Array(packetLengths.length);
    for (let i = 0; i < packetLengths.length; ++i) {
      for (let j = 0; j < packetLengths[i]; ++j)
        data[dataOffset++] = j & 0xff;
      packets[i] = {
        length: packetLengths[i],
        transferredLength: packetLengths[i],
        status: device.mojom.UsbTransferStatus.OK
      };
    }
    return Promise.resolve({ data: data, packets: packets });
  }

  isochronousTransferOut(endpointNumber, data, packetLengths, timeout) {
    assert_true(this.opened_);
    assert_false(this.currentConfiguration_ == null, 'device configured');
    // TODO(reillyg): Assert that endpoint is valid.
    let packets = new Array(packetLengths.length);
    for (let i = 0; i < packetLengths.length; ++i) {
      packets[i] = {
        length: packetLengths[i],
        transferredLength: packetLengths[i],
        status: device.mojom.UsbTransferStatus.OK
      };
    }
    return Promise.resolve({ packets: packets });
  }
}

class FakeDeviceManager {
  constructor() {
    this.bindingSet_ = new mojo.BindingSet(device.mojom.UsbDeviceManager);
    this.devices_ = new Map();
    this.devicesByGuid_ = new Map();
    this.client_ = null;
    this.nextGuid_ = 0;
  }

  addBinding(handle) {
    this.bindingSet_.addBinding(this, handle);
  }

  addDevice(fakeDevice, info) {
    let device = {
      fakeDevice: fakeDevice,
      guid: (this.nextGuid_++).toString(),
      info: info,
      bindingArray: []
    };
    this.devices_.set(fakeDevice, device);
    this.devicesByGuid_.set(device.guid, device);
    if (this.client_)
      this.client_.onDeviceAdded(fakeDeviceInitToDeviceInfo(device.guid, info));
  }

  removeDevice(fakeDevice) {
    let device = this.devices_.get(fakeDevice);
    if (!device)
      throw new Error('Cannot remove unknown device.');

    for (var binding of device.bindingArray)
      binding.close();
    this.devices_.delete(device.fakeDevice);
    this.devicesByGuid_.delete(device.guid);
    if (this.client_) {
      this.client_.onDeviceRemoved(
          fakeDeviceInitToDeviceInfo(device.guid, device.info));
    }
  }

  removeAllDevices() {
    this.devices_.forEach(device => {
      for (var binding of device.bindingArray)
        binding.close();
      this.client_.onDeviceRemoved(
          fakeDeviceInitToDeviceInfo(device.guid, device.info));
    });
    this.devices_.clear();
    this.devicesByGuid_.clear();
  }

  getDevices(options) {
    let devices = [];
    this.devices_.forEach(device => {
      devices.push(fakeDeviceInitToDeviceInfo(device.guid, device.info));
    });
    return Promise.resolve({ results: devices });
  }

  getDevice(guid, request) {
    let device = this.devicesByGuid_.get(guid);
    if (device) {
      let binding = new mojo.Binding(
          window.device.mojom.UsbDevice, new FakeDevice(device.info), request);
      binding.setConnectionErrorHandler(() => {
        if (device.fakeDevice.onclose)
          device.fakeDevice.onclose();
      });
      device.bindingArray.push(binding);
    } else {
      request.close();
    }
  }

  setClient(client) {
    this.client_ = client;
  }
}

class FakeChooserService {
  constructor() {
    this.bindingSet_ = new mojo.BindingSet(device.mojom.UsbChooserService);
    this.chosenDevice_ = null;
    this.lastFilters_ = null;
  }

  addBinding(handle) {
    this.bindingSet_.addBinding(this, handle);
  }

  setChosenDevice(fakeDevice) {
    this.chosenDevice_ = fakeDevice;
  }

  getPermission(deviceFilters) {
    this.lastFilters_ = convertMojoDeviceFilters(deviceFilters);
    let device = internal.deviceManager.devices_.get(this.chosenDevice_);
    if (device) {
      return Promise.resolve({
        result: fakeDeviceInitToDeviceInfo(device.guid, device.info)
      });
    } else {
      return Promise.resolve({ result: null });
    }
  }
}

// Unlike FakeDevice this class is exported to callers of USBTest.addFakeDevice.
class FakeUSBDevice {
  constructor() {
    this.onclose = null;
  }

  disconnect() {
    setTimeout(() => internal.deviceManager.removeDevice(this), 0);
  }
}

// A helper for forwarding MojoHandle instances from one frame to another.
class CrossFrameHandleProxy {
  constructor(callback) {
    let {handle0, handle1} = Mojo.createMessagePipe();
    this.sender_ = handle0;
    this.receiver_ = handle1;
    this.receiver_.watch({readable: true}, () => {
      let message = this.receiver_.readMessage();
      assert_equals(message.buffer.byteLength, 0);
      assert_equals(message.handles.length, 1);
      callback(message.handles[0]);
    });
  }

  forwardHandle(handle) {
    this.sender_.writeMessage(new ArrayBuffer, [handle]);
  }
}

class USBTest {
  constructor() {}

  initialize() {
    if (internal.initialized)
      return Promise.resolve();

    internal.deviceManager = new FakeDeviceManager();
    internal.deviceManagerInterceptor =
        new MojoInterfaceInterceptor(device.mojom.UsbDeviceManager.name);
    internal.deviceManagerInterceptor.oninterfacerequest =
        e => internal.deviceManager.addBinding(e.handle);
    internal.deviceManagerInterceptor.start();
    internal.deviceManagerCrossFrameProxy = new CrossFrameHandleProxy(
        handle => internal.deviceManager.addBinding(handle));

    internal.chooser = new FakeChooserService();
    internal.chooserInterceptor =
        new MojoInterfaceInterceptor(device.mojom.UsbChooserService.name);
    internal.chooserInterceptor.oninterfacerequest =
        e => internal.chooser.addBinding(e.handle);
    internal.chooserInterceptor.start();
    internal.chooserCrossFrameProxy = new CrossFrameHandleProxy(
        handle => internal.chooser.addBinding(handle));

    internal.initialized = true;
    return Promise.resolve();
  }

  attachToWindow(otherWindow) {
    if (!internal.initialized)
      throw new Error('Call initialize() before attachToWindow().');

    otherWindow.deviceManagerInterceptor =
        new otherWindow.MojoInterfaceInterceptor(
            device.mojom.UsbDeviceManager.name);
    otherWindow.deviceManagerInterceptor.oninterfacerequest =
        e => internal.deviceManagerCrossFrameProxy.forwardHandle(e.handle);
    otherWindow.deviceManagerInterceptor.start();

    otherWindow.chooserInterceptor =
        new otherWindow.MojoInterfaceInterceptor(
            device.mojom.UsbChooserService.name);
    otherWindow.chooserInterceptor.oninterfacerequest =
        e => internal.chooserCrossFrameProxy.forwardHandle(e.handle);
    otherWindow.chooserInterceptor.start();
    return Promise.resolve();
  }

  addFakeDevice(deviceInit) {
    if (!internal.initialized)
      throw new Error('Call initialize() before addFakeDevice().');

    // |addDevice| and |removeDevice| are called in a setTimeout callback so
    // that tests do not rely on the device being immediately available which
    // may not be true for all implementations of this test API.
    let fakeDevice = new FakeUSBDevice();
    setTimeout(
        () => internal.deviceManager.addDevice(fakeDevice, deviceInit), 0);
    return fakeDevice;
  }

  set chosenDevice(fakeDevice) {
    if (!internal.initialized)
      throw new Error('Call initialize() before setting chosenDevice.');

    internal.chooser.setChosenDevice(fakeDevice);
  }

  get lastFilters() {
    if (!internal.initialized)
      throw new Error('Call initialize() before getting lastFilters.');

    return internal.chooser.lastFilters_;
  }

  reset() {
    if (!internal.initialized)
      throw new Error('Call initialize() before reset().');

    // Reset the mocks in a setTimeout callback so that tests do not rely on
    // the fact that this polyfill can do this synchronously.
    return new Promise(resolve => {
      setTimeout(() => {
        internal.deviceManager.removeAllDevices();
        internal.chooser.setChosenDevice(null);
        resolve();
      }, 0);
    });
  }
}

navigator.usb.test = new USBTest();

})();
