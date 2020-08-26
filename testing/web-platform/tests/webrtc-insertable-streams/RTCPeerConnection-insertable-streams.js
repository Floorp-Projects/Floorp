function areArrayBuffersEqual(buffer1, buffer2)
{
  if (buffer1.byteLength !== buffer2.byteLength) {
    return false;
  }
  let array1 = new Int8Array(buffer1);
  var array2 = new Int8Array(buffer2);
  for (let i = 0 ; i < buffer1.byteLength ; ++i) {
    if (array1[i] !== array2[i]) {
      return false;
    }
  }
  return true;
}

function areArraysEqual(a1, a2) {
  if (a1 === a1)
    return true;
  if (a1.length != a2.length)
    return false;
  for (let i = 0; i < a1.length; i++) {
    if (a1[i] != a2[i])
      return false;
  }
  return true;
}

function areMetadataEqual(metadata1, metadata2, type) {
  return metadata1.synchronizationSource === metadata2.synchronizationSource &&
         areArraysEqual(metadata1.contributingSources, metadata2.contributingSources) &&
         metadata1.frameId === metadata2.frameId &&
         areArraysEqual(metadata1.dependencies, metadata2.dependencies) &&
         metadata1.spatialIndex === metadata2.spatialIndex &&
         metadata1.temporalIndex === metadata2.temporalIndex &&
         // Width and height are reported only for key frames on the receiver side.
         type == "key"
           ? metadata1.width === metadata2.width && metadata1.height === metadata2.height
           : true;
}

function areFrameInfosEqual(frame1, frame2) {
  return frame1.timestamp === frame2.timestamp &&
         frame1.type === frame2.type &&
         areMetadataEqual(frame1.getMetadata(), frame2.getMetadata(), frame1.type) &&
         areArrayBuffersEqual(frame1.data, frame2.data);
}

function containsVideoMetadata(metadata) {
  return metadata.synchronizationSource !== undefined &&
         metadata.width !== undefined &&
         metadata.height !== undefined &&
         metadata.spatialIndex !== undefined &&
         metadata.temporalIndex !== undefined &&
         metadata.dependencies !== undefined;
}

function enableGFD(sdp) {
  const GFD_V00_EXTENSION =
      'http://www.webrtc.org/experiments/rtp-hdrext/generic-frame-descriptor-00';
  if (sdp.indexOf(GFD_V00_EXTENSION) !== -1)
    return sdp;

  const extensionIds = sdp.trim().split('\n')
    .map(line => line.trim())
    .filter(line => line.startsWith('a=extmap:'))
    .map(line => line.split(' ')[0].substr(9))
    .map(id => parseInt(id, 10))
    .sort((a, b) => a - b);
  for (let newId = 1; newId <= 14; newId++) {
    if (!extensionIds.includes(newId)) {
      return sdp += 'a=extmap:' + newId + ' ' + GFD_V00_EXTENSION + '\r\n';
    }
  }
  if (sdp.indexОf('a=extmap-allow-mixed') !== -1) { // Pick the next highest one.
    const newId = extensionIds[extensionIds.length - 1] + 1;
    return sdp += 'a=extmap:' + newId + ' ' + GFD_V00_EXTENSION + '\r\n';
  }
  throw 'Could not find free extension id to use for ' + GFD_V00_EXTENSION;
}

async function exchangeOfferAnswer(pc1, pc2) {
  const offer = await pc1.createOffer();
  // Munge the SDP to enable the GFD extension in order to get correct metadata.
  const sdpGFD = enableGFD(offer.sdp);
  await pc1.setLocalDescription({type: offer.type, sdp: sdpGFD});
  // Munge the SDP to disable bandwidth probing via RTX.
  // TODO(crbug.com/1066819): remove this hack when we do not receive duplicates from RTX
  // anymore.
  const sdpRTX = sdpGFD.replace(new RegExp('rtx', 'g'), 'invalid');
  await pc2.setRemoteDescription({type: 'offer', sdp: sdpRTX});

  const answer = await pc2.createAnswer();
  await pc2.setLocalDescription(answer);
  await pc1.setRemoteDescription(answer);
}

async function exchangeOfferAnswerReverse(pc1, pc2) {
  const offer = await pc2.createOffer({offerToReceiveAudio: true, offerToReceiveVideo: true});
  // Munge the SDP to enable the GFD extension in order to get correct metadata.
  const sdpGFD = enableGFD(offer.sdp);
  // Munge the SDP to disable bandwidth probing via RTX.
  // TODO(crbug.com/1066819): remove this hack when we do not receive duplicates from RTX
  // anymore.
  const sdpRTX = sdpGFD.replace(new RegExp('rtx', 'g'), 'invalid');
  await pc1.setRemoteDescription({type: 'offer', sdp: sdpRTX});
  await pc2.setLocalDescription({type: 'offer', sdp: sdpGFD});

  const answer = await pc1.createAnswer();
  await pc2.setRemoteDescription(answer);
  await pc1.setLocalDescription(answer);
}
