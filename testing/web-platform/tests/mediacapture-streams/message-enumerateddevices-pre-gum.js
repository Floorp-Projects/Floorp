onmessage = async ({source}) => {
  const devices = await navigator.mediaDevices.enumerateDevices();
  source.postMessage({devices: devices.map(d => d.toJSON())}, '*');
}
