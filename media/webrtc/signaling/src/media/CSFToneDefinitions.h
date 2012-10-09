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

/* Tone Coefficients and YN_2 values */
/* Based on 8kHz sampling rate	   */
#define MILLISECONDS_TO_SAMPLES(PERIOD)   (short)((PERIOD) << 3)

#define TGN_COEF_300        (short)31863
#define TGN_COEF_350        (short)31538
#define TGN_COEF_425        (short)30959
#define TGN_COEF_440        (short)30831
#define TGN_COEF_450        (short)30743
#define TGN_COEF_480        (short)30467
#define TGN_COEF_500        (short)30274
#define TGN_COEF_548        (short)29780
#define TGN_COEF_600        (short)29197
#define TGN_COEF_620        (short)28959
#define TGN_COEF_697        (short)27980
#define TGN_COEF_770        (short)26956
#define TGN_COEF_852        (short)25701
#define TGN_COEF_941        (short)24219
#define TGN_COEF_1209       (short)19073
#define TGN_COEF_1336       (short)16325
#define TGN_COEF_1400       (short)14876
#define TGN_COEF_1477       (short)13085
#define TGN_COEF_1633       (short)9315
#define TGN_COEF_1000       (short)23170
#define TGN_COEF_1MW        (short)23098
#define TGN_COEF_1MW_neg15dBm   (short)23098

#define TGN_YN_2_300        (short)-840
#define TGN_YN_2_350        (short)-814
#define TGN_YN_2_425        (short)-1966
#define TGN_YN_2_440        (short)-2032
#define TGN_YN_2_450        (short)-1384
#define TGN_YN_2_480        (short)-1104
#define TGN_YN_2_500        (short)-1148
#define TGN_YN_2_548        (short)-1252
#define TGN_YN_2_600        (short)-2270
#define TGN_YN_2_620        (short)-1404
#define TGN_YN_2_697        (short)-1561
#define TGN_YN_2_770        (short)-1706
#define TGN_YN_2_852        (short)-1861
#define TGN_YN_2_941        (short)-2021
#define TGN_YN_2_1209       (short)-2439
#define TGN_YN_2_1336       (short)-2601
#define TGN_YN_2_1400       (short)-5346    //tone level=-11.61 dBm0, same as CallWaiting CSCsd65600
#define TGN_YN_2_1477       (short)-2750
#define TGN_YN_2_1633       (short)-2875
#define TGN_YN_2_1000       (short)-1414
#define TGN_YN_2_1MW        (short)-16192
#define TGN_YN_2_1MW_neg15dBm   (short)-2879

// for MLPP tones
#define TGN_COEF_440_PREC_RB   (short)30831
#define TGN_COEF_480_PREC_RB   (short)30467
#define TGN_COEF_440_PREEMP    (short)30831
#define TGN_COEF_620_PREEMP    (short)28959
#define TGN_COEF_440_PREC_CW   (short)30831

#define TGN_YN_2_440_PREC_RB   (short)-1016
#define TGN_YN_2_480_PREC_RB   (short)-1104
#define TGN_YN_2_440_PREEMP    (short)-1016
#define TGN_YN_2_620_PREEMP    (short)-1404
#define TGN_YN_2_440_PREC_CW   (short)-1016


/* Based on 16kHz sampling rate */
/*
#define MILLISECONDS_TO_SAMPLES(PERIOD)   ((PERIOD) << 4)
*/

/* Tone Coefficients and YN_2 values */
/* Based on 16kHz sampling rate     */
/*
#define TGN_COEF_350           (short)32459
#define TGN_COEF_440           (short)32280
#define TGN_COEF_450           (short)32258
#define TGN_COEF_480           (short)32188
#define TGN_COEF_500           (short)32138
#define TGN_COEF_548           (short)32012
#define TGN_COEF_600           (short)31863
#define TGN_COEF_620           (short)31802
#define TGN_COEF_697           (short)31548
#define TGN_COEF_770           (short)31281
#define TGN_COEF_852           (short)30951
#define TGN_COEF_941           (short)30556
#define TGN_COEF_1209          (short)29144
#define TGN_COEF_1336          (short)28361
#define TGN_COEF_1477          (short)27409
#define TGN_COEF_1633          (short)26258
#define TGN_COEF_1000          (short)30274
#define TGN_COEF_1MW           (short)30254
#define TGN_COEF_1MW_neg15dBm  (short)30254

#define TGN_YN_2_350           (short)-410
#define TGN_YN_2_440           (short)-1031
#define TGN_YN_2_450           (short)-702
#define TGN_YN_2_480           (short)-561
#define TGN_YN_2_500           (short)-584
#define TGN_YN_2_548           (short)-640
#define TGN_YN_2_600           (short)-1166
#define TGN_YN_2_620           (short)-722
#define TGN_YN_2_697           (short)-810
#define TGN_YN_2_770           (short)-892
#define TGN_YN_2_852           (short)-984
#define TGN_YN_2_941           (short)-1083
#define TGN_YN_2_1209          (short)-1370
#define TGN_YN_2_1336          (short)-1502
#define TGN_YN_2_1477          (short)-1643
#define TGN_YN_2_1633          (short)-1794
#define TGN_YN_2_1000          (short)-764
#define TGN_YN_2_1MW           (short)-8768
#define TGN_YN_2_1MW_neg15dBm  (short)-1558

// for MLPP tones
#define TGN_COEF_440_PREC_RB   (short)32280
#define TGN_COEF_480_PREC_RB   (short)32188
#define TGN_COEF_440_PREEMP    (short)32280
#define TGN_COEF_620_PREEMP    (short)31802
#define TGN_COEF_440_PREC_CW   (short)32280

#define TGN_YN_2_440_PREC_RB   (short)-515
#define TGN_YN_2_480_PREC_RB   (short)-561
#define TGN_YN_2_440_PREEMP    (short)-515
#define TGN_YN_2_620_PREEMP    (short)-722
#define TGN_YN_2_440_PREC_CW   (short)-515
*/

#define BEEP_REC_ON		MILLISECONDS_TO_SAMPLES(500)
#define BEEP_REC_OFF    MILLISECONDS_TO_SAMPLES((15000 - 500) / 2)
#define BEEP_MON_ON1	MILLISECONDS_TO_SAMPLES(1500)
#define BEEP_MON_OFF1   MILLISECONDS_TO_SAMPLES(8000)
#define BEEP_MON_ON2	MILLISECONDS_TO_SAMPLES(500)
#define BEEP_MON_OFF2   MILLISECONDS_TO_SAMPLES(8000)

