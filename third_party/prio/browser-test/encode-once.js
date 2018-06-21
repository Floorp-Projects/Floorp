/*
 * Copyright (c) 2018, Henry Corrigan-Gibbs
 * 
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

ChromeUtils.import('resource://gre/modules/Services.jsm');

let [publicKeyA, publicKeyB, batchID, param1, param2, param3] = arguments;

Services.prefs.setStringPref('prio.publicKeyA', publicKeyA);
Services.prefs.setStringPref('prio.publicKeyB', publicKeyB);

async function test() {
  let params =  {
    'startupCrashDetected': Number(param1),
    'safeModeUsage': Number(param2),
    'browserIsUserDefault': Number(param3)
  };

  try {
    let result = await PrioEncoder.encode(batchID, params);

    const toTypedArray = byteString => {
      let u8Array = new Uint8Array(byteString.length);
      for (let i in byteString) {
          u8Array[i] = byteString.charCodeAt(i);
      }
      return u8Array;
    }

    const toHexString = bytes =>
      bytes.reduce((str, byte) => str + byte.toString(16).padStart(2, '0') + ',', '');

    console.log(toHexString(result.a) + '$' + toHexString(result.b));
    console.log('');
  } catch(e) {
    console.log('Failure.', e);
    console.log(v);
  }
}

test().then();
