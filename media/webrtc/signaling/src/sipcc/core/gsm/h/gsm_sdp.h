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

#ifndef _GSM_SDP_H_
#define _GSM_SDP_H_


#define GSMSDP_VERSION_STR_LEN (20)

#define GSMSDP_MEDIA_VALID(media)  \
    ((media != NULL) && (media->refid != CC_NO_MEDIA_REF_ID))

#define GSMSDP_MEDIA_ENABLED(media) \
    (GSMSDP_MEDIA_VALID(media) && (media->src_port != 0))

#define GSMSDP_MEDIA_COUNT(dcb) \
    (SLL_LITE_NODE_COUNT(&dcb->media_list))

#define GSMSDP_FIRST_MEDIA_ENTRY(dcb) \
    ((fsmdef_media_t *)SLL_LITE_LINK_HEAD(&dcb->media_list))

#define GSMSDP_NEXT_MEDIA_ENTRY(media) \
    ((fsmdef_media_t *)SLL_LITE_LINK_NEXT_NODE(media))

#define GSMSDP_FOR_MEDIA_LIST(media, start_media, end_media, dcb) \
    for (media = start_media; (media != NULL);                \
         media = (media != end_media ?                        \
                   GSMSDP_NEXT_MEDIA_ENTRY(media) : NULL)) 

#define GSMSDP_FOR_ALL_MEDIA(media, dcb) \
    for (media = GSMSDP_FIRST_MEDIA_ENTRY(dcb); (media != NULL); \
         media = GSMSDP_NEXT_MEDIA_ENTRY(media))

cc_causes_t gsmsdp_create_local_sdp(fsmdef_dcb_t *dcb_p); 
void gsmsdp_create_options_sdp(cc_sdp_t **sdp_pp);
void gsmsdp_reset_local_sdp_media(fsmdef_dcb_t *dcb, fsmdef_media_t *media, 
                                  boolean hold);
void gsmsdp_set_local_sdp_direction(fsmdef_dcb_t *dcb_p, fsmdef_media_t *media,
                                    sdp_direction_e direction);
void gsmsdp_set_local_hold_sdp(fsmdef_dcb_t *dcb, fsmdef_media_t *media);
void gsmsdp_set_local_resume_sdp(fsmdef_dcb_t *dcb, fsmdef_media_t *media);
cc_causes_t gsmsdp_negotiate_answer_sdp(fsm_fcb_t *fcb,
                                        cc_msgbody_info_t *msg_body);
cc_causes_t gsmsdp_negotiate_offer_sdp(fsm_fcb_t *fcb,
                                       cc_msgbody_info_t *msg_body,
                                       boolean init);
boolean gsmsdp_sdp_differs_from_previous_sdp(boolean rcv_only,
                                             fsmdef_media_t *media);
cc_causes_t gsmsdp_encode_sdp(cc_sdp_t *sdp_p, cc_msgbody_info_t *msg_body);
cc_causes_t gsmsdp_encode_sdp_and_update_version(fsmdef_dcb_t *dcb_p,
                                                 cc_msgbody_info_t *msg_body);
void gsmsdp_free(fsmdef_dcb_t *dcb_p);
fsmdef_media_t *gsmsdp_find_audio_media(fsmdef_dcb_t *dcb_p);

#define gsmsdp_is_srtp_supported()          (TRUE)
extern void gsmsdp_init_sdp_media_transport(fsmdef_dcb_t *dcb,
                                            void *sdp,
                                            fsmdef_media_t *media);
extern void gsmsdp_reset_sdp_media_transport(fsmdef_dcb_t *dcb,
                                             void *sdp,
                                             fsmdef_media_t *media,
                                             boolean hold);
extern sdp_transport_e gsmsdp_negotiate_media_transport(fsmdef_dcb_t *dcb_p,
                                                        cc_sdp_t *cc_sdp_p,
                                                        boolean offer,
                                                        fsmdef_media_t *media,
                                                        uint16_t *inst_num);
extern void gsmsdp_update_local_sdp_media_transport(fsmdef_dcb_t *dcb_p,
                                                    void *sdp_p,
                                                    fsmdef_media_t *media,
                                                    sdp_transport_e transport,
                                                    boolean all);
extern void gsmsdp_update_negotiated_transport(fsmdef_dcb_t *dcb_p,
                                               cc_sdp_t *cc_sdp_p,
                                               fsmdef_media_t *media,
                                               uint16_t crypto_inst,
                                               sdp_transport_e transport);
extern void gsmsdp_update_crypto_transmit_key(fsmdef_dcb_t *dcb_p,
                                              fsmdef_media_t *media,
                                              boolean offer,
                                              boolean initial_offer,
                                              sdp_direction_e direction);
extern void gsmsdp_set_media_transport_for_option(void *sdp, uint16 level);
extern boolean gsmsdp_is_crypto_ready(fsmdef_media_t *media, boolean rx);
extern boolean gsmsdp_is_media_encrypted(fsmdef_dcb_t *dcb_p);
extern boolean gsmsdp_crypto_params_change(boolean rcv_only,
                                           fsmdef_media_t *media);
extern void gsmsdp_crypto_reset_params_change(fsmdef_media_t *media);
extern void gsmsdp_cache_crypto_keys(void);
extern boolean gsmsdp_create_free_media_list(void);
extern void gsmsdp_destroy_free_media_list(void);
extern void gsmsdp_init_media_list(fsmdef_dcb_t *dcb_p);
extern void gsmsdp_clean_media_list(fsmdef_dcb_t *dcb);
extern fsmdef_media_t *gsmsdp_find_media_by_refid(fsmdef_dcb_t *dcb_p,
                                                  media_refid_t refid);
extern boolean gsmsdp_handle_media_cap_change(fsmdef_dcb_t *dcb_p,
                                              boolean refresh, boolean hold);
extern boolean gsmsdp_update_local_sdp_media_capability(fsmdef_dcb_t *dcb_p,
                                              boolean refresh, boolean hold);
boolean is_gsmsdp_media_ip_updated_to_latest( fsmdef_dcb_t * dcb );
#endif

