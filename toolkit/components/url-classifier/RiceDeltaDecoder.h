/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef RICE_DELTA_DECODER_H
#define RICE_DELTA_DECODER_H

namespace mozilla {
namespace safebrowsing {

class RiceDeltaDecoder {
 public:
  // This decoder is tailored for safebrowsing v4, including the
  // bit reading order and how the remainder part is interpreted.
  // The caller just needs to feed the byte stream received from
  // network directly. Note that the input buffer must be mutable
  // since the decoder will do some pre-processing before decoding.
  RiceDeltaDecoder(uint8_t* aEncodedData, size_t aEncodedDataSize);

  // @param aNumEntries The number of values to be decoded, not including
  //                    the first value.
  // @param aDecodedData A pre-allocated output buffer. Note that
  //                     aDecodedData[0] will be filled with |aFirstValue|
  //                     and the buffer length (in byte) should be
  //                     ((aNumEntries + 1) * sizeof(uint32_t)).
  bool Decode(uint32_t aRiceParameter, uint32_t aFirstValue,
              uint32_t aNumEntries, uint32_t* aDecodedData);

 private:
  uint8_t* mEncodedData;
  size_t mEncodedDataSize;
};

}  // namespace safebrowsing
}  // namespace mozilla

#endif  // UPDATE_V4_DECODER_H
