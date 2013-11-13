/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/video_processing/main/source/deflickering.h"

#include <math.h>
#include <stdlib.h>

#include "webrtc/common_audio/signal_processing/include/signal_processing_library.h"
#include "webrtc/system_wrappers/interface/sort.h"
#include "webrtc/system_wrappers/interface/trace.h"

namespace webrtc {

// Detection constants
enum { kFrequencyDeviation = 39 };      // (Q4) Maximum allowed deviation for detection
enum { kMinFrequencyToDetect = 32 };    // (Q4) Minimum frequency that can be detected
enum { kNumFlickerBeforeDetect = 2 };   // Number of flickers before we accept detection
enum { kMeanValueScaling = 4 };         // (Q4) In power of 2
enum { kZeroCrossingDeadzone = 10 };    // Deadzone region in terms of pixel values

// Deflickering constants
// Compute the quantiles over 1 / DownsamplingFactor of the image.
enum { kDownsamplingFactor = 8 };
enum { kLog2OfDownsamplingFactor = 3 };

// To generate in Matlab:
// >> probUW16 = round(2^11 * [0.05,0.1,0.2,0.3,0.4,0.5,0.6,0.7,0.8,0.9,0.95,0.97]);
// >> fprintf('%d, ', probUW16)
// Resolution reduced to avoid overflow when multiplying with the (potentially) large 
// number of pixels.
const uint16_t VPMDeflickering::_probUW16[kNumProbs] =
    {102, 205, 410, 614, 819, 1024, 1229, 1434, 1638, 1843, 1946, 1987}; // <Q11>

// To generate in Matlab:
// >> numQuants = 14; maxOnlyLength = 5;
// >> weightUW16 = round(2^15 * [linspace(0.5, 1.0, numQuants - maxOnlyLength)]);
// >> fprintf('%d, %d,\n ', weightUW16);
const uint16_t VPMDeflickering::_weightUW16[kNumQuants - kMaxOnlyLength] =
    {16384, 18432, 20480, 22528, 24576, 26624, 28672, 30720, 32768}; // <Q15>
 
VPMDeflickering::VPMDeflickering() :
    _id(0)
{
    Reset();
}

VPMDeflickering::~VPMDeflickering()
{
}

int32_t
VPMDeflickering::ChangeUniqueId(const int32_t id)
{
    _id = id;
    return 0;
}

void
VPMDeflickering::Reset()
{
    _meanBufferLength = 0;
    _detectionState = 0;
    _frameRate = 0;

    memset(_meanBuffer, 0, sizeof(int32_t) * kMeanBufferLength);
    memset(_timestampBuffer, 0, sizeof(int32_t) * kMeanBufferLength);

    // Initialize the history with a uniformly distributed histogram
    _quantHistUW8[0][0] = 0;
    _quantHistUW8[0][kNumQuants - 1] = 255;
    for (int32_t i = 0; i < kNumProbs; i++)
    {
        _quantHistUW8[0][i + 1] = static_cast<uint8_t>((WEBRTC_SPL_UMUL_16_16(
            _probUW16[i], 255) + (1 << 10)) >> 11); // Unsigned round. <Q0>
    }
    
    for (int32_t i = 1; i < kFrameHistorySize; i++)
    {
        memcpy(_quantHistUW8[i], _quantHistUW8[0], sizeof(uint8_t) * kNumQuants);
    }
}

int32_t
VPMDeflickering::ProcessFrame(I420VideoFrame* frame,
                              VideoProcessingModule::FrameStats* stats)
{
    assert(frame);
    uint32_t frameMemory;
    uint8_t quantUW8[kNumQuants];
    uint8_t maxQuantUW8[kNumQuants];
    uint8_t minQuantUW8[kNumQuants];
    uint16_t targetQuantUW16[kNumQuants];
    uint16_t incrementUW16;
    uint8_t mapUW8[256];

    uint16_t tmpUW16;
    uint32_t tmpUW32;
    int width = frame->width();
    int height = frame->height();

    if (frame->IsZeroSize())
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, _id,
                     "Null frame pointer");
        return VPM_GENERAL_ERROR;
    }

    // Stricter height check due to subsampling size calculation below.
    if (height < 2)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, _id,
                     "Invalid frame size");
        return VPM_GENERAL_ERROR;
    }

    if (!VideoProcessingModule::ValidFrameStats(*stats))
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, _id,
                     "Invalid frame stats");
        return VPM_GENERAL_ERROR;
    }

    if (PreDetection(frame->timestamp(), *stats) == -1)
    {
        return VPM_GENERAL_ERROR;
    }

    // Flicker detection
    int32_t detFlicker = DetectFlicker();
    if (detFlicker < 0)
    { // Error
        return VPM_GENERAL_ERROR;
    }
    else if (detFlicker != 1)
    {
        return 0;
    }

    // Size of luminance component
    const uint32_t ySize = height * width;

    const uint32_t ySubSize = width * (((height - 1) >>
        kLog2OfDownsamplingFactor) + 1);
    uint8_t* ySorted = new uint8_t[ySubSize];
    uint32_t sortRowIdx = 0;
    for (int i = 0; i < height; i += kDownsamplingFactor)
    {
        memcpy(ySorted + sortRowIdx * width,
               frame->buffer(kYPlane) + i * width, width);
        sortRowIdx++;
    }
    
    webrtc::Sort(ySorted, ySubSize, webrtc::TYPE_UWord8);

    uint32_t probIdxUW32 = 0;
    quantUW8[0] = 0;
    quantUW8[kNumQuants - 1] = 255;

    // Ensure we won't get an overflow below.
    // In practice, the number of subsampled pixels will not become this large.
    if (ySubSize > (1 << 21) - 1)
    {
        WEBRTC_TRACE(webrtc::kTraceError, webrtc::kTraceVideoPreocessing, _id, 
            "Subsampled number of pixels too large");
        return -1;
    }

    for (int32_t i = 0; i < kNumProbs; i++)
    {
        probIdxUW32 = WEBRTC_SPL_UMUL_32_16(ySubSize, _probUW16[i]) >> 11; // <Q0>
        quantUW8[i + 1] = ySorted[probIdxUW32];
    }

    delete [] ySorted;
    ySorted = NULL;

    // Shift history for new frame.
    memmove(_quantHistUW8[1], _quantHistUW8[0], (kFrameHistorySize - 1) * kNumQuants *
        sizeof(uint8_t));
    // Store current frame in history.
    memcpy(_quantHistUW8[0], quantUW8, kNumQuants * sizeof(uint8_t));

    // We use a frame memory equal to the ceiling of half the frame rate to ensure we
    // capture an entire period of flicker.
    frameMemory = (_frameRate + (1 << 5)) >> 5; // Unsigned ceiling. <Q0>
                                                // _frameRate in Q4.
    if (frameMemory > kFrameHistorySize)
    {
        frameMemory = kFrameHistorySize;
    }

    // Get maximum and minimum.
    for (int32_t i = 0; i < kNumQuants; i++)
    {
        maxQuantUW8[i] = 0;
        minQuantUW8[i] = 255;
        for (uint32_t j = 0; j < frameMemory; j++)
        {
            if (_quantHistUW8[j][i] > maxQuantUW8[i])
            {
                maxQuantUW8[i] = _quantHistUW8[j][i];
            }

            if (_quantHistUW8[j][i] < minQuantUW8[i])
            {
                minQuantUW8[i] = _quantHistUW8[j][i];
            }
        }
    }
    
    // Get target quantiles.
    for (int32_t i = 0; i < kNumQuants - kMaxOnlyLength; i++)
    {
        targetQuantUW16[i] = static_cast<uint16_t>((WEBRTC_SPL_UMUL_16_16(
            _weightUW16[i], maxQuantUW8[i]) + WEBRTC_SPL_UMUL_16_16((1 << 15) -
            _weightUW16[i], minQuantUW8[i])) >> 8); // <Q7>
    }

    for (int32_t i = kNumQuants - kMaxOnlyLength; i < kNumQuants; i++)
    {
        targetQuantUW16[i] = ((uint16_t)maxQuantUW8[i]) << 7;
    }

    // Compute the map from input to output pixels.
    uint16_t mapUW16; // <Q7>
    for (int32_t i = 1; i < kNumQuants; i++)
    {
        // As quant and targetQuant are limited to UWord8, we're safe to use Q7 here.
        tmpUW32 = static_cast<uint32_t>(targetQuantUW16[i] -
            targetQuantUW16[i - 1]); // <Q7>
        tmpUW16 = static_cast<uint16_t>(quantUW8[i] - quantUW8[i - 1]); // <Q0>

        if (tmpUW16 > 0)
        {
            incrementUW16 = static_cast<uint16_t>(WebRtcSpl_DivU32U16(tmpUW32,
                tmpUW16)); // <Q7>
         }
        else
        {
            // The value is irrelevant; the loop below will only iterate once.
            incrementUW16 = 0;
        }

        mapUW16 = targetQuantUW16[i - 1];
        for (uint32_t j = quantUW8[i - 1]; j < (uint32_t)(quantUW8[i] + 1); j++)
        {
            mapUW8[j] = (uint8_t)((mapUW16 + (1 << 6)) >> 7); // Unsigned round. <Q0>
            mapUW16 += incrementUW16;
        }
    }

    // Map to the output frame.
    uint8_t* buffer = frame->buffer(kYPlane);
    for (uint32_t i = 0; i < ySize; i++)
    {
      buffer[i] = mapUW8[buffer[i]];
    }

    // Frame was altered, so reset stats.
    VideoProcessingModule::ClearFrameStats(stats);

    return 0;
}

/**
   Performs some pre-detection operations. Must be called before 
   DetectFlicker().

   \param[in] timestamp Timestamp of the current frame.
   \param[in] stats     Statistics of the current frame.
 
   \return 0: Success\n
           2: Detection not possible due to flickering frequency too close to
              zero.\n
          -1: Error
*/
int32_t
VPMDeflickering::PreDetection(const uint32_t timestamp,
                              const VideoProcessingModule::FrameStats& stats)
{
    int32_t meanVal; // Mean value of frame (Q4)
    uint32_t frameRate = 0;
    int32_t meanBufferLength; // Temp variable

    meanVal = ((stats.sum << kMeanValueScaling) / stats.numPixels);
    /* Update mean value buffer.
     * This should be done even though we might end up in an unreliable detection.
     */
    memmove(_meanBuffer + 1, _meanBuffer, (kMeanBufferLength - 1) * sizeof(int32_t));
    _meanBuffer[0] = meanVal;

    /* Update timestamp buffer.
     * This should be done even though we might end up in an unreliable detection.
     */
    memmove(_timestampBuffer + 1, _timestampBuffer, (kMeanBufferLength - 1) *
        sizeof(uint32_t));
    _timestampBuffer[0] = timestamp;

    /* Compute current frame rate (Q4) */
    if (_timestampBuffer[kMeanBufferLength - 1] != 0)
    {
        frameRate = ((90000 << 4) * (kMeanBufferLength - 1));
        frameRate /= (_timestampBuffer[0] - _timestampBuffer[kMeanBufferLength - 1]);
    }else if (_timestampBuffer[1] != 0)
    {
        frameRate = (90000 << 4) / (_timestampBuffer[0] - _timestampBuffer[1]);
    }

    /* Determine required size of mean value buffer (_meanBufferLength) */
    if (frameRate == 0) {
        meanBufferLength = 1;
    }
    else {
        meanBufferLength = (kNumFlickerBeforeDetect * frameRate) / kMinFrequencyToDetect;
    }
    /* Sanity check of buffer length */
    if (meanBufferLength >= kMeanBufferLength)
    {
        /* Too long buffer. The flickering frequency is too close to zero, which
         * makes the estimation unreliable.
         */
        _meanBufferLength = 0;
        return 2;
    }
    _meanBufferLength = meanBufferLength;

    if ((_timestampBuffer[_meanBufferLength - 1] != 0) && (_meanBufferLength != 1))
    {
        frameRate = ((90000 << 4) * (_meanBufferLength - 1));
        frameRate /= (_timestampBuffer[0] - _timestampBuffer[_meanBufferLength - 1]);
    }else if (_timestampBuffer[1] != 0)
    {
        frameRate = (90000 << 4) / (_timestampBuffer[0] - _timestampBuffer[1]);
    }
    _frameRate = frameRate;

    return 0;
}

/**
   This function detects flicker in the video stream. As a side effect the mean value
   buffer is updated with the new mean value.
 
   \return 0: No flickering detected\n
           1: Flickering detected\n
           2: Detection not possible due to unreliable frequency interval
          -1: Error
*/
int32_t VPMDeflickering::DetectFlicker()
{
    /* Local variables */
    uint32_t  i;
    int32_t  freqEst;       // (Q4) Frequency estimate to base detection upon
    int32_t  retVal = -1;

    /* Sanity check for _meanBufferLength */
    if (_meanBufferLength < 2)
    {
        /* Not possible to estimate frequency */
        return(2);
    }
    /* Count zero crossings with a dead zone to be robust against noise.
     * If the noise std is 2 pixel this corresponds to about 95% confidence interval.
     */
    int32_t deadzone = (kZeroCrossingDeadzone << kMeanValueScaling); // Q4
    int32_t meanOfBuffer = 0; // Mean value of mean value buffer
    int32_t numZeros     = 0; // Number of zeros that cross the deadzone
    int32_t cntState     = 0; // State variable for zero crossing regions
    int32_t cntStateOld  = 0; // Previous state variable for zero crossing regions

    for (i = 0; i < _meanBufferLength; i++)
    {
        meanOfBuffer += _meanBuffer[i];
    }
    meanOfBuffer += (_meanBufferLength >> 1); // Rounding, not truncation
    meanOfBuffer /= _meanBufferLength;

    /* Count zero crossings */
    cntStateOld = (_meanBuffer[0] >= (meanOfBuffer + deadzone));
    cntStateOld -= (_meanBuffer[0] <= (meanOfBuffer - deadzone));
    for (i = 1; i < _meanBufferLength; i++)
    {
        cntState = (_meanBuffer[i] >= (meanOfBuffer + deadzone));
        cntState -= (_meanBuffer[i] <= (meanOfBuffer - deadzone));
        if (cntStateOld == 0)
        {
            cntStateOld = -cntState;
        }
        if (((cntState + cntStateOld) == 0) && (cntState != 0))
        {
            numZeros++;
            cntStateOld = cntState;
        }
    }
    /* END count zero crossings */

    /* Frequency estimation according to:
     * freqEst = numZeros * frameRate / 2 / _meanBufferLength;
     *
     * Resolution is set to Q4
     */
    freqEst = ((numZeros * 90000) << 3);
    freqEst /= (_timestampBuffer[0] - _timestampBuffer[_meanBufferLength - 1]);

    /* Translate frequency estimate to regions close to 100 and 120 Hz */
    uint8_t freqState = 0; // Current translation state;
                               // (0) Not in interval,
                               // (1) Within valid interval,
                               // (2) Out of range
    int32_t freqAlias = freqEst;
    if (freqEst > kMinFrequencyToDetect)
    {
        uint8_t aliasState = 1;
        while(freqState == 0)
        {
            /* Increase frequency */
            freqAlias += (aliasState * _frameRate);
            freqAlias += ((freqEst << 1) * (1 - (aliasState << 1)));
            /* Compute state */
            freqState = (abs(freqAlias - (100 << 4)) <= kFrequencyDeviation);
            freqState += (abs(freqAlias - (120 << 4)) <= kFrequencyDeviation);
            freqState += 2 * (freqAlias > ((120 << 4) + kFrequencyDeviation));
            /* Switch alias state */
            aliasState++;
            aliasState &= 0x01;
        }
    }
    /* Is frequency estimate within detection region? */
    if (freqState == 1)
    {
        retVal = 1;
    }else if (freqState == 0)
    {
        retVal = 2;
    }else
    {
        retVal = 0;
    }
    return retVal;
}

}  // namespace
