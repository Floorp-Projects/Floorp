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

#ifndef _Included_cip_mmgr_MediaDefinitions
#define _Included_cip_mmgr_MediaDefinitions
#ifdef __cplusplus
extern "C" {
#endif
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_NONSTANDARD
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_NONSTANDARD 1L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ALAW64K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ALAW64K 2L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ALAW56K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ALAW56K 3L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ULAW64K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ULAW64K 4L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ULAW56K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G711ULAW56K 5L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G722_64K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G722_64K 6L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G722_56K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G722_56K 7L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G722_48K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G722_48K 8L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G7231_5P3K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G7231_5P3K 9L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G7231_6P3K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G7231_6P3K 10L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G728
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G728 11L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G729
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G729 12L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G729ANNEXA
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G729ANNEXA 13L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_IS11172AUDIOCAP
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_IS11172AUDIOCAP 14L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_IS13818AUDIOCAP
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_IS13818AUDIOCAP 15L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G729ANNEXB
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G729ANNEXB 16L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G729ANNEXAWANNEXB
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G729ANNEXAWANNEXB 17L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM_FULL_RATE
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM_FULL_RATE 18L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM_HALF_RATE
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM_HALF_RATE 19L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM_ENHANCED_FULL_RATE
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM_ENHANCED_FULL_RATE 20L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_WIDE_BAND_256K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_WIDE_BAND_256K 21L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_WIDE_BAND_128K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_WIDE_BAND_128K 22L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_DATA64
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_DATA64 23L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_DATA56
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_DATA56 24L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_GSM 25L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_ACTIVEVOICE
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_ACTIVEVOICE 26L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G726_32K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G726_32K 27L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G726_24K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G726_24K 28L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_G726_16K
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_G726_16K 29L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_H261
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_H261 30L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_H263
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_H263 31L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_T120
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_T120 32L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_H224
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_H224 33L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_PCM_44_1K_16BIT_STEREO
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_PCM_44_1K_16BIT_STEREO 34L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_PCM_44_1K_16BIT_MONO
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_PCM_44_1K_16BIT_MONO 35L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_ZORAN_VIDEO
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_ZORAN_VIDEO 36L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_LOGITECH_VIDEO
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_LOGITECH_VIDEO 37L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_RFC2833
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_RFC2833 38L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_ILBC20
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_ILBC20 39L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_ILBC30
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_ILBC30 40L
#undef cip_mmgr_MediaDefinitions_MEDIA_TYPE_ISAC
#define cip_mmgr_MediaDefinitions_MEDIA_TYPE_ISAC 41L


#undef cip_mmgr_MediaDefinitions_BANDWIDTH_NARROWBAND
#define cip_mmgr_MediaDefinitions_BANDWIDTH_NARROWBAND 200L
#undef cip_mmgr_MediaDefinitions_BANDWIDTH_WIDEBAND
#define cip_mmgr_MediaDefinitions_BANDWIDTH_WIDEBAND 201L

#undef cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_G711_BIT_POSITION
#define cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_G711_BIT_POSITION 1L
#undef cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_G729A_BIT_POSITION
#define cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_G729A_BIT_POSITION 2L
#undef cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_G729B_BIT_POSITION
#define cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_G729B_BIT_POSITION 4L
#undef cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_LINEAR_BIT_POSITION
#define cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_LINEAR_BIT_POSITION 8L
#undef cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_G722_BIT_POSITION
#define cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_G722_BIT_POSITION 16L
#undef cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_ILBC_BIT_POSITION
#define cip_mmgr_MediaDefinitions_MEDIA_RESOURCE_ILBC_BIT_POSITION 32L

#undef cip_mmgr_MediaDefinitions_FULLDUPLEX
#define cip_mmgr_MediaDefinitions_FULLDUPLEX 0L
#undef cip_mmgr_MediaDefinitions_HALFDUPLEX_DECODE
#define cip_mmgr_MediaDefinitions_HALFDUPLEX_DECODE 1L
#undef cip_mmgr_MediaDefinitions_HALFDUPLEX_ENCODE
#define cip_mmgr_MediaDefinitions_HALFDUPLEX_ENCODE 2L
#ifdef __cplusplus
}
#endif
#endif
