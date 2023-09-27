// Number of bits in the canvas that we randomize when canvas randomization is
// enabled.
const NUM_RANDOMIZED_CANVAS_BITS = 256;

/**
 * Compares two Uint8Arrays and returns the number of bits that are different.
 *
 * @param {Uint8ClampedArray} arr1 - The first Uint8ClampedArray to compare.
 * @param {Uint8ClampedArray} arr2 - The second Uint8ClampedArray to compare.
 * @returns {number} - The number of bits that are different between the two
 *                     arrays.
 */
function compareUint8Arrays(arr1, arr2) {
  let count = 0;
  for (let i = 0; i < arr1.length; i++) {
    let diff = arr1[i] ^ arr2[i];
    while (diff > 0) {
      count += diff & 1;
      diff >>= 1;
    }
  }
  return count;
}

/**
 * Compares two ArrayBuffer objects to see if they are different.
 *
 * @param {ArrayBuffer} buffer1 - The first buffer to compare.
 * @param {ArrayBuffer} buffer2 - The second buffer to compare.
 * @returns {boolean} True if the buffers are different, false if they are the
 *                    same.
 */
function compareArrayBuffer(buffer1, buffer2) {
  // compare the byte lengths of the two buffers
  if (buffer1.byteLength !== buffer2.byteLength) {
    return true;
  }

  // create typed arrays to represent the two buffers
  const view1 = new DataView(buffer1);
  const view2 = new DataView(buffer2);

  // compare the byte values of the two typed arrays
  for (let i = 0; i < buffer1.byteLength; i++) {
    if (view1.getUint8(i) !== view2.getUint8(i)) {
      return true;
    }
  }

  // the two buffers are the same
  return false;
}

function promiseObserver(topic) {
  return new Promise(resolve => {
    let obs = (aSubject, aTopic, aData) => {
      Services.obs.removeObserver(obs, aTopic);
      resolve(aSubject);
    };
    Services.obs.addObserver(obs, topic);
  });
}
