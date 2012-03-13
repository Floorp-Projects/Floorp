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
 Webrtc*
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
#include "WebrtcToneGenerator.h"
#include "CSFToneDefinitions.h"

namespace CSF {

static TONE_TABLE_TYPE ToneTable[] =
{
	// Must remain in sync with ToneType in CSFMediaTermination.h

	// INSIDE DIAL TONE pair (440 Hz, 350 Hz)
	{{{(short)TGN_INFINITE_REPEAT, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_440, TGN_COEF_440}, {TGN_YN_2_350, TGN_COEF_350}, {0x0000, 0x0000}, {0x0000, 0x0000}},
  {{0},{0},{0},{0}},
	TGN_INFINITE_REPEAT, 0x0002},
	
	// OUTSIDE DIAL TONE pair (450 Hz, 548 Hz)
	{{{(short)TGN_INFINITE_REPEAT, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_450, TGN_COEF_450}, {TGN_YN_2_548, TGN_COEF_548}, {0x0000, 0x0000}, {0x0000, 0x0000}},
  {{0},{0},{0},{0}},
	TGN_INFINITE_REPEAT, 0x0002},
	
	// BUSY pair (480 Hz, 620 Hz)
	{{{MILLISECONDS_TO_SAMPLES(500),  MILLISECONDS_TO_SAMPLES(500)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_480, TGN_COEF_480}, {TGN_YN_2_620, TGN_COEF_620}, {0x0000, 0x0000}, {0x0000, 0x0000}},
  {{0},{0},{0},{0}},
	TGN_INFINITE_REPEAT, 0x0002},
	
	// ALERTING pair (440 Hz, 480 Hz)
	{{{MILLISECONDS_TO_SAMPLES(2000), MILLISECONDS_TO_SAMPLES(4000)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_440, TGN_COEF_440}, {TGN_YN_2_480, TGN_COEF_480}, {0x0000, 0x0000}, {0x0000, 0x0000}},
  {{0},{0},{0},{0}},
	TGN_INFINITE_REPEAT, 0x0002},
	
    // BUSY VERIFICATION tone (440 Hz)
    {{{MILLISECONDS_TO_SAMPLES(2000), MILLISECONDS_TO_SAMPLES(100)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_440, TGN_COEF_440}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{0},{0},{0},{0}},
    0x0000, 0x0002},

    // STUTTER pair (350 Hz, 440 Hz)
    {{{MILLISECONDS_TO_SAMPLES(100), MILLISECONDS_TO_SAMPLES(100)}, {(short)TGN_INFINITE_REPEAT, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_350, TGN_COEF_350}, {TGN_YN_2_440, TGN_COEF_440}, {TGN_YN_2_350, TGN_COEF_350}, {TGN_YN_2_440, TGN_COEF_440}},
    {{9},{0},{0},{0}},
    TGN_INFINITE_REPEAT, 0x0022},

    // MESSAGE WAITING pair (350 Hz, 440 Hz) 
    {{{MILLISECONDS_TO_SAMPLES(100), MILLISECONDS_TO_SAMPLES(100)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_350, TGN_COEF_350}, {TGN_YN_2_440, TGN_COEF_440}, {0x0000, 0x0000}, {0x0000, 0x0000}}, 
    {{0},{0},{0},{0}},
    0x0009, 0x0002},

	// REORDER pair (480 Hz, 620 Hz)
	{{{MILLISECONDS_TO_SAMPLES(250),  MILLISECONDS_TO_SAMPLES(250)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_480, TGN_COEF_480}, {TGN_YN_2_620, TGN_COEF_620}, {0x0000, 0x0000}, {0x0000, 0x0000}},
  {{0},{0},{0},{0}},
	TGN_INFINITE_REPEAT, 0x0002},
	
	// CALL WAITING tone (440 Hz)
	{{{MILLISECONDS_TO_SAMPLES(400), MILLISECONDS_TO_SAMPLES(100)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_440, TGN_COEF_440}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
  {{0},{0},{0},{0}},
	0x0000, 0x0001},
	
    // CALL WAITING 2 tone (440 Hz) 
    {{{MILLISECONDS_TO_SAMPLES(100), MILLISECONDS_TO_SAMPLES(100)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_440, TGN_COEF_440}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}}, 
    {{0},{0},{0},{0}},
    0x0001, 0x0001},

    // CALL WAITING 3 tone (440 Hz) 
    {{{MILLISECONDS_TO_SAMPLES(100), MILLISECONDS_TO_SAMPLES(100)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_440, TGN_COEF_440}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}}, 
    {{0},{0},{0},{0}},
    0x0002, 0x0001},
    
    // CALL WAITING 4 tone (440 Hz) 
    {{{MILLISECONDS_TO_SAMPLES(100), MILLISECONDS_TO_SAMPLES(100)}, {MILLISECONDS_TO_SAMPLES(300), MILLISECONDS_TO_SAMPLES(100)},
    {MILLISECONDS_TO_SAMPLES(100), 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_440, TGN_COEF_440}, {TGN_YN_2_440, TGN_COEF_440}, {TGN_YN_2_440, TGN_COEF_440}, {TGN_YN_2_440, TGN_COEF_440}}, 
    {{0},{0},{0},{0}},
    0x0000, 0x0111},
    
	// HOLD tone (500 Hz)
	{{{MILLISECONDS_TO_SAMPLES(100), MILLISECONDS_TO_SAMPLES(150)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_500, TGN_COEF_500}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
  {{0},{0},{0},{0}},
	0x0002, 0x0001},
	
    // CONFIRMATION TONE pair (440 Hz, 350 Hz) 
    {{{MILLISECONDS_TO_SAMPLES(100), MILLISECONDS_TO_SAMPLES(100)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_440, TGN_COEF_440}, {TGN_YN_2_350, TGN_COEF_350}, {0x0000, 0x0000}, {0x0000, 0x0000}}, 
    {{0},{0},{0},{0}},
    0x0002, 0x0001},

    // PERMANENT SIGNAL TONE (480 Hz)
    {{{(short)TGN_INFINITE_REPEAT,	0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_480, TGN_COEF_480}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}}, 
    {{0},{0},{0},{0}},
    TGN_INFINITE_REPEAT, 0x0001}, 
    
    // REMINDER RING pair (440 Hz, 480 Hz)     
    {{{MILLISECONDS_TO_SAMPLES(500), MILLISECONDS_TO_SAMPLES(500)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_440, TGN_COEF_440}, {TGN_YN_2_480, TGN_COEF_480}, {0x0000, 0x0000}, {0x0000, 0x0000}},       
    {{0},{0},{0},{0}},
    0x0000, 0x0001},

	// dummy
	{{{MILLISECONDS_TO_SAMPLES(250),  MILLISECONDS_TO_SAMPLES(250)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_480, TGN_COEF_480}, {TGN_YN_2_620, TGN_COEF_620}, {0x0000, 0x0000}, {0x0000, 0x0000}},
  {{0},{0},{0},{0}},
	TGN_INFINITE_REPEAT, 0x0002},
	
	// ZIP ZIP tone (480 Hz)
	{{{MILLISECONDS_TO_SAMPLES(300), MILLISECONDS_TO_SAMPLES(100)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_480, TGN_COEF_480}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
  {{0},{0},{0},{0}},
	0x0001, 0x0001},
	
	// ZIP tone (480 Hz)
	{{{MILLISECONDS_TO_SAMPLES(300), MILLISECONDS_TO_SAMPLES(100)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_480, TGN_COEF_480}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
  {{0},{0},{0},{0}},
	0x0000, 0x0001},
	
	// BEEP BONK tone (1000 Hz)
	{{{MILLISECONDS_TO_SAMPLES(2000), MILLISECONDS_TO_SAMPLES(100)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_1000, TGN_COEF_1000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
  {{0},{0},{0},{0}},
	0x0000, 0x0001},
	
	// TODO: the next two don't quite match the definitions...

	// RECORDING TONE - must use multiple offs to prevent overflow of short type
    {{{BEEP_REC_ON, BEEP_REC_OFF}, {0x0000, BEEP_REC_OFF}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_1400, TGN_COEF_1400}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}}, 
    {{0},{0},{0},{0}},
    TGN_INFINITE_REPEAT, 0x0001},

	// RECORDING TONE - must use multiple offs to prevent overflow of short type
    {{{BEEP_REC_ON, BEEP_REC_OFF}, {0x0000, BEEP_REC_OFF}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_1400, TGN_COEF_1400}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}}, 
    {{0},{0},{0},{0}},
    TGN_INFINITE_REPEAT, 0x0001},

	// MONITORING TONE - must use multiple offs to prevent overflow of short type
    {{{BEEP_MON_ON1, BEEP_MON_OFF1}, {BEEP_MON_ON2, BEEP_MON_OFF2},{0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_480, TGN_COEF_480}, {TGN_YN_2_480, TGN_COEF_480}, {0x0000, 0x0000}, {0x0000, 0x0000}}, 
    {{0},{0},{0},{0}},
    TGN_INFINITE_REPEAT, 0x0011},

	// SECURE TONE
    {{{MILLISECONDS_TO_SAMPLES(333), 0x0000}, {MILLISECONDS_TO_SAMPLES(333), 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_425, TGN_COEF_425}, {TGN_YN_2_300, TGN_COEF_300}, {0x0000, 0x0000}, {0x0000, 0x0000}}, 
    {{0},{0},{0},{0}},
    0x0002, 0x0011},
/*
	// 10  Milliwatt (1004 Hz, -15 dB)
	{{{0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_1MW_neg15dBm, TGN_COEF_1MW_neg15dBm}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{0,0,0,0},
	0x0000, 0x0001},
	
	// 12  PRECEDENCE_RINGBACK_TONE - Precedence Ringback pair (440 Hz, 480 Hz), JIEO Technical Report 8249
	{{{MILLISECONDS_TO_SAMPLES(1640), MILLISECONDS_TO_SAMPLES(360)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_440_PREC_RB, TGN_COEF_440_PREC_RB}, {TGN_YN_2_480_PREC_RB, TGN_COEF_480_PREC_RB}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{0,0,0,0},
	TGN_INFINITE_REPEAT, 0x0002},
	
	// 13  PREEMPTION_TONE - Preemption pair (440 Hz, 620 Hz), JIEO Technical Report 8249
	{{{(short)TGN_INFINITE_REPEAT, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_440_PREEMP, TGN_COEF_440_PREEMP}, {TGN_YN_2_620_PREEMP, TGN_COEF_620_PREEMP}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{0,0,0,0},
	TGN_INFINITE_REPEAT, 0x0002},
	
	// 14  PRECEDENCE_CALL_WAITING_TONE - Precedence Call Waiting (440 Hz), JIEO Technical Report 8249
	// This tone is 3 short bursts followed by a 9.7 seconds of silience.  Since we cannot
	// exceed 8.192 seconds (2 bytes), we split the 9.7 and used the last tone slot. (descriptor changed from 0x0111 to 0x1111)
	{{{MILLISECONDS_TO_SAMPLES(80), MILLISECONDS_TO_SAMPLES(20)}, {MILLISECONDS_TO_SAMPLES(80), MILLISECONDS_TO_SAMPLES(20)},
	{MILLISECONDS_TO_SAMPLES(80), 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_440_PREC_CW, TGN_COEF_440_PREC_CW}, {TGN_YN_2_440_PREC_CW, TGN_COEF_440_PREC_CW}, {TGN_YN_2_440_PREC_CW, TGN_COEF_440_PREC_CW}, {0x0000, 0x0000}},
	{0,0,0,0},
	0x0000, 0x1111},
	
	// 15  MUTE ON tone (600 Hz)
	{{{MILLISECONDS_TO_SAMPLES(100), MILLISECONDS_TO_SAMPLES(100)}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_600, TGN_COEF_600}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{0,0,0,0},
	0x0000, 0x0001},
	
	// 16  MUTE OFF tone (600 Hz)
	{{{MILLISECONDS_TO_SAMPLES(100), MILLISECONDS_TO_SAMPLES(100)}, {MILLISECONDS_TO_SAMPLES(100), MILLISECONDS_TO_SAMPLES(200)},
    {0x0000, 0x0000}, {0x0000, 0x0000}},
	{{TGN_YN_2_600, TGN_COEF_600}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}},
	{0,0,0,0},
	0x0000, 0x0011},

    // 26  Single beep - must use multiple offs to prevent overflow of short type
    {{{BEEP_REC_ON, BEEP_REC_OFF}, {0x0000, BEEP_REC_OFF}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_1400, TGN_COEF_1400}, {0x0000, 0x0000}, {0x0000, 0x0000}, {0x0000, 0x0000}}, 
	{0,0,0,0},
    0, 0x0001}, 

    // 27  Single beep monitoring
    {{{BEEP_MON_ON1, BEEP_MON_OFF1}, {BEEP_MON_ON2, BEEP_MON_OFF2}, {0x0000, 0x0000}, {0x0000, 0x0000}},
    {{TGN_YN_2_480, TGN_COEF_480}, {TGN_YN_2_480, TGN_COEF_480}, {0x0000, 0x0000}, {0x0000, 0x0000}}, 
	{0,0,0,0},
    0, 0x0011}, 
*/
};

// ----------------------------------------------------------------------------
WebrtcToneGenerator::WebrtcToneGenerator( ToneType type )
{
	TONE_TABLE_TYPE *tone = &ToneTable[type];

	// fill in tone memory
	m_SinewaveIdx = 0;
	m_CadenceIdx = 0;
	memcpy( m_Cadence, tone->Cadence, sizeof( m_Cadence ) );
	for ( int i = 0; i < MAX_TONEGENS; i++ )
	{
		m_Sinewave[i].Coef = tone->Coefmem[i].FilterCoef;
		m_Sinewave[i].Yn_1 = 0;
		m_Sinewave[i].Yn_2 = tone->Coefmem[i].FilterMemory;
	}
	m_RepeatCount = tone->RepeatCount;
	m_Descriptor  = tone->Descriptor;

	// set first sample
	m_Sample = m_Cadence[0];
}

// ----------------------------------------------------------------------------
void WebrtcToneGenerator::ToneGen( PSINEWAVE param, short *dst, unsigned long length, unsigned long numTones )
{
	unsigned long j;
	unsigned long i;
	long  A, B;
	short T;

	memset( dst, 0, length * sizeof( short ) );

	for ( i = 0; i < numTones; i++ )
	{
		for ( j = 0; j < length; j++ )
		{
			A = -(((long)param->Yn_2) << 15);
			T = param->Yn_1;
			param->Yn_2 = param->Yn_1;
			B = T * ((long)param->Coef) * 2;
			A = A + B;

			// this is evidently intended to "clip", but I don't think the math really works...
			if ( A >= (long)2147483647 )
			{
				A = (long)2147483647;
			}
			if ( A <= (long)-2147483647 )
			{
				A = (long)-2147483647;
			}
			param->Yn_1 = (short)(A >> 15);
			dst[j] += (short)(A >> 15);
		}
		param++;
	}
}

// Webrtc InStream implementation
int WebrtcToneGenerator::Read( void *buf, int len )
{
	return TGNGenerateTone( (short *)buf, (unsigned long)(len/sizeof(short)) ) ? len : 0;
}

// ----------------------------------------------------------------------------
bool WebrtcToneGenerator::TGNGenerateTone( short *dst, unsigned long length )
{
	unsigned long numTone = 0;

	// tone or silent period done
	if ( m_Sample == 0 )
	{
		// the current ON or OFF duration has expired

		// go to the next ON or OFF duration
		m_CadenceIdx = (m_CadenceIdx + 1) & (MAX_CADENCES - 1);

		// look for the next non-zero ON or OFF duration in the sequence
		while ( (m_CadenceIdx != 0) && (m_Cadence[m_CadenceIdx] == 0) )
		{
			m_CadenceIdx = (m_CadenceIdx + 1) & (MAX_CADENCES - 1);
		}

		// set the duration to the next ON or OFF duration
		m_Sample = m_Cadence[m_CadenceIdx];

		// the complete ON/OFF sequence has been done => decrease the number of repeats
		if ( m_CadenceIdx == 0 )
		{
			// reset to beginning of parameters
			m_SinewaveIdx = 0;

			// finite number of repeats
			if ( m_RepeatCount != TGN_INFINITE_REPEAT )
			{
				// no more repeats -> stop the tone generator
				if ( m_RepeatCount <= 0 )
				{
					return false;
				}
				else
				{
					m_RepeatCount--;
				}
			}
		}
	}

	// corresponds to the ON duration
	if ( (m_CadenceIdx & 0x1) == 0 )
	{
		numTone = (m_Descriptor >> (m_CadenceIdx << 1)) & 0xf;

		ToneGen( &m_Sinewave[m_SinewaveIdx], dst, length, numTone );
	}
	else	// insert silence
	{
		memset( dst, 0, length * sizeof( short ) );
	}

	if ( m_Sample != TGN_INFINITE_REPEAT )
	{
		if ( (length) < m_Sample )
		{
			m_Sample -= length;
		}
		else
		{
			m_Sample = 0;
		}

		if ( !m_Sample )
		{
			// corresponds to the ON duration
			if ( (m_CadenceIdx & 0x1) == 0 )
			{
				m_SinewaveIdx += numTone;
			}
		}
	}
	return true;
}

} // namespace CSF

#endif
