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

#ifndef WebrtcTONEGENERATOR_H
#define WebrtcTONEGENERATOR_H

#ifndef _USE_CPVE

#include <CSFAudioTermination.h>
#include "common_types.h"

#define MAX_TONEGENS		4
#define MAX_CADENCES		4
#define MAX_REPEATCNTS		4
#define TGN_INFINITE_REPEAT	65535L

#define TG_DESCRIPTOR_CADENCE_MASK  0xF     // descriptor cadence mask
#define TG_MAX_CADENCES				MAX_CADENCES * sizeof(CADENCE_DURATION) / sizeof(unsigned short)
#define TG_MAX_REPEATCNTS			MAX_REPEATCNTS * sizeof (REPEAT_COUNT_TABLE) / sizeof(short)

namespace CSF {

	typedef struct {
		short OnDuration;
		short OffDuration;
	} CADENCE_DURATION;

	typedef struct {
		short FilterMemory;
		short FilterCoef;
	} FREQ_COEF_TABLE;

	typedef struct {
		short RepeatCount;
	} REPEAT_COUNT_TABLE;

	typedef struct {
		CADENCE_DURATION	Cadence[MAX_CADENCES];   // on/off pairs
		FREQ_COEF_TABLE		Coefmem[MAX_TONEGENS];   // coeff mem
		REPEAT_COUNT_TABLE	rCount[MAX_REPEATCNTS];
		unsigned short		RepeatCount;
		unsigned short		Descriptor;
	} TONE_TABLE_TYPE, *PTONE_TABLE_TYPE;

	class WebrtcToneGenerator : public webrtc::InStream
	{
	public:
		WebrtcToneGenerator( ToneType type );

		// InStream interface
		int Read( void *buf, int len );

	private:
		typedef struct {
			short Coef;
			short Yn_1;
			short Yn_2;
		} SINEWAVE, *PSINEWAVE;

		SINEWAVE			m_Sinewave[MAX_TONEGENS];
		unsigned long		m_SinewaveIdx;
		unsigned short		m_Cadence[TG_MAX_CADENCES];
		unsigned long		m_CadenceIdx;
		short				m_rCount[TG_MAX_REPEATCNTS];
		unsigned long		m_Sample;
		int					m_RepeatCount;
		unsigned short		m_Descriptor;
		short				m_CadenceRepeatCount;

		bool	TGNGenerateTone( short *dst, unsigned long length );
		void	ToneGen( PSINEWAVE param, short *dst, unsigned long length, unsigned long numTones );
	};

} // namespace CSF

#endif
#endif // WebrtcTONEGENERATOR_H
