/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <errno.h>
#include <limits.h>
#include <stdio.h>

#include "plstr.h"
#include "sdp_os_defs.h"
#include "sdp.h"
#include "sdp_private.h"
#include "sdp_base64.h"

#include "CSFLog.h"

static const char* logTag = "sdp_attr";

/*
 * Macro for sdp_build_attr_fmtp
 * Adds name-value pair where value is char*
 */
#define FMTP_BUILD_STRING(condition, name, value) \
  if ((condition)) { \
    sdp_append_name_and_string(fs, (name), (value), semicolon); \
    semicolon = TRUE; \
  }

/*
 * Macro for sdp_build_attr_fmtp
 * Adds name-value pair where value is unsigned
 */
#define FMTP_BUILD_UNSIGNED(condition, name, value) \
  if ((condition)) { \
    sdp_append_name_and_unsigned(fs, (name), (value), semicolon); \
    semicolon = TRUE; \
  }

/*
 * Macro for sdp_build_attr_fmtp
 * Adds flag string on condition
 */
#define FMTP_BUILD_FLAG(condition, name) \
  if ((condition)) { \
    if (semicolon) { \
      flex_string_append(fs, ";"); \
    } \
    flex_string_append(fs, name); \
    semicolon = TRUE; \
  }

static int find_token_enum(const char *attr_name,
                           sdp_t *sdp_p,
                           const char **ptr,
                           const sdp_namearray_t *types,
                           int type_count,
                           int unknown_value)
{
    sdp_result_e  result = SDP_SUCCESS;
    char          tmp[SDP_MAX_STRING_LEN+1];
    int           i;

    *ptr = sdp_getnextstrtok(*ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: problem parsing %s", sdp_p->debug_str, attr_name);
        sdp_p->conf_p->num_invalid_param++;
        return -1;
    }

    for (i=0; i < type_count; i++) {
        if (!cpr_strncasecmp(tmp, types[i].name, types[i].strlen)) {
            return i;
        }
    }
    return unknown_value;
}

/*
 * Helper function for adding nv-pair where value is string.
 */
static void sdp_append_name_and_string(flex_string *fs,
  const char *name,
  const char *value,
  tinybool semicolon)
{
  flex_string_sprintf(fs, "%s%s=%s",
    semicolon ? ";" : "",
    name,
    value);
}

/*
 * Helper function for adding nv-pair where value is unsigned.
 */
static void sdp_append_name_and_unsigned(flex_string *fs,
  const char *name,
  unsigned int value,
  tinybool semicolon)
{
  flex_string_sprintf(fs, "%s%s=%u",
    semicolon ? ";" : "",
    name,
    value);
}

/* Function:    sdp_parse_attribute
 * Description: Figure out the type of attribute and call the appropriate
 *              parsing routine.  If parsing errors are encountered,
 *              warnings will be printed and the attribute will be ignored.
 *              Unrecognized/invalid attributes do not cause overall parsing
 *              errors.  All errors detected are noted as warnings.
 * Parameters:  sdp_p       The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              ptr         Pointer to the attribute string to parse.
 */
sdp_result_e sdp_parse_attribute (sdp_t *sdp_p, uint16_t level, const char *ptr)
{
    int           i;
    uint8_t            xcpar_flag = FALSE;
    sdp_result_e  result;
    sdp_mca_t    *mca_p=NULL;
    sdp_attr_t   *attr_p;
    sdp_attr_t   *next_attr_p;
    sdp_attr_t   *prev_attr_p = NULL;
    char          tmp[SDP_MAX_STRING_LEN];

    /* Validate the level */
    if (level != SDP_SESSION_LEVEL) {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (SDP_FAILURE);
        }
    }

    /* Find the attribute type. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), ": \t", &result);
    if (ptr == NULL) {
        sdp_parse_error(sdp_p,
          "%s No attribute type specified, parse failed.", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    if (ptr[0] == ':') {
        /* Skip the ':' char for parsing attribute parameters. */
        ptr++;
    }
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
          "%s No attribute type specified, parse failed.", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = (sdp_attr_t *)SDP_MALLOC(sizeof(sdp_attr_t));
    if (attr_p == NULL) {
        sdp_p->conf_p->num_no_resource++;
        return (SDP_NO_RESOURCE);
    }
    attr_p->line_number = sdp_p->parse_line;
    attr_p->type = SDP_ATTR_INVALID;
    attr_p->next_p = NULL;
    for (i=0; i < SDP_MAX_ATTR_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_attr[i].name, sdp_attr[i].strlen) == 0) {
            attr_p->type = (sdp_attr_e)i;
            break;
        }
    }
    if (attr_p->type == SDP_ATTR_INVALID) {
        sdp_parse_error(sdp_p,
          "%s Warning: Unrecognized attribute (%s) ",
          sdp_p->debug_str, tmp);
        sdp_free_attr(attr_p);
        return (SDP_SUCCESS);
    }

    /* If this is an X-cpar or cpar attribute, set the flag.  The attribute
     * type will be changed by the parse. */
    if ((attr_p->type == SDP_ATTR_X_CPAR) ||
        (attr_p->type == SDP_ATTR_CPAR)) {
        xcpar_flag = TRUE;
    }

    /* Parse the attribute. */
    result = sdp_attr[attr_p->type].parse_func(sdp_p, attr_p, ptr);
    if (result != SDP_SUCCESS) {
        sdp_free_attr(attr_p);
        /* Return success so the parse won't fail.  We don't want to
         * fail on errors with attributes but just ignore them.
         */
        return (SDP_SUCCESS);
    }

    /* If this was an X-cpar/cpar attribute, it was hooked into the X-cap/cdsc
     * structure, so we're finished.
     */
    if (xcpar_flag == TRUE) {
        return (result);
    }

    /* Add the attribute in the appropriate place. */
    if (level == SDP_SESSION_LEVEL) {
        for (next_attr_p = sdp_p->sess_attrs_p; next_attr_p != NULL;
             prev_attr_p = next_attr_p,
                 next_attr_p = next_attr_p->next_p) {
            ; /* Empty for */
        }
        if (prev_attr_p == NULL) {
            sdp_p->sess_attrs_p = attr_p;
        } else {
            prev_attr_p->next_p = attr_p;
        }
    } else {
        for (next_attr_p = mca_p->media_attrs_p; next_attr_p != NULL;
             prev_attr_p = next_attr_p,
                 next_attr_p = next_attr_p->next_p) {
            ; /* Empty for */
        }
        if (prev_attr_p == NULL) {
            mca_p->media_attrs_p = attr_p;
        } else {
            prev_attr_p->next_p = attr_p;
        }
    }

    return (result);
}

/* Build all of the attributes defined for the specified level. */
sdp_result_e sdp_build_attribute (sdp_t *sdp_p, uint16_t level, flex_string *fs)
{
    sdp_attr_t   *attr_p;
    sdp_mca_t    *mca_p=NULL;
    sdp_result_e  result;

    if (level == SDP_SESSION_LEVEL) {
        attr_p = sdp_p->sess_attrs_p;
    } else {
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (SDP_FAILURE);
        }
        attr_p = mca_p->media_attrs_p;
    }
    /* Re-initialize the current capability number for this new level. */
    sdp_p->cur_cap_num = 1;

    /* Build all of the attributes for this level. Note that if there
     * is a problem building an attribute, we don't fail but just ignore it.*/
    while (attr_p != NULL) {
        if (attr_p->type >= SDP_MAX_ATTR_TYPES) {
            if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
                CSFLogDebug(logTag, "%s Invalid attribute type to build (%u)",
                         sdp_p->debug_str, (unsigned)attr_p->type);
            }
        } else {
            result = sdp_attr[attr_p->type].build_func(sdp_p, attr_p, fs);

            if (result != SDP_SUCCESS) {
              CSFLogError(logTag, "%s error building attribute %d", __FUNCTION__, result);
              return result;
            }

            if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
                SDP_PRINT("%s Built a=%s attribute line", sdp_p->debug_str,
                          sdp_get_attr_name(attr_p->type));
            }
        }
        attr_p = attr_p->next_p;
    }

    return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_simple_string (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                           const char *ptr)
{
    sdp_result_e  result;

    ptr = sdp_getnextstrtok(ptr, attr_p->attr.string_val,
      sizeof(attr_p->attr.string_val), " \t", &result);

    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No string token found for %s attribute",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type),
                      attr_p->attr.string_val);
        }
        return (SDP_SUCCESS);
    }
}

sdp_result_e sdp_build_attr_simple_string (sdp_t *sdp_p, sdp_attr_t *attr_p,
  flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%s\r\n", sdp_attr[attr_p->type].name,
    attr_p->attr.string_val);

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_simple_u32 (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                        const char *ptr)
{
    sdp_result_e  result;

    attr_p->attr.u32_val = sdp_getnextnumtok(ptr, &ptr, " \t", &result);

    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: Numeric token for %s attribute not found",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, %u", sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type), attr_p->attr.u32_val);
        }
        return (SDP_SUCCESS);
    }
}

sdp_result_e sdp_build_attr_simple_u32 (sdp_t *sdp_p, sdp_attr_t *attr_p,
  flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%u\r\n", sdp_attr[attr_p->type].name,
    attr_p->attr.u32_val);

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_simple_bool (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                         const char *ptr)
{
    sdp_result_e  result;

    if (sdp_getnextnumtok(ptr, &ptr, " \t", &result) == 0) {
        attr_p->attr.boolean_val = FALSE;
    } else {
        attr_p->attr.boolean_val= TRUE;
    }

    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: Boolean token for %s attribute not found",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            if (attr_p->attr.boolean_val) {
                SDP_PRINT("%s Parsed a=%s, boolean is TRUE", sdp_p->debug_str,
                          sdp_get_attr_name(attr_p->type));
            } else {
                SDP_PRINT("%s Parsed a=%s, boolean is FALSE", sdp_p->debug_str,
                          sdp_get_attr_name(attr_p->type));
            }
        }
        return (SDP_SUCCESS);
    }
}

sdp_result_e sdp_build_attr_simple_bool (sdp_t *sdp_p, sdp_attr_t *attr_p,
  flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%s\r\n", sdp_attr[attr_p->type].name,
    attr_p->attr.boolean_val ? "1" : "0");

  return SDP_SUCCESS;
}

/*
 * sdp_parse_attr_maxprate
 *
 * This function parses maxprate attribute lines. The ABNF for this a=
 * line is:
 *    max-p-rate-def = "a" "=" "maxprate" ":" packet-rate CRLF
 *    packet-rate = 1*DIGIT ["." 1*DIGIT]
 *
 * Returns:
 * SDP_INVALID_PARAMETER - If we are unable to parse the string OR if
 *                         packet-rate is not in the right format as per
 *                         the ABNF.
 *
 * SDP_SUCCESS - If we are able to successfully parse the a= line.
 */
sdp_result_e sdp_parse_attr_maxprate (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                      const char *ptr)
{
    sdp_result_e  result;

    ptr = sdp_getnextstrtok(ptr, attr_p->attr.string_val,
      sizeof(attr_p->attr.string_val), " \t", &result);

    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No string token found for %s attribute",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (!sdp_validate_maxprate(attr_p->attr.string_val)) {
            sdp_parse_error(sdp_p,
                "%s is not a valid maxprate value.",
                attr_p->attr.string_val);
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }

        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type),
                      attr_p->attr.string_val);
        }
        return (SDP_SUCCESS);
    }
}

/*
 * sdp_attr_fmtp_no_value
 * Helper function for sending the warning when a parameter value is
 * missing.
 *
 */
static void sdp_attr_fmtp_no_value(sdp_t *sdp, char *param_name)
{
  sdp_parse_error(sdp,
    "%s Warning: No %s value specified for fmtp attribute",
    sdp->debug_str, param_name);
  sdp->conf_p->num_invalid_param++;
}

/*
 * sdp_attr_fmtp_invalid_value
 * Helper function for sending the warning when a parameter value is
 * incorrect.
 *
 */

static void sdp_attr_fmtp_invalid_value(sdp_t *sdp, char *param_name,
  char* param_value)
{
  sdp_parse_error(sdp,
    "%s Warning: Invalid %s: %s specified for fmtp attribute",
    sdp->debug_str, param_name, param_value);
  sdp->conf_p->num_invalid_param++;
}

/* Note:  The fmtp attribute formats currently handled are:
 *        fmtp:<payload type> <event>,<event>...
 *        fmtp:<payload_type> [annexa=yes/no] [annexb=yes/no] [bitrate=<value>]
 *        [QCIF =<value>] [CIF =<value>] [MaxBR = <value>] one or more
 *        Other FMTP params as per H.263, H.263+, H.264 codec support.
 *        Note -"value" is a numeric value > 0 and each event is a
 *        single number or a range separated by a '-'.
 *        Example:  fmtp:101 1,3-15,20
 * Video codecs have annexes that can be listed in the following legal formats:
 * a) a=fmtp:34 param1=token;D;I;J;K=1;N=2;P=1,3
 * b) a=fmtp:34 param1=token;D;I;J;K=1;N=2;P=1,3;T
 * c) a=fmtp:34 param1=token;D;I;J
 *
 */

sdp_result_e sdp_parse_attr_fmtp (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                  const char *ptr)
{
    uint16_t           i;
    uint32_t           mapword;
    uint32_t           bmap;
    uint8_t            low_val;
    uint8_t            high_val;
    const char    *ptr2;
    const char    *fmtp_ptr;
    sdp_result_e  result1 = SDP_SUCCESS;
    sdp_result_e  result2 = SDP_SUCCESS;
    tinybool      done = FALSE;
    tinybool      codec_info_found = FALSE;
    sdp_fmtp_t   *fmtp_p;
    char          tmp[SDP_MAX_STRING_LEN];
    char          *src_ptr;
    char          *temp_ptr = NULL;
    char         *tok=NULL;
    char         *temp=NULL;
    uint16_t          custom_x=0;
    uint16_t          custom_y=0;
    uint16_t          custom_mpi=0;
    uint16_t          par_height=0;
    uint16_t          par_width=0;
    uint16_t          cpcf=0;
    uint16_t          iter=0;

    ulong        l_val = 0;
    char*        strtok_state;
    unsigned long strtoul_result;
    char*        strtoul_end;

    /* Find the payload type number. */
    attr_p->attr.fmtp.payload_num = (uint16_t)sdp_getnextnumtok(ptr, &ptr,
                                                      " \t", &result1);
    if (result1 != SDP_SUCCESS) {
        sdp_attr_fmtp_no_value(sdp_p, "payload type");
        return SDP_INVALID_PARAMETER;
    }
    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_UNKNOWN_TYPE;
    fmtp_p->parameter_add = 1;
    fmtp_p->flag = 0;

    /*
     * set default value of packetization mode and level-asymmetry-allowed. If
     * remote sdp does not specify any value for these two parameters, then the
     * default value will be assumed for remote sdp. If remote sdp does specify
     * any value for these parameters, then default value will be overridden.
    */
    fmtp_p->packetization_mode = SDP_DEFAULT_PACKETIZATION_MODE_VALUE;
    fmtp_p->level_asymmetry_allowed = SDP_DEFAULT_LEVEL_ASYMMETRY_ALLOWED_VALUE;

    temp_ptr = cpr_strdup(ptr);
    if (temp_ptr == NULL) {
        return (SDP_FAILURE);
    }
    fmtp_ptr = src_ptr = temp_ptr;

    src_ptr = temp_ptr;
    while (!done) {
      fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "= \t", &result1);
      if (result1 == SDP_SUCCESS) {
        if (cpr_strncasecmp(tmp, sdp_fmtp_codec_param[1].name,
                        sdp_fmtp_codec_param[1].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr  = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "annexb");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            if (cpr_strncasecmp(tok,sdp_fmtp_codec_param_val[0].name,
                            sdp_fmtp_codec_param_val[0].strlen) == 0) {
                fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
                fmtp_p->annexb_required = TRUE;
                fmtp_p->annexb = TRUE;
            } else if (cpr_strncasecmp(tok,sdp_fmtp_codec_param_val[1].name,
                                   sdp_fmtp_codec_param_val[1].strlen) == 0) {
                fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
                fmtp_p->annexb_required = TRUE;
                fmtp_p->annexb = FALSE;
            } else {
                sdp_attr_fmtp_invalid_value(sdp_p, "annexb", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            codec_info_found = TRUE;

        } else if (cpr_strncasecmp(tmp, sdp_fmtp_codec_param[0].name,
                               sdp_fmtp_codec_param[0].strlen) == 0) {

            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "annexa");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            if (cpr_strncasecmp(tok,sdp_fmtp_codec_param_val[0].name,
                            sdp_fmtp_codec_param_val[0].strlen) == 0) {
                fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
                fmtp_p->annexa = TRUE;
                fmtp_p->annexa_required = TRUE;
            } else if (cpr_strncasecmp(tok,sdp_fmtp_codec_param_val[1].name,
                                   sdp_fmtp_codec_param_val[1].strlen) == 0) {
                fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
                fmtp_p->annexa = FALSE;
                fmtp_p->annexa_required = TRUE;
            } else {
                sdp_attr_fmtp_invalid_value(sdp_p, "annexa", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            codec_info_found = TRUE;

        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[2].name,
                               sdp_fmtp_codec_param[2].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "bitrate");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > UINT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "bitrate", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->bitrate = (uint32_t) strtoul_result;
            codec_info_found = TRUE;

         } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[41].name,
                               sdp_fmtp_codec_param[41].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "mode");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result > UINT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "mode", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_MODE;
            fmtp_p->mode = (uint32_t) strtoul_result;
            codec_info_found = TRUE;

        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[3].name,
                               sdp_fmtp_codec_param[3].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
           fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
            if (result1 != SDP_SUCCESS) {
                sdp_attr_fmtp_no_value(sdp_p, "qcif");
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
        }
        tok = tmp;
        tok++;

        errno = 0;
        strtoul_result = strtoul(tok, &strtoul_end, 10);

        if (errno || tok == strtoul_end ||
            strtoul_result < SDP_MIN_CIF_VALUE || strtoul_result > SDP_MAX_CIF_VALUE) {
            sdp_attr_fmtp_invalid_value(sdp_p, "qcif", tok);
            SDP_FREE(temp_ptr);
            return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->qcif = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[4].name,
                               sdp_fmtp_codec_param[4].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
            if (result1 != SDP_SUCCESS) {
                sdp_attr_fmtp_no_value(sdp_p, "cif");
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
        }
        tok = tmp;
        tok++;

        errno = 0;
        strtoul_result = strtoul(tok, &strtoul_end, 10);

        if (errno || tok == strtoul_end ||
            strtoul_result < SDP_MIN_CIF_VALUE || strtoul_result > SDP_MAX_CIF_VALUE) {
            sdp_attr_fmtp_invalid_value(sdp_p, "cif", tok);
            SDP_FREE(temp_ptr);
            return SDP_INVALID_PARAMETER;
        }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->cif = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[5].name,
                               sdp_fmtp_codec_param[5].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "maxbr");
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
        }
        tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end ||
                strtoul_result == 0 || strtoul_result > USHRT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "maxbr", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->maxbr = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[6].name,
                               sdp_fmtp_codec_param[6].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "sqcif");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

        errno = 0;
        strtoul_result = strtoul(tok, &strtoul_end, 10);

        if (errno || tok == strtoul_end ||
            strtoul_result < SDP_MIN_CIF_VALUE || strtoul_result > SDP_MAX_CIF_VALUE) {
            sdp_attr_fmtp_invalid_value(sdp_p, "sqcif", tok);
            SDP_FREE(temp_ptr);
            return SDP_INVALID_PARAMETER;
        }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->sqcif = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[7].name,
                               sdp_fmtp_codec_param[7].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "cif4");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

        errno = 0;
        strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end ||
                strtoul_result < SDP_MIN_CIF_VALUE || strtoul_result > SDP_MAX_CIF_VALUE) {
                sdp_attr_fmtp_invalid_value(sdp_p, "cif4", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->cif4 = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[8].name,
                               sdp_fmtp_codec_param[8].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "cif16");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

        errno = 0;
        strtoul_result = strtoul(tok, &strtoul_end, 10);

        if (errno || tok == strtoul_end ||
            strtoul_result < SDP_MIN_CIF_VALUE || strtoul_result > SDP_MAX_CIF_VALUE) {
            sdp_attr_fmtp_invalid_value(sdp_p, "cif16", tok);
            SDP_FREE(temp_ptr);
            return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->cif16 = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else  if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[9].name,
                               sdp_fmtp_codec_param[9].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "custom");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++; temp=PL_strtok_r(tok, ",", &strtok_state);
            iter++;
        if (temp) {
            iter=1;
            while (temp != NULL) {
                errno = 0;
                strtoul_result = strtoul(temp, &strtoul_end, 10);

                if (errno || temp == strtoul_end || strtoul_result > USHRT_MAX){
                    custom_x = custom_y = custom_mpi = 0;
                    break;
                }

                if (iter == 1)
                    custom_x = (uint16_t) strtoul_result;
                if (iter == 2)
                    custom_y = (uint16_t) strtoul_result;
                if (iter == 3)
                    custom_mpi = (uint16_t) strtoul_result;

                temp=PL_strtok_r(NULL, ",", &strtok_state);
                iter++;
            }
        }

        /* custom x,y and mpi values from tmp */
            if (!custom_x || !custom_y || !custom_mpi) {
                sdp_attr_fmtp_invalid_value(sdp_p, "x/y/MPI", temp);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->custom_x = custom_x;
            fmtp_p->custom_y = custom_y;
            fmtp_p->custom_mpi = custom_mpi;
            codec_info_found = TRUE;
        } else  if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[10].name,
                               sdp_fmtp_codec_param[10].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "par");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++; temp=PL_strtok_r(tok, ":", &strtok_state);
        if (temp) {
            iter=1;
            /* get par width and par height for the aspect ratio */
            while (temp != NULL) {
                errno = 0;
                strtoul_result = strtoul(temp, &strtoul_end, 10);

                if (errno || temp == strtoul_end || strtoul_result > USHRT_MAX) {
                    par_width = par_height = 0;
                    break;
                }

                if (iter == 1)
                    par_width = (uint16_t) strtoul_result;
                else
                    par_height = (uint16_t) strtoul_result;

                temp=PL_strtok_r(NULL, ",", &strtok_state);
                iter++;
            }
        }
            if (!par_width || !par_height) {
                sdp_attr_fmtp_invalid_value(sdp_p, "par_width or par_height", temp);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->par_width = par_width;
            fmtp_p->par_height = par_height;
            codec_info_found = TRUE;
        } else  if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[11].name,
                               sdp_fmtp_codec_param[11].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "cpcf");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++; temp=PL_strtok_r(tok, ".", &strtok_state);
        if ( temp != NULL  ) {
            errno = 0;
            strtoul_result = strtoul(temp, &strtoul_end, 10);

            if (errno || temp == strtoul_end || strtoul_result > USHRT_MAX) {
                cpcf = 0;
            } else {
                cpcf = (uint16_t) strtoul_result;
            }
        }

            if (!cpcf) {
                sdp_attr_fmtp_invalid_value(sdp_p, "cpcf", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->cpcf = cpcf;
            codec_info_found = TRUE;
        } else  if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[12].name,
                               sdp_fmtp_codec_param[12].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "bpp");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

        errno = 0;
        strtoul_result = strtoul(tok, &strtoul_end, 10);

        if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > USHRT_MAX) {
            sdp_attr_fmtp_invalid_value(sdp_p, "bpp", tok);
            SDP_FREE(temp_ptr);
            return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
        fmtp_p->bpp = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else  if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[13].name,
                               sdp_fmtp_codec_param[13].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "hrd");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > USHRT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "hrd", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->hrd = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[14].name,
                               sdp_fmtp_codec_param[14].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "profile");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end ||
                strtoul_result > SDP_MAX_PROFILE_VALUE) {
                sdp_attr_fmtp_invalid_value(sdp_p, "profile", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->profile = (short) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[15].name,
                               sdp_fmtp_codec_param[15].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "level");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

        errno = 0;
        strtoul_result = strtoul(tok, &strtoul_end, 10);

        if (errno || tok == strtoul_end ||
            strtoul_result > SDP_MAX_LEVEL_VALUE) {
            sdp_attr_fmtp_invalid_value(sdp_p, "level", tok);
            SDP_FREE(temp_ptr);
            return SDP_INVALID_PARAMETER;
        }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->level = (short) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[16].name,
                               sdp_fmtp_codec_param[16].strlen) == 0) {
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->is_interlace = TRUE;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[17].name,
                               sdp_fmtp_codec_param[17].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "profile_level_id");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            sstrncpy(fmtp_p->profile_level_id , tok, sizeof(fmtp_p->profile_level_id));
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[18].name,
                               sdp_fmtp_codec_param[18].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "parameter_sets");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            sstrncpy(fmtp_p->parameter_sets , tok, sizeof(fmtp_p->parameter_sets));
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[19].name,
                               sdp_fmtp_codec_param[19].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "packetization_mode");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result > 2) {
                sdp_attr_fmtp_invalid_value(sdp_p, "packetization_mode", tok);
                sdp_p->conf_p->num_invalid_param++;
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->packetization_mode = (int16_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[20].name,
                               sdp_fmtp_codec_param[20].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "interleaving_depth");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > USHRT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "interleaving_depth", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->interleaving_depth = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[21].name,
                               sdp_fmtp_codec_param[21].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "deint_buf");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            if (sdp_checkrange(sdp_p, tok, &l_val) == TRUE) {
                fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
                fmtp_p->deint_buf_req = (uint32_t) l_val;
                fmtp_p->flag |= SDP_DEINT_BUF_REQ_FLAG;
                codec_info_found = TRUE;
            } else {
                sdp_attr_fmtp_invalid_value(sdp_p, "deint_buf_req", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[22].name,
                               sdp_fmtp_codec_param[22].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "max_don_diff");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > UINT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "max_don_diff", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_don_diff = (uint32_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[23].name,
                               sdp_fmtp_codec_param[23].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "init_buf_time");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            if (sdp_checkrange(sdp_p, tok, &l_val) == TRUE) {
                fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
                fmtp_p->init_buf_time = (uint32_t) l_val;
                fmtp_p->flag |= SDP_INIT_BUF_TIME_FLAG;
                codec_info_found = TRUE;
            } else {
                sdp_attr_fmtp_invalid_value(sdp_p, "init_buf_time", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[24].name,
                               sdp_fmtp_codec_param[24].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "max_mbps");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > UINT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "max_mbps", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
        fmtp_p->max_mbps = (uint32_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[25].name,
                               sdp_fmtp_codec_param[25].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "max-fs");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > UINT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "max-fs", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_fs = (uint32_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[26].name,
                               sdp_fmtp_codec_param[26].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "max_cbp");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > UINT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "max_cpb", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_cpb = (uint32_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[27].name,
                               sdp_fmtp_codec_param[27].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "max_dpb");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > UINT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "max_dpb", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_dpb = (uint32_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[28].name,
                               sdp_fmtp_codec_param[28].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "max_br");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > UINT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "max_br", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_br = (uint32_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[29].name,
                               sdp_fmtp_codec_param[29].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "redundant_pic_cap");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

        errno = 0;
        strtoul_result = strtoul(tok, &strtoul_end, 10);

        if (!errno && tok != strtoul_end && strtoul_result == 1) {
            fmtp_p->redundant_pic_cap = TRUE;
        } else {
            fmtp_p->redundant_pic_cap = FALSE;
        }

            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[30].name,
                               sdp_fmtp_codec_param[30].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "deint_buf_cap");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            if (sdp_checkrange(sdp_p, tok, &l_val) == TRUE) {
                fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
                fmtp_p->deint_buf_cap = (uint32_t) l_val;
                fmtp_p->flag |= SDP_DEINT_BUF_CAP_FLAG;
                codec_info_found = TRUE;
            } else {
                sdp_attr_fmtp_invalid_value(sdp_p, "deint_buf_cap", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
        }  else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[31].name,
                               sdp_fmtp_codec_param[31].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "max_rcmd_nalu_size");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            if (sdp_checkrange(sdp_p, tok, &l_val) == TRUE) {
                fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
                fmtp_p->max_rcmd_nalu_size = (uint32_t) l_val;
                fmtp_p->flag |= SDP_MAX_RCMD_NALU_SIZE_FLAG;
                codec_info_found = TRUE;
            } else {
                sdp_attr_fmtp_invalid_value(sdp_p, "max_rcmd_nalu_size", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[32].name,
                               sdp_fmtp_codec_param[32].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "parameter_add");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result > 1) {
                sdp_attr_fmtp_invalid_value(sdp_p, "parameter_add", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->parameter_add = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[33].name,
                               sdp_fmtp_codec_param[33].strlen) == 0) {
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_d = TRUE;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[34].name,
                               sdp_fmtp_codec_param[34].strlen) == 0) {
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_f = TRUE;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[35].name,
                               sdp_fmtp_codec_param[35].strlen) == 0) {
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_i = TRUE;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[36].name,
                               sdp_fmtp_codec_param[36].strlen) == 0) {
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_j = TRUE;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[37].name,
                               sdp_fmtp_codec_param[36].strlen) == 0) {
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_t = TRUE;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[38].name,
                             sdp_fmtp_codec_param[38].strlen) == 0) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                    if (result1 != SDP_SUCCESS) {
                        sdp_attr_fmtp_no_value(sdp_p, "annex_k");
                        SDP_FREE(temp_ptr);
                        return SDP_INVALID_PARAMETER;
                    }
                }
                tok = tmp;
                tok++;

                errno = 0;
                strtoul_result = strtoul(tok, &strtoul_end, 10);

                if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > USHRT_MAX) {
                    sdp_attr_fmtp_invalid_value(sdp_p, "annex_k", tok);
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
                fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
                fmtp_p->annex_k_val = (uint16_t) strtoul_result;
                codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[39].name,
                               sdp_fmtp_codec_param[39].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "annex_n");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > USHRT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "annex_n", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->annex_n_val = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[40].name,
                               sdp_fmtp_codec_param[40].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "annex_p");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            fmtp_p->annex_p_val_picture_resize = 0;
            fmtp_p->annex_p_val_warp = 0;
            tok = tmp;
            tok++; temp=PL_strtok_r(tok, ",", &strtok_state);
            if (temp) {
                iter=1;
                while (temp != NULL) {
                    errno = 0;
                    strtoul_result = strtoul(temp, &strtoul_end, 10);

                    if (errno || temp == strtoul_end || strtoul_result > USHRT_MAX) {
                        break;
                    }

                    if (iter == 1)
                        fmtp_p->annex_p_val_picture_resize = (uint16_t) strtoul_result;
                    else if (iter == 2)
                        fmtp_p->annex_p_val_warp = (uint16_t) strtoul_result;

                    temp=PL_strtok_r(NULL, ",", &strtok_state);
                    iter++;
                }
            }

            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[42].name,
                               sdp_fmtp_codec_param[42].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "level_asymmetry_allowed");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;

            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result > SDP_MAX_LEVEL_ASYMMETRY_ALLOWED_VALUE) {
                sdp_attr_fmtp_invalid_value(sdp_p, "level_asymmetry_allowed", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->level_asymmetry_allowed = (int) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[43].name,
                                   sdp_fmtp_codec_param[43].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "maxaveragebitrate");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result == 0 || strtoul_result > UINT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "maxaveragebitrate", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->maxaveragebitrate = (uint32_t) strtoul_result;
            codec_info_found = TRUE;

        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[44].name,
                                   sdp_fmtp_codec_param[44].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "usedtx");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result > 1) {
                sdp_attr_fmtp_invalid_value(sdp_p, "usedtx", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->usedtx = (uint16_t) strtoul_result;
            codec_info_found = TRUE;

        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[45].name,
                                   sdp_fmtp_codec_param[45].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "stereo");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            errno = 0;

            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result > 1) {
                sdp_attr_fmtp_invalid_value(sdp_p, "stereo", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->stereo = (uint16_t) strtoul_result;
            codec_info_found = TRUE;

        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[46].name,
                                   sdp_fmtp_codec_param[46].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "useinbandfec");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            errno = 0;

            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result > 1) {
                sdp_attr_fmtp_invalid_value(sdp_p, "useinbandfec", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->useinbandfec = (uint16_t) strtoul_result;
            codec_info_found = TRUE;

        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[47].name,
                                       sdp_fmtp_codec_param[47].strlen) == 0) {
                    fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
                    if (result1 != SDP_SUCCESS) {
                        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                        if (result1 != SDP_SUCCESS) {
                            sdp_attr_fmtp_no_value(sdp_p, "maxcodedaudiobandwidth");
                            sdp_p->conf_p->num_invalid_param++;
                            SDP_FREE(temp_ptr);
                            return SDP_INVALID_PARAMETER;
                        }
                    }
                    tok = tmp;
                    tok++;
                    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
                    sstrncpy(fmtp_p->maxcodedaudiobandwidth , tok, sizeof(fmtp_p->maxcodedaudiobandwidth));
                    codec_info_found = TRUE;

        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[48].name,
                                   sdp_fmtp_codec_param[48].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t", &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "cbr");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            errno = 0;

            strtoul_result = strtoul(tok, &strtoul_end, 10);

            if (errno || tok == strtoul_end || strtoul_result > 1) {
                sdp_attr_fmtp_invalid_value(sdp_p, "cbr", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->cbr = (uint16_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (cpr_strncasecmp(tmp,sdp_fmtp_codec_param[49].name,
                                   sdp_fmtp_codec_param[49].strlen) == 0) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t",
                                         &result1);
            if (result1 != SDP_SUCCESS) {
                fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp),
                                             " \t", &result1);
                if (result1 != SDP_SUCCESS) {
                    sdp_attr_fmtp_no_value(sdp_p, "max-fr");
                    SDP_FREE(temp_ptr);
                    return SDP_INVALID_PARAMETER;
                }
            }
            tok = tmp;
            tok++;
            errno = 0;
            strtoul_result = strtoul(tok, &strtoul_end, 10);
            if (errno || tok == strtoul_end || strtoul_result == 0 ||
                strtoul_result > UINT_MAX) {
                sdp_attr_fmtp_invalid_value(sdp_p, "max-fr", tok);
                SDP_FREE(temp_ptr);
                return SDP_INVALID_PARAMETER;
            }
            fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
            fmtp_p->max_fr = (uint32_t) strtoul_result;
            codec_info_found = TRUE;
        } else if (fmtp_ptr != NULL && *fmtp_ptr == '\n') {
            temp=PL_strtok_r(tmp, ";", &strtok_state);
            if (temp) {
                if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
                    SDP_PRINT("%s Annexes are possibly there for this fmtp %s  tmp: %s line\n",
                              sdp_p->debug_str, fmtp_ptr, tmp);
                }
                while (temp != NULL) {
                    if (strchr(temp, 'D') !=NULL) {
                        attr_p->attr.fmtp.annex_d = TRUE;
                    }
                    if (strchr(temp, 'F') !=NULL) {
                        attr_p->attr.fmtp.annex_f = TRUE;
                    }
                    if (strchr(temp, 'I') !=NULL) {
                        attr_p->attr.fmtp.annex_i = TRUE;
                    }
                    if (strchr(temp, 'J') !=NULL) {
                        attr_p->attr.fmtp.annex_j = TRUE;
                    }
                    if (strchr(temp, 'T') !=NULL) {
                        attr_p->attr.fmtp.annex_t = TRUE;
                    }
                    temp=PL_strtok_r(NULL, ";", &strtok_state);
                }
            } /* if (temp) */
            done = TRUE;
        } else {
          // XXX Note that DTMF fmtp will fall into here:
          // a=fmtp:101 0-15 (or 0-15,NN,NN etc)

          // unknown parameter - eat chars until ';'
          CSFLogDebug(logTag, "%s Unknown fmtp type (%s) - ignoring", __FUNCTION__,
                      tmp);
          fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), "; \t",
                                       &result1);
          if (result1 != SDP_SUCCESS) {
            fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), " \t", &result1);
            if (result1 != SDP_SUCCESS) {
              // hmmm, no ; or spaces or tabs; continue on
            }
          }
        }
        fmtp_ptr++;
      } else {
          done = TRUE;
      }
    } /* while  - done loop*/

    if (codec_info_found) {

        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, payload type %u, bitrate %u, mode %u QCIF = %u, CIF = %u, MAXBR= %u, SQCIF=%u, CIF4= %u, CIF16=%u, CUSTOM=%u,%u,%u , PAR=%u:%u,CPCF=%u, BPP=%u, HRD=%u \n",
                      sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type),
                      attr_p->attr.fmtp.payload_num,
                      attr_p->attr.fmtp.bitrate,
                      attr_p->attr.fmtp.mode,
                      attr_p->attr.fmtp.qcif,
                      attr_p->attr.fmtp.cif,
                      attr_p->attr.fmtp.maxbr,
                      attr_p->attr.fmtp.sqcif,
                      attr_p->attr.fmtp.cif4,
                      attr_p->attr.fmtp.cif16,
                      attr_p->attr.fmtp.custom_x,attr_p->attr.fmtp.custom_y,
                      attr_p->attr.fmtp.custom_mpi,
                      attr_p->attr.fmtp.par_width,
                      attr_p->attr.fmtp.par_height,
                      attr_p->attr.fmtp.cpcf,
                      attr_p->attr.fmtp.bpp,
                      attr_p->attr.fmtp.hrd
                      );
        }

        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, payload type %u,PROFILE=%u,LEVEL=%u, INTERLACE - %s",
                      sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type),
                      attr_p->attr.fmtp.payload_num,
                      attr_p->attr.fmtp.profile,
                      attr_p->attr.fmtp.level,
                      attr_p->attr.fmtp.is_interlace ? "YES":"NO");
        }

        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed H.264 attributes: profile-level-id=%s, parameter-sets=%s, packetization-mode=%d level-asymmetry-allowed=%d interleaving-depth=%d deint-buf-req=%u max-don-diff=%u, init_buf-time=%u\n",
                      sdp_p->debug_str,
                      attr_p->attr.fmtp.profile_level_id,
                      attr_p->attr.fmtp.parameter_sets,
                      attr_p->attr.fmtp.packetization_mode,
                      attr_p->attr.fmtp.level_asymmetry_allowed,
                      attr_p->attr.fmtp.interleaving_depth,
                      attr_p->attr.fmtp.deint_buf_req,
                      attr_p->attr.fmtp.max_don_diff,
                      attr_p->attr.fmtp.init_buf_time
                      );
        }

        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("\n%s Parsed H.264 opt attributes: max-mbps=%u, max-fs=%u, max-cpb=%u max-dpb=%u max-br=%u redundant-pic-cap=%d, deint-buf-cap=%u, max-rcmd-nalu-size=%u , parameter-add=%d\n",
                      sdp_p->debug_str,
                      attr_p->attr.fmtp.max_mbps,
                      attr_p->attr.fmtp.max_fs,
                      attr_p->attr.fmtp.max_cpb,
                      attr_p->attr.fmtp.max_dpb,
                      attr_p->attr.fmtp.max_br,
                      attr_p->attr.fmtp.redundant_pic_cap,
                      attr_p->attr.fmtp.deint_buf_cap,
                      attr_p->attr.fmtp.max_rcmd_nalu_size,
                      attr_p->attr.fmtp.parameter_add);

        }
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed annexes are : D=%d F=%d I=%d J=%d T=%d, K=%d N=%d P=%d,%d\n",
                      sdp_p->debug_str,
                      attr_p->attr.fmtp.annex_d,
                      attr_p->attr.fmtp.annex_f,  attr_p->attr.fmtp.annex_i,
                      attr_p->attr.fmtp.annex_j,  attr_p->attr.fmtp.annex_t,
                      attr_p->attr.fmtp.annex_k_val,
                      attr_p->attr.fmtp.annex_n_val,
                      attr_p->attr.fmtp.annex_p_val_picture_resize,
                      attr_p->attr.fmtp.annex_p_val_warp);

        }
        SDP_FREE(temp_ptr);
        return (SDP_SUCCESS);
    } else {
        done = FALSE;
        fmtp_ptr = src_ptr;
        tmp[0] = '\0';
    }

    for (i=0; !done; i++) {
        fmtp_p->fmtp_format = SDP_FMTP_NTE;
        /* Look for comma separated events */
        fmtp_ptr = sdp_getnextstrtok(fmtp_ptr, tmp, sizeof(tmp), ", \t", &result1);
        if (result1 != SDP_SUCCESS) {
            done = TRUE;
            continue;
        }
        /* Now look for '-' separated range */
        ptr2 = tmp;
        low_val = (uint8_t)sdp_getnextnumtok(ptr2, (const char **)&ptr2,
                                    "- \t", &result1);
        if (*ptr2 == '-') {
            high_val = (uint8_t)sdp_getnextnumtok(ptr2, (const char **)&ptr2,
                                         "- \t", &result2);
        } else {
            high_val = low_val;
        }

        if ((result1 != SDP_SUCCESS) || (result2 != SDP_SUCCESS)) {
            sdp_parse_error(sdp_p,
                "%s Warning: Invalid named events specified for fmtp attribute.",
                sdp_p->debug_str);
            sdp_p->conf_p->num_invalid_param++;
            SDP_FREE(temp_ptr);
            return (SDP_INVALID_PARAMETER);
        }

        for (i = low_val; i <= high_val; i++) {
            mapword = i/SDP_NE_BITS_PER_WORD;
            bmap = SDP_NE_BIT_0 << (i%32);
            fmtp_p->bmap[mapword] |= bmap;
        }
        if (high_val > fmtp_p->maxval) {
            fmtp_p->maxval = high_val;
        }
    }

    if (fmtp_p->maxval == 0) {
        sdp_parse_error(sdp_p,
            "%s Warning: No named events specified for fmtp attribute.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        SDP_FREE(temp_ptr);
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, payload type %u, ", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  attr_p->attr.fmtp.payload_num);
    }
    SDP_FREE(temp_ptr);
    return (SDP_SUCCESS);
}

sdp_result_e
sdp_build_attr_fmtp_params (sdp_t *sdp_p, sdp_fmtp_t *fmtp_p, flex_string *fs)
{
  uint16_t         event_id;
  uint32_t         mask;
  uint32_t         mapword;
  uint8_t          min = 0;
  uint8_t          max = 0;
  tinybool    range_start = FALSE;
  tinybool    range_end = FALSE;
  tinybool    semicolon = FALSE;

  switch (fmtp_p->fmtp_format) {
    case SDP_FMTP_MODE:
      sdp_append_name_and_unsigned(fs, "mode", fmtp_p->mode, FALSE);
      break;

    case SDP_FMTP_CODEC_INFO:
      FMTP_BUILD_UNSIGNED(fmtp_p->bitrate > 0, "bitrate", fmtp_p->bitrate)

      FMTP_BUILD_STRING(fmtp_p->annexa_required,
        "annexa", (fmtp_p->annexa ? "yes" : "no"))

      FMTP_BUILD_STRING(fmtp_p->annexb_required,
        "annexb", (fmtp_p->annexa ? "yes" : "no"))

      FMTP_BUILD_UNSIGNED(fmtp_p->qcif > 0, "QCIF", fmtp_p->qcif)

      FMTP_BUILD_UNSIGNED(fmtp_p->cif > 0, "CIF", fmtp_p->cif)

      FMTP_BUILD_UNSIGNED(fmtp_p->maxbr > 0, "MAXBR", fmtp_p->maxbr)

      FMTP_BUILD_UNSIGNED(fmtp_p->sqcif > 0, "SQCIF", fmtp_p->sqcif)

      FMTP_BUILD_UNSIGNED(fmtp_p->cif4 > 0, "CIF4", fmtp_p->cif4)

      FMTP_BUILD_UNSIGNED(fmtp_p->cif16 > 0, "CIF16", fmtp_p->cif16)

      if ((fmtp_p->custom_x > 0) && (fmtp_p->custom_y > 0) &&
        (fmtp_p->custom_mpi > 0)) {
        flex_string_sprintf(fs, "%sCUSTOM=%u,%u,%u",
          semicolon ? ";" : "",
          fmtp_p->custom_x,
          fmtp_p->custom_y,
          fmtp_p->custom_mpi);

        semicolon = TRUE;
      }

      if ((fmtp_p->par_height > 0) && (fmtp_p->par_width > 0)) {
        flex_string_sprintf(fs, "%sPAR=%u:%u",
          semicolon ? ";" : "",
          fmtp_p->par_width,
          fmtp_p->par_width);

        semicolon = TRUE;
      }

      FMTP_BUILD_UNSIGNED(fmtp_p->cpcf > 0, "CPCF", fmtp_p->cpcf)

      FMTP_BUILD_UNSIGNED(fmtp_p->bpp > 0, "BPP", fmtp_p->bpp)

      FMTP_BUILD_UNSIGNED(fmtp_p->hrd > 0, "HRD", fmtp_p->hrd)

      FMTP_BUILD_UNSIGNED(fmtp_p->profile >= 0, "PROFILE", fmtp_p->profile)

      FMTP_BUILD_UNSIGNED(fmtp_p->level >= 0, "LEVEL", fmtp_p->level)

      FMTP_BUILD_FLAG(fmtp_p->is_interlace, "INTERLACE")

      FMTP_BUILD_FLAG(fmtp_p->annex_d, "D")

      FMTP_BUILD_FLAG(fmtp_p->annex_f, "F")

      FMTP_BUILD_FLAG(fmtp_p->annex_i, "I")

      FMTP_BUILD_FLAG(fmtp_p->annex_j, "J")

      FMTP_BUILD_FLAG(fmtp_p->annex_t, "T")

      FMTP_BUILD_UNSIGNED(fmtp_p->annex_k_val > 0,
        "K", fmtp_p->annex_k_val)

      FMTP_BUILD_UNSIGNED(fmtp_p->annex_n_val > 0,
        "N", fmtp_p->annex_n_val)

      if ((fmtp_p->annex_p_val_picture_resize > 0) &&
        (fmtp_p->annex_p_val_warp > 0)) {
        flex_string_sprintf(fs, "%sP=%d:%d",
          semicolon ? ";" : "",
          fmtp_p->annex_p_val_picture_resize,
          fmtp_p->annex_p_val_warp);

        semicolon = TRUE;
      }

      FMTP_BUILD_STRING(strlen(fmtp_p->profile_level_id) > 0,
        "profile-level-id", fmtp_p->profile_level_id)

      FMTP_BUILD_STRING(strlen(fmtp_p->parameter_sets) > 0,
        "sprop-parameter-sets", fmtp_p->parameter_sets)

      FMTP_BUILD_UNSIGNED(
        fmtp_p->packetization_mode < SDP_MAX_PACKETIZATION_MODE_VALUE,
        "packetization-mode", fmtp_p->packetization_mode)

      FMTP_BUILD_UNSIGNED(
        fmtp_p->level_asymmetry_allowed <=
        SDP_MAX_LEVEL_ASYMMETRY_ALLOWED_VALUE,
        "level-asymmetry-allowed", fmtp_p->level_asymmetry_allowed)

      FMTP_BUILD_UNSIGNED(fmtp_p->interleaving_depth > 0,
        "sprop-interleaving-depth", fmtp_p->interleaving_depth)

      FMTP_BUILD_UNSIGNED(fmtp_p->flag & SDP_DEINT_BUF_REQ_FLAG,
        "sprop-deint-buf-req", fmtp_p->deint_buf_req)

      FMTP_BUILD_UNSIGNED(fmtp_p->max_don_diff > 0,
        "sprop-max-don-diff", fmtp_p->max_don_diff)

      FMTP_BUILD_UNSIGNED(fmtp_p->flag & SDP_INIT_BUF_TIME_FLAG,
        "sprop-init-buf-time", fmtp_p->init_buf_time)

      FMTP_BUILD_UNSIGNED(fmtp_p->max_mbps > 0,
        "max-mbps", fmtp_p->max_mbps)

      FMTP_BUILD_UNSIGNED(fmtp_p->max_fs > 0, "max-fs", fmtp_p->max_fs)

      FMTP_BUILD_UNSIGNED(fmtp_p->max_fr > 0, "max-fr", fmtp_p->max_fr)

      FMTP_BUILD_UNSIGNED(fmtp_p->max_cpb > 0, "max-cpb", fmtp_p->max_cpb)

      FMTP_BUILD_UNSIGNED(fmtp_p->max_dpb > 0, "max-dpb", fmtp_p->max_dpb)

      FMTP_BUILD_UNSIGNED(fmtp_p->max_br > 0, "max-br", fmtp_p->max_br)

      FMTP_BUILD_UNSIGNED(fmtp_p->redundant_pic_cap > 0,
        "redundant-pic-cap", fmtp_p->redundant_pic_cap)

      FMTP_BUILD_UNSIGNED(fmtp_p->flag & SDP_DEINT_BUF_CAP_FLAG,
        "deint-buf-cap", fmtp_p->deint_buf_cap)

      FMTP_BUILD_UNSIGNED(fmtp_p->flag & SDP_MAX_RCMD_NALU_SIZE_FLAG,
        "max-rcmd-naFMTP_BUILD_FLlu-size", fmtp_p->max_rcmd_nalu_size)

      FMTP_BUILD_UNSIGNED(fmtp_p->parameter_add <= 1, "parameter-add",
                          fmtp_p->parameter_add)

      FMTP_BUILD_UNSIGNED(fmtp_p->maxaveragebitrate > 0,
        "maxaveragebitrate", fmtp_p->maxaveragebitrate)

      FMTP_BUILD_UNSIGNED(fmtp_p->usedtx <= 1, "usedtx", fmtp_p->usedtx)

      FMTP_BUILD_UNSIGNED(fmtp_p->stereo <= 1, "stereo", fmtp_p->stereo)

      FMTP_BUILD_UNSIGNED(fmtp_p->useinbandfec <= 1,
        "useinbandfec", fmtp_p->useinbandfec)

      FMTP_BUILD_STRING(strlen(fmtp_p->maxcodedaudiobandwidth) > 0,
        "maxcodedaudiobandwidth", fmtp_p->maxcodedaudiobandwidth)

      FMTP_BUILD_UNSIGNED(fmtp_p->cbr <= 1, "cbr", fmtp_p->cbr)

      break;

    case SDP_FMTP_NTE:
    default:
      break;
  }

     for(event_id = 0, mapword = 0, mask = SDP_NE_BIT_0;
         event_id <= fmtp_p->maxval;
         event_id++, mapword = event_id/SDP_NE_BITS_PER_WORD ) {

         if (event_id % SDP_NE_BITS_PER_WORD) {
             mask <<= 1;
         } else {
         /* crossed a bitmap word boundary */
         mask = SDP_NE_BIT_0;
             if (!range_start && !range_end && !fmtp_p->bmap[mapword]) {
            /* no events in this word, skip to the last event id
             * in this bitmap word. */
                event_id += SDP_NE_BITS_PER_WORD - 1;
                continue;
            }
         }

        if (fmtp_p->bmap[mapword] & mask) {
            if (!range_start) {
                range_start = TRUE;
                min = max = (uint8_t)event_id;
            } else {
                max = (uint8_t)event_id;
            }
        range_end = (max == fmtp_p->maxval);
        } else {
        /* If we were in the middle of a range, then we've hit the
         * end.  If we weren't, there is no end to hit. */
            range_end = range_start;
        }

        /* If this is the end of the range, print it to the string. */
        if (range_end) {
            range_start = range_end = FALSE;

            flex_string_sprintf(fs, "%u", min);

            if (min != max) {
              flex_string_sprintf(fs, "-%u", max);
            }

            if (max != fmtp_p->maxval) {
              flex_string_append(fs, ",");
            }
        }
    }
    return SDP_SUCCESS;
}

sdp_result_e sdp_build_attr_fmtp (sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs)
{
  sdp_fmtp_t *fmtp_p;
  sdp_result_e result;

  flex_string_sprintf(fs, "a=%s:%u ",
    sdp_attr[attr_p->type].name,
    attr_p->attr.fmtp.payload_num);

  fmtp_p = &(attr_p->attr.fmtp);

  result = sdp_build_attr_fmtp_params(sdp_p, fmtp_p, fs);

  if (result != SDP_SUCCESS) {
    return result;
  }

  flex_string_append(fs, "\r\n");

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_sctpmap(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                    const char *ptr)
{
    sdp_result_e result = SDP_SUCCESS;
    char tmp[SDP_MAX_STRING_LEN];
    uint32_t streams;

    /* Find the payload type number. */
    attr_p->attr.sctpmap.port = (uint16_t)sdp_getnextnumtok(ptr, &ptr,
                                                      " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: no sctpmap port number",
            sdp_p->debug_str);
        return SDP_INVALID_PARAMETER;
    }

    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No sctpmap protocol specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }
    sstrncpy(attr_p->attr.sctpmap.protocol, tmp,
        sizeof (attr_p->attr.sctpmap.protocol));

    streams = sdp_getnextnumtok(ptr, &ptr, " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No sctpmap streams specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }

    attr_p->attr.sctpmap.streams = streams;

    return SDP_SUCCESS;
}

sdp_result_e sdp_build_attr_sctpmap(sdp_t *sdp_p, sdp_attr_t *attr_p,
                                    flex_string *fs)
{
    flex_string_sprintf(fs, "a=%s:%u %s %u\r\n",
        sdp_attr[attr_p->type].name,
        attr_p->attr.sctpmap.port,
        attr_p->attr.sctpmap.protocol,
        attr_p->attr.sctpmap.streams);

    return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_direction (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                       const char *ptr)
{
    /* No parameters to parse. */
    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_direction (sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s\r\n", sdp_get_attr_name(attr_p->type));

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_qos (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                 const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    /* Find the strength tag. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No qos strength tag specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.qos.strength = SDP_QOS_STRENGTH_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_STRENGTH; i++) {
        if (cpr_strncasecmp(tmp, sdp_qos_strength[i].name,
                        sdp_qos_strength[i].strlen) == 0) {
            attr_p->attr.qos.strength = (sdp_qos_strength_e)i;
        }
    }
    if (attr_p->attr.qos.strength == SDP_QOS_STRENGTH_UNKNOWN) {
        sdp_parse_error(sdp_p,
            "%s Warning: QOS strength tag unrecognized (%s)",
            sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the qos direction. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No qos direction specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.qos.direction = SDP_QOS_DIR_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_DIR; i++) {
        if (cpr_strncasecmp(tmp, sdp_qos_direction[i].name,
                        sdp_qos_direction[i].strlen) == 0) {
            attr_p->attr.qos.direction = (sdp_qos_dir_e)i;
        }
    }
    if (attr_p->attr.qos.direction == SDP_QOS_DIR_UNKNOWN) {
        sdp_parse_error(sdp_p,
            "%s Warning: QOS direction unrecognized (%s)",
            sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* See if confirm was specified.  Defaults to FALSE. */
    attr_p->attr.qos.confirm = FALSE;
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result == SDP_SUCCESS) {
        if (cpr_strncasecmp(tmp, "confirm", sizeof("confirm")) == 0) {
            attr_p->attr.qos.confirm = TRUE;
        }
        if (attr_p->attr.qos.confirm == FALSE) {
            sdp_parse_error(sdp_p,
                "%s Warning: QOS confirm parameter invalid (%s)",
                sdp_p->debug_str, tmp);
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, strength %s, direction %s, confirm %s",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_qos_strength_name(attr_p->attr.qos.strength),
                  sdp_get_qos_direction_name(attr_p->attr.qos.direction),
                  (attr_p->attr.qos.confirm ? "set" : "not set"));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_qos (sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%s %s%s\r\n", sdp_attr[attr_p->type].name,
    sdp_get_qos_strength_name(attr_p->attr.qos.strength),
    sdp_get_qos_direction_name(attr_p->attr.qos.direction),
    attr_p->attr.qos.confirm ? " confirm" : "");

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_curr (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                 const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    /* Find the curr type tag. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No curr attr type specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.curr.type = SDP_CURR_UNKNOWN_TYPE;
    for (i=0; i < SDP_MAX_CURR_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_curr_type[i].name,
                        sdp_curr_type[i].strlen) == 0) {
            attr_p->attr.curr.type = (sdp_curr_type_e)i;
        }
    }

    if (attr_p->attr.curr.type != SDP_CURR_QOS_TYPE) {
        sdp_parse_error(sdp_p,
            "%s Warning: Unknown curr type.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Check qos status type */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
     if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No curr attr type specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.curr.status_type = SDP_QOS_STATUS_TYPE_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_STATUS_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_qos_status_type[i].name,
                        sdp_qos_status_type[i].strlen) == 0) {
            attr_p->attr.curr.status_type = (sdp_qos_status_types_e)i;
        }
    }


    /* Find the qos direction. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No qos direction specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.curr.direction = SDP_QOS_DIR_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_DIR; i++) {
        if (cpr_strncasecmp(tmp, sdp_qos_direction[i].name,
                        sdp_qos_direction[i].strlen) == 0) {
            attr_p->attr.curr.direction = (sdp_qos_dir_e)i;
        }
    }
    if (attr_p->attr.curr.direction == SDP_QOS_DIR_UNKNOWN) {
        sdp_parse_error(sdp_p,
            "%s Warning: QOS direction unrecognized (%s)",
            sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, type %s status type %s, direction %s",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_curr_type_name(attr_p->attr.curr.type),
                  sdp_get_qos_status_type_name(attr_p->attr.curr.status_type),
                  sdp_get_qos_direction_name(attr_p->attr.curr.direction));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_curr (sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%s %s %s\r\n",
    sdp_attr[attr_p->type].name,
    sdp_get_curr_type_name(attr_p->attr.curr.type),
    sdp_get_qos_status_type_name(attr_p->attr.curr.status_type),
    sdp_get_qos_direction_name(attr_p->attr.curr.direction));

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_des (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                 const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    /* Find the curr type tag. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No des attr type specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.des.type = SDP_DES_UNKNOWN_TYPE;
    for (i=0; i < SDP_MAX_CURR_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_des_type[i].name,
                        sdp_des_type[i].strlen) == 0) {
            attr_p->attr.des.type = (sdp_des_type_e)i;
        }
    }

    if (attr_p->attr.des.type != SDP_DES_QOS_TYPE) {
        sdp_parse_error(sdp_p,
            "%s Warning: Unknown conf type.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the strength tag. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No qos strength tag specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.des.strength = SDP_QOS_STRENGTH_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_STRENGTH; i++) {
        if (cpr_strncasecmp(tmp, sdp_qos_strength[i].name,
                        sdp_qos_strength[i].strlen) == 0) {
            attr_p->attr.des.strength = (sdp_qos_strength_e)i;
        }
    }
    if (attr_p->attr.des.strength == SDP_QOS_STRENGTH_UNKNOWN) {
        sdp_parse_error(sdp_p,
            "%s Warning: QOS strength tag unrecognized (%s)",
            sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Check qos status type */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
     if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No des attr type specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.des.status_type = SDP_QOS_STATUS_TYPE_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_STATUS_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_qos_status_type[i].name,
                        sdp_qos_status_type[i].strlen) == 0) {
            attr_p->attr.des.status_type = (sdp_qos_status_types_e)i;
        }
    }


    /* Find the qos direction. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No qos direction specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.des.direction = SDP_QOS_DIR_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_DIR; i++) {
        if (cpr_strncasecmp(tmp, sdp_qos_direction[i].name,
                        sdp_qos_direction[i].strlen) == 0) {
            attr_p->attr.des.direction = (sdp_qos_dir_e)i;
        }
    }
    if (attr_p->attr.des.direction == SDP_QOS_DIR_UNKNOWN) {
        sdp_parse_error(sdp_p,
            "%s Warning: QOS direction unrecognized (%s)",
            sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, type %s strength %s status type %s, direction %s",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_des_type_name(attr_p->attr.des.type),
                  sdp_get_qos_strength_name(attr_p->attr.qos.strength),
                  sdp_get_qos_status_type_name(attr_p->attr.des.status_type),
                  sdp_get_qos_direction_name(attr_p->attr.des.direction));
    }

    return (SDP_SUCCESS);
}


sdp_result_e sdp_build_attr_des (sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%s %s %s %s\r\n",
    sdp_attr[attr_p->type].name,
    sdp_get_curr_type_name((sdp_curr_type_e)attr_p->attr.des.type),
    sdp_get_qos_strength_name(attr_p->attr.des.strength),
    sdp_get_qos_status_type_name(attr_p->attr.des.status_type),
    sdp_get_qos_direction_name(attr_p->attr.des.direction));

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_conf (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                 const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    /* Find the curr type tag. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No conf attr type specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.conf.type = SDP_CONF_UNKNOWN_TYPE;
    for (i=0; i < SDP_MAX_CURR_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_conf_type[i].name,
                        sdp_conf_type[i].strlen) == 0) {
            attr_p->attr.conf.type = (sdp_conf_type_e)i;
        }
    }

    if (attr_p->attr.conf.type != SDP_CONF_QOS_TYPE) {
        sdp_parse_error(sdp_p,
            "%s Warning: Unknown conf type.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Check qos status type */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
     if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No conf attr type specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.conf.status_type = SDP_QOS_STATUS_TYPE_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_STATUS_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_qos_status_type[i].name,
                        sdp_qos_status_type[i].strlen) == 0) {
            attr_p->attr.conf.status_type = (sdp_qos_status_types_e)i;
        }
    }


    /* Find the qos direction. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No qos direction specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.conf.direction = SDP_QOS_DIR_UNKNOWN;
    for (i=0; i < SDP_MAX_QOS_DIR; i++) {
        if (cpr_strncasecmp(tmp, sdp_qos_direction[i].name,
                        sdp_qos_direction[i].strlen) == 0) {
            attr_p->attr.conf.direction = (sdp_qos_dir_e)i;
        }
    }
    if (attr_p->attr.conf.direction == SDP_QOS_DIR_UNKNOWN) {
        sdp_parse_error(sdp_p,
            "%s Warning: QOS direction unrecognized (%s)",
            sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, type %s status type %s, direction %s",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_conf_type_name(attr_p->attr.conf.type),
                  sdp_get_qos_status_type_name(attr_p->attr.conf.status_type),
                  sdp_get_qos_direction_name(attr_p->attr.conf.direction));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_conf (sdp_t *sdp_p, sdp_attr_t *attr_p, flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%s %s %s\r\n",
    sdp_attr[attr_p->type].name,
    sdp_get_conf_type_name(attr_p->attr.conf.type),
    sdp_get_qos_status_type_name(attr_p->attr.conf.status_type),
    sdp_get_qos_direction_name(attr_p->attr.conf.direction));

  return SDP_SUCCESS;
}

/*
 *  Parse a rtpmap or a sprtmap. Both formats use the same structure
 *  the only difference being the keyword "rtpmap" vs "sprtmap". The
 *  rtpmap field in the sdp_attr_t is used to store both mappings.
 */
sdp_result_e sdp_parse_attr_transport_map (sdp_t *sdp_p, sdp_attr_t *attr_p,
        const char *ptr)
{
    sdp_result_e  result;

    attr_p->attr.transport_map.payload_num = 0;
    attr_p->attr.transport_map.encname[0]  = '\0';
    attr_p->attr.transport_map.clockrate   = 0;
    attr_p->attr.transport_map.num_chan    = 1;

    /* Find the payload type number. */
    attr_p->attr.transport_map.payload_num =
    (uint16_t)sdp_getnextnumtok(ptr, &ptr, " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: Invalid payload type specified for %s attribute.",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the encoding name. */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.transport_map.encname,
                            sizeof(attr_p->attr.transport_map.encname), "/ \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No encoding name specified in %s attribute.",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the clockrate. */
    attr_p->attr.transport_map.clockrate =
        sdp_getnextnumtok(ptr, &ptr, "/ \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No clockrate specified for "
            "%s attribute, set to default of 8000.",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        attr_p->attr.transport_map.clockrate = 8000;
    }

    /* Find the number of channels, if specified. This is optional. */
    if (*ptr == '/') {
        /* If a '/' exists, expect something valid beyond it. */
        attr_p->attr.transport_map.num_chan =
            (uint16_t)sdp_getnextnumtok(ptr, &ptr, "/ \t", &result);
        if (result != SDP_SUCCESS) {
            sdp_parse_error(sdp_p,
                "%s Warning: Invalid number of channels parameter"
                " for rtpmap attribute.", sdp_p->debug_str);
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, payload type %u, encoding name %s, "
                  "clockrate %u", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  attr_p->attr.transport_map.payload_num,
                  attr_p->attr.transport_map.encname,
                  attr_p->attr.transport_map.clockrate);
        if (attr_p->attr.transport_map.num_chan != 1) {
            SDP_PRINT("/%u", attr_p->attr.transport_map.num_chan);
        }
    }

    return (SDP_SUCCESS);
}

/*
 *  Build a rtpmap or a sprtmap. Both formats use the same structure
 *  the only difference being the keyword "rtpmap" vs "sprtmap". The
 *  rtpmap field in the sdp_attr_t is used for both mappings.
 */
sdp_result_e sdp_build_attr_transport_map (sdp_t *sdp_p, sdp_attr_t *attr_p,
        flex_string *fs)
{
  if (attr_p->attr.transport_map.num_chan == 1) {
    flex_string_sprintf(fs, "a=%s:%u %s/%u\r\n",
      sdp_attr[attr_p->type].name,
      attr_p->attr.transport_map.payload_num,
      attr_p->attr.transport_map.encname,
      attr_p->attr.transport_map.clockrate);
  } else {
    flex_string_sprintf(fs, "a=%s:%u %s/%u/%u\r\n",
      sdp_attr[attr_p->type].name,
      attr_p->attr.transport_map.payload_num,
      attr_p->attr.transport_map.encname,
      attr_p->attr.transport_map.clockrate,
      attr_p->attr.transport_map.num_chan);
  }

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_subnet (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                    const char *ptr)
{
    int i;
    char         *slash_ptr;
    sdp_result_e  result;
    tinybool      type_found = FALSE;
    char          tmp[SDP_MAX_STRING_LEN];

    /* Find the subnet network type. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No network type specified in subnet attribute.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.subnet.nettype = SDP_NT_UNSUPPORTED;
    for (i=0; i < SDP_MAX_NETWORK_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_nettype[i].name,
                        sdp_nettype[i].strlen) == 0) {
            type_found = TRUE;
        }
        if (type_found == TRUE) {
            if (sdp_p->conf_p->nettype_supported[i] == TRUE) {
                attr_p->attr.subnet.nettype = (sdp_nettype_e)i;
            }
            type_found = FALSE;
        }
    }
    if (attr_p->attr.subnet.nettype == SDP_NT_UNSUPPORTED) {
        sdp_parse_error(sdp_p,
            "%s Warning: Subnet network type unsupported (%s).",
            sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the subnet address type. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No address type specified in subnet attribute.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.subnet.addrtype = SDP_AT_UNSUPPORTED;
    for (i=0; i < SDP_MAX_ADDR_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_addrtype[i].name,
                        sdp_addrtype[i].strlen) == 0) {
            type_found = TRUE;
        }
        if (type_found == TRUE) {
            if (sdp_p->conf_p->addrtype_supported[i] == TRUE) {
                attr_p->attr.subnet.addrtype = (sdp_addrtype_e)i;
            }
            type_found = FALSE;
        }
    }
    if (attr_p->attr.subnet.addrtype == SDP_AT_UNSUPPORTED) {
        sdp_parse_error(sdp_p,
            "%s Warning: Subnet address type unsupported (%s).",
            sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the subnet address.  */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.subnet.addr,
                            sizeof(attr_p->attr.subnet.addr), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No subnet address specified in "
            "subnet attribute.", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    slash_ptr = sdp_findchar(attr_p->attr.subnet.addr, "/");
    if (*slash_ptr == '/') {
        *slash_ptr++ = '\0';
        /* If the '/' exists, expect a valid prefix to follow. */
        attr_p->attr.subnet.prefix = sdp_getnextnumtok(slash_ptr,
                                                  (const char **)&slash_ptr,
                                                  " \t", &result);
        if (result != SDP_SUCCESS) {
            sdp_parse_error(sdp_p,
                "%s Warning: Invalid subnet prefix specified in "
                "subnet attribute.", sdp_p->debug_str);
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
    } else {
        attr_p->attr.subnet.prefix = SDP_INVALID_VALUE;
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, network %s, addr type %s, address %s ",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_network_name(attr_p->attr.subnet.nettype),
                  sdp_get_address_name(attr_p->attr.subnet.addrtype),
                  attr_p->attr.subnet.addr);
        if (attr_p->attr.subnet.prefix != SDP_INVALID_VALUE) {
            SDP_PRINT("/%u", (ushort)attr_p->attr.subnet.prefix);
        }
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_subnet (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                    flex_string *fs)
{
  if (attr_p->attr.subnet.prefix == SDP_INVALID_VALUE) {
    flex_string_sprintf(fs, "a=%s:%s %s %s\r\n",
      sdp_attr[attr_p->type].name,
      sdp_get_network_name(attr_p->attr.subnet.nettype),
      sdp_get_address_name(attr_p->attr.subnet.addrtype),
      attr_p->attr.subnet.addr);
  } else {
    flex_string_sprintf(fs, "a=%s:%s %s %s/%u\r\n",
      sdp_attr[attr_p->type].name,
      sdp_get_network_name(attr_p->attr.subnet.nettype),
      sdp_get_address_name(attr_p->attr.subnet.addrtype),
      attr_p->attr.subnet.addr,
      (ushort)attr_p->attr.subnet.prefix);
  }

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_t38_ratemgmt (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                          const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    /* Find the rate mgmt. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No t38 rate management specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.t38ratemgmt = SDP_T38_UNKNOWN_RATE;
    for (i=0; i < SDP_T38_MAX_RATES; i++) {
        if (cpr_strncasecmp(tmp, sdp_t38_rate[i].name,
                        sdp_t38_rate[i].strlen) == 0) {
            attr_p->attr.t38ratemgmt = (sdp_t38_ratemgmt_e)i;
        }
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, rate %s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  sdp_get_t38_ratemgmt_name(attr_p->attr.t38ratemgmt));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_t38_ratemgmt (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                          flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%s\r\n",
    sdp_attr[attr_p->type].name,
    sdp_get_t38_ratemgmt_name(attr_p->attr.t38ratemgmt));

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_t38_udpec (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                       const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    /* Find the udpec. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No t38 udpEC specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.t38udpec = SDP_T38_UDPEC_UNKNOWN;
    for (i=0; i < SDP_T38_MAX_UDPEC; i++) {
        if (cpr_strncasecmp(tmp, sdp_t38_udpec[i].name,
                        sdp_t38_udpec[i].strlen) == 0) {
            attr_p->attr.t38udpec = (sdp_t38_udpec_e)i;
        }
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, udpec %s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  sdp_get_t38_udpec_name(attr_p->attr.t38udpec));
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_t38_udpec (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                       flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%s\r\n",
    sdp_attr[attr_p->type].name,
    sdp_get_t38_udpec_name(attr_p->attr.t38udpec));

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_pc_codec (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                      const char *ptr)
{
    uint16_t i;
    sdp_result_e result;

    for (i=0; i < SDP_MAX_PAYLOAD_TYPES; i++) {
        attr_p->attr.pccodec.payload_type[i] = (ushort)sdp_getnextnumtok(ptr, &ptr,
                                                               " \t", &result);
        if (result != SDP_SUCCESS) {
            break;
        }
        attr_p->attr.pccodec.num_payloads++;
    }

    if (attr_p->attr.pccodec.num_payloads == 0) {
        sdp_parse_error(sdp_p,
            "%s Warning: No payloads specified for %s attr.",
            sdp_p->debug_str, sdp_attr[attr_p->type].name);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, num payloads %u, payloads: ",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  attr_p->attr.pccodec.num_payloads);
        for (i=0; i < attr_p->attr.pccodec.num_payloads; i++) {
            SDP_PRINT("%u ", attr_p->attr.pccodec.payload_type[i]);
        }
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_pc_codec (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                      flex_string *fs)
{
  int i;

  flex_string_sprintf(fs, "a=%s: ", sdp_attr[attr_p->type].name);

  for (i=0; i < attr_p->attr.pccodec.num_payloads; i++) {
    flex_string_sprintf(fs, "%u ", attr_p->attr.pccodec.payload_type[i]);
  }

  flex_string_append(fs, "\r\n");

  return SDP_SUCCESS;
}


sdp_result_e sdp_parse_attr_cap (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                 const char *ptr)
{
    uint16_t           i;
    sdp_result_e  result;
    sdp_mca_t    *cap_p;
    char          tmp[SDP_MAX_STRING_LEN];

    /* Set the capability pointer to NULL for now in case we encounter
     * an error in parsing.
     */
    attr_p->attr.cap_p = NULL;
    /* Set the capability valid flag to FALSE in case we encounter an
     * error.  If we do, we don't want to process any X-cpar/cpar attributes
     * from this point until we process the next valid X-cap/cdsc attr. */
    sdp_p->cap_valid = FALSE;

    /* Allocate resource for new capability. Note that the capability
     * uses the same structure used for media lines.
     */
    cap_p = sdp_alloc_mca(sdp_p->parse_line);
    if (cap_p == NULL) {
        sdp_p->conf_p->num_no_resource++;
        return (SDP_NO_RESOURCE);
    }

    /* Find the capability number. We don't need to store this since we
     * calculate it for ourselves as we need to. But it must be specified. */
    (void)sdp_getnextnumtok(ptr, &ptr, "/ \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: Capability not specified for %s, "
            "unable to parse.", sdp_p->debug_str,
            sdp_get_attr_name(attr_p->type));
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the media type. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s No media type specified for %s attribute, "
            "unable to parse.",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    cap_p->media = SDP_MEDIA_UNSUPPORTED;
    for (i=0; i < SDP_MAX_MEDIA_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_media[i].name, sdp_media[i].strlen) == 0) {
            cap_p->media = (sdp_media_e)i;
            break;
        }
    }
    if (cap_p->media == SDP_MEDIA_UNSUPPORTED) {
        sdp_parse_error(sdp_p,
            "%s Warning: Media type unsupported (%s).",
            sdp_p->debug_str, tmp);
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the transport protocol type. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s No transport protocol type specified, "
            "unable to parse.", sdp_p->debug_str);
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    cap_p->transport = SDP_TRANSPORT_UNSUPPORTED;
    for (i=0; i < SDP_MAX_TRANSPORT_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_transport[i].name,
                        sdp_transport[i].strlen) == 0) {
            cap_p->transport = (sdp_transport_e)i;
            break;
        }
    }
    if (cap_p->transport == SDP_TRANSPORT_UNSUPPORTED) {
        sdp_parse_error(sdp_p,
            "%s Warning: Transport protocol type unsupported (%s).",
            sdp_p->debug_str, tmp);
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find payload formats. AAL2 X-cap lines allow multiple
     * transport/profile types per line, so these are handled differently.
     */
    if ((cap_p->transport == SDP_TRANSPORT_AAL2_ITU) ||
        (cap_p->transport == SDP_TRANSPORT_AAL2_ATMF) ||
        (cap_p->transport == SDP_TRANSPORT_AAL2_CUSTOM)) {
        /* Capability processing is not currently defined for AAL2 types
         * with multiple profiles. We don't process. */
        sdp_parse_error(sdp_p,
            "%s Warning: AAL2 profiles unsupported with "
            "%s attributes.", sdp_p->debug_str,
            sdp_get_attr_name(attr_p->type));
        SDP_FREE(cap_p);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        /* Transport is a non-AAL2 type.  Parse payloads normally. */
        sdp_parse_payload_types(sdp_p, cap_p, ptr);
        if (cap_p->num_payloads == 0) {
            SDP_FREE(cap_p);
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
    }

    attr_p->attr.cap_p = cap_p;
    /*
     * This capability attr is valid.  We can now handle X-cpar or
     * cpar attrs.
     */
    sdp_p->cap_valid = TRUE;
    sdp_p->last_cap_inst++;

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed %s media type %s, Transport %s, "
                  "Num payloads %u", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  sdp_get_media_name(cap_p->media),
                  sdp_get_transport_name(cap_p->transport),
                  cap_p->num_payloads);
    }
    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_cap (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                 flex_string *fs)
{
    uint16_t                   i, j;
    sdp_mca_t            *cap_p;
    sdp_media_profiles_t *profile_p;

    /* Get a pointer to the capability structure. */
    cap_p = attr_p->attr.cap_p;

    if (cap_p == NULL) {
        CSFLogError(logTag, "%s Invalid %s attribute, unable to build.",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        /* Return success so build won't fail. */
        return (SDP_SUCCESS);
    }

    /* Validate params for this capability line */
    if ((cap_p->media >= SDP_MAX_MEDIA_TYPES) ||
        (cap_p->transport >= SDP_MAX_TRANSPORT_TYPES)) {
        CSFLogDebug(logTag, logTag, "%s Media or transport type invalid for %s "
            "attribute, unable to build.", sdp_p->debug_str,
                        sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        /* Return success so build won't fail. */
        return (SDP_SUCCESS);
    }

    flex_string_sprintf(fs, "a=%s: %u %s ", sdp_attr[attr_p->type].name,
                     sdp_p->cur_cap_num, sdp_get_media_name(cap_p->media));

    /* If the X-cap line has AAL2 profiles, build them differently. */
    if ((cap_p->transport == SDP_TRANSPORT_AAL2_ITU) ||
        (cap_p->transport == SDP_TRANSPORT_AAL2_ATMF) ||
        (cap_p->transport == SDP_TRANSPORT_AAL2_CUSTOM)) {
        profile_p = cap_p->media_profiles_p;
        for (i=0; i < profile_p->num_profiles; i++) {
            flex_string_sprintf(fs, "%s",
                             sdp_get_transport_name(profile_p->profile[i]));

            for (j=0; j < profile_p->num_payloads[i]; j++) {
                flex_string_sprintf(fs, " %u",
                                 profile_p->payload_type[i][j]);
            }
            flex_string_append(fs, " ");
        }

        flex_string_append(fs, "\r\n");
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Built m= media line", sdp_p->debug_str);
        }
        return SDP_SUCCESS;
    }

    /* Build the transport name */
    flex_string_sprintf(fs, "%s", sdp_get_transport_name(cap_p->transport));

    /* Build the format lists */
    for (i=0; i < cap_p->num_payloads; i++) {
        if (cap_p->payload_indicator[i] == SDP_PAYLOAD_ENUM) {
            flex_string_sprintf(fs, " %s",
                             sdp_get_payload_name((sdp_payload_e)cap_p->payload_type[i]));
        } else {
            flex_string_sprintf(fs, " %u", cap_p->payload_type[i]);
        }
    }

    flex_string_append(fs, "\r\n");

    /* Increment the current capability number for the next X-cap/cdsc attr. */
    sdp_p->cur_cap_num += cap_p->num_payloads;
    sdp_p->last_cap_type = attr_p->type;

    /* Build any X-cpar/cpar attributes associated with this X-cap/cdsc line. */
    return sdp_build_attr_cpar(sdp_p, cap_p->media_attrs_p, fs);
}


sdp_result_e sdp_parse_attr_cpar (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                  const char *ptr)
{
    uint16_t           i;
    sdp_result_e  result;
    sdp_mca_t    *cap_p;
    sdp_attr_t   *cap_attr_p = NULL;
    sdp_attr_t   *prev_attr_p;
    char          tmp[SDP_MAX_STRING_LEN];

    /* Make sure we've processed a valid X-cap/cdsc attr prior to this and
     * if so, get the cap pointer. */
    if (sdp_p->cap_valid == TRUE) {
        sdp_attr_e cap_type;

        if (attr_p->type == SDP_ATTR_CPAR) {
            cap_type = SDP_ATTR_CDSC;
        } else {
            /* Default to X-CAP for everything else */
            cap_type = SDP_ATTR_X_CAP;
        }

        if (sdp_p->mca_count == 0) {
            cap_attr_p = sdp_find_attr(sdp_p, SDP_SESSION_LEVEL, 0,
                                       cap_type, sdp_p->last_cap_inst);
        } else {
            cap_attr_p = sdp_find_attr(sdp_p, sdp_p->mca_count, 0,
                                       cap_type, sdp_p->last_cap_inst);
        }
    }
    if ((cap_attr_p == NULL) || (cap_attr_p->attr.cap_p == NULL)) {
        sdp_parse_error(sdp_p,
            "%s Warning: %s attribute specified with no "
            "prior %s attribute", sdp_p->debug_str,
                         sdp_get_attr_name(attr_p->type),
                         (attr_p->type == SDP_ATTR_CPAR)?
                               (sdp_get_attr_name(SDP_ATTR_CDSC)) :
                               (sdp_get_attr_name(SDP_ATTR_X_CAP)) );
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /*
     * Ensure there is no mixed syntax like CDSC followed by X-CPAR
     * or X-CAP followed by CPAR.
     */
    if (((cap_attr_p->type == SDP_ATTR_CDSC) &&
        (attr_p->type == SDP_ATTR_X_CPAR)) ||
        ( (cap_attr_p->type == SDP_ATTR_X_CAP) &&
          (attr_p->type == SDP_ATTR_CPAR)) ) {
        sdp_parse_error(sdp_p,
            "%s Warning: %s attribute inconsistent with "
            "prior %s attribute", sdp_p->debug_str,
            sdp_get_attr_name(attr_p->type),
            sdp_get_attr_name(cap_attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    cap_p = cap_attr_p->attr.cap_p;

    /* a= is the only token we handle in an X-cpar/cpar attribute. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), "= \t", &result);

    if ((result != SDP_SUCCESS) || (tmp[0] != 'a') || (tmp[1] != '\0')) {
        sdp_parse_error(sdp_p,
            "%s Warning: Invalid token type (%s) in %s "
            "attribute, unable to parse", sdp_p->debug_str, tmp,
            sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    /*sa_ignore NO_NULL_CHK
     *{ptr is valid since the pointer was checked earlier and the
     * function would have exited if NULL.}
     */
    if (*ptr == '=') {
        ptr++;
    }

    /* Find the attribute type. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), ": \t", &result);
    /*sa_ignore NO_NULL_CHK
     *{ptr is valid since the pointer was checked earlier and the
     * function would have exited if NULL.}
     */
    if (ptr[0] == ':') {
        /* Skip the ':' char for parsing attribute parameters. */
        ptr++;
    }
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s No attribute type specified for %s attribute, unable to parse.",
            sdp_p->debug_str,
            sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Reset the type of the attribute from X-cpar/cpar to whatever the
     * specified type is. */
    attr_p->type = SDP_ATTR_INVALID;
    attr_p->next_p = NULL;
    for (i=0; i < SDP_MAX_ATTR_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_attr[i].name, sdp_attr[i].strlen) == 0) {
            attr_p->type = (sdp_attr_e)i;
        }
    }
    if (attr_p->type == SDP_ATTR_INVALID) {
        sdp_parse_error(sdp_p,
            "%s Warning: Unrecognized attribute (%s) for %s attribute, unable to parse.",
            sdp_p->debug_str, tmp,
            sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* We don't allow recursion with the capability attributes. */
    if ((attr_p->type == SDP_ATTR_X_SQN) ||
        (attr_p->type == SDP_ATTR_X_CAP) ||
        (attr_p->type == SDP_ATTR_X_CPAR) ||
        (attr_p->type == SDP_ATTR_SQN) ||
        (attr_p->type == SDP_ATTR_CDSC) ||
        (attr_p->type == SDP_ATTR_CPAR)) {
        sdp_parse_error(sdp_p,
            "%s Warning: Invalid attribute (%s) for %s"
            " attribute, unable to parse.", sdp_p->debug_str, tmp,
            sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Parse the attribute. */
    result = sdp_attr[attr_p->type].parse_func(sdp_p, attr_p, ptr);
    if (result != SDP_SUCCESS) {
        return (result);
    }

    /* Hook the attribute into the capability structure. */
    if (cap_p->media_attrs_p == NULL) {
        cap_p->media_attrs_p = attr_p;
    } else {
        for (prev_attr_p = cap_p->media_attrs_p;
             prev_attr_p->next_p != NULL;
             prev_attr_p = prev_attr_p->next_p) {
            ; /* Empty for */
        }
        prev_attr_p->next_p = attr_p;
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_cpar (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                  flex_string *fs)
{
    sdp_result_e  result;
    const char   *cpar_name;

    /* Determine whether to use cpar or X-cpar */
    if (sdp_p->last_cap_type == SDP_ATTR_CDSC) {
        cpar_name = sdp_get_attr_name(SDP_ATTR_CPAR);
    } else {
        /*
         * Default to X-CPAR if anything else. This is the backward
         * compatible value.
         */
        cpar_name = sdp_get_attr_name(SDP_ATTR_X_CPAR);
    }

    while (attr_p != NULL) {
        if (attr_p->type >= SDP_MAX_ATTR_TYPES) {
            CSFLogDebug(logTag, "%s Invalid attribute type to build (%u)",
                sdp_p->debug_str, (unsigned)attr_p->type);
        } else {
            flex_string_sprintf(fs, "a=%s: ", cpar_name);

            result = sdp_attr[attr_p->type].build_func(sdp_p, attr_p, fs);

            if (result == SDP_SUCCESS) {
                if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
                    SDP_PRINT("%s Built %s a=%s attribute line",
                              sdp_p->debug_str, cpar_name,
                              sdp_get_attr_name(attr_p->type));
                }
            }
        }
        attr_p = attr_p->next_p;
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_rtcp (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                           const char *ptr)
{
  sdp_result_e result;
  char nettype[SDP_MAX_STRING_LEN];
  sdp_rtcp_t *rtcp_p = &(attr_p->attr.rtcp);
  int enum_raw;

  memset(rtcp_p, 0, sizeof(sdp_rtcp_t));

  rtcp_p->port = (uint16_t)sdp_getnextnumtok(ptr, &ptr, " \t", &result);
  if (result != SDP_SUCCESS) {
    sdp_parse_error(sdp_p,
                    "%s Warning: could not parse port for rtcp attribute",
                    sdp_p->debug_str);
    sdp_p->conf_p->num_invalid_param++;

    return SDP_INVALID_PARAMETER;
  }

  /* The rest is optional, although it is all-or-nothing */
  (void)sdp_getnextstrtok(ptr, nettype, sizeof(nettype), " \t", &result);
  if (result == SDP_EMPTY_TOKEN) {
    /* Nothing after the port */
    return SDP_SUCCESS;
  }

  enum_raw = find_token_enum("Nettype", sdp_p, &ptr, sdp_nettype,
                             SDP_MAX_NETWORK_TYPES, SDP_NT_UNSUPPORTED);
  if (enum_raw == -1) {
    return SDP_INVALID_PARAMETER;
  }
  rtcp_p->nettype = (sdp_nettype_e)enum_raw;

  enum_raw = find_token_enum("Addrtype", sdp_p, &ptr, sdp_addrtype,
                             SDP_MAX_ADDR_TYPES, SDP_AT_UNSUPPORTED);
  if (enum_raw == -1) {
    return SDP_INVALID_PARAMETER;
  }
  rtcp_p->addrtype = (sdp_addrtype_e)enum_raw;

  ptr = sdp_getnextstrtok(ptr, rtcp_p->addr, sizeof(rtcp_p->addr), " \t",
                          &result);
  if (result != SDP_SUCCESS) {
    sdp_parse_error(sdp_p,
                    "%s Warning: could not parse addr for rtcp attribute",
                    sdp_p->debug_str);
    sdp_p->conf_p->num_invalid_param++;

    return SDP_INVALID_PARAMETER;
  }

  return SDP_SUCCESS;
}

sdp_result_e sdp_build_attr_rtcp (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                           flex_string *fs)
{
  /* We should not be serializing SDP anyway, but we need this function until
   * Bug 1112737 is resolved. */
  return SDP_FAILURE;
}

sdp_result_e sdp_parse_attr_rtr (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                           const char *ptr)
{
    sdp_result_e  result;
    char tmp[SDP_MAX_STRING_LEN];

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsing a=%s, %s", sdp_p->debug_str,
                     sdp_get_attr_name(attr_p->type),
                     tmp);
    }
    /*Default confirm to FALSE. */
    attr_p->attr.rtr.confirm = FALSE;

    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS){ // No confirm tag specified is not an error
        return (SDP_SUCCESS);
    } else {
       /* See if confirm was specified.  Defaults to FALSE. */
       if (cpr_strncasecmp(tmp, "confirm", sizeof("confirm")) == 0) {
           attr_p->attr.rtr.confirm = TRUE;
       }
       if (attr_p->attr.rtr.confirm == FALSE) {
          sdp_parse_error(sdp_p,
              "%s Warning: RTR confirm parameter invalid (%s)",
              sdp_p->debug_str, tmp);
           sdp_p->conf_p->num_invalid_param++;
           return (SDP_INVALID_PARAMETER);
       }
       if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
           SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                     sdp_get_attr_name(attr_p->type),
                     tmp);
       }
       return (SDP_SUCCESS);
    }
}

sdp_result_e sdp_build_attr_rtr (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                           flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s%s\r\n",
    sdp_attr[attr_p->type].name,
    attr_p->attr.rtr.confirm ? ":confirm" : "");

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_comediadir (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                        const char *ptr)
{
    int i;
    sdp_result_e  result;
    tinybool      type_found = FALSE;
    char          tmp[SDP_MAX_STRING_LEN];

    attr_p->attr.comediadir.role = SDP_MEDIADIR_ROLE_PASSIVE;
    attr_p->attr.comediadir.conn_info_present = FALSE;
    attr_p->attr.comediadir.conn_info.nettype = SDP_NT_INVALID;
    attr_p->attr.comediadir.src_port = 0;

    /* Find the media direction role. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), ": \t", &result);

    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No role parameter specified for "
            "comediadir attribute.", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.comediadir.role = SDP_MEDIADIR_ROLE_UNSUPPORTED;
    for (i=0; i < SDP_MAX_MEDIADIR_ROLES; i++) {
        if (cpr_strncasecmp(tmp, sdp_mediadir_role[i].name,
                        sdp_mediadir_role[i].strlen) == 0) {
            type_found = TRUE;
            attr_p->attr.comediadir.role = (sdp_mediadir_role_e)i;
            break;
        }
    }
    if (attr_p->attr.comediadir.role == SDP_MEDIADIR_ROLE_UNSUPPORTED) {
        sdp_parse_error(sdp_p,
            "%s Warning: Invalid role type specified for "
            "comediadir attribute (%s).", sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* If the role is passive, we don't expect any more params. */
    if (attr_p->attr.comediadir.role == SDP_MEDIADIR_ROLE_PASSIVE) {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, passive",
                      sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        }
        return (SDP_SUCCESS);
    }

    /* Find the connection information if present */
    /* parse to get the nettype */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No network type specified in comediadir "
            "attribute.", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_SUCCESS); /* as the optional parameters are not there */
    }
    attr_p->attr.comediadir.conn_info.nettype = SDP_NT_UNSUPPORTED;
    for (i=0; i < SDP_MAX_NETWORK_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_nettype[i].name,
                        sdp_nettype[i].strlen) == 0) {
            type_found = TRUE;
        }
        if (type_found == TRUE) {
            if (sdp_p->conf_p->nettype_supported[i] == TRUE) {
                attr_p->attr.comediadir.conn_info.nettype = (sdp_nettype_e)i;
            }
            type_found = FALSE;
        }
    }
    if (attr_p->attr.comediadir.conn_info.nettype == SDP_NT_UNSUPPORTED) {
        sdp_parse_error(sdp_p,
            "%s Warning: ConnInfo in Comediadir: network type "
            "unsupported (%s).", sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
    }

    /* Find the comedia address type. */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No address type specified in comediadir"
            " attribute.", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
    }
    attr_p->attr.comediadir.conn_info.addrtype = SDP_AT_UNSUPPORTED;
    for (i=0; i < SDP_MAX_ADDR_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_addrtype[i].name,
                        sdp_addrtype[i].strlen) == 0) {
            type_found = TRUE;
        }
        if (type_found == TRUE) {
            if (sdp_p->conf_p->addrtype_supported[i] == TRUE) {
                attr_p->attr.comediadir.conn_info.addrtype = (sdp_addrtype_e)i;
            }
            type_found = FALSE;
        }
    }
    if (attr_p->attr.comediadir.conn_info.addrtype == SDP_AT_UNSUPPORTED) {
        sdp_parse_error(sdp_p,
            "%s Warning: Conninfo address type unsupported "
            "(%s).", sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
    }

    /* Find the conninfo address.  */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.comediadir.conn_info.conn_addr,
                            sizeof(attr_p->attr.comediadir.conn_info.conn_addr), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No conninfo address specified in "
            "comediadir attribute.", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
    }

    /* Find the src port info , if any */
    attr_p->attr.comediadir.src_port  = sdp_getnextnumtok(ptr, &ptr, " \t",
                                                          &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No src port specified in "
            "comediadir attribute.", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, network %s, addr type %s, address %s "
                  "srcport %u ",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  sdp_get_network_name(attr_p->attr.comediadir.conn_info.nettype),
                  sdp_get_address_name(attr_p->attr.comediadir.conn_info.addrtype),
                  attr_p->attr.comediadir.conn_info.conn_addr,
                  (unsigned int)attr_p->attr.comediadir.src_port);
    }

    if (sdp_p->conf_p->num_invalid_param > 0) {
        return (SDP_INVALID_PARAMETER);
    }
    return (SDP_SUCCESS);
}

sdp_result_e
sdp_build_attr_comediadir (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                    flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%s\r\n",
    sdp_attr[attr_p->type].name,
    sdp_get_mediadir_role_name(attr_p->attr.comediadir.role));

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_silencesupp (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                         const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    /* Find silenceSuppEnable */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);

    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s No silenceSupp enable value specified, parse failed.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (cpr_strncasecmp(tmp, "on", sizeof("on")) == 0) {
        attr_p->attr.silencesupp.enabled = TRUE;
    } else if (cpr_strncasecmp(tmp, "off", sizeof("off")) == 0) {
        attr_p->attr.silencesupp.enabled = FALSE;
    } else if (cpr_strncasecmp(tmp, "-", sizeof("-")) == 0) {
        attr_p->attr.silencesupp.enabled = FALSE;
    } else {
        sdp_parse_error(sdp_p,
            "%s Warning: silenceSuppEnable parameter invalid (%s)",
            sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find silenceTimer -- uint16_t or "-" */

    attr_p->attr.silencesupp.timer =
        (uint16_t)sdp_getnextnumtok_or_null(ptr, &ptr, " \t",
                                       &attr_p->attr.silencesupp.timer_null,
                                       &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: Invalid timer value specified for "
            "silenceSupp attribute.", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find suppPref */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No silenceSupp pref specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.silencesupp.pref = SDP_SILENCESUPP_PREF_UNKNOWN;
    for (i=0; i < SDP_MAX_SILENCESUPP_PREF; i++) {
        if (cpr_strncasecmp(tmp, sdp_silencesupp_pref[i].name,
                        sdp_silencesupp_pref[i].strlen) == 0) {
            attr_p->attr.silencesupp.pref = (sdp_silencesupp_pref_e)i;
        }
    }
    if (attr_p->attr.silencesupp.pref == SDP_SILENCESUPP_PREF_UNKNOWN) {
        sdp_parse_error(sdp_p,
            "%s Warning: silenceSupp pref unrecognized (%s)",
            sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find sidUse */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No silenceSupp sidUse specified.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.silencesupp.siduse = SDP_SILENCESUPP_SIDUSE_UNKNOWN;
    for (i=0; i < SDP_MAX_SILENCESUPP_SIDUSE; i++) {
        if (cpr_strncasecmp(tmp, sdp_silencesupp_siduse[i].name,
                        sdp_silencesupp_siduse[i].strlen) == 0) {
            attr_p->attr.silencesupp.siduse = (sdp_silencesupp_siduse_e)i;
        }
    }
    if (attr_p->attr.silencesupp.siduse == SDP_SILENCESUPP_SIDUSE_UNKNOWN) {
        sdp_parse_error(sdp_p,
            "%s Warning: silenceSupp sidUse unrecognized (%s)",
            sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find fxnslevel -- uint8_t or "-" */
    attr_p->attr.silencesupp.fxnslevel =
        (uint8_t)sdp_getnextnumtok_or_null(ptr, &ptr, " \t",
                                      &attr_p->attr.silencesupp.fxnslevel_null,
                                      &result);

    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: Invalid fxnslevel value specified for "
            "silenceSupp attribute.", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, enabled %s",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  (attr_p->attr.silencesupp.enabled ? "on" : "off"));
        if (attr_p->attr.silencesupp.timer_null) {
            SDP_PRINT(" timer=-");
        } else {
            SDP_PRINT(" timer=%u,", attr_p->attr.silencesupp.timer);
        }
        SDP_PRINT(" pref=%s, siduse=%s,",
                  sdp_get_silencesupp_pref_name(attr_p->attr.silencesupp.pref),
                  sdp_get_silencesupp_siduse_name(
                                             attr_p->attr.silencesupp.siduse));
        if (attr_p->attr.silencesupp.fxnslevel_null) {
            SDP_PRINT(" fxnslevel=-");
        } else {
            SDP_PRINT(" fxnslevel=%u,", attr_p->attr.silencesupp.fxnslevel);
        }
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_silencesupp (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                         flex_string *fs)
{
  char temp_timer_string[11];
  char temp_fxnslevel_string[11];

  if (attr_p->attr.silencesupp.timer_null) {
    snprintf(temp_timer_string, sizeof(temp_timer_string), "-");
  } else {
    snprintf(temp_timer_string, sizeof(temp_timer_string), "%u", attr_p->attr.silencesupp.timer);
  }

  if (attr_p->attr.silencesupp.fxnslevel_null) {
    snprintf(temp_fxnslevel_string, sizeof(temp_fxnslevel_string), "-");
  } else {
    snprintf(temp_fxnslevel_string, sizeof(temp_fxnslevel_string), "%u", attr_p->attr.silencesupp.fxnslevel);
  }

  flex_string_sprintf(fs, "a=%s:%s %s %s %s %s\r\n",
    sdp_attr[attr_p->type].name,
    (attr_p->attr.silencesupp.enabled ? "on" : "off"),
    temp_timer_string,
    sdp_get_silencesupp_pref_name(attr_p->attr.silencesupp.pref),
    sdp_get_silencesupp_siduse_name(attr_p->attr.silencesupp.siduse),
    temp_fxnslevel_string);

  return SDP_SUCCESS;
}

/*
 * sdp_parse_context_crypto_suite
 *
 * This routine parses the crypto suite pointed to by str, stores the crypto suite value into the
 * srtp context suite component of the LocalConnectionOptions pointed to by lco_node_ptr and stores
 * pointer to the next crypto parameter in tmp_ptr
 */
tinybool sdp_parse_context_crypto_suite(char * str,  sdp_attr_t *attr_p, sdp_t *sdp_p) {
      /*
       * Three crypto_suites are defined: (Notice no SPACE between "crypto:" and the <crypto-suite>
       * AES_CM_128_HMAC_SHA1_80
       * AES_CM_128_HMAC_SHA1_32
       * F8_128_HMAC_SHA1_80
       */

       int i;

       /* Check crypto suites */
       for(i=0; i<SDP_SRTP_MAX_NUM_CRYPTO_SUITES; i++) {
         if (!cpr_strcasecmp(sdp_srtp_crypto_suite_array[i].crypto_suite_str, str)) {
           attr_p->attr.srtp_context.suite = sdp_srtp_crypto_suite_array[i].crypto_suite_val;
           attr_p->attr.srtp_context.master_key_size_bytes =
               sdp_srtp_crypto_suite_array[i].key_size_bytes;
           attr_p->attr.srtp_context.master_salt_size_bytes =
               sdp_srtp_crypto_suite_array[i].salt_size_bytes;
           return TRUE; /* There is a succesful match so exit */
         }
       }
       /* couldn't find a matching crypto suite */
       sdp_parse_error(sdp_p,
            "%s No Matching crypto suite for SRTP Context(%s)-'X-crypto:v1' expected",
            sdp_p->debug_str, str);

       return FALSE;
}


sdp_result_e sdp_build_attr_srtpcontext (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                    flex_string *fs)
{
#define MAX_BASE64_ENCODE_SIZE_BYTES 60
    int          output_len = MAX_BASE64_ENCODE_SIZE_BYTES;
    int                  key_size = attr_p->attr.srtp_context.master_key_size_bytes;
    int                  salt_size = attr_p->attr.srtp_context.master_salt_size_bytes;
    unsigned char  base64_encoded_data[MAX_BASE64_ENCODE_SIZE_BYTES];
    unsigned char  base64_encoded_input[MAX_BASE64_ENCODE_SIZE_BYTES];
    base64_result_t status;

    output_len = MAX_BASE64_ENCODE_SIZE_BYTES;

    /* Append master and salt keys */
    memcpy(base64_encoded_input,
           attr_p->attr.srtp_context.master_key,
           key_size );
    memcpy(base64_encoded_input + key_size,
           attr_p->attr.srtp_context.master_salt,
           salt_size );

    if ((status = base64_encode(base64_encoded_input, key_size + salt_size,
                      base64_encoded_data, &output_len)) != BASE64_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Error: Failure to Base64 Encoded data (%s) ",
                     sdp_p->debug_str, BASE64_RESULT_TO_STRING(status));
        }
        return (SDP_INVALID_PARAMETER);
    }

    *(base64_encoded_data + output_len) = '\0';

    flex_string_sprintf(fs, "a=%s:%s inline:%s||\r\n",
      sdp_attr[attr_p->type].name,
      sdp_srtp_context_crypto_suite[attr_p->attr.srtp_context.suite].name,
      base64_encoded_data);

    return SDP_SUCCESS;
}

/*
 * sdp_parse_attr_mptime
 * This function parses the a=mptime sdp line. This parameter consists of
 * one or more numbers or hyphens ("-"). The first parameter must be a
 * number. The number of parameters must match the number of formats specified
 * on the m= line. This function is liberal in that it does not match against
 * the m= line or require a number for the first parameter.
 */
sdp_result_e sdp_parse_attr_mptime (
    sdp_t *sdp_p,
    sdp_attr_t *attr_p,
    const char *ptr)
{
    uint16_t i;                      /* loop counter for parameters */
    sdp_result_e result;        /* value returned by this function */
    tinybool null_ind;          /* true if a parameter is "-" */

    /*
     * Scan the input line up to the maximum number of parameters supported.
     * Look for numbers or hyphens and store the resulting values. Hyphens
     * are stored as zeros.
     */
    for (i=0; i<SDP_MAX_PAYLOAD_TYPES; i++) {
        attr_p->attr.mptime.intervals[i] =
            (ushort)sdp_getnextnumtok_or_null(ptr,&ptr," \t",&null_ind,&result);
        if (result != SDP_SUCCESS) {
            break;
        }
        attr_p->attr.mptime.num_intervals++;
    }

    /*
     * At least one parameter must be supplied. If not, return an error
     * and optionally log the failure.
     */
    if (attr_p->attr.mptime.num_intervals == 0) {
        sdp_parse_error(sdp_p,
            "%s Warning: No intervals specified for %s attr.",
            sdp_p->debug_str, sdp_attr[attr_p->type].name);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /*
     * Here is some debugging code that helps us track what data
     * is received and parsed.
     */
    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, num intervals %u, intervals: ",
                  sdp_p->debug_str, sdp_get_attr_name(attr_p->type),
                  attr_p->attr.mptime.num_intervals);
        for (i=0; i < attr_p->attr.mptime.num_intervals; i++) {
            SDP_PRINT("%u ", attr_p->attr.mptime.intervals[i]);
        }
    }

    return SDP_SUCCESS;
}

/*
 * sdp_build_attr_mptime
 * This function builds the a=mptime sdp line. It reads the selected attribute
 * from the sdp structure. Parameters with a value of zero are replaced by
 * hyphens.
 */
sdp_result_e sdp_build_attr_mptime (
    sdp_t *sdp_p,
    sdp_attr_t *attr_p,
    flex_string *fs)
{
  int i;

  flex_string_sprintf(fs, "a=%s:", sdp_attr[attr_p->type].name);

  /*
   * Run the list of mptime parameter values and write each one
   * to the sdp line. Replace zeros with hyphens.
   */
  for (i=0; i < attr_p->attr.mptime.num_intervals; i++) {
    if (i > 0) {
      flex_string_append(fs, " ");
    }

    if (attr_p->attr.mptime.intervals[i] == 0) {
      flex_string_append(fs, "-");
    } else {
      flex_string_sprintf(fs, "%u", attr_p->attr.mptime.intervals[i]);
    }
  }

  flex_string_append(fs, "\r\n");

  return SDP_SUCCESS;
}



sdp_result_e sdp_parse_attr_x_sidin (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                     const char *ptr)
{
    sdp_result_e  result;
    attr_p->attr.stream_data.x_sidin[0]  = '\0';

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsing a=%s", sdp_p->debug_str,
                     sdp_get_attr_name(attr_p->type));
    }

    /* Find the X-sidin value */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.stream_data.x_sidin,
                            sizeof(attr_p->attr.stream_data.x_sidin), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No Stream Id incoming specified for X-sidin attribute.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  attr_p->attr.stream_data.x_sidin);
    }
   return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_x_sidin (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                      flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%s\r\n",
    sdp_attr[attr_p->type].name,
    attr_p->attr.stream_data.x_sidin);

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_x_sidout (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                      const char *ptr)
{
    sdp_result_e  result;
    attr_p->attr.stream_data.x_sidout[0]  = '\0';

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsing a=%s", sdp_p->debug_str,
                     sdp_get_attr_name(attr_p->type));
    }

    /* Find the X-sidout value */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.stream_data.x_sidout,
                            sizeof(attr_p->attr.stream_data.x_sidout), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No Stream Id outgoing specified for X-sidout attribute.",
            sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  attr_p->attr.stream_data.x_sidout);
    }
   return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_x_sidout (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                      flex_string *fs)
{
  flex_string_sprintf(fs, "a=%s:%s\r\n",
    sdp_attr[attr_p->type].name,
    attr_p->attr.stream_data.x_sidout);

  return SDP_SUCCESS;
}


sdp_result_e sdp_parse_attr_x_confid (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                      const char *ptr)
{
    sdp_result_e  result;
    attr_p->attr.stream_data.x_confid[0]  = '\0';

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsing a=%s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type));
    }

    /* Find the X-confid value */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.stream_data.x_confid,
                            sizeof(attr_p->attr.stream_data.x_confid), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No Conf Id incoming specified for "
            "X-confid attribute.", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  attr_p->attr.stream_data.x_confid);
    }
    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_x_confid (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                      flex_string *fs)
{
  if (strlen(attr_p->attr.stream_data.x_confid) <= 0) {
    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
      SDP_PRINT("%s X-confid value is not set. Cannot build a=X-confid line\n",
        sdp_p->debug_str);
    }

    return SDP_INVALID_PARAMETER;
  }

  flex_string_sprintf(fs, "a=%s:%s\r\n",
    sdp_attr[attr_p->type].name,
    attr_p->attr.stream_data.x_confid);

  return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_group (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                   const char *ptr)
{
    sdp_result_e  result;
    char tmp[64];
    int i=0;

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsing a=%s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type));
    }

    /* Find the a=group:<attrib> <id1> < id2> ... values */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No group attribute value specified for "
            "a=group line", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    attr_p->attr.stream_data.group_attr = SDP_GROUP_ATTR_UNSUPPORTED;
    for (i=0; i < SDP_MAX_GROUP_ATTR_VAL; i++) {
        if (cpr_strncasecmp(tmp, sdp_group_attr_val[i].name,
                        sdp_group_attr_val[i].strlen) == 0) {
            attr_p->attr.stream_data.group_attr = (sdp_group_attr_e)i;
            break;
        }
    }

    if (attr_p->attr.stream_data.group_attr == SDP_GROUP_ATTR_UNSUPPORTED) {
        sdp_parse_error(sdp_p,
            "%s Warning: Group attribute type unsupported (%s).",
            sdp_p->debug_str, tmp);
    }


    /*
     * Scan the input line up after group:<attr>  to the maximum number
     * of id available.
     */
    attr_p->attr.stream_data.num_group_id =0;

    for (i=0; i<SDP_MAX_MEDIA_STREAMS; i++) {
        ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);

        if (result != SDP_SUCCESS) {
            break;
        }
        attr_p->attr.stream_data.group_ids[i] = cpr_strdup(tmp);
        if (!attr_p->attr.stream_data.group_ids[i]) {
            break;
        }

        attr_p->attr.stream_data.num_group_id++;
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s:%s\n", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  sdp_get_group_attr_name (attr_p->attr.stream_data.group_attr));
        for (i=0; i < attr_p->attr.stream_data.num_group_id; i++) {
            SDP_PRINT("%s Parsed group line id : %s\n", sdp_p->debug_str,
                      attr_p->attr.stream_data.group_ids[i]);
        }
    }
    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_group (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                   flex_string *fs)
{
  int i;

  flex_string_sprintf(fs, "a=%s:%s",
    sdp_attr[attr_p->type].name,
    sdp_get_group_attr_name(attr_p->attr.stream_data.group_attr));

  for (i=0; i < attr_p->attr.stream_data.num_group_id; i++) {
    if (attr_p->attr.stream_data.group_ids[i]) {
      flex_string_sprintf(fs, " %s",
        attr_p->attr.stream_data.group_ids[i]);
    }
  }

  flex_string_append(fs, "\r\n");

  return SDP_SUCCESS;
}

/* Parse the source-filter attribute
 * "a=source-filter:<filter-mode><filter-spec>"
 *  <filter-spec> = <nettype><addrtype><dest-addr><src_addr><src_addr>...
 */
sdp_result_e sdp_parse_attr_source_filter (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                           const char *ptr)
{
    int i;
    sdp_result_e result;
    char tmp[SDP_MAX_STRING_LEN];

    attr_p->attr.source_filter.mode = SDP_FILTER_MODE_NOT_PRESENT;
    attr_p->attr.source_filter.nettype = SDP_NT_UNSUPPORTED;
    attr_p->attr.source_filter.addrtype = SDP_AT_UNSUPPORTED;
    attr_p->attr.source_filter.dest_addr[0] = '\0';
    attr_p->attr.source_filter.num_src_addr = 0;

    /* Find the filter mode */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No src filter attribute value specified for "
                     "a=source-filter line", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    for (i = 0; i < SDP_MAX_FILTER_MODE; i++) {
        if (cpr_strncasecmp(tmp, sdp_src_filter_mode_val[i].name,
                        sdp_src_filter_mode_val[i].strlen) == 0) {
            attr_p->attr.source_filter.mode = (sdp_src_filter_mode_e)i;
            break;
        }
    }
    if (attr_p->attr.source_filter.mode == SDP_FILTER_MODE_NOT_PRESENT) {
        /* No point continuing */
        sdp_parse_error(sdp_p,
            "%s Warning: Invalid src filter mode for a=source-filter "
            "line", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the network type */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    for (i = 0; i < SDP_MAX_NETWORK_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_nettype[i].name,
                        sdp_nettype[i].strlen) == 0) {
            if (sdp_p->conf_p->nettype_supported[i] == TRUE) {
                attr_p->attr.source_filter.nettype = (sdp_nettype_e)i;
            }
        }
    }
    if (attr_p->attr.source_filter.nettype == SDP_NT_UNSUPPORTED) {
        sdp_parse_error(sdp_p,
            "%s Warning: Network type unsupported "
            "(%s) for a=source-filter", sdp_p->debug_str, tmp);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the address type */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    for (i = 0; i < SDP_MAX_ADDR_TYPES; i++) {
        if (cpr_strncasecmp(tmp, sdp_addrtype[i].name,
                        sdp_addrtype[i].strlen) == 0) {
            if (sdp_p->conf_p->addrtype_supported[i] == TRUE) {
                attr_p->attr.source_filter.addrtype = (sdp_addrtype_e)i;
            }
        }
    }
    if (attr_p->attr.source_filter.addrtype == SDP_AT_UNSUPPORTED) {
        if (strncmp(tmp, "*", 1) == 0) {
            attr_p->attr.source_filter.addrtype = SDP_AT_FQDN;
        } else {
            sdp_parse_error(sdp_p,
                "%s Warning: Address type unsupported "
                "(%s) for a=source-filter", sdp_p->debug_str, tmp);
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
    }

    /* Find the destination addr */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.source_filter.dest_addr,
                            sizeof(attr_p->attr.source_filter.dest_addr), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s No filter destination address specified for "
            "a=source-filter", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Find the list of source address to apply the filter */
    for (i = 0; i < SDP_MAX_SRC_ADDR_LIST; i++) {
        ptr = sdp_getnextstrtok(ptr, attr_p->attr.source_filter.src_list[i],
                                sizeof(attr_p->attr.source_filter.src_list[i]), " \t", &result);
        if (result != SDP_SUCCESS) {
            break;
        }
        attr_p->attr.source_filter.num_src_addr++;
    }
    if (attr_p->attr.source_filter.num_src_addr == 0) {
        sdp_parse_error(sdp_p,
            "%s Warning: No source list provided "
            "for a=source-filter", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_source_filter (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                      flex_string *fs)
{
  int i;

  flex_string_sprintf(fs, "a=%s:%s %s %s %s",
    sdp_get_attr_name(attr_p->type),
    sdp_get_src_filter_mode_name(attr_p->attr.source_filter.mode),
    sdp_get_network_name(attr_p->attr.source_filter.nettype),
    sdp_get_address_name(attr_p->attr.source_filter.addrtype),
    attr_p->attr.source_filter.dest_addr);

  for (i = 0; i < attr_p->attr.source_filter.num_src_addr; i++) {
    flex_string_append(fs, " ");
    flex_string_append(fs, attr_p->attr.source_filter.src_list[i]);
  }

  flex_string_append(fs, "\r\n");

  return SDP_SUCCESS;
}

/* Parse the rtcp-unicast attribute
 * "a=rtcp-unicast:<reflection|rsi>"
 */
sdp_result_e sdp_parse_attr_rtcp_unicast (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                          const char *ptr)
{
    sdp_result_e result;
    uint32_t i;
    char tmp[SDP_MAX_STRING_LEN];

    attr_p->attr.u32_val = SDP_RTCP_UNICAST_MODE_NOT_PRESENT;

    memset(tmp, 0, sizeof(tmp));

    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No rtcp unicast mode specified for "
            "a=rtcp-unicast line", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    for (i = 0; i < SDP_RTCP_MAX_UNICAST_MODE;  i++) {
        if (cpr_strncasecmp(tmp, sdp_rtcp_unicast_mode_val[i].name,
                        sdp_rtcp_unicast_mode_val[i].strlen) == 0) {
            attr_p->attr.u32_val = i;
            break;
        }
    }
    if (attr_p->attr.u32_val == SDP_RTCP_UNICAST_MODE_NOT_PRESENT) {
        sdp_parse_error(sdp_p,
            "%s Warning: Invalid rtcp unicast mode for "
            "a=rtcp-unicast line", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    return (SDP_SUCCESS);
}

sdp_result_e sdp_build_attr_rtcp_unicast (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                          flex_string *fs)
{
  if (attr_p->attr.u32_val >= SDP_RTCP_MAX_UNICAST_MODE) {
    return SDP_INVALID_PARAMETER;
  }

  flex_string_sprintf(fs, "a=%s:%s\r\n",
    sdp_get_attr_name(attr_p->type),
    sdp_get_rtcp_unicast_mode_name((sdp_rtcp_unicast_mode_e)attr_p->attr.u32_val));

  return SDP_SUCCESS;
}


/*
 * store_sdescriptions_mki_or_lifetime
 *
 * Verifies the syntax of the MKI or lifetime parameter and stores
 * it in the sdescriptions attribute struct.
 *
 * Inputs:
 *   buf    - pointer to MKI or lifetime string assumes string is null
 *            terminated.
 *   attr_p - pointer to attribute struct
 *
 * Outputs:
 *   Return TRUE all is good otherwise FALSE for error.
 */

tinybool
store_sdescriptions_mki_or_lifetime (char *buf, sdp_attr_t *attr_p)
{

    tinybool  result;
    uint16_t       mkiLen;
    char      mkiValue[SDP_SRTP_MAX_MKI_SIZE_BYTES];

    /* MKI has a colon */
    if (strstr(buf, ":")) {
        result = verify_sdescriptions_mki(buf, mkiValue, &mkiLen);
        if (result) {
            attr_p->attr.srtp_context.mki_size_bytes = mkiLen;
            sstrncpy((char*)attr_p->attr.srtp_context.mki, mkiValue,
                     SDP_SRTP_MAX_MKI_SIZE_BYTES);
        }

    } else {
        result =  verify_sdescriptions_lifetime(buf);
        if (result) {
            sstrncpy((char*)attr_p->attr.srtp_context.master_key_lifetime, buf,
                     SDP_SRTP_MAX_LIFETIME_BYTES);
        }
    }

    return result;

}

/*
 * sdp_parse_sdescriptions_key_param
 *
 * This routine parses the srtp key-params pointed to by str.
 *
 * key-params    = <key-method> ":" <key-info>
 * key-method    = "inline" / key-method-ext [note V9 only supports 'inline']
 * key-info      = srtp-key-info
 * srtp-key-info = key-salt ["|" lifetime] ["|" mki]
 * key-salt      = 1*(base64)   ; binary key and salt values
 *                              ; concatenated together, and then
 *                              ; base64 encoded [section 6.8 of
 *                              ; RFC2046]
 *
 * lifetime      = ["2^"] 1*(DIGIT)
 * mki           = mki-value ":" mki-length
 * mki-value     = 1*DIGIT
 * mki-length    = 1*3DIGIT   ; range 1..128.
 *
 * Inputs: str - pointer to beginning of key-params and assumes
 *               null terminated string.
 */


tinybool
sdp_parse_sdescriptions_key_param (const char *str, sdp_attr_t *attr_p,
                                   sdp_t *sdp_p)
{
    char            buf[SDP_MAX_STRING_LEN],
                    base64decodeData[SDP_MAX_STRING_LEN];
    const char      *ptr;
    sdp_result_e    result = SDP_SUCCESS;
    tinybool        keyFound = FALSE;
    int             len,
                    keySize,
                    saltSize;
    base64_result_t status;

    ptr = str;
    if (cpr_strncasecmp(ptr, "inline:", 7) != 0) {
        sdp_parse_error(sdp_p,
            "%s Could not find keyword inline", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return FALSE;
    }

    /* advance pass the inline key word */
    ptr = ptr + 7;
    ptr = sdp_getnextstrtok(ptr, buf, sizeof(buf), "|", &result);
    while (result == SDP_SUCCESS) {
        /* the fist time this loop executes, the key is gotten */
        if (keyFound == FALSE) {
            keyFound = TRUE;
            len = SDP_MAX_STRING_LEN;
            /* The key is base64 encoded composed of the master key concatenated with the
             * master salt.
             */
            status = base64_decode((unsigned char *)buf, strlen(buf),
                                   (unsigned char *)base64decodeData, &len);

        if (status != BASE64_SUCCESS) {
            sdp_parse_error(sdp_p,
                "%s key-salt error decoding buffer: %s",
                sdp_p->debug_str, BASE64_RESULT_TO_STRING(status));
            return FALSE;
        }

        keySize = attr_p->attr.srtp_context.master_key_size_bytes;
        saltSize = attr_p->attr.srtp_context.master_salt_size_bytes;

        if (len != keySize + saltSize) {
            sdp_parse_error(sdp_p,
                "%s key-salt size doesn't match: (%d, %d, %d)",
                sdp_p->debug_str, len, keySize, saltSize);
            return(FALSE);
        }

            memcpy(attr_p->attr.srtp_context.master_key,
                   base64decodeData,
                   keySize);

            memcpy(attr_p->attr.srtp_context.master_salt,
                   base64decodeData + keySize,
                   saltSize);

            /* Used only for MGCP */
            SDP_SRTP_CONTEXT_SET_MASTER_KEY
                     (attr_p->attr.srtp_context.selection_flags);
            SDP_SRTP_CONTEXT_SET_MASTER_SALT
                     (attr_p->attr.srtp_context.selection_flags);

       } else if (store_sdescriptions_mki_or_lifetime(buf, attr_p) == FALSE) {
           return FALSE;
       }

       /* if we haven't reached the end of line, get the next token */
       ptr = sdp_getnextstrtok(ptr, buf, sizeof(buf), "|", &result);
    }

    /* if we didn't find the key, error out */
    if (keyFound == FALSE) {
        sdp_parse_error(sdp_p,
            "%s Could not find sdescriptions key", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return FALSE;
    }

    return TRUE;

}

/*
 * sdp_build_attr_sdescriptions
 *
 * Builds a=crypto line for attribute type SDP_ATTR_SDESCRIPTIONS.
 *
 * a=crypto:tag 1*WSP crypto-suite 1*WSP key-params
 *
 * Where key-params = inline: <key|salt> ["|"lifetime] ["|" MKI:length]
 * The key and salt is base64 encoded and lifetime and MKI/length are optional.
 */

sdp_result_e
sdp_build_attr_sdescriptions (sdp_t *sdp_p, sdp_attr_t *attr_p,
                              flex_string *fs)
{

    unsigned char  base64_encoded_data[MAX_BASE64_STRING_LEN];
    unsigned char  base64_encoded_input[MAX_BASE64_STRING_LEN];
    int            keySize,
                   saltSize,
                   outputLen;
    base64_result_t status;

    keySize = attr_p->attr.srtp_context.master_key_size_bytes;
    saltSize = attr_p->attr.srtp_context.master_salt_size_bytes;

    /* concatenate the master key + salt then base64 encode it */
    memcpy(base64_encoded_input,
           attr_p->attr.srtp_context.master_key,
           keySize);

    memcpy(base64_encoded_input + keySize,
           attr_p->attr.srtp_context.master_salt,
           saltSize);

    outputLen = MAX_BASE64_STRING_LEN;
    status = base64_encode(base64_encoded_input, keySize + saltSize,
                           base64_encoded_data, &outputLen);

    if (status != BASE64_SUCCESS) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Error: Failure to Base64 Encoded data (%s) ",
                       sdp_p->debug_str, BASE64_RESULT_TO_STRING(status));
        }
        return (SDP_INVALID_PARAMETER);

    }

    base64_encoded_data[outputLen] = 0;

    /* lifetime and MKI parameters are optional. Only inlcude them if
     * they were set.
     */


    if (attr_p->attr.srtp_context.master_key_lifetime[0] != 0 &&
        attr_p->attr.srtp_context.mki[0] != 0) {
      flex_string_sprintf(fs, "a=%s:%d %s inline:%s|%s|%s:%d\r\n",
        sdp_attr[attr_p->type].name,
        attr_p->attr.srtp_context.tag,
        sdp_srtp_context_crypto_suite[attr_p->attr.srtp_context.suite].name,
        base64_encoded_data,
        attr_p->attr.srtp_context.master_key_lifetime,
        attr_p->attr.srtp_context.mki,
        attr_p->attr.srtp_context.mki_size_bytes);

      return SDP_SUCCESS;
    }

    /* if we get here, either lifetime is populated and mki and is not or mki is populated
     * and lifetime is not or neither is populated
     */

    if (attr_p->attr.srtp_context.master_key_lifetime[0] != 0) {
      flex_string_sprintf(fs, "a=%s:%d %s inline:%s|%s\r\n",
        sdp_attr[attr_p->type].name,
        attr_p->attr.srtp_context.tag,
        sdp_srtp_context_crypto_suite[attr_p->attr.srtp_context.suite].name,
        base64_encoded_data,
        attr_p->attr.srtp_context.master_key_lifetime);

    } else if (attr_p->attr.srtp_context.mki[0] != 0) {
      flex_string_sprintf(fs, "a=%s:%d %s inline:%s|%s:%d\r\n",
        sdp_attr[attr_p->type].name,
        attr_p->attr.srtp_context.tag,
        sdp_srtp_context_crypto_suite[attr_p->attr.srtp_context.suite].name,
        base64_encoded_data,
        attr_p->attr.srtp_context.mki,
        attr_p->attr.srtp_context.mki_size_bytes);

    } else {
      flex_string_sprintf(fs, "a=%s:%d %s inline:%s\r\n",
        sdp_attr[attr_p->type].name,
        attr_p->attr.srtp_context.tag,
        sdp_srtp_context_crypto_suite[attr_p->attr.srtp_context.suite].name,
        base64_encoded_data);

    }

    return SDP_SUCCESS;

}


/*
 * sdp_parse_attr_srtp
 *
 * Parses Session Description for Protocol Security Descriptions
 * version 2 or version 9. Grammar is of the form:
 *
 * a=crypto:<tag> <crypto-suite> <key-params> [<session-params>]
 *
 * Note session-params is not supported and will not be parsed.
 * Version 2 does not contain a tag.
 *
 * Inputs:
 *   sdp_p  - pointer to sdp handle
 *   attr_p - pointer to attribute structure
 *   ptr    - pointer to string to be parsed
 *   vtype  - version type
 */

sdp_result_e
sdp_parse_attr_srtp (sdp_t *sdp_p, sdp_attr_t *attr_p,
                     const char *ptr, sdp_attr_e vtype)
{

    char         tmp[SDP_MAX_STRING_LEN];
    sdp_result_e result = SDP_FAILURE;
    int          k = 0;

    /* initialize only the optional parameters */
    attr_p->attr.srtp_context.master_key_lifetime[0] = 0;
    attr_p->attr.srtp_context.mki[0] = 0;

     /* used only for MGCP */
    SDP_SRTP_CONTEXT_SET_ENCRYPT_AUTHENTICATE
             (attr_p->attr.srtp_context.selection_flags);

    /* get the tag only if we are version 9 */
    if (vtype == SDP_ATTR_SDESCRIPTIONS) {
        attr_p->attr.srtp_context.tag =
                sdp_getnextnumtok(ptr, &ptr, " \t", &result);

        if (result != SDP_SUCCESS) {
            sdp_parse_error(sdp_p,
                "%s Could not find sdescriptions tag",
                sdp_p->debug_str);
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);

        }
    }

    /* get the crypto suite */
    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Could not find sdescriptions crypto suite", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (!sdp_parse_context_crypto_suite(tmp, attr_p, sdp_p)) {
        sdp_parse_error(sdp_p,
            "%s Unsupported crypto suite", sdp_p->debug_str);
            return (SDP_INVALID_PARAMETER);
    }

    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Could not find sdescriptions key params", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (!sdp_parse_sdescriptions_key_param(tmp, attr_p, sdp_p)) {
        sdp_parse_error(sdp_p,
            "%s Failed to parse key-params", sdp_p->debug_str);
        return (SDP_INVALID_PARAMETER);
    }

    /* if there are session parameters, scan the session parameters
     * into tmp until we reach end of line. Currently the sdp parser
     * does not parse session parameters but if they are present,
     * we store them for the application.
     */
    /*sa_ignore NO_NULL_CHK
     *{ptr is valid since the pointer was checked earlier and the
     * function would have exited if NULL.}
     */
    while (*ptr && *ptr != '\n' && *ptr != '\r' && k < SDP_MAX_STRING_LEN) {
         tmp[k++] = *ptr++;
    }

    if ((k) && (k < SDP_MAX_STRING_LEN)) {
        tmp[k] = 0;
        attr_p->attr.srtp_context.session_parameters = cpr_strdup(tmp);
    }

    return SDP_SUCCESS;

}

/* Parses crypto attribute based on the sdescriptions version
 * 9 grammar.
 *
 */

sdp_result_e
sdp_parse_attr_sdescriptions (sdp_t *sdp_p, sdp_attr_t *attr_p,
                              const char *ptr)
{

   return sdp_parse_attr_srtp(sdp_p, attr_p, ptr,
                              SDP_ATTR_SDESCRIPTIONS);

}

/* Parses X-crypto attribute based on the sdescriptions version
 * 2 grammar.
 *
 */

sdp_result_e sdp_parse_attr_srtpcontext (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                         const char *ptr)
{

    return sdp_parse_attr_srtp(sdp_p, attr_p, ptr,
                               SDP_ATTR_SRTP_CONTEXT);
}


sdp_result_e sdp_build_attr_ice_attr (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                          flex_string *fs) {
  flex_string_sprintf(fs, "a=%s:%s\r\n",
                      sdp_get_attr_name(attr_p->type),
                      attr_p->attr.ice_attr);

  return SDP_SUCCESS;
}


sdp_result_e sdp_parse_attr_ice_attr (sdp_t *sdp_p, sdp_attr_t *attr_p, const char *ptr) {
    sdp_result_e  result;
    char tmp[SDP_MAX_STRING_LEN];

    ptr = sdp_getnextstrtok(ptr, tmp, sizeof(tmp), "\r\n", &result);
    if (result != SDP_SUCCESS){
        sdp_parse_error(sdp_p,
            "%s Warning: problem parsing ice attribute ", sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    snprintf(attr_p->attr.ice_attr, sizeof(attr_p->attr.ice_attr), "%s", tmp);

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
      SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str, sdp_get_attr_name(attr_p->type), tmp);
    }
    return (SDP_SUCCESS);
}


sdp_result_e sdp_build_attr_simple_flag (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                         flex_string *fs) {
    flex_string_sprintf(fs, "a=%s\r\n", sdp_get_attr_name(attr_p->type));

    return SDP_SUCCESS;
}


sdp_result_e sdp_parse_attr_simple_flag (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                      const char *ptr) {
    /* No parameters to parse. */

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type));
    }

    return (SDP_SUCCESS);
}


sdp_result_e sdp_parse_attr_complete_line (sdp_t *sdp_p, sdp_attr_t *attr_p,
                                           const char *ptr)
{
    sdp_result_e  result;

    ptr = sdp_getnextstrtok(ptr, attr_p->attr.string_val, sizeof(attr_p->attr.string_val), "\r\n", &result);

    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No string token found for %s attribute",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Parsed a=%s, %s", sdp_p->debug_str,
                      sdp_get_attr_name(attr_p->type),
                      attr_p->attr.string_val);
        }
        return (SDP_SUCCESS);
    }
}

sdp_result_e sdp_build_attr_rtcp_fb(sdp_t *sdp_p,
                                    sdp_attr_t *attr_p,
                                    flex_string *fs)
{
    flex_string_sprintf(fs, "a=%s:", sdp_attr[attr_p->type].name);

    /* Payload Type */
    if (attr_p->attr.rtcp_fb.payload_num == SDP_ALL_PAYLOADS) {
      flex_string_sprintf(fs, "* ");
    } else {
      flex_string_sprintf(fs, "%d ",attr_p->attr.rtcp_fb.payload_num);
    }

    /* Feedback Type */
    if (attr_p->attr.rtcp_fb.feedback_type < SDP_RTCP_FB_UNKNOWN) {
        flex_string_sprintf(fs, "%s",
            sdp_rtcp_fb_type_val[attr_p->attr.rtcp_fb.feedback_type].name);
    }

    /* Feedback Type Parameters */
    switch (attr_p->attr.rtcp_fb.feedback_type) {
        case SDP_RTCP_FB_ACK:
            if (attr_p->attr.rtcp_fb.param.ack < SDP_MAX_RTCP_FB_ACK) {
                flex_string_sprintf(fs, " %s",
                    sdp_rtcp_fb_ack_type_val[attr_p->attr.rtcp_fb.param.ack]
                        .name);
            }
            break;
        case SDP_RTCP_FB_CCM: /* RFC 5104 */
            if (attr_p->attr.rtcp_fb.param.ccm < SDP_MAX_RTCP_FB_CCM) {
                flex_string_sprintf(fs, " %s",
                    sdp_rtcp_fb_ccm_type_val[attr_p->attr.rtcp_fb.param.ccm]
                        .name);
            }
            break;
        case SDP_RTCP_FB_NACK:
            if (attr_p->attr.rtcp_fb.param.nack > SDP_RTCP_FB_NACK_BASIC
                && attr_p->attr.rtcp_fb.param.nack < SDP_MAX_RTCP_FB_NACK) {
                flex_string_sprintf(fs, " %s",
                    sdp_rtcp_fb_nack_type_val[attr_p->attr.rtcp_fb.param.nack]
                        .name);
            }
            break;
        case SDP_RTCP_FB_TRR_INT:
            flex_string_sprintf(fs, " %u", attr_p->attr.rtcp_fb.param.trr_int);
            break;

        case SDP_RTCP_FB_UNKNOWN:
            /* Contents are in the "extra" field */
            break;

        default:
            CSFLogError(logTag, "%s Error: Invalid rtcp-fb enum (%d)",
                        sdp_p->debug_str, attr_p->attr.rtcp_fb.feedback_type);
            return SDP_FAILURE;
    }

    /* Tack on any information that cannot otherwise be represented by
     * the sdp_fmtp_fb_t structure. */
    if (attr_p->attr.rtcp_fb.extra[0]) {
        flex_string_sprintf(fs, " %s", attr_p->attr.rtcp_fb.extra);
    }

    /* Line ending */
    flex_string_sprintf(fs, "\r\n");

    return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_rtcp_fb (sdp_t *sdp_p,
                                     sdp_attr_t *attr_p,
                                     const char *ptr)
{
    sdp_result_e     result = SDP_SUCCESS;
    sdp_fmtp_fb_t   *rtcp_fb_p = &(attr_p->attr.rtcp_fb);
    int              i;

    /* Set up attribute fields */
    rtcp_fb_p->payload_num = 0;
    rtcp_fb_p->feedback_type = SDP_RTCP_FB_UNKNOWN;
    rtcp_fb_p->extra[0] = '\0';

    /* Skip WS (just in case) */
    while (*ptr == ' ' || *ptr == '\t') {
        ptr++;
    }

    /* Look for the special "*" payload type */
    if (*ptr == '*') {
        rtcp_fb_p->payload_num = SDP_ALL_PAYLOADS;
        ptr++;
    } else {
        /* If the pt is not '*', parse it out as an integer */
        rtcp_fb_p->payload_num = (uint16_t)sdp_getnextnumtok(ptr, &ptr,
                                                        " \t", &result);
        if (result != SDP_SUCCESS) {
            sdp_parse_error(sdp_p,
              "%s Warning: could not parse payload type for rtcp-fb attribute",
              sdp_p->debug_str);
            sdp_p->conf_p->num_invalid_param++;

            return SDP_INVALID_PARAMETER;
        }
    }

    /* Read feedback type */
    i = find_token_enum("rtcp-fb attribute", sdp_p, &ptr, sdp_rtcp_fb_type_val,
                        SDP_MAX_RTCP_FB, SDP_RTCP_FB_UNKNOWN);
    if (i < 0) {
        sdp_parse_error(sdp_p,
          "%s Warning: could not parse feedback type for rtcp-fb attribute",
          sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }
    rtcp_fb_p->feedback_type = (sdp_rtcp_fb_type_e) i;

    switch(rtcp_fb_p->feedback_type) {
        case SDP_RTCP_FB_ACK:
            i = find_token_enum("rtcp-fb ack type", sdp_p, &ptr,
                                sdp_rtcp_fb_ack_type_val,
                                SDP_MAX_RTCP_FB_ACK, SDP_RTCP_FB_ACK_UNKNOWN);
            if (i < 0) {
                sdp_parse_error(sdp_p,
                  "%s Warning: could not parse ack type for rtcp-fb attribute",
                  sdp_p->debug_str);
                sdp_p->conf_p->num_invalid_param++;
                return SDP_INVALID_PARAMETER;
            }
            rtcp_fb_p->param.ack = (sdp_rtcp_fb_ack_type_e) i;
            break;

        case SDP_RTCP_FB_CCM:
            i = find_token_enum("rtcp-fb ccm type", sdp_p, &ptr,
                                sdp_rtcp_fb_ccm_type_val,
                                SDP_MAX_RTCP_FB_CCM, SDP_RTCP_FB_CCM_UNKNOWN);
            if (i < 0) {
                sdp_parse_error(sdp_p,
                  "%s Warning: could not parse ccm type for rtcp-fb attribute",
                  sdp_p->debug_str);
                sdp_p->conf_p->num_invalid_param++;
                return SDP_INVALID_PARAMETER;
            }
            rtcp_fb_p->param.ccm = (sdp_rtcp_fb_ccm_type_e) i;

            /* TODO -- We don't currently parse tmmbr parameters or vbcm
               submessage types. If we decide to support these modes of
               operation, we probably want to add parsing code for them.
               For the time being, they'll just end up parsed into "extra"
               Bug 1097169.
            */
            break;

        case SDP_RTCP_FB_NACK:
            /* Skip any remaining WS -- see
               http://code.google.com/p/webrtc/issues/detail?id=1922 */
            while (*ptr == ' ' || *ptr == '\t') {
                ptr++;
            }
            /* Check for empty string */
            if (*ptr == '\r') {
                rtcp_fb_p->param.nack = SDP_RTCP_FB_NACK_BASIC;
                break;
            }
            i = find_token_enum("rtcp-fb nack type", sdp_p, &ptr,
                                sdp_rtcp_fb_nack_type_val,
                                SDP_MAX_RTCP_FB_NACK, SDP_RTCP_FB_NACK_UNKNOWN);
            if (i < 0) {
                sdp_parse_error(sdp_p,
                  "%s Warning: could not parse nack type for rtcp-fb attribute",
                  sdp_p->debug_str);
                sdp_p->conf_p->num_invalid_param++;
                return SDP_INVALID_PARAMETER;
            }
            rtcp_fb_p->param.nack = (sdp_rtcp_fb_nack_type_e) i;
            break;

        case SDP_RTCP_FB_TRR_INT:
            rtcp_fb_p->param.trr_int = sdp_getnextnumtok(ptr, &ptr,
                                                         " \t", &result);
            if (result != SDP_SUCCESS) {
                sdp_parse_error(sdp_p,
                  "%s Warning: could not parse trr-int value for rtcp-fb "
                  "attribute", sdp_p->debug_str);
                sdp_p->conf_p->num_invalid_param++;
                return SDP_INVALID_PARAMETER;
            }
            break;

        case SDP_RTCP_FB_UNKNOWN:
            /* Handled by "extra", below */
            break;

        default:
            /* This is an internal error, not a parsing error */
            CSFLogError(logTag, "%s Error: Invalid rtcp-fb enum (%d)",
                        sdp_p->debug_str, attr_p->attr.rtcp_fb.feedback_type);
            return SDP_FAILURE;
    }

    /* Skip any remaining WS  */
    while (*ptr == ' ' || *ptr == '\t') {
        ptr++;
    }

    /* Just store the rest of the line in "extra" -- this will return
       a failure result if there is no more text, but that's fine. */
    ptr = sdp_getnextstrtok(ptr, rtcp_fb_p->extra,
                            sizeof(rtcp_fb_p->extra), "\r\n", &result);

    return SDP_SUCCESS;
}

sdp_result_e sdp_build_attr_setup(sdp_t *sdp_p,
                                  sdp_attr_t *attr_p,
                                  flex_string *fs)
{
    switch (attr_p->attr.setup) {
    case SDP_SETUP_ACTIVE:
    case SDP_SETUP_PASSIVE:
    case SDP_SETUP_ACTPASS:
    case SDP_SETUP_HOLDCONN:
        flex_string_sprintf(fs, "a=%s:%s\r\n",
            sdp_attr[attr_p->type].name,
            sdp_setup_type_val[attr_p->attr.setup].name);
        break;
    default:
        CSFLogError(logTag, "%s Error: Invalid setup enum (%d)",
                    sdp_p->debug_str, attr_p->attr.setup);
        return SDP_FAILURE;
    }

    return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_setup(sdp_t *sdp_p,
                                   sdp_attr_t *attr_p,
                                   const char *ptr)
{
    int i = find_token_enum("setup attribute", sdp_p, &ptr,
        sdp_setup_type_val,
        SDP_MAX_SETUP, SDP_SETUP_UNKNOWN);

    if (i < 0) {
        sdp_parse_error(sdp_p,
          "%s Warning: could not parse setup attribute",
          sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }

    attr_p->attr.setup = (sdp_setup_type_e) i;

    switch (attr_p->attr.setup) {
    case SDP_SETUP_ACTIVE:
    case SDP_SETUP_PASSIVE:
    case SDP_SETUP_ACTPASS:
    case SDP_SETUP_HOLDCONN:
        /* All these values are OK */
        break;
    case SDP_SETUP_UNKNOWN:
        sdp_parse_error(sdp_p,
            "%s Warning: Unknown setup attribute",
            sdp_p->debug_str);
        return SDP_INVALID_PARAMETER;
        break;
    default:
        /* This is an internal error, not a parsing error */
        CSFLogError(logTag, "%s Error: Invalid setup enum (%d)",
                    sdp_p->debug_str, attr_p->attr.setup);
        return SDP_FAILURE;
        break;
    }

    return SDP_SUCCESS;
}

sdp_result_e sdp_build_attr_connection(sdp_t *sdp_p,
                                       sdp_attr_t *attr_p,
                                       flex_string *fs)
{
    switch (attr_p->attr.connection) {
    case SDP_CONNECTION_NEW:
    case SDP_CONNECTION_EXISTING:
        flex_string_sprintf(fs, "a=%s:%s\r\n",
            sdp_attr[attr_p->type].name,
            sdp_connection_type_val[attr_p->attr.connection].name);
        break;
    default:
        CSFLogError(logTag, "%s Error: Invalid connection enum (%d)",
                    sdp_p->debug_str, attr_p->attr.connection);
        return SDP_FAILURE;
    }

    return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_connection(sdp_t *sdp_p,
                                       sdp_attr_t *attr_p,
                                       const char *ptr)
{
    int i = find_token_enum("connection attribute", sdp_p, &ptr,
        sdp_connection_type_val,
        SDP_MAX_CONNECTION, SDP_CONNECTION_UNKNOWN);

    if (i < 0) {
        sdp_parse_error(sdp_p,
          "%s Warning: could not parse connection attribute",
          sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }

    attr_p->attr.connection = (sdp_connection_type_e) i;

    switch (attr_p->attr.connection) {
    case SDP_CONNECTION_NEW:
    case SDP_CONNECTION_EXISTING:
        /* All these values are OK */
        break;
    case SDP_CONNECTION_UNKNOWN:
        sdp_parse_error(sdp_p,
            "%s Warning: Unknown connection attribute",
            sdp_p->debug_str);
        return SDP_INVALID_PARAMETER;
        break;
    default:
        /* This is an internal error, not a parsing error */
        CSFLogError(logTag, "%s Error: Invalid connection enum (%d)",
                    sdp_p->debug_str, attr_p->attr.connection);
        return SDP_FAILURE;
        break;
    }
    return SDP_SUCCESS;
}

sdp_result_e sdp_build_attr_extmap(sdp_t *sdp_p,
                                       sdp_attr_t *attr_p,
                                       flex_string *fs)
{
    flex_string_sprintf(fs, "a=extmap:%d %s\r\n",
        attr_p->attr.extmap.id,
        attr_p->attr.extmap.uri);

    return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_extmap(sdp_t *sdp_p,
                                   sdp_attr_t *attr_p,
                                   const char *ptr)
{
    sdp_result_e  result;

    attr_p->attr.extmap.id = 0;
    attr_p->attr.extmap.media_direction = SDP_DIRECTION_SENDRECV;
    attr_p->attr.extmap.media_direction_specified = FALSE;
    attr_p->attr.extmap.uri[0] = '\0';
    attr_p->attr.extmap.extension_attributes[0] = '\0';

    /* Find the payload type number. */
    attr_p->attr.extmap.id =
    (uint16_t)sdp_getnextnumtok(ptr, &ptr, "/ \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: Invalid extmap id specified for %s attribute.",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    if (*ptr == '/') {
        char direction[SDP_MAX_STRING_LEN+1];
        ++ptr; /* Skip over '/' */
        ptr = sdp_getnextstrtok(ptr, direction,
                                sizeof(direction), " \t", &result);
        if (result != SDP_SUCCESS) {
            sdp_parse_error(sdp_p,
                "%s Warning: Invalid direction specified in %s attribute.",
                sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }

        if (!cpr_strcasecmp(direction, "sendrecv")) {
          attr_p->attr.extmap.media_direction = SDP_DIRECTION_SENDRECV;
        } else if (!cpr_strcasecmp(direction, "sendonly")) {
          attr_p->attr.extmap.media_direction = SDP_DIRECTION_SENDONLY;
        } else if (!cpr_strcasecmp(direction, "recvonly")) {
          attr_p->attr.extmap.media_direction = SDP_DIRECTION_RECVONLY;
        } else if (!cpr_strcasecmp(direction, "inactive")) {
          attr_p->attr.extmap.media_direction = SDP_DIRECTION_INACTIVE;
        } else {
            sdp_parse_error(sdp_p,
                "%s Warning: Invalid direction specified in %s attribute.",
                sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        attr_p->attr.extmap.media_direction_specified = TRUE;
    }

    ptr = sdp_getnextstrtok(ptr, attr_p->attr.extmap.uri,
                            sizeof(attr_p->attr.extmap.uri), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p,
            "%s Warning: No uri specified in %s attribute.",
            sdp_p->debug_str, sdp_get_attr_name(attr_p->type));
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    while (*ptr == ' ' || *ptr == '\t') {
      ++ptr;
    }

    /* Grab everything that follows, even if it contains whitespace */
    ptr = sdp_getnextstrtok(ptr, attr_p->attr.extmap.extension_attributes,
                            sizeof(attr_p->attr.extmap.extension_attributes), "\r\n", &result);

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=%s, id %u, direction %s, "
                  "uri %s, extension %s", sdp_p->debug_str,
                  sdp_get_attr_name(attr_p->type),
                  attr_p->attr.extmap.id,
                  SDP_DIRECTION_PRINT(attr_p->attr.extmap.media_direction),
                  attr_p->attr.extmap.uri,
                  attr_p->attr.extmap.extension_attributes);
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_parse_attr_msid(sdp_t *sdp_p,
                                 sdp_attr_t *attr_p,
                                 const char *ptr)
{
    sdp_result_e result;

    ptr = sdp_getnextstrtok(ptr, attr_p->attr.msid.identifier,
                            sizeof(attr_p->attr.msid.identifier), " \t", &result);
    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p, "%s Warning: Bad msid identity value",
                        sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }

    ptr = sdp_getnextstrtok(ptr, attr_p->attr.msid.appdata,
                            sizeof(attr_p->attr.msid.appdata), " \t", &result);
    if ((result != SDP_SUCCESS) && (result != SDP_EMPTY_TOKEN)) {
        sdp_parse_error(sdp_p, "%s Warning: Bad msid appdata value",
                        sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }
    if (result == SDP_EMPTY_TOKEN) {
        attr_p->attr.msid.appdata[0] = '\0';
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=msid, %s %s", sdp_p->debug_str,
                  attr_p->attr.msid.identifier, attr_p->attr.msid.appdata);
    }

    return SDP_SUCCESS;
}


sdp_result_e sdp_build_attr_msid(sdp_t *sdp_p,
                                 sdp_attr_t *attr_p,
                                 flex_string *fs)
{
    flex_string_sprintf(fs, "a=msid:%s%s%s\r\n",
                        attr_p->attr.msid.identifier,
                        attr_p->attr.msid.appdata[0] ? " " : "",
                        attr_p->attr.msid.appdata);
    return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_msid_semantic(sdp_t *sdp_p,
                                          sdp_attr_t *attr_p,
                                          const char *ptr)
{
    sdp_result_e result;
    int i;

    ptr = sdp_getnextstrtok(ptr,
                            attr_p->attr.msid_semantic.semantic,
                            sizeof(attr_p->attr.msid_semantic.semantic),
                            " \t",
                            &result);

    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p, "%s Warning: Bad msid-semantic attribute; "
                        "missing semantic",
                        sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }

    for (i = 0; i < SDP_MAX_MEDIA_STREAMS; ++i) {
      /* msid-id can be up to 64 characters long, plus null terminator */
      char temp[65];
      ptr = sdp_getnextstrtok(ptr, temp, sizeof(temp), " \t", &result);

      if (result != SDP_SUCCESS) {
        break;
      }

      attr_p->attr.msid_semantic.msids[i] = cpr_strdup(temp);
    }

    if ((result != SDP_SUCCESS) && (result != SDP_EMPTY_TOKEN)) {
        sdp_parse_error(sdp_p, "%s Warning: Bad msid-semantic attribute",
                        sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }

    if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
        SDP_PRINT("%s Parsed a=msid-semantic, %s", sdp_p->debug_str,
                  attr_p->attr.msid_semantic.semantic);
        for (i = 0; i < SDP_MAX_MEDIA_STREAMS; ++i) {
          if (!attr_p->attr.msid_semantic.msids[i]) {
            break;
          }

          SDP_PRINT("%s ... msid %s", sdp_p->debug_str,
                    attr_p->attr.msid_semantic.msids[i]);
        }
    }

    return SDP_SUCCESS;
}


sdp_result_e sdp_build_attr_msid_semantic(sdp_t *sdp_p,
                                          sdp_attr_t *attr_p,
                                          flex_string *fs)
{
    int i;
    flex_string_sprintf(fs, "a=msid-semantic:%s",
                        attr_p->attr.msid_semantic.semantic);
    for (i = 0; i < SDP_MAX_MEDIA_STREAMS; ++i) {
        if (!attr_p->attr.msid_semantic.msids[i]) {
            break;
        }

        flex_string_sprintf(fs, " %s",
                            attr_p->attr.msid_semantic.msids[i]);
    }
    flex_string_sprintf(fs, "\r\n");
    return SDP_SUCCESS;
}

sdp_result_e sdp_parse_attr_ssrc(sdp_t *sdp_p,
                                 sdp_attr_t *attr_p,
                                 const char *ptr)
{
    sdp_result_e result;

    attr_p->attr.ssrc.ssrc =
        (uint32_t)sdp_getnextnumtok(ptr, &ptr, " \t", &result);

    if (result != SDP_SUCCESS) {
        sdp_parse_error(sdp_p, "%s Warning: Bad ssrc attribute, cannot parse ssrc",
                        sdp_p->debug_str);
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }

    /* Skip any remaining WS  */
    while (*ptr == ' ' || *ptr == '\t') {
        ptr++;
    }

    /* Just store the rest of the line in "attribute" -- this will return
       a failure result if there is no more text, but that's fine. */
    ptr = sdp_getnextstrtok(ptr,
                            attr_p->attr.ssrc.attribute,
                            sizeof(attr_p->attr.ssrc.attribute),
                            "\r\n",
                            &result);

    return SDP_SUCCESS;
}


sdp_result_e sdp_build_attr_ssrc(sdp_t *sdp_p,
                                 sdp_attr_t *attr_p,
                                 flex_string *fs)
{
    flex_string_sprintf(fs, "a=ssrc:%s%s%s\r\n",
                        attr_p->attr.ssrc.ssrc,
                        attr_p->attr.ssrc.attribute[0] ? " " : "",
                        attr_p->attr.ssrc.attribute);
    return SDP_SUCCESS;
}

