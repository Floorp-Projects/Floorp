/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDP_PRIVATE_H_
#define _SDP_PRIVATE_H_


#include "sdp.h"

extern const sdp_attrarray_t sdp_attr[];
extern const sdp_namearray_t sdp_media[];
extern const sdp_namearray_t sdp_nettype[];
extern const sdp_namearray_t sdp_addrtype[];
extern const sdp_namearray_t sdp_transport[];
extern const sdp_namearray_t sdp_encrypt[];
extern const sdp_namearray_t sdp_payload[];
extern const sdp_namearray_t sdp_t38_rate[];
extern const sdp_namearray_t sdp_t38_udpec[];
extern const sdp_namearray_t sdp_qos_strength[];
extern const sdp_namearray_t sdp_qos_direction[];
extern const sdp_namearray_t sdp_qos_status_type[];
extern const sdp_namearray_t sdp_curr_type[];
extern const sdp_namearray_t sdp_des_type[];
extern const sdp_namearray_t sdp_conf_type[];
extern const sdp_namearray_t sdp_mediadir_role[];
extern const sdp_namearray_t sdp_fmtp_codec_param[];
extern const sdp_namearray_t sdp_fmtp_codec_param_val[];
extern const sdp_namearray_t sdp_silencesupp_pref[];
extern const sdp_namearray_t sdp_silencesupp_siduse[];
extern const sdp_namearray_t sdp_srtp_context_crypto_suite[];
extern const sdp_namearray_t sdp_bw_modifier_val[];
extern const sdp_namearray_t sdp_group_attr_val[];
extern const sdp_namearray_t sdp_src_filter_mode_val[];
extern const sdp_namearray_t sdp_rtcp_unicast_mode_val[];
extern const sdp_namearray_t sdp_rtcp_fb_type_val[];
extern const sdp_namearray_t sdp_rtcp_fb_nack_type_val[];
extern const sdp_namearray_t sdp_rtcp_fb_ack_type_val[];
extern const sdp_namearray_t sdp_rtcp_fb_ccm_type_val[];
extern const sdp_namearray_t sdp_setup_type_val[];
extern const sdp_namearray_t sdp_connection_type_val[];


extern const  sdp_srtp_crypto_suite_list sdp_srtp_crypto_suite_array[];
/* Function Prototypes */

/* sdp_access.c */
extern sdp_mca_t *sdp_find_media_level(sdp_t *sdp_p, u16 level);
extern sdp_bw_data_t* sdp_find_bw_line (void *sdp_ptr, u16 level, u16 inst_num);

/* sdp_attr.c */
extern sdp_result_e sdp_parse_attribute(sdp_t *sdp_p, u16 level,
                                        const char *ptr);
extern sdp_result_e sdp_parse_attr_simple_string(sdp_t *sdp_p,
                                     sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_simple_string(sdp_t *sdp_p,
                                     sdp_attr_t *attr_p, flex_string *fs);
extern sdp_result_e sdp_parse_attr_simple_u32(sdp_t *sdp_p,
                                     sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_simple_u32(sdp_t *sdp_p,
                                     sdp_attr_t *attr_p, flex_string *fs);
extern sdp_result_e sdp_parse_attr_simple_bool(sdp_t *sdp_p,
                                     sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_simple_bool(sdp_t *sdp_p,
                                     sdp_attr_t *attr_p, flex_string *fs);
extern sdp_result_e sdp_parse_attr_maxprate(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_parse_attr_fmtp(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_fmtp(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_sctpmap(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                           const char *ptr);
extern sdp_result_e sdp_build_attr_sctpmap(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                           flex_string *fs);
extern sdp_result_e sdp_parse_attr_direction(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_direction(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_qos(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_qos(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_curr(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_curr (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_des(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_des (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_conf(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_conf (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_transport_map(sdp_t *sdp_p,
				     sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_transport_map(sdp_t *sdp_p,
				     sdp_attr_t *attr_p, flex_string *fs);
extern sdp_result_e sdp_parse_attr_subnet(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_subnet(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_t38_ratemgmt(sdp_t *sdp_p,
                                     sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_t38_ratemgmt(sdp_t *sdp_p,
                                     sdp_attr_t *attr_p, flex_string *fs);
extern sdp_result_e sdp_parse_attr_t38_udpec(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_t38_udpec(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_cap(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_cap(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_cpar(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_cpar(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_pc_codec(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_pc_codec(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_xcap(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                        const char *ptr);
extern sdp_result_e sdp_build_attr_xcap(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                        flex_string *fs);
extern sdp_result_e sdp_parse_attr_xcpar(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                         const char *ptr);
extern sdp_result_e sdp_build_attr_xcpar(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                         flex_string *fs);
extern sdp_result_e sdp_parse_attr_rtr(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr);
extern sdp_result_e sdp_build_attr_rtr(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     flex_string *fs);
extern sdp_result_e sdp_parse_attr_comediadir(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                              const char *ptr);
extern sdp_result_e sdp_build_attr_comediadir(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                              flex_string *fs);
extern sdp_result_e sdp_parse_attr_silencesupp(sdp_t *sdp_p,
                                               sdp_attr_t *attr_p,
                                               const char *ptr);
extern sdp_result_e sdp_build_attr_silencesupp(sdp_t *sdp_p,
                                               sdp_attr_t *attr_p,
                                               flex_string *fs);
extern sdp_result_e sdp_parse_attr_srtpcontext(sdp_t *sdp_p,
                                               sdp_attr_t *attr_p,
                                               const char *ptr);
extern sdp_result_e sdp_build_attr_srtpcontext(sdp_t *sdp_p,
                                               sdp_attr_t *attr_p,
                                               flex_string *fs);
extern sdp_result_e sdp_parse_attr_rtcp_fb(sdp_t *sdp_p,
                                           sdp_attr_t *attr_p,
                                           const char *ptr);
extern sdp_result_e sdp_build_attr_rtcp_fb(sdp_t *sdp_p,
                                           sdp_attr_t *attr_p,
                                           flex_string *fs);
extern sdp_result_e sdp_parse_attr_setup(sdp_t *sdp_p,
                                         sdp_attr_t *attr_p,
                                         const char *ptr);
extern sdp_result_e sdp_build_attr_setup(sdp_t *sdp_p,
                                         sdp_attr_t *attr_p,
                                         flex_string *fs);
extern sdp_result_e sdp_parse_attr_connection(sdp_t *sdp_p,
                                              sdp_attr_t *attr_p,
                                              const char *ptr);
extern sdp_result_e sdp_build_attr_connection(sdp_t *sdp_p,
                                              sdp_attr_t *attr_p,
                                              flex_string *fs);
extern sdp_result_e sdp_parse_attr_extmap(sdp_t *sdp_p,
                                          sdp_attr_t *attr_p,
                                          const char *ptr);
extern sdp_result_e sdp_build_attr_extmap(sdp_t *sdp_p,
                                          sdp_attr_t *attr_p,
                                          flex_string *fs);
extern sdp_result_e sdp_parse_attr_mptime(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_mptime(
    sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs);
extern sdp_result_e sdp_parse_attr_x_sidin(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_x_sidin(
    sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs);

extern sdp_result_e sdp_parse_attr_x_sidout(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_x_sidout(
    sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs);

extern sdp_result_e sdp_parse_attr_x_confid(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_x_confid(
    sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs);

extern sdp_result_e sdp_parse_attr_group(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_group(
    sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs);

extern sdp_result_e sdp_parse_attr_source_filter(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_source_filter(
    sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs);

extern sdp_result_e sdp_parse_attr_rtcp_unicast(
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_build_attr_rtcp_unicast(
    sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs);

extern sdp_result_e sdp_build_attr_ice_attr (
	sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs);
extern sdp_result_e sdp_parse_attr_ice_attr (
	sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);

extern sdp_result_e sdp_build_attr_rtcp_mux_attr (
	sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs);
extern sdp_result_e sdp_parse_attr_rtcp_mux_attr (
	sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);
extern sdp_result_e sdp_parse_attr_fingerprint_attr (
    sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr);

/* sdp_attr_access.c */
extern void sdp_free_attr(sdp_attr_t *attr_p);
extern sdp_result_e sdp_find_attr_list(sdp_t *sdp_p, u16 level, u8 cap_num,
                                       sdp_attr_t **attr_p, char *fname);
extern sdp_attr_t *sdp_find_attr(sdp_t *sdp_p, u16 level, u8 cap_num,
                                 sdp_attr_e attr_type, u16 inst_num);
extern sdp_attr_t *sdp_find_capability(sdp_t *sdp_p, u16 level, u8 cap_num);

/* sdp_config.c */
extern tinybool sdp_verify_conf_ptr(sdp_conf_options_t *conf_p);

/* sdp_main.c */
extern const char *sdp_get_attr_name(sdp_attr_e attr_type);
extern const char *sdp_get_media_name(sdp_media_e media_type);
extern const char *sdp_get_network_name(sdp_nettype_e network_type);
extern const char *sdp_get_address_name(sdp_addrtype_e addr_type);
extern const char *sdp_get_transport_name(sdp_transport_e transport_type);
extern const char *sdp_get_encrypt_name(sdp_encrypt_type_e encrypt_type);
extern const char *sdp_get_payload_name(sdp_payload_e payload);
extern const char *sdp_get_t38_ratemgmt_name(sdp_t38_ratemgmt_e rate);
extern const char *sdp_get_t38_udpec_name(sdp_t38_udpec_e udpec);
extern const char *sdp_get_qos_strength_name(sdp_qos_strength_e strength);
extern const char *sdp_get_qos_direction_name(sdp_qos_dir_e direction);
extern const char *sdp_get_qos_status_type_name(sdp_qos_status_types_e status_type);
extern const char *sdp_get_curr_type_name(sdp_curr_type_e curr_type);
extern const char *sdp_get_des_type_name(sdp_des_type_e des_type);
extern const char *sdp_get_conf_type_name(sdp_conf_type_e conf_type);
extern const char *sdp_get_mediadir_role_name (sdp_mediadir_role_e role);
extern const char *sdp_get_silencesupp_pref_name(sdp_silencesupp_pref_e pref);
extern const char *sdp_get_silencesupp_siduse_name(sdp_silencesupp_siduse_e
                                                   siduse);

extern const char *sdp_get_bw_modifier_name(sdp_bw_modifier_e bw_modifier);
extern const char *sdp_get_group_attr_name(sdp_group_attr_e group_attr);
extern const char *sdp_get_src_filter_mode_name(sdp_src_filter_mode_e type);
extern const char *sdp_get_rtcp_unicast_mode_name(sdp_rtcp_unicast_mode_e type);

extern tinybool sdp_verify_sdp_ptr(sdp_t *sdp_p);


/* sdp_tokens.c */
extern sdp_result_e sdp_parse_version(sdp_t *sdp_p, u16 token,
                                      const char *ptr);
extern sdp_result_e sdp_build_version(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_owner(sdp_t *sdp_p, u16 token,
                                    const char *ptr);
extern sdp_result_e sdp_build_owner(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_sessname(sdp_t *sdp_p, u16 token,
                                       const char *ptr);
extern sdp_result_e sdp_build_sessname(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_sessinfo(sdp_t *sdp_p, u16 token,
                                       const char *ptr);
extern sdp_result_e sdp_build_sessinfo(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_uri(sdp_t *sdp_p, u16 token, const char *ptr);
extern sdp_result_e sdp_build_uri(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_email(sdp_t *sdp_p, u16 token, const char *ptr);
extern sdp_result_e sdp_build_email(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_phonenum(sdp_t *sdp_p, u16 token,
                                       const char *ptr);
extern sdp_result_e sdp_build_phonenum(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_connection(sdp_t *sdp_p, u16 token,
                                         const char *ptr);
extern sdp_result_e sdp_build_connection(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_bandwidth(sdp_t *sdp_p, u16 token,
                                        const char *ptr);
extern sdp_result_e sdp_build_bandwidth(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_timespec(sdp_t *sdp_p, u16 token,
                                       const char *ptr);
extern sdp_result_e sdp_build_timespec(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_repeat_time(sdp_t *sdp_p, u16 token,
                                          const char *ptr);
extern sdp_result_e sdp_build_repeat_time(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_timezone_adj(sdp_t *sdp_p, u16 token,
                                           const char *ptr);
extern sdp_result_e sdp_build_timezone_adj(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_encryption(sdp_t *sdp_p, u16 token,
                                         const char *ptr);
extern sdp_result_e sdp_build_encryption(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_media(sdp_t *sdp_p, u16 token, const char *ptr);
extern sdp_result_e sdp_build_media(sdp_t *sdp_p, u16 token, flex_string *fs);
extern sdp_result_e sdp_parse_attribute(sdp_t *sdp_p, u16 token,
                                        const char *ptr);
extern sdp_result_e sdp_build_attribute(sdp_t *sdp_p, u16 token, flex_string *fs);

extern void sdp_parse_payload_types(sdp_t *sdp_p, sdp_mca_t *mca_p,
                                     const char *ptr);
extern sdp_result_e sdp_parse_multiple_profile_payload_types(sdp_t *sdp_p,
                                               sdp_mca_t *mca_p,
                                               const char *ptr);
extern sdp_result_e
sdp_parse_attr_sdescriptions(sdp_t *sdp_p, sdp_attr_t *attr_p,
                             const char *ptr);

extern sdp_result_e
sdp_build_attr_sdescriptions(sdp_t *sdp_p, sdp_attr_t *attr_p,
                             flex_string *fs);


/* sdp_utils.c */
extern sdp_mca_t *sdp_alloc_mca(void);
extern tinybool sdp_validate_maxprate(const char *string_parm);
extern char *sdp_findchar(const char *ptr, char *char_list);
extern const char *sdp_getnextstrtok(const char *str, char *tokenstr, unsigned tokenstr_len,
                               const char *delim, sdp_result_e *result);
extern u32 sdp_getnextnumtok(const char *str, const char **str_end,
                             const char *delim, sdp_result_e *result);
extern u32 sdp_getnextnumtok_or_null(const char *str, const char **str_end,
                                     const char *delim, tinybool *null_ind,
                                     sdp_result_e *result);
extern tinybool sdp_getchoosetok(const char *str, const char **str_end,
                                 const char *delim, sdp_result_e *result);

extern
tinybool verify_sdescriptions_mki(char *buf, char *mkiVal, u16 *mkiLen);

extern
tinybool verify_sdescriptions_lifetime(char *buf);

/* sdp_services_xxx.c */
extern void sdp_dump_buffer(char *_ptr, int _size_bytes);

tinybool sdp_checkrange(sdp_t *sdp, char *num, ulong* lval);

#endif /* _SDP_PRIVATE_H_ */
