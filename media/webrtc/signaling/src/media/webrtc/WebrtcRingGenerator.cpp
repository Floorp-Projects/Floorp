/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Cisco Systems SIP Stack.
 *
 * The Initial Developer of the Original Code is
 * Cisco Systems (CSCO).
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Enda Mannion <emannion@cisco.com>
 *  Suhas Nandakumar <snandaku@cisco.com>
 *  Ethan Hugg <ehugg@cisco.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _USE_CPVE

#include <string.h>
#include "WebrtcRingGenerator.h"

namespace CSF {

/*
 * 480 samples of 16-bit, 8kHz linear PCM ring tone,
 * for a total duration of 60 milliseconds.
 * Repeat as directed.
 */
static short Ring1[480] =
{
     0,      0,      0,   -112,    988,   1564,  -1564,  -3260,
  -180,   -104,  -4860,  -3004,   5628,   5628,   -876,   2236,
  9340,   1308, -10876,  -5884,   1980,  -6652, -12412,   3644,
 14972,   3772,  -1564,  12924,  12924, -10876, -17788,   -372,
 -1372, -19836,  -9340,  18812,  15484,  -2620,   6652,  21884,
  1436, -21884,  -9852,   3260, -12412, -19836,   6908,  21884,
  4348,  -1564,  17788,  14972, -13948, -18812,    180,  -2364,
-20860,  -8316,  19836,  14460,  -2876,   7676,  20860,      8,
-21884,  -8828,   3132, -13436, -18812,   8316,  21884,   3516,
 -1052,  17788,  13948, -15484, -18812,    780,  -3132, -20860,
 -6908,  19836,  13436,  -3132,   8316,  20860,  -1436, -21884,
 -7932,   3004, -13948, -18812,   9340,  20860,   2620,   -460,
 18812,  12924, -15996, -17788,   1308,  -4092, -20860,  -5372,
 20860,  12924,  -3260,   9340,  20860,  -2876, -21884,  -7164,
  2748, -14972, -17788,  10876,  20860,   1884,    164,  18812,
 11900, -16764, -16764,   1820,  -4860, -20860,  -4092,  20860,
 11900,  -3388,  10364,  20860,  -4348, -21884,  -6140,   2364,
-15996, -16764,  11900,  19836,   1116,    844,  19836,  10364,
-17788, -16764,   2236,  -5884, -20860,  -2748,  21884,  10876,
 -3388,  11388,  19836,  -5628, -21884,  -5116,   1980, -16764,
-15996,  13436,  19836,    428,   1564,  19836,   9340, -18812,
-15484,   2620,  -6652, -20860,  -1244,  19836,   8828,  -2748,
  9852,  15484,  -5116, -15484,  -3004,    924, -10876,  -8828,
  7932,  10364,    -56,   1180,   8828,   3260,  -7164,  -4860,
   812,  -2236,  -5372,      0,   4092,   1372,   -308,   1244,
  1180,   -212,     72,     -8,      0,      0,      0,      0,
				/* remainder is all zeroes */
};

typedef struct
{
	short*	samples;
	int		sampleCnt;
	int		stepDuration;	// milliseconds, replay samples as needed

} RingCadence;

#ifndef min
#define min(a, b)  (((a) < (b)) ? (a) : (b))
#endif

#define MILLIS_TO_SAMPLES(millis)	((millis) * (8000/1000))
#define SAMPLES_TO_MILLIS(samples)	((samples) / (8000/1000))

#define SILENCE NULL

static RingCadence INSIDE_RING[] =
{
	{ Ring1, sizeof(Ring1)/sizeof(short), 1020 },
	{ SILENCE, 0, 3000 },
	{ NULL, 0, 0 }
};

static RingCadence OUTSIDE_RING[] =
{
	{ Ring1, sizeof(Ring1)/sizeof(short), 420 },
	{ SILENCE, 0, 200 },
	{ Ring1, sizeof(Ring1)/sizeof(short), 420 },
	{ SILENCE, 0, 3000 },
	{ NULL, 0, 0 }
};

static RingCadence FEATURE_RING[] =
{
	{ Ring1, sizeof(Ring1)/sizeof(short), 240 },
	{ SILENCE, 0, 150 },
	{ Ring1, sizeof(Ring1)/sizeof(short), 120 },
	{ SILENCE, 0, 150 },
	{ Ring1, sizeof(Ring1)/sizeof(short), 360 },
	{ SILENCE, 0, 3000 },
	{ NULL, 0, 0 }
};

static RingCadence BELLCORE_DR1[] =
{
	{ Ring1, sizeof(Ring1)/sizeof(short), 1980 },
	{ SILENCE, 0, 4000 },
	{ NULL, 0, 0 }
};

static RingCadence BELLCORE_DR2[] =
{
	{ Ring1, sizeof(Ring1)/sizeof(short), 780 },
	{ SILENCE, 0, 400 },
	{ Ring1, sizeof(Ring1)/sizeof(short), 780 },
	{ SILENCE, 0, 4000 },
	{ NULL, 0, 0 }
};

static RingCadence BELLCORE_DR3[] =
{
	{ Ring1, sizeof(Ring1)/sizeof(short), 420 },
	{ SILENCE, 0, 200 },
	{ Ring1, sizeof(Ring1)/sizeof(short), 300 },
	{ SILENCE, 0, 200 },
	{ Ring1, sizeof(Ring1)/sizeof(short), 780 },
	{ SILENCE, 0, 4000 },
	{ NULL, 0, 0 }
};

static RingCadence BELLCORE_DR4[] =
{
	{ Ring1, sizeof(Ring1)/sizeof(short), 300 },
	{ SILENCE, 0, 200 },
	{ Ring1, sizeof(Ring1)/sizeof(short), 1020 },
	{ SILENCE, 0, 200 },
	{ Ring1, sizeof(Ring1)/sizeof(short), 300 },
	{ SILENCE, 0, 4000 },
	{ NULL, 0, 0 }
};

static RingCadence BELLCORE_DR5[] =
{
	{ Ring1, sizeof(Ring1)/sizeof(short), 480 },
	{ NULL, 0, 0 }
};

static RingCadence FLASHONLY_RING[] =
{
	{ NULL, 0, 0 }
};

static RingCadence PRECEDENCE_RING[] =
{
	{ Ring1, sizeof(Ring1)/sizeof(short), 1680 },
	{ SILENCE, 0, 360 },
	{ NULL, 0, 0 }
};

static RingCadence* CadenceTable[] =
{
	// Must remain in sync with RingMode in CSFMediaTermination.h

	&INSIDE_RING[0],
    &OUTSIDE_RING[0],
    &FEATURE_RING[0],
    &BELLCORE_DR1[0],
    &BELLCORE_DR2[0],
    &BELLCORE_DR3[0],
    &BELLCORE_DR4[0],
    &BELLCORE_DR5[0],
    &FLASHONLY_RING[0],
    &PRECEDENCE_RING[0]
};

// ----------------------------------------------------------------------------
WebrtcRingGenerator::WebrtcRingGenerator( RingMode mode, bool once )
: mode(mode), once(once), currentStep(0), done(false), scaleFactor(100)
{
	timeRemaining = CadenceTable[mode]->stepDuration;
	// if sole entry is empty (FLASHONLY_RING case), generate no samples
	if ( timeRemaining == 0 ) done = true;
}

void WebrtcRingGenerator::SetScaleFactor(int scaleFactor)
{
	this->scaleFactor = scaleFactor;
}

// Webrtc InStream implementation
int WebrtcRingGenerator::Read( void *buf, int len /* bytes */ )
{
	int result = generateTone( (short *)buf, len/sizeof(short) );
	applyScaleFactor( (short *)buf, len/sizeof(short) );
	return result;
}

int WebrtcRingGenerator::generateTone( short *buf, int numSamples )
{
	RingCadence* cadence = CadenceTable[mode];
	int samplesGenerated = 0;

	while ( !done && samplesGenerated < numSamples )
	{
		RingCadence* step = &cadence[currentStep];

		int samplesToCopy = min( numSamples, MILLIS_TO_SAMPLES(timeRemaining) );
		if ( step->samples != SILENCE )
		{
			int elapsedSamples = MILLIS_TO_SAMPLES(step->stepDuration - timeRemaining);
			int sampleOffset = elapsedSamples % step->sampleCnt;
			int samplesRemaining = step->sampleCnt - sampleOffset;

			if ( samplesRemaining < samplesToCopy )
				samplesToCopy = samplesRemaining;

			memcpy( buf, &step->samples[sampleOffset], samplesToCopy*sizeof(short) );
		}
		else
		{
			memset( buf, 0, samplesToCopy*sizeof(short) );
		}
		samplesGenerated += samplesToCopy;
		timeRemaining -= SAMPLES_TO_MILLIS(samplesToCopy);

		if ( timeRemaining <= 0 )
		{
			// if there's no SILENCE at the end of a sequence, it's a one-shot
			bool noSilence = (step->samples != SILENCE);

			step = &cadence[++currentStep];
			if ( step->stepDuration == 0 )	// end of sequence, start over
			{
				step = cadence;
				currentStep = 0;

				if ( once || noSilence )	// one-shot
				{
					done = true;
				}
			}
			timeRemaining = step->stepDuration;
		}
	}
	return samplesGenerated * sizeof(short);
}

void WebrtcRingGenerator::applyScaleFactor( short *buf, int numSamples )
{
	for(int i = 0; i < numSamples; i++)
	{
		buf[i] = (short)((buf[i] * scaleFactor)/100);
	}
}

} // namespace CSF

#endif
