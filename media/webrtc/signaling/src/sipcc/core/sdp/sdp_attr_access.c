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

#include "sdp_os_defs.h"
#include "sdp.h"
#include "sdp_private.h"

/* Attribute access routines are all defined by the following parameters.
 *
 * sdp_ptr     The SDP handle returned by sdp_init_description.
 * level       The level the attribute is defined.  Can be either 
 *             SDP_SESSION_LEVEL or 0-n specifying a media line level.
 * inst_num    The instance number of the attribute.  Multiple instances
 *             of a particular attribute may exist at each level and so
 *             the inst_num determines the particular attribute at that
 *             level that should be accessed.  Note that this is the
 *             instance number of the specified type of attribute, not the
 *             overall attribute number at the level.  Also note that the
 *             instance number is 1-based.  For example:
 *             v=0
 *             o=mhandley 2890844526 2890842807 IN IP4 126.16.64.4
 *             s=SDP Seminar
 *             c=IN IP4 10.1.0.2
 *             t=0 0
 *             m=audio 1234 RTP/AVP 0 101 102
 *             a=foo 1
 *             a=foo 2
 *             a=bar 1   # This is instance 1 of attribute bar.
 *             a=foo 3   # This is instance 3 of attribute foo.
 * cap_num     Almost all of the attributes may be defined as X-cpar
 *             parameters (with the exception of X-sqn, X-cap, and X-cpar).
 *             If the cap_num is set to zero, then the attribute is not
 *             an X-cpar parameter attribute.  If the cap_num is any other
 *             value, it specifies the capability number that the X-cpar
 *             attribute is specified for.
 */

/* Attribute handling:
 * 
 * There are two basic types of attributes handled by the SDP library,
 * those defined by a= token lines, and those embedded with a=X-cpar lines.
 * The handling for each of these is described here.
 * 
 * Simple (non X-cpar attributes):
 *
 * Attributes not embedded in a=X-cpar lines are referenced by level and 
 * instance number.  For these attributes the capability number is always
 * set to zero.
 *
 * An application will typically process these attributes in one of two ways.
 * With the first method, the application can determine the total number 
 * of attributes defined at a given level and process them one at a time.  
 * For each attribute, the application will query the library to find out
 * what type of attribute it is and which instance within that type. The
 * application can then process this particular attribute referencing it
 * by level and instance number.
 *
 * A second method of processing attributes is for applications to determine
 * each type of attribute they are interested in, query the SDP library to
 * find out how many of that type of attribute exist at a given level, and
 * process each one at a time.
 *
 * X-cpar attribute processing:
 *
 * X-cpar attributes can contain embedded attributes.  They are associated
 * with X-cap attribute lines.  An example of X-cap and X-cpar attributes 
 * found in an SDP is as follows:
 *
 * v=0
 * o=- 25678 753849 IN IP4 128.96.41.1
 * s=-
 * t=0 0
 * c=IN IP4 10.1.0.2
 * m=audio 3456 RTP/AVP 18 96
 * a=rtpmap:96 telephone-event/8000
 * a=fmtp:96 0-15,32-35
 * a=X-sqn: 0
 * a=X-cap: 1 audio RTP/AVP 0 18 96 97
 * a=X-cpar: a=fmtp:96 0-16,32-35
 * a=X-cpar: a=rtpmap:97 X-NSE/8000
 * a=X-cpar: a=fmtp:97 195-197
 * a=X-cap: 5 image udptl t38
 * a=X-cap: 6 application udp X-tmr
 * a=X-cap: 7 audio RTP/AVP 100 101
 * a=X-cpar: a=rtpmap:100 g.711/8000
 * a=X-cpar: a=rtpmap:101 g.729/8000
 * 
 * X-cap attributes can be defined at the SESSION_LEVEL or any media level.
 * An X-cap attr is defined by the level and instance number just like 
 * other attributes.  In the example above, X-cap attrs are defined at 
 * media level 1 and there are four instances at that level.
 * 
 * The X-cpar attributes can also be referenced by level and instance number.
 * However, the embedded attribute within an X-cpar attribute must be 
 * referenced by level, instance number, and capability number.  This is
 * because the X-cpar attribute is associated with a particular X-cap/
 * capability.  
 * For all attributes that are not embedded within an X-cpar attribute, the
 * cap_num should be referenced as zero.  But for X-cpar attributes, the
 * cap_num is specified to be one of the capability numbers of the previous
 * X-cap line.  The number of capabilities specified in an X-cap line is
 * equal to the number of payloads.  Thus, in this example, the first X-cap 
 * attr instance specifies capabilities 1-4, the second specifies capability
 * 5, the third capability 6, and the fourth capabilities 7-8.
 *
 * X-cpar attributes can be processed with methods similar to the two 
 * previously mentioned.  For each X-cap attribute, the application can
 * use one of two methods to process the X-cpar attributes.  First, it
 * can query the total number of X-cpar attributes associated with a 
 * given X-cap attribute.  The X-cap attribute is here defined by a level
 * and a capability number.  In the example above, the total number of 
 * attributes defined is as follows:
 *  level 1, cap_num 1 - total attrs: 3
 *  level 1, cap_num 5 - total attrs: 0
 *  level 1, cap_num 6 - total attrs: 0
 *  level 1, cap_num 7 - total attrs: 2
 * 
 * Note that if the application queried the number of attributes for 
 * cap_num 2, 3, or 4, it would also return 3 attrs, and for cap_num
 * 8 the library would return 2.
 *
 * Once the application determines the total number of attributes for
 * that capability, it can again query the embedded attribute type and
 * instance.  For example, sdp_get_attr_type would return the following:
 *  level 1, cap_num 1, attr 1 -> attr type fmtp, instance 1
 *  level 1, cap_num 1, attr 2 -> attr type rtpmap, instance 1 
 *  level 1, cap_num 1, attr 3 -> attr type fmtp, instance 2
 *  level 1, cap_num 7, attr 1 -> attr type rtpmap, instance 1
 *  level 1, cap_num 7, attr 2 -> attr type rtpmap, instance 2
 * 
 * The individual embedded attributes can then be accessed by level,
 * cap_num, and instance number.
 *
 * With the second method for handling X-cpar attributes, the application
 * determines the types of attributes it is interested in.  It can then
 * query the SDP library to determine the number of attributes of that 
 * type found for that level and cap_num, and then process each one at 
 * a time.  e.g., calling sdp_attr_num_instances would give:
 *  level 1, cap_num 1, attr_type fmtp -> two instances
 *  level 1, cap_num 1, attr_type rtpmap -> one instance
 *  level 1, cap_num 7, attr_type fmtp -> zero instances
 *  level 1, cap_num 7, attr_type rtpmap -> two instances
 */


/* Function:    sdp_add_new_attr
 * Description: Add a new attribute of the specified type at the given
 *              level and capability level or base attribute if cap_num
 *              is zero.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              attr_type   The type of attribute to add.
 *              inst_num    Pointer to a u16 in which to return the instance
 *                          number of the newly added attribute.
 * Returns:     SDP_SUCCESS            Attribute was added successfully.
 *              SDP_NO_RESOURCE        No memory avail for new attribute.
 *              SDP_INVALID_PARAMETER  Specified media line is not defined.
 */
sdp_result_e sdp_add_new_attr (void *sdp_ptr, u16 level, u8 cap_num,
                               sdp_attr_e attr_type, u16 *inst_num)
{
    u16          i;
    sdp_mca_t   *mca_p;
    sdp_mca_t   *cap_p;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_attr_t  *new_attr_p;
    sdp_attr_t  *prev_attr_p=NULL;
    sdp_fmtp_t  *fmtp_p;
    sdp_comediadir_t  *comediadir_p;

    *inst_num = 0;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if ((cap_num != 0) && 
        ((attr_type == SDP_ATTR_X_CAP) || (attr_type == SDP_ATTR_X_CPAR) ||
         (attr_type == SDP_ATTR_X_SQN) || (attr_type == SDP_ATTR_CDSC) || 
	 (attr_type == SDP_ATTR_CPAR) || (attr_type == SDP_ATTR_SQN))) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid attribute type for X-cpar/cdsc "
                     "parameter.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Some attributes are valid only under media level */ 
    if (level == SDP_SESSION_LEVEL) {
        switch (attr_type) {
            case SDP_ATTR_RTCP:
            case SDP_ATTR_LABEL:
                return (SDP_INVALID_MEDIA_LEVEL);

            default:
                break;
        }
    }

    new_attr_p = (sdp_attr_t *)SDP_MALLOC(sizeof(sdp_attr_t));
    if (new_attr_p == NULL) {
        sdp_p->conf_p->num_no_resource++;
        return (SDP_NO_RESOURCE);
    }

    new_attr_p->type = attr_type;
    new_attr_p->next_p = NULL;

    /* Initialize the new attribute structure */
    if ((new_attr_p->type == SDP_ATTR_X_CAP) || 
	(new_attr_p->type == SDP_ATTR_CDSC)) {
        new_attr_p->attr.cap_p = (sdp_mca_t *)SDP_MALLOC(sizeof(sdp_mca_t));
        if (new_attr_p->attr.cap_p == NULL) {
            sdp_free_attr(new_attr_p);
            sdp_p->conf_p->num_no_resource++;
            return (SDP_NO_RESOURCE);
        }
    } else if (new_attr_p->type == SDP_ATTR_FMTP) {
        fmtp_p = &(new_attr_p->attr.fmtp);
        fmtp_p->fmtp_format = SDP_FMTP_UNKNOWN_TYPE;
        // set to invalid value
        fmtp_p->packetization_mode = 0xff;
        fmtp_p->level_asymmetry_allowed = SDP_INVALID_LEVEL_ASYMMETRY_ALLOWED_VALUE;
	fmtp_p->annexb_required = FALSE;
	fmtp_p->annexa_required = FALSE;
        fmtp_p->maxval = 0;
	fmtp_p->bitrate = 0;
	fmtp_p->cif = 0;
	fmtp_p->qcif = 0;
        fmtp_p->profile = SDP_INVALID_VALUE; 
        fmtp_p->level = SDP_INVALID_VALUE;
        fmtp_p->parameter_add = TRUE;
	for (i=0; i < SDP_NE_NUM_BMAP_WORDS; i++) {
            fmtp_p->bmap[i] = 0;
        }
    } else if ((new_attr_p->type == SDP_ATTR_RTPMAP) ||
	       (new_attr_p->type == SDP_ATTR_SPRTMAP)) {
        new_attr_p->attr.transport_map.num_chan = 1;
    } else if (new_attr_p->type == SDP_ATTR_DIRECTION) {
        comediadir_p = &(new_attr_p->attr.comediadir);
	comediadir_p->role = SDP_MEDIADIR_ROLE_PASSIVE;
        comediadir_p->conn_info_present = FALSE; 
    } else if (new_attr_p->type == SDP_ATTR_MPTIME) {
        sdp_mptime_t *mptime = &(new_attr_p->attr.mptime);
        mptime->num_intervals = 0;
    }

    if (cap_num == 0) {
        /* Add a new attribute. */
        if (level == SDP_SESSION_LEVEL) {
            if (sdp_p->sess_attrs_p == NULL) {
                sdp_p->sess_attrs_p = new_attr_p;
            } else {
                for (attr_p = sdp_p->sess_attrs_p;
                     attr_p != NULL;
                     prev_attr_p = attr_p, attr_p = attr_p->next_p) {
                    /* Count the num instances of this type. */
                    if (attr_p->type == attr_type) {
                        (*inst_num)++;
                    }
                }
                prev_attr_p->next_p = new_attr_p;
            }
        } else {
            mca_p = sdp_find_media_level(sdp_p, level);
            if (mca_p == NULL) {
                sdp_free_attr(new_attr_p);
                sdp_p->conf_p->num_invalid_param++;
                return (SDP_INVALID_PARAMETER);
            }
            if (mca_p->media_attrs_p == NULL) {
                mca_p->media_attrs_p = new_attr_p;
            } else {
                for (attr_p = mca_p->media_attrs_p;
                     attr_p != NULL;
                     prev_attr_p = attr_p, attr_p = attr_p->next_p) {
                    /* Count the num instances of this type. */
                    if (attr_p->type == attr_type) {
                        (*inst_num)++;
                    }
                }
                prev_attr_p->next_p = new_attr_p;
            }
        }
    } else {
        /* Add a new capability attribute - find the capability attr. */
        attr_p = sdp_find_capability(sdp_p, level, cap_num);
        if (attr_p == NULL) {
            sdp_free_attr(new_attr_p);
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        cap_p = attr_p->attr.cap_p;
        if (cap_p->media_attrs_p == NULL) {
            cap_p->media_attrs_p = new_attr_p;
        } else {
            for (attr_p = cap_p->media_attrs_p;
                 attr_p != NULL;
                 prev_attr_p = attr_p, attr_p = attr_p->next_p) {
                /* Count the num instances of this type. */
                if (attr_p->type == attr_type) {
                    (*inst_num)++;
                }
            }
            prev_attr_p->next_p = new_attr_p;
        }
    }

    /* Increment the instance num for the attr just added. */
    (*inst_num)++;
    return (SDP_SUCCESS);
}

/* Function:    sdp_copy_attr_fields
 * Description: Copy the fields of an attribute based on attr type.
 *              This is an INTERNAL SDP routine only.  It will not copy
 *              X-Cap, X-Cpar, CDSC, or CPAR attrs.
 * Parameters:  src_attr_p  Ptr to the source attribute.
 *              dst_attr_p  Ptr to the dst attribute.
 * Returns:     Nothing.
 */
void sdp_copy_attr_fields (sdp_attr_t *src_attr_p, sdp_attr_t *dst_attr_p)
{
    u16 i;

    /* Copy over all the attribute information. */
    dst_attr_p->type = src_attr_p->type;
    dst_attr_p->next_p = NULL;

    switch (src_attr_p->type) {

    case SDP_ATTR_BEARER:
    case SDP_ATTR_CALLED:
    case SDP_ATTR_CONN_TYPE:
    case SDP_ATTR_DIALED:
    case SDP_ATTR_DIALING:
    case SDP_ATTR_FRAMING:
    case SDP_ATTR_MAXPRATE:
    case SDP_ATTR_LABEL:
        sstrncpy(dst_attr_p->attr.string_val, src_attr_p->attr.string_val,
		 SDP_MAX_STRING_LEN+1);
        break;

    case SDP_ATTR_EECID:
    case SDP_ATTR_PTIME:
    case SDP_ATTR_T38_VERSION:
    case SDP_ATTR_T38_MAXBITRATE:
    case SDP_ATTR_T38_MAXBUFFER:
    case SDP_ATTR_T38_MAXDGRAM:
    case SDP_ATTR_X_SQN:
    case SDP_ATTR_TC1_PAYLOAD_BYTES:
    case SDP_ATTR_TC1_WINDOW_SIZE:
    case SDP_ATTR_TC2_PAYLOAD_BYTES:
    case SDP_ATTR_TC2_WINDOW_SIZE:
    case SDP_ATTR_RTCP:
    case SDP_ATTR_MID:
    case SDP_ATTR_RTCP_UNICAST:
        dst_attr_p->attr.u32_val = src_attr_p->attr.u32_val;
        break;

    case SDP_ATTR_T38_FILLBITREMOVAL:
    case SDP_ATTR_T38_TRANSCODINGMMR:
    case SDP_ATTR_T38_TRANSCODINGJBIG:
    case SDP_ATTR_TMRGWXID:
        dst_attr_p->attr.boolean_val = src_attr_p->attr.boolean_val;
        break;

    case SDP_ATTR_QOS:
    case SDP_ATTR_SECURE:
    case SDP_ATTR_X_PC_QOS:
    case SDP_ATTR_X_QOS:
        dst_attr_p->attr.qos.strength  = src_attr_p->attr.qos.strength;
        dst_attr_p->attr.qos.direction = src_attr_p->attr.qos.direction;
        dst_attr_p->attr.qos.confirm   = src_attr_p->attr.qos.confirm;
        break;
  
    case SDP_ATTR_CURR:
        dst_attr_p->attr.curr.type         = src_attr_p->attr.curr.type;
        dst_attr_p->attr.curr.direction    = src_attr_p->attr.curr.direction;
        dst_attr_p->attr.curr.status_type  = src_attr_p->attr.curr.status_type;
        break;
    case SDP_ATTR_DES:
        dst_attr_p->attr.des.type         = src_attr_p->attr.des.type;
        dst_attr_p->attr.des.direction    = src_attr_p->attr.des.direction;
        dst_attr_p->attr.des.status_type  = src_attr_p->attr.des.status_type;
        dst_attr_p->attr.des.strength     = src_attr_p->attr.des.strength;
        break;
         
    
    case SDP_ATTR_CONF:
        dst_attr_p->attr.conf.type         = src_attr_p->attr.conf.type;
        dst_attr_p->attr.conf.direction    = src_attr_p->attr.conf.direction;
        dst_attr_p->attr.conf.status_type  = src_attr_p->attr.conf.status_type;
        break;

    case SDP_ATTR_INACTIVE:
    case SDP_ATTR_RECVONLY:
    case SDP_ATTR_SENDONLY:
    case SDP_ATTR_SENDRECV:
        /* These attrs have no parameters. */
        break;

    case SDP_ATTR_FMTP:
        dst_attr_p->attr.fmtp.payload_num = src_attr_p->attr.fmtp.payload_num;
        dst_attr_p->attr.fmtp.maxval      = src_attr_p->attr.fmtp.maxval;
        dst_attr_p->attr.fmtp.bitrate     = src_attr_p->attr.fmtp.bitrate;
        dst_attr_p->attr.fmtp.annexa      = src_attr_p->attr.fmtp.annexa;
        dst_attr_p->attr.fmtp.annexb      = src_attr_p->attr.fmtp.annexb;
        dst_attr_p->attr.fmtp.qcif      = src_attr_p->attr.fmtp.qcif;
        dst_attr_p->attr.fmtp.cif      = src_attr_p->attr.fmtp.cif;
        dst_attr_p->attr.fmtp.sqcif      = src_attr_p->attr.fmtp.sqcif;
        dst_attr_p->attr.fmtp.cif4      = src_attr_p->attr.fmtp.cif4;
        dst_attr_p->attr.fmtp.cif16      = src_attr_p->attr.fmtp.cif16;
        dst_attr_p->attr.fmtp.maxbr      = src_attr_p->attr.fmtp.maxbr;
        dst_attr_p->attr.fmtp.custom_x   = src_attr_p->attr.fmtp.custom_x;
        dst_attr_p->attr.fmtp.custom_y      = src_attr_p->attr.fmtp.custom_y;
        dst_attr_p->attr.fmtp.custom_mpi      = src_attr_p->attr.fmtp.custom_mpi;
        dst_attr_p->attr.fmtp.par_width      = src_attr_p->attr.fmtp.par_width;
        dst_attr_p->attr.fmtp.par_height    = src_attr_p->attr.fmtp.par_height;
        dst_attr_p->attr.fmtp.cpcf      = src_attr_p->attr.fmtp.cpcf;
        dst_attr_p->attr.fmtp.bpp      = src_attr_p->attr.fmtp.bpp;
        dst_attr_p->attr.fmtp.hrd      = src_attr_p->attr.fmtp.hrd;
        
        dst_attr_p->attr.fmtp.profile      = src_attr_p->attr.fmtp.profile;
        dst_attr_p->attr.fmtp.level      = src_attr_p->attr.fmtp.level;
        dst_attr_p->attr.fmtp.is_interlace = src_attr_p->attr.fmtp.is_interlace;
        
        sstrncpy(dst_attr_p->attr.fmtp.profile_level_id, 
                 src_attr_p->attr.fmtp.profile_level_id,
                 SDP_MAX_STRING_LEN+1);
        sstrncpy(dst_attr_p->attr.fmtp.parameter_sets, 
                 src_attr_p->attr.fmtp.parameter_sets,
                 SDP_MAX_STRING_LEN+1);
        dst_attr_p->attr.fmtp.deint_buf_req =
		 src_attr_p->attr.fmtp.deint_buf_req;
        dst_attr_p->attr.fmtp.max_don_diff = 
            src_attr_p->attr.fmtp.max_don_diff;
        dst_attr_p->attr.fmtp.init_buf_time =
		 src_attr_p->attr.fmtp.init_buf_time;
        dst_attr_p->attr.fmtp.packetization_mode =
		 src_attr_p->attr.fmtp.packetization_mode;
        dst_attr_p->attr.fmtp.flag = 
                 src_attr_p->attr.fmtp.flag;
     
        dst_attr_p->attr.fmtp.max_mbps = src_attr_p->attr.fmtp.max_mbps;
        dst_attr_p->attr.fmtp.max_fs = src_attr_p->attr.fmtp.max_fs;        
        dst_attr_p->attr.fmtp.max_cpb = src_attr_p->attr.fmtp.max_cpb;
        dst_attr_p->attr.fmtp.max_dpb = src_attr_p->attr.fmtp.max_dpb;
        dst_attr_p->attr.fmtp.max_br = src_attr_p->attr.fmtp.max_br;
        dst_attr_p->attr.fmtp.redundant_pic_cap = 
            src_attr_p->attr.fmtp.redundant_pic_cap;        
        dst_attr_p->attr.fmtp.deint_buf_cap =
		 src_attr_p->attr.fmtp.deint_buf_cap;
        dst_attr_p->attr.fmtp.max_rcmd_nalu_size = 
                 src_attr_p->attr.fmtp.max_rcmd_nalu_size; 
        dst_attr_p->attr.fmtp.interleaving_depth =
		 src_attr_p->attr.fmtp.interleaving_depth;
        dst_attr_p->attr.fmtp.parameter_add = 
            src_attr_p->attr.fmtp.parameter_add;
        
        dst_attr_p->attr.fmtp.annex_d = src_attr_p->attr.fmtp.annex_d;
        dst_attr_p->attr.fmtp.annex_f = src_attr_p->attr.fmtp.annex_f;
        dst_attr_p->attr.fmtp.annex_i = src_attr_p->attr.fmtp.annex_i;
        dst_attr_p->attr.fmtp.annex_j = src_attr_p->attr.fmtp.annex_j;
        dst_attr_p->attr.fmtp.annex_t = src_attr_p->attr.fmtp.annex_t;
        dst_attr_p->attr.fmtp.annex_k_val = src_attr_p->attr.fmtp.annex_k_val;
        dst_attr_p->attr.fmtp.annex_n_val = src_attr_p->attr.fmtp.annex_n_val;
        dst_attr_p->attr.fmtp.annex_p_val_picture_resize = 
            src_attr_p->attr.fmtp.annex_p_val_picture_resize;
        dst_attr_p->attr.fmtp.annex_p_val_warp  =
            src_attr_p->attr.fmtp.annex_p_val_warp;
        
        dst_attr_p->attr.fmtp.annexb_required  =
            src_attr_p->attr.fmtp.annexb_required;
        dst_attr_p->attr.fmtp.annexa_required  =
            src_attr_p->attr.fmtp.annexa_required;
        dst_attr_p->attr.fmtp.fmtp_format   
            = src_attr_p->attr.fmtp.fmtp_format;

        for (i=0; i < SDP_NE_NUM_BMAP_WORDS; i++) {
            dst_attr_p->attr.fmtp.bmap[i] = src_attr_p->attr.fmtp.bmap[i];
        }
        break;

    case SDP_ATTR_RTPMAP:
        dst_attr_p->attr.transport_map.payload_num = 
            src_attr_p->attr.transport_map.payload_num;
        dst_attr_p->attr.transport_map.clockrate = 
          src_attr_p->attr.transport_map.clockrate;
        dst_attr_p->attr.transport_map.num_chan  = 
          src_attr_p->attr.transport_map.num_chan;
        sstrncpy(dst_attr_p->attr.transport_map.encname, 
               src_attr_p->attr.transport_map.encname,
               SDP_MAX_STRING_LEN+1);
        break;

    case SDP_ATTR_SUBNET:
        dst_attr_p->attr.subnet.nettype  = src_attr_p->attr.subnet.nettype;
        dst_attr_p->attr.subnet.addrtype = src_attr_p->attr.subnet.addrtype;
        dst_attr_p->attr.subnet.prefix   = src_attr_p->attr.subnet.prefix;
        sstrncpy(dst_attr_p->attr.subnet.addr, src_attr_p->attr.subnet.addr,
		 SDP_MAX_STRING_LEN+1);
        break;

    case SDP_ATTR_T38_RATEMGMT:
        dst_attr_p->attr.t38ratemgmt = src_attr_p->attr.t38ratemgmt;
        break;

    case SDP_ATTR_T38_UDPEC:
        dst_attr_p->attr.t38udpec = src_attr_p->attr.t38udpec;
        break;

    case SDP_ATTR_X_PC_CODEC:
        dst_attr_p->attr.pccodec.num_payloads = 
            src_attr_p->attr.pccodec.num_payloads;
        for (i=0; i < src_attr_p->attr.pccodec.num_payloads; i++) {
            dst_attr_p->attr.pccodec.payload_type[i] = 
                src_attr_p->attr.pccodec.payload_type[i];
        }
        break;

    case SDP_ATTR_DIRECTION:

	dst_attr_p->attr.comediadir.role =
	    src_attr_p->attr.comediadir.role;

	if (src_attr_p->attr.comediadir.conn_info.nettype) {
	    dst_attr_p->attr.comediadir.conn_info_present = TRUE; 
	    dst_attr_p->attr.comediadir.conn_info.nettype =
		src_attr_p->attr.comediadir.conn_info.nettype;
	    dst_attr_p->attr.comediadir.conn_info.addrtype =
		src_attr_p->attr.comediadir.conn_info.addrtype;
	    sstrncpy(dst_attr_p->attr.comediadir.conn_info.conn_addr,
		     src_attr_p->attr.comediadir.conn_info.conn_addr,
		     SDP_MAX_STRING_LEN+1);
	    dst_attr_p->attr.comediadir.src_port =
		src_attr_p->attr.comediadir.src_port;
	}
	break;

    case SDP_ATTR_SILENCESUPP:
        dst_attr_p->attr.silencesupp.enabled =
            src_attr_p->attr.silencesupp.enabled;
        dst_attr_p->attr.silencesupp.timer_null =
            src_attr_p->attr.silencesupp.timer_null;
        dst_attr_p->attr.silencesupp.timer =
            src_attr_p->attr.silencesupp.timer;
        dst_attr_p->attr.silencesupp.pref =
            src_attr_p->attr.silencesupp.pref;
        dst_attr_p->attr.silencesupp.siduse =
            src_attr_p->attr.silencesupp.siduse;
        dst_attr_p->attr.silencesupp.fxnslevel_null =
            src_attr_p->attr.silencesupp.fxnslevel_null;
        dst_attr_p->attr.silencesupp.fxnslevel =
            src_attr_p->attr.silencesupp.fxnslevel;
        break;

    case SDP_ATTR_RTR:
	dst_attr_p->attr.rtr.confirm = src_attr_p->attr.rtr.confirm;
	break;

    case SDP_ATTR_X_SIDIN:
    case SDP_ATTR_X_SIDOUT:
    case SDP_ATTR_X_CONFID:
        sstrncpy(dst_attr_p->attr.stream_data.x_sidin,
                 src_attr_p->attr.stream_data.x_sidin,SDP_MAX_STRING_LEN+1);
        sstrncpy(dst_attr_p->attr.stream_data.x_sidout,
                 src_attr_p->attr.stream_data.x_sidout,SDP_MAX_STRING_LEN+1);
        sstrncpy(dst_attr_p->attr.stream_data.x_confid,
                 src_attr_p->attr.stream_data.x_confid,SDP_MAX_STRING_LEN+1);
        break;

    case SDP_ATTR_GROUP:
        dst_attr_p->attr.stream_data.group_attr =
            src_attr_p->attr.stream_data.group_attr;
        dst_attr_p->attr.stream_data.num_group_id =
             src_attr_p->attr.stream_data.num_group_id;
        
        for (i=0; i < src_attr_p->attr.stream_data.num_group_id; i++) {
            dst_attr_p->attr.stream_data.group_id_arr[i] = 
                src_attr_p->attr.stream_data.group_id_arr[i];
        }
        break;

    case SDP_ATTR_SOURCE_FILTER:
        dst_attr_p->attr.source_filter.mode = 
                         src_attr_p->attr.source_filter.mode;
        dst_attr_p->attr.source_filter.nettype =
                         src_attr_p->attr.source_filter.nettype;
        dst_attr_p->attr.source_filter.addrtype =
                         src_attr_p->attr.source_filter.addrtype;
        sstrncpy(dst_attr_p->attr.source_filter.dest_addr,
                 src_attr_p->attr.source_filter.dest_addr,
                 SDP_MAX_STRING_LEN+1);
        for (i=0; i<src_attr_p->attr.source_filter.num_src_addr; ++i) {
            sstrncpy(dst_attr_p->attr.source_filter.src_list[i],
                     src_attr_p->attr.source_filter.src_list[i],
                     SDP_MAX_STRING_LEN+1);
        }
        dst_attr_p->attr.source_filter.num_src_addr = 
                src_attr_p->attr.source_filter.num_src_addr;
        break;

    case SDP_ATTR_SRTP_CONTEXT:
    case SDP_ATTR_SDESCRIPTIONS:
         /* Tag parameter is not valid for version 2 sdescriptions */
        if (src_attr_p->type == SDP_ATTR_SDESCRIPTIONS) {
            dst_attr_p->attr.srtp_context.tag = 
	        src_attr_p->attr.srtp_context.tag;
        }
	
	dst_attr_p->attr.srtp_context.selection_flags =
	            src_attr_p->attr.srtp_context.selection_flags;
		    
	dst_attr_p->attr.srtp_context.suite = src_attr_p->attr.srtp_context.suite;
		    
	dst_attr_p->attr.srtp_context.master_key_size_bytes =
	            src_attr_p->attr.srtp_context.master_key_size_bytes;
		    
        dst_attr_p->attr.srtp_context.master_salt_size_bytes =
	            src_attr_p->attr.srtp_context.master_salt_size_bytes;
		    
        bcopy(src_attr_p->attr.srtp_context.master_key,
	      dst_attr_p->attr.srtp_context.master_key,
	      SDP_SRTP_MAX_KEY_SIZE_BYTES);
		 
	bcopy(src_attr_p->attr.srtp_context.master_salt,
	      dst_attr_p->attr.srtp_context.master_salt,
	      SDP_SRTP_MAX_SALT_SIZE_BYTES);
		 
	
	sstrncpy((char*)dst_attr_p->attr.srtp_context.master_key_lifetime,
	         (char*)src_attr_p->attr.srtp_context.master_key_lifetime,
		 SDP_SRTP_MAX_LIFETIME_BYTES);
		 
	sstrncpy((char*)dst_attr_p->attr.srtp_context.mki,
	         (char*)src_attr_p->attr.srtp_context.mki,
		 SDP_SRTP_MAX_MKI_SIZE_BYTES);
		 
	dst_attr_p->attr.srtp_context.mki_size_bytes =
	            src_attr_p->attr.srtp_context.mki_size_bytes;
		    
	if (src_attr_p->attr.srtp_context.session_parameters) {
	    dst_attr_p->attr.srtp_context.session_parameters =
	                cpr_strdup(src_attr_p->attr.srtp_context.session_parameters);
	}
		    
        break;

    default:
        break;
    }

    return;
}

/* Function:    sdp_copy_attr
 * Description: Copy an attribute from one SDP/level to another. A new
 *              attribute is automatically added to the end of the attr
 *              list at the dst SDP/level and all parameter values are
 *              copied.
 * Description: Add a new attribute of the specified type at the given
 *              level and capability level or base attribute if cap_num
 *              is zero.  
 * Parameters:  src_sdp_ptr The source SDP handle.
 *              dst_sdp_ptr The dest SDP handle.
 *              src_level   The level of the source attribute.
 *              dst_level   The level of the source attribute.
 *              src_cap_num The src capability number associated with the
 *                          attribute if any.  
 *              dst_cap_num The dst capability number associated with the
 *                          attribute if any. Note that src and dst
 *                          cap_num must both be zero or both be non-zero. 
 *              src_attr_type  The type of source attribute. This cannot
 *                             be SDP_ATTR_X_CAP or SDP_ATTR_X_CPAR.
 *              src_inst_num   The instance number of the source attr.
 * Returns:     SDP_SUCCESS    Attribute was successfully copied.
 */
sdp_result_e sdp_copy_attr (void *src_sdp_ptr, void *dst_sdp_ptr,
                            u16 src_level, u16 dst_level,
                            u8 src_cap_num, u8 dst_cap_num,
                            sdp_attr_e src_attr_type, u16 src_inst_num)
{
    u16          i;
    sdp_mca_t   *mca_p;
    sdp_mca_t   *cap_p;
    sdp_t       *src_sdp_p = (sdp_t *)src_sdp_ptr;
    sdp_t       *dst_sdp_p = (sdp_t *)dst_sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_attr_t  *new_attr_p;
    sdp_attr_t  *prev_attr_p;
    sdp_attr_t  *src_attr_p;

    if (sdp_verify_sdp_ptr(src_sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    /* Make sure if one is a capability attribute, then both are. */
    if (((src_cap_num == 0) && (dst_cap_num != 0)) ||
        ((src_cap_num != 0) && (dst_cap_num == 0))) {
        src_sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    /* Cannot copy X_CAP/CDSC attributes directly using this routine. 
     * You also can't copy X_CPAR/CPAR attributes by specifying them directly,
     * but you can copy them by giving the corresponding cap_num. */
    if ((src_attr_type == SDP_ATTR_X_CAP) ||
        (src_attr_type == SDP_ATTR_X_CPAR) ||
	(src_attr_type == SDP_ATTR_CDSC) ||
        (src_attr_type == SDP_ATTR_CPAR)) {
        src_sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    src_attr_p = sdp_find_attr(src_sdp_p, src_level, src_cap_num, 
                               src_attr_type, src_inst_num);
    if (src_attr_p == NULL) {
        if (src_sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Error: Source attribute for copy not found.",
                      src_sdp_p->debug_str);
        }
        src_sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    new_attr_p = (sdp_attr_t *)SDP_MALLOC(sizeof(sdp_attr_t));
    if (new_attr_p == NULL) {
        src_sdp_p->conf_p->num_no_resource++;
        return (SDP_NO_RESOURCE);
    }

    /* Copy over all the attribute information. */
    new_attr_p->type = src_attr_type;
    new_attr_p->next_p = NULL;

    switch (src_attr_type) {

    case SDP_ATTR_BEARER:
    case SDP_ATTR_CALLED:
    case SDP_ATTR_CONN_TYPE:
    case SDP_ATTR_DIALED:
    case SDP_ATTR_DIALING:
    case SDP_ATTR_FRAMING:
    case SDP_ATTR_MAXPRATE:
    case SDP_ATTR_LABEL:
        sstrncpy(new_attr_p->attr.string_val, src_attr_p->attr.string_val,
		 SDP_MAX_STRING_LEN+1);
        break;

    case SDP_ATTR_EECID:
    case SDP_ATTR_PTIME:
    case SDP_ATTR_T38_VERSION:
    case SDP_ATTR_T38_MAXBITRATE:
    case SDP_ATTR_T38_MAXBUFFER:
    case SDP_ATTR_T38_MAXDGRAM:
    case SDP_ATTR_X_SQN:
    case SDP_ATTR_TC1_PAYLOAD_BYTES:
    case SDP_ATTR_TC1_WINDOW_SIZE:
    case SDP_ATTR_TC2_PAYLOAD_BYTES:
    case SDP_ATTR_TC2_WINDOW_SIZE:
    case SDP_ATTR_RTCP:
    case SDP_ATTR_MID:
    case SDP_ATTR_RTCP_UNICAST:
        new_attr_p->attr.u32_val = src_attr_p->attr.u32_val;
        break;

    case SDP_ATTR_T38_FILLBITREMOVAL:
    case SDP_ATTR_T38_TRANSCODINGMMR:
    case SDP_ATTR_T38_TRANSCODINGJBIG:
    case SDP_ATTR_TMRGWXID:
        new_attr_p->attr.boolean_val = src_attr_p->attr.boolean_val;
        break;

    case SDP_ATTR_QOS:
    case SDP_ATTR_SECURE:
    case SDP_ATTR_X_PC_QOS:
    case SDP_ATTR_X_QOS:
        new_attr_p->attr.qos.strength  = src_attr_p->attr.qos.strength;
        new_attr_p->attr.qos.direction = src_attr_p->attr.qos.direction;
        new_attr_p->attr.qos.confirm   = src_attr_p->attr.qos.confirm;
        break;
        
    case SDP_ATTR_CURR:
        new_attr_p->attr.curr.type         = src_attr_p->attr.curr.type;
        new_attr_p->attr.curr.direction    = src_attr_p->attr.curr.direction;
        new_attr_p->attr.curr.status_type  = src_attr_p->attr.curr.status_type;
        break;
    case SDP_ATTR_DES:
        new_attr_p->attr.des.type         = src_attr_p->attr.des.type;
        new_attr_p->attr.des.direction    = src_attr_p->attr.des.direction;
        new_attr_p->attr.des.status_type  = src_attr_p->attr.des.status_type;
        new_attr_p->attr.des.strength     = src_attr_p->attr.des.strength;
        break;
         
    case SDP_ATTR_CONF:
        new_attr_p->attr.conf.type         = src_attr_p->attr.conf.type;
        new_attr_p->attr.conf.direction    = src_attr_p->attr.conf.direction;
        new_attr_p->attr.conf.status_type  = src_attr_p->attr.conf.status_type;
        break;

    case SDP_ATTR_INACTIVE:
    case SDP_ATTR_RECVONLY:
    case SDP_ATTR_SENDONLY:
    case SDP_ATTR_SENDRECV:
        /* These attrs have no parameters. */
        break;

    case SDP_ATTR_FMTP:
        new_attr_p->attr.fmtp.payload_num = src_attr_p->attr.fmtp.payload_num;
        new_attr_p->attr.fmtp.maxval      = src_attr_p->attr.fmtp.maxval;
	new_attr_p->attr.fmtp.bitrate     = src_attr_p->attr.fmtp.bitrate;
	new_attr_p->attr.fmtp.annexa      = src_attr_p->attr.fmtp.annexa;
	new_attr_p->attr.fmtp.annexb      = src_attr_p->attr.fmtp.annexb;
	new_attr_p->attr.fmtp.cif      = src_attr_p->attr.fmtp.cif;
	new_attr_p->attr.fmtp.qcif      = src_attr_p->attr.fmtp.qcif;
	new_attr_p->attr.fmtp.sqcif      = src_attr_p->attr.fmtp.qcif;
	new_attr_p->attr.fmtp.cif4      = src_attr_p->attr.fmtp.cif4;
	new_attr_p->attr.fmtp.cif16      = src_attr_p->attr.fmtp.cif16;
	new_attr_p->attr.fmtp.maxbr      = src_attr_p->attr.fmtp.maxbr;
	new_attr_p->attr.fmtp.custom_x      = src_attr_p->attr.fmtp.custom_x;
	new_attr_p->attr.fmtp.custom_y      = src_attr_p->attr.fmtp.custom_y;
	new_attr_p->attr.fmtp.custom_mpi    = src_attr_p->attr.fmtp.custom_mpi;
	new_attr_p->attr.fmtp.par_width      = src_attr_p->attr.fmtp.par_width;
	new_attr_p->attr.fmtp.par_height      = src_attr_p->attr.fmtp.par_height;
	new_attr_p->attr.fmtp.cpcf      = src_attr_p->attr.fmtp.cpcf;
	new_attr_p->attr.fmtp.bpp      = src_attr_p->attr.fmtp.bpp;
	new_attr_p->attr.fmtp.hrd      = src_attr_p->attr.fmtp.hrd;
        new_attr_p->attr.fmtp.profile    = src_attr_p->attr.fmtp.profile;
        new_attr_p->attr.fmtp.level      = src_attr_p->attr.fmtp.level;
        new_attr_p->attr.fmtp.is_interlace = src_attr_p->attr.fmtp.is_interlace;
        
        sstrncpy(new_attr_p->attr.fmtp.profile_level_id, 
                src_attr_p->attr.fmtp.profile_level_id,
                SDP_MAX_STRING_LEN+1);
        sstrncpy(new_attr_p->attr.fmtp.parameter_sets, 
                src_attr_p->attr.fmtp.parameter_sets,
                SDP_MAX_STRING_LEN+1);
        new_attr_p->attr.fmtp.deint_buf_req =
		 src_attr_p->attr.fmtp.deint_buf_req;
        new_attr_p->attr.fmtp.max_don_diff = 
            src_attr_p->attr.fmtp.max_don_diff;
        new_attr_p->attr.fmtp.init_buf_time =
                 src_attr_p->attr.fmtp.init_buf_time;
        new_attr_p->attr.fmtp.packetization_mode =
                 src_attr_p->attr.fmtp.packetization_mode;
        new_attr_p->attr.fmtp.flag = 
                 src_attr_p->attr.fmtp.flag;

        new_attr_p->attr.fmtp.max_mbps = src_attr_p->attr.fmtp.max_mbps;
        new_attr_p->attr.fmtp.max_fs = src_attr_p->attr.fmtp.max_fs;        
        new_attr_p->attr.fmtp.max_cpb = src_attr_p->attr.fmtp.max_cpb;
        new_attr_p->attr.fmtp.max_dpb = src_attr_p->attr.fmtp.max_dpb;
        new_attr_p->attr.fmtp.max_br = src_attr_p->attr.fmtp.max_br;
        new_attr_p->attr.fmtp.redundant_pic_cap = 
            src_attr_p->attr.fmtp.redundant_pic_cap;        
        new_attr_p->attr.fmtp.deint_buf_cap =
		 src_attr_p->attr.fmtp.deint_buf_cap;
        new_attr_p->attr.fmtp.max_rcmd_nalu_size =
                 src_attr_p->attr.fmtp.max_rcmd_nalu_size; 
        new_attr_p->attr.fmtp.interleaving_depth =
		 src_attr_p->attr.fmtp.interleaving_depth;
        new_attr_p->attr.fmtp.parameter_add = 
            src_attr_p->attr.fmtp.parameter_add;
        
        new_attr_p->attr.fmtp.annex_d = src_attr_p->attr.fmtp.annex_d;
        new_attr_p->attr.fmtp.annex_f = src_attr_p->attr.fmtp.annex_f;
        new_attr_p->attr.fmtp.annex_i = src_attr_p->attr.fmtp.annex_i;
        new_attr_p->attr.fmtp.annex_j = src_attr_p->attr.fmtp.annex_j;
        new_attr_p->attr.fmtp.annex_t = src_attr_p->attr.fmtp.annex_t;
        new_attr_p->attr.fmtp.annex_k_val = src_attr_p->attr.fmtp.annex_k_val;
        new_attr_p->attr.fmtp.annex_n_val = src_attr_p->attr.fmtp.annex_n_val;
        new_attr_p->attr.fmtp.annex_p_val_picture_resize = 
            src_attr_p->attr.fmtp.annex_p_val_picture_resize;
        new_attr_p->attr.fmtp.annex_p_val_warp  =
            src_attr_p->attr.fmtp.annex_p_val_warp;
        
	new_attr_p->attr.fmtp.annexb_required  =
	                                  src_attr_p->attr.fmtp.annexb_required;
        new_attr_p->attr.fmtp.annexa_required  =
	                                  src_attr_p->attr.fmtp.annexa_required;
	new_attr_p->attr.fmtp.fmtp_format   
	                                  = src_attr_p->attr.fmtp.fmtp_format;

        for (i=0; i < SDP_NE_NUM_BMAP_WORDS; i++) {
            new_attr_p->attr.fmtp.bmap[i] = src_attr_p->attr.fmtp.bmap[i];
        }
        break;

    case SDP_ATTR_RTPMAP:
        new_attr_p->attr.transport_map.payload_num = 
            src_attr_p->attr.transport_map.payload_num;
        new_attr_p->attr.transport_map.clockrate = 
          src_attr_p->attr.transport_map.clockrate;
        new_attr_p->attr.transport_map.num_chan  = 
          src_attr_p->attr.transport_map.num_chan;
        sstrncpy(new_attr_p->attr.transport_map.encname, 
               src_attr_p->attr.transport_map.encname,
               SDP_MAX_STRING_LEN+1);
        break;

    case SDP_ATTR_SUBNET:
        new_attr_p->attr.subnet.nettype  = src_attr_p->attr.subnet.nettype;
        new_attr_p->attr.subnet.addrtype = src_attr_p->attr.subnet.addrtype;
        new_attr_p->attr.subnet.prefix   = src_attr_p->attr.subnet.prefix;
        sstrncpy(new_attr_p->attr.subnet.addr, src_attr_p->attr.subnet.addr,
		 SDP_MAX_STRING_LEN+1);
        break;

    case SDP_ATTR_T38_RATEMGMT:
        new_attr_p->attr.t38ratemgmt = src_attr_p->attr.t38ratemgmt;
        break;

    case SDP_ATTR_T38_UDPEC:
        new_attr_p->attr.t38udpec = src_attr_p->attr.t38udpec;
        break;

    case SDP_ATTR_X_PC_CODEC:
        new_attr_p->attr.pccodec.num_payloads = 
            src_attr_p->attr.pccodec.num_payloads;
        for (i=0; i < src_attr_p->attr.pccodec.num_payloads; i++) {
            new_attr_p->attr.pccodec.payload_type[i] = 
                src_attr_p->attr.pccodec.payload_type[i];
        }
        break;

    case SDP_ATTR_DIRECTION:

	new_attr_p->attr.comediadir.role =
	    src_attr_p->attr.comediadir.role;

	if (src_attr_p->attr.comediadir.conn_info.nettype) {
	    new_attr_p->attr.comediadir.conn_info_present = TRUE; 
	    new_attr_p->attr.comediadir.conn_info.nettype =
		src_attr_p->attr.comediadir.conn_info.nettype;
	    new_attr_p->attr.comediadir.conn_info.addrtype =
		src_attr_p->attr.comediadir.conn_info.addrtype;
	    sstrncpy(new_attr_p->attr.comediadir.conn_info.conn_addr,
		     src_attr_p->attr.comediadir.conn_info.conn_addr,
		     SDP_MAX_STRING_LEN+1);
	    new_attr_p->attr.comediadir.src_port =
		src_attr_p->attr.comediadir.src_port;
	}
	break;


    case SDP_ATTR_SILENCESUPP:
        new_attr_p->attr.silencesupp.enabled =
            src_attr_p->attr.silencesupp.enabled;
        new_attr_p->attr.silencesupp.timer_null =
            src_attr_p->attr.silencesupp.timer_null;
        new_attr_p->attr.silencesupp.timer =
            src_attr_p->attr.silencesupp.timer;
        new_attr_p->attr.silencesupp.pref =
            src_attr_p->attr.silencesupp.pref;
        new_attr_p->attr.silencesupp.siduse =
            src_attr_p->attr.silencesupp.siduse;
        new_attr_p->attr.silencesupp.fxnslevel_null =
            src_attr_p->attr.silencesupp.fxnslevel_null;
        new_attr_p->attr.silencesupp.fxnslevel =
            src_attr_p->attr.silencesupp.fxnslevel;
        break;

    case SDP_ATTR_MPTIME:
        new_attr_p->attr.mptime.num_intervals = 
            src_attr_p->attr.mptime.num_intervals;
        for (i=0; i < src_attr_p->attr.mptime.num_intervals; i++) {
            new_attr_p->attr.mptime.intervals[i] = 
                src_attr_p->attr.mptime.intervals[i];
        }
        break;

    case SDP_ATTR_X_SIDIN:
    case SDP_ATTR_X_SIDOUT:
    case SDP_ATTR_X_CONFID:
        sstrncpy(new_attr_p->attr.stream_data.x_sidin,
                 src_attr_p->attr.stream_data.x_sidin,SDP_MAX_STRING_LEN+1);
        sstrncpy(new_attr_p->attr.stream_data.x_sidout,
                 src_attr_p->attr.stream_data.x_sidout,SDP_MAX_STRING_LEN+1);
        sstrncpy(new_attr_p->attr.stream_data.x_confid,
                 src_attr_p->attr.stream_data.x_confid,SDP_MAX_STRING_LEN+1);
	break;
        
    case SDP_ATTR_GROUP:
        new_attr_p->attr.stream_data.group_attr =
            src_attr_p->attr.stream_data.group_attr;
        new_attr_p->attr.stream_data.num_group_id =
            src_attr_p->attr.stream_data.num_group_id;
        
        for (i=0; i < src_attr_p->attr.stream_data.num_group_id; i++) {
            new_attr_p->attr.stream_data.group_id_arr[i] = 
                src_attr_p->attr.stream_data.group_id_arr[i];
        }
        break;

    case SDP_ATTR_SOURCE_FILTER:
        new_attr_p->attr.source_filter.mode =
             src_attr_p->attr.source_filter.mode;
        new_attr_p->attr.source_filter.nettype =
                         src_attr_p->attr.source_filter.nettype;
        new_attr_p->attr.source_filter.addrtype =
                         src_attr_p->attr.source_filter.addrtype;
        sstrncpy(new_attr_p->attr.source_filter.dest_addr,
                 src_attr_p->attr.source_filter.dest_addr,
                 SDP_MAX_STRING_LEN+1);
        for (i=0; i<src_attr_p->attr.source_filter.num_src_addr; ++i) {
             sstrncpy(new_attr_p->attr.source_filter.src_list[i],
                     src_attr_p->attr.source_filter.src_list[i],
                     SDP_MAX_STRING_LEN+1);
        }
        new_attr_p->attr.source_filter.num_src_addr =
                src_attr_p->attr.source_filter.num_src_addr;
        break;

    case SDP_ATTR_SRTP_CONTEXT:
    case SDP_ATTR_SDESCRIPTIONS:
        /* Tag parameter is only valid for version 9 sdescriptions */
        if (src_attr_type == SDP_ATTR_SDESCRIPTIONS) {
            new_attr_p->attr.srtp_context.tag = 
	        src_attr_p->attr.srtp_context.tag;
	}
	
	new_attr_p->attr.srtp_context.suite =  
	            src_attr_p->attr.srtp_context.suite;
		    
	new_attr_p->attr.srtp_context.selection_flags =  
	            src_attr_p->attr.srtp_context.selection_flags;
		    
	new_attr_p->attr.srtp_context.master_key_size_bytes =
	            src_attr_p->attr.srtp_context.master_key_size_bytes;
		    
        new_attr_p->attr.srtp_context.master_salt_size_bytes =
	            src_attr_p->attr.srtp_context.master_salt_size_bytes;
		    
        bcopy(src_attr_p->attr.srtp_context.master_key,
	      new_attr_p->attr.srtp_context.master_key,
	      SDP_SRTP_MAX_KEY_SIZE_BYTES);
		 
	bcopy(src_attr_p->attr.srtp_context.master_salt,
	      new_attr_p->attr.srtp_context.master_salt,
	      SDP_SRTP_MAX_SALT_SIZE_BYTES);
		 
	
	sstrncpy((char*)new_attr_p->attr.srtp_context.master_key_lifetime,
	         (char*)src_attr_p->attr.srtp_context.master_key_lifetime,
		 SDP_SRTP_MAX_LIFETIME_BYTES);
		 
	sstrncpy((char*)new_attr_p->attr.srtp_context.mki,
	         (char*)src_attr_p->attr.srtp_context.mki,
		 SDP_SRTP_MAX_MKI_SIZE_BYTES);
		 
	new_attr_p->attr.srtp_context.mki_size_bytes =
	            src_attr_p->attr.srtp_context.mki_size_bytes;
		    
	if (src_attr_p->attr.srtp_context.session_parameters) {
	    new_attr_p->attr.srtp_context.session_parameters =
	                cpr_strdup(src_attr_p->attr.srtp_context.session_parameters);
	}
		    
        break;

    default:
        break;
    }

    if (src_cap_num == 0) {
        if (dst_level == SDP_SESSION_LEVEL) {
            if (dst_sdp_p->sess_attrs_p == NULL) {
                dst_sdp_p->sess_attrs_p = new_attr_p;
            } else {
                for (prev_attr_p = dst_sdp_p->sess_attrs_p;
                     prev_attr_p->next_p != NULL;
                     prev_attr_p = prev_attr_p->next_p) {
                    ; /* Empty for */
                }
                prev_attr_p->next_p = new_attr_p;
            }
        } else {
            mca_p = sdp_find_media_level(dst_sdp_p, dst_level);
            if (mca_p == NULL) {
                sdp_free_attr(new_attr_p);
                src_sdp_p->conf_p->num_invalid_param++;
                return (SDP_INVALID_PARAMETER);
            }
            if (mca_p->media_attrs_p == NULL) {
                mca_p->media_attrs_p = new_attr_p;
            } else {
                for (prev_attr_p = mca_p->media_attrs_p;
                     prev_attr_p->next_p != NULL;
                     prev_attr_p = prev_attr_p->next_p) {
                    ; /* Empty for */
                }
                prev_attr_p->next_p = new_attr_p;
            }
        }
    } else {
        /* Add a new capability attribute - find the capability attr. */
        attr_p = sdp_find_capability(dst_sdp_p, dst_level, dst_cap_num);
        if (attr_p == NULL) {
            sdp_free_attr(new_attr_p);
            src_sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        cap_p = attr_p->attr.cap_p;
        if (cap_p->media_attrs_p == NULL) {
            cap_p->media_attrs_p = new_attr_p;
        } else {
            for (prev_attr_p = cap_p->media_attrs_p;
                 prev_attr_p->next_p != NULL;
                 prev_attr_p = prev_attr_p->next_p) {                  
                 ; /* Empty for */
            }
            prev_attr_p->next_p = new_attr_p;
        }
    }

    return (SDP_SUCCESS);
}

/* Function:    sdp_copy_all_attrs
 * Description: Copy all attributes from one SDP/level to another.
 * Parameters:  src_sdp_ptr The source SDP handle.
 *              dst_sdp_ptr The dest SDP handle.
 *              src_level   The level of the source attributes.
 *              dst_level   The level of the source attributes.
 * Returns:     SDP_SUCCESS    Attributes were successfully copied.
 */
sdp_result_e sdp_copy_all_attrs (void *src_sdp_ptr, void *dst_sdp_ptr,
                                 u16 src_level, u16 dst_level)
{
    int i;
    sdp_mca_t   *mca_p = NULL;
    sdp_t       *src_sdp_p = (sdp_t *)src_sdp_ptr;
    sdp_t       *dst_sdp_p = (sdp_t *)dst_sdp_ptr;
    sdp_mca_t   *src_cap_p;
    sdp_mca_t   *dst_cap_p;
    sdp_attr_t  *src_attr_p;
    sdp_attr_t  *dst_attr_p;
    sdp_attr_t  *new_attr_p;
    sdp_attr_t  *src_cap_attr_p;
    sdp_attr_t  *dst_cap_attr_p;
    sdp_attr_t  *new_cap_attr_p;

    if (sdp_verify_sdp_ptr(src_sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (sdp_verify_sdp_ptr(dst_sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    /* Find src attribute list. */
    if (src_level == SDP_SESSION_LEVEL) {
        src_attr_p = src_sdp_p->sess_attrs_p;
    } else {
        mca_p = sdp_find_media_level(src_sdp_p, src_level);
        if (mca_p == NULL) {
            if (src_sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s Invalid src media level (%u) for copy all "
                          "attrs ", src_sdp_p->debug_str, src_level);
            }
            return (SDP_INVALID_PARAMETER);
        }
        src_attr_p = mca_p->media_attrs_p;
    }

    /* Find dst attribute list. */
    if (dst_level == SDP_SESSION_LEVEL) {
        dst_attr_p = dst_sdp_p->sess_attrs_p;
    } else {
        mca_p = sdp_find_media_level(dst_sdp_p, dst_level);
        if (mca_p == NULL) {
            if (src_sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s Invalid dst media level (%u) for copy all "
                          "attrs ", src_sdp_p->debug_str, dst_level);
            }
            return (SDP_INVALID_PARAMETER);
        }
        dst_attr_p = mca_p->media_attrs_p;
    }

    /* Now find the end of the dst attr list.  This is where we will
     * add new attributes. */
    while ((dst_attr_p != NULL) && (dst_attr_p->next_p != NULL)) {
        dst_attr_p = dst_attr_p->next_p;
    }

    /* For each src attribute, allocate a new dst attr and copy the info */
    while (src_attr_p != NULL) {

        /* Allocate the new attr. */
        new_attr_p = (sdp_attr_t *)SDP_MALLOC(sizeof(sdp_attr_t));
        if (new_attr_p == NULL) {
            src_sdp_p->conf_p->num_no_resource++;
            return (SDP_NO_RESOURCE);
        }

        if ((src_attr_p->type != SDP_ATTR_X_CAP) &&
	    (src_attr_p->type != SDP_ATTR_CDSC)) {
            /* Simple attr type - copy over all the attr info. */
            sdp_copy_attr_fields(src_attr_p, new_attr_p);
        } else {
            /* X-cap/cdsc attrs must be handled differently. Allocate an 
             * mca structure and copy over any X-cpar/cdsc attrs. */
            
            new_attr_p->attr.cap_p = 
                (sdp_mca_t *)SDP_MALLOC(sizeof(sdp_mca_t));
            if (new_attr_p->attr.cap_p == NULL) {
	        sdp_free_attr(new_attr_p);
                return (SDP_NO_RESOURCE);
            }

            new_attr_p->type = src_attr_p->type;

            src_cap_p = src_attr_p->attr.cap_p;
            dst_cap_p = new_attr_p->attr.cap_p;

            dst_cap_p->media         = src_cap_p->media;
            dst_cap_p->conn.nettype  = src_cap_p->conn.nettype;
            dst_cap_p->conn.addrtype = src_cap_p->conn.addrtype;
            sstrncpy(dst_cap_p->conn.conn_addr, src_cap_p->conn.conn_addr,
                     SDP_MAX_LINE_LEN+1);
            dst_cap_p->transport     = src_cap_p->transport;
            dst_cap_p->port_format   = src_cap_p->port_format;
            dst_cap_p->port          = src_cap_p->port;
            dst_cap_p->num_ports     = src_cap_p->num_ports;
            dst_cap_p->vpi           = src_cap_p->vpi;
            dst_cap_p->vci           = src_cap_p->vci;
            dst_cap_p->vcci          = src_cap_p->vcci;
            dst_cap_p->cid           = src_cap_p->cid;
            dst_cap_p->num_payloads  = src_cap_p->num_payloads;
            dst_cap_p->mid           = src_cap_p->mid;

            for (i=0; i < SDP_MAX_PAYLOAD_TYPES; i++) {
                new_attr_p->attr.cap_p->payload_indicator[i] = 
                    src_attr_p->attr.cap_p->payload_indicator[i];
                new_attr_p->attr.cap_p->payload_type[i] = 
                    src_attr_p->attr.cap_p->payload_type[i];
            }

            src_cap_attr_p = src_attr_p->attr.cap_p->media_attrs_p;
            dst_cap_attr_p = NULL;

            /* Copy all of the X-cpar/cpar attrs from the src. */
            while (src_cap_attr_p != NULL) {

                new_cap_attr_p = (sdp_attr_t *)SDP_MALLOC(sizeof(sdp_attr_t));
                if (new_cap_attr_p == NULL) {
		    sdp_free_attr (new_attr_p);
                    return (SDP_NO_RESOURCE);
                }
                
                /* Copy X-cpar/cpar attribute info. */
                sdp_copy_attr_fields(src_cap_attr_p, new_cap_attr_p);

                /* Now add the new X-cpar/cpar attr in the right place. */
                if (dst_cap_attr_p == NULL) {
                    new_attr_p->attr.cap_p->media_attrs_p = new_cap_attr_p;
                    dst_cap_attr_p = new_cap_attr_p;
                } else {
                    dst_cap_attr_p->next_p = new_cap_attr_p;
                    dst_cap_attr_p = new_cap_attr_p;
                }

                /* Move to the next X-cpar/cpar attr. */
                src_cap_attr_p = src_cap_attr_p->next_p;
            }

        }

        /* New add the new base attr at the correct place. */
        if (dst_attr_p == NULL) {
            if (dst_level == SDP_SESSION_LEVEL) {
                dst_sdp_p->sess_attrs_p = new_attr_p;
                dst_attr_p = dst_sdp_p->sess_attrs_p;
            } else {
                mca_p->media_attrs_p = new_attr_p;
                dst_attr_p = mca_p->media_attrs_p;
            }
        } else {
            dst_attr_p->next_p = new_attr_p;
            dst_attr_p = dst_attr_p->next_p;
        }

        /* Now move on to the next src attr. */
        src_attr_p = src_attr_p->next_p;
    }

    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_num_instances
 * Description: Get the number of attributes of the specified type at 
 *              the given level and capability level.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              attr_type   The type of attribute to add.
 *              num_attr_inst Pointer to a u16 in which to return the
 *                          number of attributes.
 * Returns:     SDP_SUCCESS            Attribute was added successfully.
 *              SDP_INVALID_PARAMETER  Specified media line is not defined.
 */
sdp_result_e sdp_attr_num_instances (void *sdp_ptr, u16 level, u8 cap_num,
                                     sdp_attr_e attr_type, u16 *num_attr_inst)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_result_e rc;
    static char  fname[] = "attr_num_instances";

    *num_attr_inst = 0;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    rc = sdp_find_attr_list(sdp_p, level, cap_num, &attr_p, fname);
    if (rc == SDP_SUCCESS) {
        /* Found the attr list. Count the number of attrs of the given 
         * type at this level. */
        for (; attr_p != NULL; attr_p = attr_p->next_p) {
            if (attr_p->type == attr_type) {
                (*num_attr_inst)++;
            }
        }

    }

    return (rc);
}


/* Function:    sdp_get_total_attrs
 * Description: Get the total number of attributes at the given level and 
 *              capability level.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              num_attrs   Pointer to a u16 in which to return the
 *                          number of attributes.
 * Returns:     SDP_SUCCESS            Attribute was added successfully.
 *              SDP_INVALID_PARAMETER  Specified media line is not defined.
 */
sdp_result_e sdp_get_total_attrs (void *sdp_ptr, u16 level, u8 cap_num,
                                  u16 *num_attrs)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_result_e rc;
    static char  fname[] = "get_total_attrs";

    *num_attrs = 0;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    rc = sdp_find_attr_list(sdp_p, level, cap_num, &attr_p, fname);
    if (rc == SDP_SUCCESS) {
        /* Found the attr list. Count the total number of attrs  
         * at this level. */
        for (; attr_p != NULL; attr_p = attr_p->next_p) {
            (*num_attrs)++;
        }

    }

    return (rc);
}


/* Function:    sdp_get_attr_type
 * Description: Given an overall attribute number at a specified level, i.e.,
 *              attr number is not specific to the type of attribute, return
 *              the attribute type and the instance number of that particular
 *              type of attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              attr_num    Attribute number to retrieve. This is an overall
 *                          attribute number over all attrs at this level.
 *                          Range is (1 - max attrs at this level).
 * Returns:     SDP_SUCCESS            Attribute was added successfully.
 *              SDP_INVALID_PARAMETER  Specified media line is not defined.
 */
sdp_result_e sdp_get_attr_type (void *sdp_ptr, u16 level, u8 cap_num,
                           u16 attr_num, sdp_attr_e *attr_type, u16 *inst_num)
{
    int          i;
    u16          attr_total_count=0;
    u16          attr_count[SDP_MAX_ATTR_TYPES];
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_result_e rc;
    static char  fname[] = "get_attr_type";

    *attr_type = SDP_ATTR_INVALID;
    *inst_num = 0;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (attr_num < 1) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s, invalid attr num specified (%u) at level %u", 
                      sdp_p->debug_str, fname, attr_num, level);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    for (i=0; i < SDP_MAX_ATTR_TYPES; i++) {
        attr_count[i] = 0;
    }

    rc = sdp_find_attr_list(sdp_p, level, cap_num, &attr_p, fname);
    if (rc == SDP_SUCCESS) {
        /* Found the attr list. Now find the particular attribute
         * at the given level. */
        for (; attr_p != NULL; attr_p = attr_p->next_p) {
            attr_count[attr_p->type]++;
            if (++attr_total_count == attr_num) {
                *attr_type = attr_p->type;
                *inst_num = attr_count[attr_p->type];
                break;
            }
        }

    }

    return (rc);
}


/* Internal routine to free the memory associated with an attribute.
 * Certain attributes allocate additional memory.  Free this and then
 * free the attribute itself.  
 * Note that this routine may be called at any point (i.e., may be 
 * called due to a failure case) and so the additional memory 
 * associated with an attribute may or may not have been already 
 * allocated. This routine should check this carefully.
 */
void sdp_free_attr (sdp_attr_t *attr_p) 
{
    sdp_mca_t   *cap_p;
    sdp_attr_t  *cpar_p;
    sdp_attr_t  *next_cpar_p;

    /* If this is an X-cap/cdsc attr, free the cap_p structure and 
     * all X-cpar/cpar attributes. */
    if ((attr_p->type == SDP_ATTR_X_CAP) || 
	(attr_p->type == SDP_ATTR_CDSC)) {
        cap_p = attr_p->attr.cap_p;
        if (cap_p != NULL) {
            for (cpar_p = cap_p->media_attrs_p; cpar_p != NULL;) {
                next_cpar_p = cpar_p->next_p;
                sdp_free_attr(cpar_p);
                cpar_p = next_cpar_p;
            }
            SDP_FREE(cap_p);
        }
    } else if ((attr_p->type == SDP_ATTR_SDESCRIPTIONS) ||
              (attr_p->type == SDP_ATTR_SRTP_CONTEXT)) {
              SDP_FREE(attr_p->attr.srtp_context.session_parameters);
    }

    /* Now free the actual attribute memory. */
    SDP_FREE(attr_p);

}


/* Function:    sdp_delete_attr
 * Description: Delete the specified attribute from the SDP structure.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              attr_type   The type of attribute to delete.
 *              inst_num    The instance num of the attribute to delete.
 * Returns:     SDP_SUCCESS            Attribute was deleted successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_delete_attr (void *sdp_ptr, u16 level, u8 cap_num,
                              sdp_attr_e attr_type, u16 inst_num)
{
    u16          attr_count=0;
    sdp_mca_t   *mca_p;
    sdp_mca_t   *cap_p;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_attr_t  *prev_attr_p = NULL;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (cap_num == 0) {
        /* Find and delete the specified instance. */
        if (level == SDP_SESSION_LEVEL) {
            for (attr_p = sdp_p->sess_attrs_p; attr_p != NULL;
                 prev_attr_p = attr_p, attr_p = attr_p->next_p) {
                if (attr_p->type == attr_type) {
                    attr_count++;
                    if (attr_count == inst_num) {
                        break;
                    }
                }
            }
            if (attr_p == NULL) {
                if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                    SDP_ERROR("%s Delete attribute (%s) instance %d not "
                              "found.", sdp_p->debug_str,
                              sdp_get_attr_name(attr_type), inst_num);
                }
                sdp_p->conf_p->num_invalid_param++;
                return (SDP_INVALID_PARAMETER);
            }
            if (prev_attr_p == NULL) {
                sdp_p->sess_attrs_p = attr_p->next_p;
            } else {
                prev_attr_p->next_p = attr_p->next_p;
            }
            sdp_free_attr(attr_p);
        } else {  /* Attr is at a media level */
            mca_p = sdp_find_media_level(sdp_p, level);
            if (mca_p == NULL) {
                sdp_p->conf_p->num_invalid_param++;
                return (SDP_INVALID_PARAMETER);
            }
            for (attr_p = mca_p->media_attrs_p; attr_p != NULL;
                 prev_attr_p = attr_p, attr_p = attr_p->next_p) {
                if (attr_p->type == attr_type) {
                    attr_count++;
                    if (attr_count == inst_num) {
                        break;
                    }
                }
            }
            if (attr_p == NULL) {
                if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                    SDP_ERROR("%s Delete attribute (%s) instance %d "
                              "not found.", sdp_p->debug_str, 
                              sdp_get_attr_name(attr_type), inst_num);
                }
                sdp_p->conf_p->num_invalid_param++;
                return (SDP_INVALID_PARAMETER);
            }
            if (prev_attr_p == NULL) {
                mca_p->media_attrs_p = attr_p->next_p;
            } else {
                prev_attr_p->next_p = attr_p->next_p;
            }
            sdp_free_attr(attr_p);
        }  /* Attr is at a media level */
    } else {
        /* Attr is a capability X-cpar/cpar attribute, find the capability. */
        attr_p = sdp_find_capability(sdp_p, level, cap_num);
        if (attr_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        cap_p = attr_p->attr.cap_p;
        /* Now find the specific attribute to delete. */
        for (attr_p = cap_p->media_attrs_p; attr_p != NULL;
             prev_attr_p = attr_p, attr_p = attr_p->next_p) {
            if (attr_p->type == attr_type) {
                attr_count++;
                if (attr_count == inst_num) {
                    break;
                }
            }
        }
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s Delete X-cpar/cpar attribute (%s) cap_num %u, "
                          "instance %d not found.", sdp_p->debug_str,
                          sdp_get_attr_name(attr_type), cap_num, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        if (prev_attr_p == NULL) {
            cap_p->media_attrs_p = attr_p->next_p;
        } else {
            prev_attr_p->next_p = attr_p->next_p;
        }
        sdp_free_attr(attr_p);
    }

    return (SDP_SUCCESS);
}


/* Function:    sdp_delete_all_attrs
 * Description: Delete all attributes at the specified level from 
 *              the SDP structure.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 * Returns:     SDP_SUCCESS            Attributes were deleted successfully.
 */
sdp_result_e sdp_delete_all_attrs (void *sdp_ptr, u16 level, u8 cap_num)
{
    sdp_mca_t   *mca_p;
    sdp_mca_t   *cap_p;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_attr_t  *next_attr_p = NULL;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (cap_num == 0) {
        if (level == SDP_SESSION_LEVEL) {
            attr_p = sdp_p->sess_attrs_p;
            while (attr_p != NULL) {
                next_attr_p = attr_p->next_p;
                sdp_free_attr(attr_p);
                attr_p = next_attr_p;
            }
            sdp_p->sess_attrs_p = NULL;
        } else {  /* Attr is at a media level */
            mca_p = sdp_find_media_level(sdp_p, level);
            if (mca_p == NULL) {
                sdp_p->conf_p->num_invalid_param++;
                return (SDP_INVALID_PARAMETER);
            }
            attr_p = mca_p->media_attrs_p;
            while (attr_p != NULL) {
                next_attr_p = attr_p->next_p;
                sdp_free_attr(attr_p);
                attr_p = next_attr_p;
            }
            mca_p->media_attrs_p = NULL;
        }
    } else {
        /* Attr is a capability X-cpar/cpar attribute, find the capability. */
        attr_p = sdp_find_capability(sdp_p, level, cap_num);
        if (attr_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        cap_p = attr_p->attr.cap_p;
        /* Now find the specific attribute to delete. */
        attr_p = cap_p->media_attrs_p;
        while (attr_p != NULL) {
            next_attr_p = attr_p->next_p;
            sdp_free_attr(attr_p);
            attr_p = next_attr_p;
        }
        cap_p->media_attrs_p = NULL;
    }

    return (SDP_SUCCESS);
}


/* Function:    sdp_find_attr_list
 * Description: Find the attribute list for the specified level and cap_num.
 *              Note: This is not an API for the application but an internal
 *              routine used by the SDP library.
 * Parameters:  sdp_p       Pointer to the SDP to search.
 *              level       The level to check for the attribute list.
 *              cap_num     The capability number associated with the
 *                          attribute list.  If none, should be zero.
 *              attr_p      Pointer to the attr list pointer. Will be
 *                          filled in on return if successful.
 *              fname       String function name calling this routine.
 *                          Use for printing debug.
 * Returns:     SDP_SUCCESS 
 *              SDP_INVALID_MEDIA_LEVEL 
 *              SDP_INVALID_CAPABILITY 
 *              SDP_FAILURE
 */
sdp_result_e sdp_find_attr_list (sdp_t *sdp_p, u16 level, u8 cap_num, 
                                 sdp_attr_t **attr_p, char *fname)
{
    sdp_mca_t   *mca_p;
    sdp_mca_t   *cap_p;
    sdp_attr_t  *cap_attr_p;

    /* Initialize the attr pointer. */
    *attr_p = NULL;

    if (cap_num == 0) {
        /* Find attribute list at the specified level. */
        if (level == SDP_SESSION_LEVEL) {
            *attr_p = sdp_p->sess_attrs_p;
        } else {
            mca_p = sdp_find_media_level(sdp_p, level);
            if (mca_p == NULL) {
                sdp_p->conf_p->num_invalid_param++;
                return (SDP_INVALID_PARAMETER);
            }
            *attr_p = mca_p->media_attrs_p;
        }
    } else {
        /* Find the attr list for the capability specified. */
        cap_attr_p = sdp_find_capability(sdp_p, level, cap_num);
        if (cap_attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s %s, invalid capability %u at "
                          "level %u specified.", sdp_p->debug_str, fname,
                          cap_num, level);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_CAPABILITY);
        }
        cap_p = cap_attr_p->attr.cap_p;
        *attr_p = cap_p->media_attrs_p;
    }

    return (SDP_SUCCESS);
}

/* Function:    sdp_find_attr
 * Description: Find the specified attribute in an SDP structure.  
 *              Note: This is not an API for the application but an internal
 *              routine used by the SDP library.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              attr_type   The type of attribute to find.
 *              inst_num    The instance num of the attribute to delete.
 *                          Range should be (1 - max num insts of this 
 *                          particular type of attribute at this level).
 * Returns:     Pointer to the attribute or NULL if not found.
 */
sdp_attr_t *sdp_find_attr (sdp_t *sdp_p, u16 level, u8 cap_num,
                           sdp_attr_e attr_type, u16 inst_num)
{
    u16          attr_count=0;
    sdp_mca_t   *mca_p;
    sdp_mca_t   *cap_p;
    sdp_attr_t  *attr_p;

    if (inst_num < 1) {
        return (NULL);
    }

    if (cap_num == 0) {
        if (level == SDP_SESSION_LEVEL) {
            for (attr_p = sdp_p->sess_attrs_p; attr_p != NULL;
                 attr_p = attr_p->next_p) {
                if (attr_p->type == attr_type) {
                    attr_count++;
                    if (attr_count == inst_num) {
                        return (attr_p);
                    }
                }
            }
        } else {  /* Attr is at a media level */
            mca_p = sdp_find_media_level(sdp_p, level);
            if (mca_p == NULL) {
                return (NULL);
            }
            for (attr_p = mca_p->media_attrs_p; attr_p != NULL;
                 attr_p = attr_p->next_p) {
                if (attr_p->type == attr_type) {
                    attr_count++;
                    if (attr_count == inst_num) {
                        return (attr_p);
                    }
                }
            }
        }  /* Attr is at a media level */
    } else {
        /* Attr is a capability X-cpar/cpar attribute. */
        attr_p = sdp_find_capability(sdp_p, level, cap_num);
        if (attr_p == NULL) {
            return (NULL);
        }
        cap_p = attr_p->attr.cap_p;
        /* Now find the specific attribute. */
        for (attr_p = cap_p->media_attrs_p; attr_p != NULL;
             attr_p = attr_p->next_p) {
            if (attr_p->type == attr_type) {
                attr_count++;
                if (attr_count == inst_num) {
                    return (attr_p);
                }
            }
        }
    }

    return (NULL);
}

/* Function:    sdp_find_capability
 * Description: Find the specified capability attribute in an SDP structure.  
 *              Note: This is not an API for the application but an internal
 *              routine used by the SDP library.
 * Parameters:  sdp_p       The SDP handle.
 *              level       The level to check for the capability.  
 *              cap_num     The capability number to locate.
 * Returns:     Pointer to the capability attribute or NULL if not found.
 */
sdp_attr_t *sdp_find_capability (sdp_t *sdp_p, u16 level, u8 cap_num)
{
    u8           cur_cap_num=0;
    sdp_mca_t   *mca_p;
    sdp_mca_t   *cap_p;
    sdp_attr_t  *attr_p;

    if (level == SDP_SESSION_LEVEL) {
        for (attr_p = sdp_p->sess_attrs_p; attr_p != NULL;
             attr_p = attr_p->next_p) {
            if ((attr_p->type == SDP_ATTR_X_CAP) || 
	        (attr_p->type == SDP_ATTR_CDSC)) {
                cap_p = attr_p->attr.cap_p;
                cur_cap_num += cap_p->num_payloads;
                if (cap_num <= cur_cap_num) {
                    /* This is the right capability */
                    return (attr_p);
                }
            }
        }
    } else {  /* Capability is at a media level */
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (NULL);
        }
        for (attr_p = mca_p->media_attrs_p; attr_p != NULL;
             attr_p = attr_p->next_p) {
            if ((attr_p->type == SDP_ATTR_X_CAP) || 
	        (attr_p->type == SDP_ATTR_CDSC)) {
                cap_p = attr_p->attr.cap_p;
                cur_cap_num += cap_p->num_payloads;
                if (cap_num <= cur_cap_num) {
                    /* This is the right capability */
                    return (attr_p);
                }
            }
        }
    }

    /* We didn't find the specified capability. */
    if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
        SDP_ERROR("%s Unable to find specified capability (level %u, "
                  "cap_num %u).", sdp_p->debug_str, level, cap_num);
    }
    sdp_p->conf_p->num_invalid_param++;
    return (NULL);
}

/* Function:    sdp_attr_valid(void *sdp_ptr)
 * Description: Returns true or false depending on whether the specified
 *              instance of the given attribute has been defined at the 
 *              given level.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              attr_type   The attribute type to validate.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_valid (void *sdp_ptr, sdp_attr_e attr_type, u16 level, 
                         u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if (sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num) == NULL) {
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/* Function:    sdp_attr_get_simple_string
 * Description: Returns a pointer to a string attribute parameter.  This
 *              routine can only be called for attributes that have just
 *              one string parameter.  The value is returned as a const
 *              ptr and so cannot be modified by the application.  If the
 *              given attribute is not defined, NULL will be returned.
 *              Attributes with a simple string parameter currently include:
 *              bearer, called, connection_type, dialed, dialing, direction
 *              and framing.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              attr_type   The simple string attribute type.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Pointer to the parameter value.
 */
const char *sdp_attr_get_simple_string (void *sdp_ptr, sdp_attr_e attr_type,
                                        u16 level, u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    if ((attr_type != SDP_ATTR_BEARER) && 
        (attr_type != SDP_ATTR_CALLED) &&
        (attr_type != SDP_ATTR_CONN_TYPE) && 
        (attr_type != SDP_ATTR_DIALED) &&
        (attr_type != SDP_ATTR_DIALING) &&
        (attr_type != SDP_ATTR_FRAMING) &&
        (attr_type != SDP_ATTR_X_SIDIN) &&
        (attr_type != SDP_ATTR_X_SIDOUT)&&
        (attr_type != SDP_ATTR_X_CONFID) &&
        (attr_type != SDP_ATTR_LABEL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute type is not a simple string (%s)",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type), 
                      level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        return (attr_p->attr.string_val);
    }
}

/* Function:    sdp_attr_set_simple_string
 * Description: Sets the value of a string attribute parameter.  This
 *              routine can only be called for attributes that have just
 *              one string parameter.  The string is copied into the SDP
 *              structure so application memory will not be referenced by
 *              the SDP library. 
 *              Attributes with a simple string parameter currently include:
 *              bearer, called, connection_type, dialed, dialing, and
 *              framing.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              attr_type   The simple string attribute type.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              string_parm Ptr to new string value.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_simple_string (void *sdp_ptr, sdp_attr_e attr_type,
                                         u16 level, u8 cap_num,
                                         u16 inst_num, const char *string_parm)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if ((attr_type != SDP_ATTR_BEARER) && 
        (attr_type != SDP_ATTR_CALLED) &&
        (attr_type != SDP_ATTR_CONN_TYPE) && 
        (attr_type != SDP_ATTR_DIALED) &&
        (attr_type != SDP_ATTR_DIALING) &&
        (attr_type != SDP_ATTR_FRAMING) &&
        (attr_type != SDP_ATTR_X_SIDIN) &&
        (attr_type != SDP_ATTR_X_SIDOUT)&&
        (attr_type != SDP_ATTR_X_CONFID) &&
        (attr_type != SDP_ATTR_LABEL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute type is not a simple string (%s)",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type),
                      level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        sstrncpy(attr_p->attr.string_val, string_parm, 
                 sizeof(attr_p->attr.string_val));
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_simple_u32
 * Description: Returns an unsigned 32-bit attribute parameter.  This
 *              routine can only be called for attributes that have just
 *              one u32 parameter.  If the given attribute is not defined,
 *              zero will be returned.  There is no way for the application
 *              to determine if zero is the actual value or the attribute
 *              wasn't defined, so the application must use the 
 *              sdp_attr_valid function to determine this.
 *              Attributes with a simple u32 parameter currently include:
 *              eecid, ptime, T38FaxVersion, T38maxBitRate, T38FaxMaxBuffer,
 *              T38FaxMaxDatagram, X-sqn, TC1PayloadBytes, TC1WindowSize,
 *              TC2PayloadBytes, TC2WindowSize, rtcp.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              attr_type   The simple u32 attribute type.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     u32 parameter value.
 */
u32 sdp_attr_get_simple_u32 (void *sdp_ptr, sdp_attr_e attr_type, u16 level,
                             u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    if ((attr_type != SDP_ATTR_EECID) && 
        (attr_type != SDP_ATTR_PTIME) &&
        (attr_type != SDP_ATTR_T38_VERSION) && 
        (attr_type != SDP_ATTR_T38_MAXBITRATE) &&
        (attr_type != SDP_ATTR_T38_MAXBUFFER) &&
        (attr_type != SDP_ATTR_T38_MAXDGRAM) &&
        (attr_type != SDP_ATTR_X_SQN) &&
        (attr_type != SDP_ATTR_TC1_PAYLOAD_BYTES) &&
        (attr_type != SDP_ATTR_TC1_WINDOW_SIZE) &&
        (attr_type != SDP_ATTR_TC2_PAYLOAD_BYTES) &&
        (attr_type != SDP_ATTR_TC2_WINDOW_SIZE) &&
        (attr_type != SDP_ATTR_RTCP) &&
        (attr_type != SDP_ATTR_MID) &&
        (attr_type != SDP_ATTR_FRAMERATE)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute type is not a simple u32 (%s)",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type),
                      level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.u32_val);
    }
}

/* Function:    sdp_attr_set_simple_u32
 * Description: Sets the value of an unsigned 32-bit attribute parameter.  
 *              This routine can only be called for attributes that have just
 *              one u32 parameter.
 *              Attributes with a simple u32 parameter currently include:
 *              eecid, ptime, T38FaxVersion, T38maxBitRate, T38FaxMaxBuffer,
 *              T38FaxMaxDatagram, X-sqn, TC1PayloadBytes, TC1WindowSize,
 *              TC2PayloadBytes, TC2WindowSize, rtcp.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              attr_type   The simple u32 attribute type.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              num_parm    New u32 parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_simple_u32 (void *sdp_ptr, sdp_attr_e attr_type, 
                           u16 level, u8 cap_num, u16 inst_num, u32 num_parm)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if ((attr_type != SDP_ATTR_EECID) && 
        (attr_type != SDP_ATTR_PTIME) &&
        (attr_type != SDP_ATTR_T38_VERSION) && 
        (attr_type != SDP_ATTR_T38_MAXBITRATE) &&
        (attr_type != SDP_ATTR_T38_MAXBUFFER) &&
        (attr_type != SDP_ATTR_T38_MAXDGRAM) &&
        (attr_type != SDP_ATTR_X_SQN) &&
        (attr_type != SDP_ATTR_TC1_PAYLOAD_BYTES) &&
        (attr_type != SDP_ATTR_TC1_WINDOW_SIZE) &&
        (attr_type != SDP_ATTR_TC2_PAYLOAD_BYTES) &&
        (attr_type != SDP_ATTR_TC2_WINDOW_SIZE) &&
        (attr_type != SDP_ATTR_RTCP) &&
        (attr_type != SDP_ATTR_MID) &&
        (attr_type != SDP_ATTR_FRAMERATE)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute type is not a simple u32 (%s)",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type),
                      level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.u32_val = num_parm;
        return (SDP_SUCCESS);
    }
}


/* Function:    sdp_attr_get_simple_boolean
 * Description: Returns a boolean attribute parameter.  This
 *              routine can only be called for attributes that have just
 *              one boolean parameter.  If the given attribute is not defined,
 *              FALSE will be returned.  There is no way for the application
 *              to determine if FALSE is the actual value or the attribute
 *              wasn't defined, so the application must use the 
 *              sdp_attr_valid function to determine this.
 *              Attributes with a simple boolean parameter currently include:
 *              T38FaxFillBitRemoval, T38FaxTranscodingMMR, 
 *              T38FaxTranscodingJBIG, and TMRGwXid.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              attr_type   The simple boolean attribute type.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Boolean value.
 */
tinybool sdp_attr_get_simple_boolean (void *sdp_ptr, sdp_attr_e attr_type,
                                      u16 level, u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if ((attr_type != SDP_ATTR_T38_FILLBITREMOVAL) && 
        (attr_type != SDP_ATTR_T38_TRANSCODINGMMR) &&
        (attr_type != SDP_ATTR_T38_TRANSCODINGJBIG) &&
        (attr_type != SDP_ATTR_TMRGWXID)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute type is not a simple boolean (%s)",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type),
                      level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.boolean_val);
    }
}

/* Function:    sdp_attr_set_simple_boolean
 * Description: Sets the value of a boolean attribute parameter.  
 *              This routine can only be called for attributes that have just
 *              one boolean parameter.
 *              Attributes with a simple boolean parameter currently include:
 *              T38FaxFillBitRemoval, T38FaxTranscodingMMR, 
 *              T38FaxTranscodingJBIG, and TMRGwXid.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              attr_type   The simple boolean attribute type.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              bool_parm   New boolean parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_simple_boolean (void *sdp_ptr, sdp_attr_e attr_type,
                                          u16 level, u8 cap_num,
                                          u16 inst_num, u32 bool_parm)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if ((attr_type != SDP_ATTR_T38_FILLBITREMOVAL) && 
        (attr_type != SDP_ATTR_T38_TRANSCODINGMMR) &&
        (attr_type != SDP_ATTR_T38_TRANSCODINGJBIG) &&
        (attr_type != SDP_ATTR_TMRGWXID)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute type is not a simple boolean (%s)",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type),
                      level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.boolean_val = (tinybool)bool_parm;
        return (SDP_SUCCESS);
    }
}

/*
 * sdp_attr_get_maxprate
 *
 * This function is used by the application layer to get the packet-rate
 * within the maxprate attribute.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to set.
 *
 * Returns a pointer to a constant char array that stores the packet-rate, 
 * OR null if the attribute does not exist.
 */
const char*
sdp_attr_get_maxprate (void *sdp_ptr, u16 level, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_MAXPRATE, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(SDP_ATTR_MAXPRATE), 
                      level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        return (attr_p->attr.string_val);
    }
}

/*
 * sdp_attr_set_maxprate
 *
 * This function is used by the application layer to set the packet rate of
 * the maxprate attribute line appropriately. The maxprate attribute is
 * defined as follows:
 *
 *    max-p-rate-def = "a" "=" "maxprate" ":" packet-rate CRLF
 *    packet-rate = 1*DIGIT ["." 1*DIGIT]
 * 
 * 
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to set.
 *              string_parm A string that contains the value of packet-rate 
 *                          that needs to be advertised.
 *
 * Note that we use a string to set the packet-rate, so the application layer
 * must format a string, as per the packet-rate format and pass it to this
 * function.
 *
 * The decision to use a string to communicate the packet-rate was
 * made because some operating systems do not support floating point
 * operations.
 *
 * Returns:
 * SDP_INVALID_SDP_PTR - If an invalid sdp handle is passed in.
 * SDP_INVALID_PARAMETER - If the maxprate attribute is not defined at the
 *                         specified level OR if the packet-rate is not
 *                         formatted correctly in string_parm.
 * SDP_SUCCESS - If we are successfully able to set the maxprate attribute.
 */
sdp_result_e
sdp_attr_set_maxprate (void *sdp_ptr, u16 level, u16 inst_num, 
                       const char *string_parm)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_MAXPRATE, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(SDP_ATTR_MAXPRATE),
                      level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (!sdp_validate_maxprate(string_parm)) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s is not a valid maxprate value.", string_parm);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }

        sstrncpy(attr_p->attr.string_val, string_parm, 
                 sizeof(attr_p->attr.string_val));
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_t38ratemgmt
 * Description: Returns the value of the t38ratemgmt attribute 
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_T38_UNKNOWN_RATE is returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Ratemgmt value.
 */
sdp_t38_ratemgmt_e sdp_attr_get_t38ratemgmt (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_T38_UNKNOWN_RATE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_T38_RATEMGMT, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s t38ratemgmt attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_T38_UNKNOWN_RATE);
    } else {
        return (attr_p->attr.t38ratemgmt);
    }
}

/* Function:    sdp_attr_set_t38ratemgmt
 * Description: Sets the value of the t38ratemgmt attribute parameter
 *              for the given attribute. 
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              t38ratemgmt New t38ratemgmt parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_t38ratemgmt (void *sdp_ptr, u16 level, 
                                       u8 cap_num, u16 inst_num,
                                       sdp_t38_ratemgmt_e t38ratemgmt)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_T38_RATEMGMT, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s t38ratemgmt attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.t38ratemgmt = t38ratemgmt;
        return (SDP_SUCCESS);
    }
}


/* Function:    sdp_attr_get_t38udpec
 * Description: Returns the value of the t38udpec attribute 
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_T38_UDPEC_UNKNOWN is returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     UDP EC value.
 */
sdp_t38_udpec_e sdp_attr_get_t38udpec (void *sdp_ptr, u16 level,
                                       u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_T38_UDPEC_UNKNOWN);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_T38_UDPEC, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s t38udpec attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_T38_UDPEC_UNKNOWN);
    } else {
        return (attr_p->attr.t38udpec);
    }
}

/* Function:    sdp_attr_set_t38udpec
 * Description: Sets the value of the t38ratemgmt attribute parameter
 *              for the given attribute. 
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              t38udpec    New t38udpec parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_t38udpec (void *sdp_ptr, u16 level, 
                                    u8 cap_num, u16 inst_num,
                                    sdp_t38_udpec_e t38udpec)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_T38_UDPEC, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s t38udpec attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.t38udpec = t38udpec;
        return (SDP_SUCCESS);
    }
}


/* Function:    sdp_get_media_direction
 * Description: Determines the direction defined for a given level.  The
 *              direction will be inactive, sendonly, recvonly, or sendrecv
 *              as determined by the last of these attributes specified at
 *              the given level.  If none of these attributes are specified,
 *              the direction will be sendrecv by default.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 * Returns:     An SDP direction enum value.
 */
sdp_direction_e sdp_get_media_direction (void *sdp_ptr, u16 level,
                                         u8 cap_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t   *mca_p;
    sdp_attr_t  *attr_p;
    sdp_direction_e direction = SDP_DIRECTION_SENDRECV;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (direction);
    }

    if (cap_num == 0) {
        /* Find the pointer to the attr list for this level. */
        if (level == SDP_SESSION_LEVEL) {
            attr_p = sdp_p->sess_attrs_p;
        } else {  /* Attr is at a media level */
            mca_p = sdp_find_media_level(sdp_p, level);
            if (mca_p == NULL) {
                return (direction);
            }
            attr_p = mca_p->media_attrs_p;
        }

        /* Scan for direction oriented attributes.  Last one wins. */
        for (; attr_p != NULL; attr_p = attr_p->next_p) {
            if (attr_p->type == SDP_ATTR_INACTIVE) {
                direction = SDP_DIRECTION_INACTIVE;
            } else if (attr_p->type == SDP_ATTR_SENDONLY) {
                direction = SDP_DIRECTION_SENDONLY;
            } else if (attr_p->type == SDP_ATTR_RECVONLY) {
                direction = SDP_DIRECTION_RECVONLY;
            } else if (attr_p->type == SDP_ATTR_SENDRECV) {
                direction = SDP_DIRECTION_SENDRECV;
            }
        }
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid cap_num for media direction.",
                     sdp_p->debug_str);
        }
    }

    return (direction);
}

/*
 * sdp_delete_all_media_direction_attrs
 * 
 * Deletes all the media direction attributes from a given SDP level. 
 * Media direction attributes include:
 * a=sendonly
 * a=recvonly
 * a=sendrecv
 * a=inactive
 *
 * This function can be used in conjunction with sdp_add_new_attr, to ensure 
 * that there is only one direction attribute per level. 
 * It can also be used to delete all attributes when the client wants to 
 * advertise the default direction, i.e. a=sendrecv.
 */
sdp_result_e sdp_delete_all_media_direction_attrs (void *sdp_ptr, u16 level) 
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_mca_t   *mca_p;
    sdp_attr_t  *attr_p;
    sdp_attr_t  *prev_attr_p = NULL;
    sdp_attr_t  *tmp_attr_p = NULL;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    /* Find the pointer to the attr list for this level. */
    if (level == SDP_SESSION_LEVEL) {
        attr_p = sdp_p->sess_attrs_p;
        while (attr_p != NULL) {
            if ((attr_p->type == SDP_ATTR_INACTIVE) || 
                (attr_p->type == SDP_ATTR_SENDONLY) ||
                (attr_p->type == SDP_ATTR_RECVONLY) ||
                (attr_p->type == SDP_ATTR_SENDRECV)) {
                
                tmp_attr_p = attr_p;

                if (prev_attr_p == NULL) {
                    sdp_p->sess_attrs_p = attr_p->next_p;
                } else {
                    prev_attr_p->next_p = attr_p->next_p;
                }
                attr_p = attr_p->next_p;

                sdp_free_attr(tmp_attr_p);                
            } else {
                prev_attr_p = attr_p;
                attr_p = attr_p->next_p;
            }
        }
    } else {  /* Attr is at a media level */
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            return (SDP_INVALID_MEDIA_LEVEL);
        }

        attr_p = mca_p->media_attrs_p;
        while (attr_p != NULL) {
            if ((attr_p->type == SDP_ATTR_INACTIVE) || 
                (attr_p->type == SDP_ATTR_SENDONLY) ||
                (attr_p->type == SDP_ATTR_RECVONLY) ||
                (attr_p->type == SDP_ATTR_SENDRECV)) {

                tmp_attr_p = attr_p;

                if (prev_attr_p == NULL) {
                    mca_p->media_attrs_p = attr_p->next_p;
                } else {
                    prev_attr_p->next_p = attr_p->next_p;
                }
                attr_p = attr_p->next_p;

                sdp_free_attr(tmp_attr_p);
            } else {
                prev_attr_p = attr_p;
                attr_p = attr_p->next_p;
            }
        }
    }

    return (SDP_SUCCESS);
}

/* Since there are four different attribute names which all have the same
 * qos parameters, all of these attributes are accessed through this same
 * set of APIs.  To distinguish between specific attributes, the application
 * must also pass the attribute type.  The attribute must be one of:
 * SDP_ATTR_QOS, SDP_ATTR_SECURE, SDP_ATTR_X_PC_QOS, and SDP_ATTR_X_QOS.
 */
tinybool sdp_validate_qos_attr (sdp_attr_e qos_attr)
{
    if ((qos_attr == SDP_ATTR_QOS) ||
        (qos_attr == SDP_ATTR_SECURE) ||
        (qos_attr == SDP_ATTR_X_PC_QOS) ||
        (qos_attr == SDP_ATTR_X_QOS) ||
        (qos_attr == SDP_ATTR_CURR) ||
        (qos_attr == SDP_ATTR_DES) ||
        (qos_attr == SDP_ATTR_CONF)){
        return (TRUE);
    } else {
        return (FALSE);
    }
}

/* Function:    sdp_attr_get_qos_strength
 * Description: Returns the value of the qos attribute strength
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_QOS_STRENGTH_UNKNOWN is 
 *              returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     Qos strength value.
 */
sdp_qos_strength_e sdp_attr_get_qos_strength (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_QOS_STRENGTH_UNKNOWN);
    }

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid QOS attribute specified for"
                     "get qos strength.", sdp_p->debug_str);
        }
        return (SDP_QOS_STRENGTH_UNKNOWN);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, 
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_QOS_STRENGTH_UNKNOWN);
    } else {
        switch (qos_attr) {
            case SDP_ATTR_QOS:
                return (attr_p->attr.qos.strength);
            case SDP_ATTR_DES:
                return (attr_p->attr.des.strength);
            default:
                return SDP_QOS_STRENGTH_UNKNOWN;
        
        }
    }
}

/* Function:    sdp_attr_get_qos_direction
 * Description: Returns the value of the qos attribute direction
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_QOS_DIR_UNKNOWN is 
 *              returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     Qos direction value.
 */
sdp_qos_dir_e sdp_attr_get_qos_direction (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_QOS_DIR_UNKNOWN);
    }

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid QOS attribute specified "
                     "for get qos direction.", sdp_p->debug_str);
        }
        return (SDP_QOS_DIR_UNKNOWN);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_QOS_DIR_UNKNOWN);
    } else {
         switch (qos_attr) {
            case SDP_ATTR_QOS:
                 return (attr_p->attr.qos.direction);
            case SDP_ATTR_CURR:
                 return (attr_p->attr.curr.direction);
            case SDP_ATTR_DES:
                 return (attr_p->attr.des.direction);
            case SDP_ATTR_CONF:
                 return (attr_p->attr.conf.direction);
            default:
                return SDP_QOS_DIR_UNKNOWN;
        
        }
    }
}

/* Function:    sdp_attr_get_qos_status_type
 * Description: Returns the value of the qos attribute status_type
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_QOS_STATUS_TYPE_UNKNOWN is 
 *              returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     Qos direction value.
 */
sdp_qos_status_types_e sdp_attr_get_qos_status_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_QOS_STATUS_TYPE_UNKNOWN);
    }

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid QOS attribute specified "
                     "for get qos status_type.", sdp_p->debug_str);
        }
        return (SDP_QOS_STATUS_TYPE_UNKNOWN);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_QOS_STATUS_TYPE_UNKNOWN);
    } else {
        switch (qos_attr) {
            case SDP_ATTR_CURR:
                return (attr_p->attr.curr.status_type);
            case SDP_ATTR_DES:
                return (attr_p->attr.des.status_type);
            case SDP_ATTR_CONF:
                return (attr_p->attr.conf.status_type);
            default:
                return SDP_QOS_STATUS_TYPE_UNKNOWN;
        
        }
    }
}

/* Function:    sdp_attr_get_qos_confirm
 * Description: Returns the value of the qos attribute confirm
 *              parameter specified for the given attribute.  Returns TRUE if
 *              the confirm parameter is specified.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     Boolean value.
 */
tinybool sdp_attr_get_qos_confirm (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid QOS attribute specified "
                     "for get qos confirm.", sdp_p->debug_str);
        }
        return (FALSE);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, 
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.qos.confirm);
    }
}

/* Function:    sdp_attr_set_qos_strength
 * Description: Sets the qos strength value for the specified qos attribute. 
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 *              strength    New qos strength parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_qos_strength (void *sdp_ptr, u16 level, u8 cap_num, 
                                        sdp_attr_e qos_attr, u16 inst_num,
                                        sdp_qos_strength_e strength)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid QOS attribute specified "
                     "for set qos strength.", sdp_p->debug_str);
        }
        return (SDP_FAILURE);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        switch (qos_attr) {
            case SDP_ATTR_QOS:
                attr_p->attr.qos.strength = strength;
                return (SDP_SUCCESS);
            case SDP_ATTR_DES:
                attr_p->attr.des.strength = strength;
                return (SDP_SUCCESS);
            default:
                return (SDP_FAILURE);
        
        }
    }
}

/* Function:    sdp_attr_get_curr_type
 * Description: Returns the value of the curr attribute status_type
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_CURR_UNKNOWN_TYPE is 
 *              returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     Curr type value.
 */
sdp_curr_type_e sdp_attr_get_curr_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_CURR_UNKNOWN_TYPE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_CURR_UNKNOWN_TYPE);
    } else {
        return (attr_p->attr.curr.type);
    }
}

/* Function:    sdp_attr_get_des_type
 * Description: Returns the value of the des attribute status_type
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_DES_UNKNOWN_TYPE is 
 *              returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     DES type value.
 */
sdp_des_type_e sdp_attr_get_des_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_DES_UNKNOWN_TYPE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_DES_UNKNOWN_TYPE);
    } else {
        return (attr_p->attr.des.type);
    }
}

/* Function:    sdp_attr_get_conf_type
 * Description: Returns the value of the des attribute status_type
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_CONF_UNKNOWN_TYPE is 
 *              returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     CONF type value.
 */
sdp_conf_type_e sdp_attr_get_conf_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_CONF_UNKNOWN_TYPE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_CONF_UNKNOWN_TYPE);
    } else {
        return (attr_p->attr.conf.type);
    }
}

/* Function:    sdp_attr_set_curr_type
 * Description: Returns the value of the curr attribute status_type
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_CURR_UNKNOWN_TYPE is 
 *              returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_curr_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num,
                                sdp_curr_type_e curr_type)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid curr attribute specified "
                     "for set curr type.", sdp_p->debug_str);
        }
        return (SDP_FAILURE);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, 
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.curr.type = curr_type;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_des_type
 * Description: Sets the value of the des attribute type
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_DES_UNKNOWN_TYPE is 
 *              returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_des_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num,
                                sdp_des_type_e des_type)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid des attribute specified "
                     "for set des type.", sdp_p->debug_str);
        }
        return (SDP_FAILURE);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, 
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.des.type = des_type;
        return (SDP_SUCCESS);
    }
}


/* Function:    sdp_attr_set_conf_type
 * Description: Sets the value of the conf attribute type
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_CONF_UNKNOWN_TYPE is 
 *              returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_conf_type (void *sdp_ptr, u16 level,
                                u8 cap_num, sdp_attr_e qos_attr, u16 inst_num,
                                sdp_conf_type_e conf_type)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid conf attribute specified "
                     "for set conf type.", sdp_p->debug_str);
        }
        return (SDP_FAILURE);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, 
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.conf.type = conf_type;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_qos_direction
 * Description: Sets the qos direction value for the specified qos attribute. 
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 *              direction   New qos direction parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_qos_direction (void *sdp_ptr, u16 level, u8 cap_num, 
                                         sdp_attr_e qos_attr, u16 inst_num,
                                         sdp_qos_dir_e direction)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid QOS attribute specified "
                     "for set qos direction.", sdp_p->debug_str);
        }
        return (SDP_FAILURE);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, 
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        switch (qos_attr) {
            case SDP_ATTR_QOS:
                attr_p->attr.qos.direction = direction;
                return (SDP_SUCCESS);
            case SDP_ATTR_CURR:
                attr_p->attr.curr.direction = direction;
                return (SDP_SUCCESS);
            case SDP_ATTR_DES:
                 attr_p->attr.des.direction = direction;
                return (SDP_SUCCESS);
            case SDP_ATTR_CONF:
                 attr_p->attr.conf.direction = direction;
                return (SDP_SUCCESS);
            default:
                return (SDP_FAILURE);
        
        }
    }
}

/* Function:    sdp_attr_set_qos_status_type
 * Description: Sets the qos status_type value for the specified qos attribute. 
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 *              direction   New qos direction parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_qos_status_type (void *sdp_ptr, u16 level, u8 cap_num, 
                                         sdp_attr_e qos_attr, u16 inst_num,
                                         sdp_qos_status_types_e status_type)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid QOS attribute specified "
                     "for set qos status_type.", sdp_p->debug_str);
        }
        return (SDP_FAILURE);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, 
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        switch (qos_attr) {
            case SDP_ATTR_CURR:
                attr_p->attr.curr.status_type = status_type;
                return (SDP_SUCCESS);
            case SDP_ATTR_DES:
                attr_p->attr.des.status_type = status_type;
                return (SDP_SUCCESS);
            case SDP_ATTR_CONF:
                attr_p->attr.conf.status_type = status_type;
                return (SDP_SUCCESS);
            default:
                return (SDP_FAILURE);
        
        }
    }
}

/* Function:    sdp_attr_set_qos_confirm
 * Description: Sets the qos confirm value for the specified qos attribute.
 *              If this parameter is TRUE, the confirm parameter will be
 *              specified when the SDP description is built.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 *              confirm     New qos confirm parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_qos_confirm (void *sdp_ptr, u16 level, u8 cap_num, 
                                       sdp_attr_e qos_attr, u16 inst_num,
                                       tinybool qos_confirm)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            SDP_WARN("%s Warning: Invalid QOS attribute specified "
                     "for set qos confirm.", sdp_p->debug_str);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.qos.confirm = qos_confirm;
        return (SDP_SUCCESS);
    }
}


/* Function:    sdp_attr_get_subnet_nettype
 * Description: Returns the value of the subnet attribute network type 
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_NT_INVALID is returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Nettype value.
 */
sdp_nettype_e sdp_attr_get_subnet_nettype (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_NT_INVALID);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_NT_INVALID);
    } else {
        return (attr_p->attr.subnet.nettype);
    }
}

/* Function:    sdp_attr_get_subnet_addrtype
 * Description: Returns the value of the subnet attribute address type 
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_AT_INVALID is returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Addrtype value.
 */
sdp_addrtype_e sdp_attr_get_subnet_addrtype (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_AT_INVALID);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_AT_INVALID);
    } else {
        return (attr_p->attr.subnet.addrtype);
    }
}

/* Function:    sdp_attr_get_subnet_addr
 * Description: Returns the value of the subnet attribute address 
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, NULL is returned.  Value is
 *              returned as a const ptr and so cannot be modified by the
 *              application.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Pointer to address or NULL.
 */
const char *sdp_attr_get_subnet_addr (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        return (attr_p->attr.subnet.addr);
    }
}

/* Function:    sdp_attr_get_subnet_prefix
 * Description: Returns the value of the subnet attribute prefix 
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_INVALID_PARAM is returned.
 *              Note that this is value is defined to be (-2) and is 
 *              different from the return code SDP_INVALID_PARAMETER.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Prefix value or SDP_INVALID_PARAM.
 */
int32 sdp_attr_get_subnet_prefix (void *sdp_ptr, u16 level,
                                  u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.subnet.prefix);
    }
}

/* Function:    sdp_attr_set_subnet_nettype
 * Description: Sets the value of the subnet attribute nettype parameter
 *              for the given attribute. 
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              nettype     The network type for the attribute.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_subnet_nettype (void *sdp_ptr, u16 level, 
                                          u8 cap_num, u16 inst_num,
                                          sdp_nettype_e nettype)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.subnet.nettype = nettype;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_subnet_addrtype
 * Description: Sets the value of the subnet attribute addrtype parameter
 *              for the given attribute. 
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              addrtype    The address type for the attribute.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_subnet_addrtype (void *sdp_ptr, u16 level, 
                                           u8 cap_num, u16 inst_num,
                                           sdp_addrtype_e sdp_addrtype)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.subnet.addrtype = sdp_addrtype;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_subnet_addr
 * Description: Sets the value of the subnet attribute addr parameter
 *              for the given attribute. The address is copied into the 
 *              SDP structure so application memory will not be 
 *              referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              addr        Ptr to the address string for the attribute.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_subnet_addr (void *sdp_ptr, u16 level, 
                                       u8 cap_num, u16 inst_num,
                                       const char *addr)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        sstrncpy(attr_p->attr.subnet.addr, addr, 
                 sizeof(attr_p->attr.subnet.addr)) ;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_subnet_prefix
 * Description: Sets the value of the subnet attribute prefix parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              prefix      Prefix value for the attribute.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_subnet_prefix (void *sdp_ptr, u16 level, 
                                         u8 cap_num, u16 inst_num,
                                         int32 prefix)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.subnet.prefix = prefix;
        return (SDP_SUCCESS);
    }
}


/* Function:    sdp_attr_rtpmap_payload_valid
 * Description: Returns true or false depending on whether an rtpmap 
 *              attribute was specified with the given payload value
 *              at the given level.  If it was, the instance number of
 *              that attribute is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number of the attribute
 *                          found is returned via this param.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_rtpmap_payload_valid (void *sdp_ptr, u16 level, u8 cap_num, 
                                        u16 *inst_num, u16 payload_type)
{
    u16          i;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    u16          num_instances;

    *inst_num = 0;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if (sdp_attr_num_instances(sdp_ptr, level, cap_num, 
                          SDP_ATTR_RTPMAP, &num_instances) != SDP_SUCCESS) {
        return (FALSE);
    }

    for (i=1; i <= num_instances; i++) {
        attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, i);
        if ((attr_p != NULL) && 
            (attr_p->attr.transport_map.payload_num == payload_type)) {
            *inst_num = i;
            return (TRUE);
        }
    }   

    return (FALSE);
}

/* Function:    sdp_attr_get_rtpmap_payload_type
 * Description: Returns the value of the rtpmap attribute payload type
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, zero is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Payload type value.
 */
u16 sdp_attr_get_rtpmap_payload_type (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s rtpmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.transport_map.payload_num);
    }
}

/* Function:    sdp_attr_get_rtpmap_encname
 * Description: Returns a pointer to the value of the encoding name
 *              parameter specified for the given attribute.  Value is
 *              returned as a const ptr and so cannot be modified by the
 *              application.  If the given attribute is not defined, NULL
 *              will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Codec value or SDP_CODEC_INVALID.
 */
const char *sdp_attr_get_rtpmap_encname (void *sdp_ptr, u16 level,
                                         u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s rtpmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        return (attr_p->attr.transport_map.encname);
    }
}

/* Function:    sdp_attr_get_rtpmap_clockrate
 * Description: Returns the value of the rtpmap attribute clockrate
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, zero is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Clockrate value.
 */
u32 sdp_attr_get_rtpmap_clockrate (void *sdp_ptr, u16 level,
                                   u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s rtpmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.transport_map.clockrate);
    }
}

/* Function:    sdp_attr_get_rtpmap_num_chan
 * Description: Returns the value of the rtpmap attribute num_chan
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, zero is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of channels param or zero.
 */
u16 sdp_attr_get_rtpmap_num_chan (void *sdp_ptr, u16 level,
                                  u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s rtpmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.transport_map.num_chan);
    }
}

/* Function:    sdp_attr_set_rtpmap_payload_type
 * Description: Sets the value of the rtpmap attribute payload type parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              payload_num New payload type value.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_rtpmap_payload_type (void *sdp_ptr, u16 level, 
                                               u8 cap_num, u16 inst_num, 
                                               u16 payload_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s rtpmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.transport_map.payload_num = payload_num;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_rtpmap_encname
 * Description: Sets the value of the rtpmap attribute encname parameter
 *              for the given attribute.  String is copied into SDP memory.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              encname     Ptr to string encoding name.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_rtpmap_encname (void *sdp_ptr, u16 level, u8 cap_num,
                                          u16 inst_num, const char *encname)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s rtpmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (encname) {
            sstrncpy(attr_p->attr.transport_map.encname, encname, 
                     sizeof(attr_p->attr.transport_map.encname));
        }
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_rtpmap_clockrate
 * Description: Sets the value of the rtpmap attribute clockrate parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              clockrate   New clockrate value.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_rtpmap_clockrate (void *sdp_ptr, u16 level, 
                                            u8 cap_num, u16 inst_num, 
                                            u32 clockrate)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s rtpmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.transport_map.clockrate = clockrate;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_rtpmap_num_chan
 * Description: Sets the value of the rtpmap attribute num_chan parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              num_chan    New number of channels value.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_rtpmap_num_chan (void *sdp_ptr, u16 level, 
                                           u8 cap_num, u16 inst_num, 
                                           u16 num_chan)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s rtpmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.transport_map.num_chan = num_chan;
        return (SDP_SUCCESS);
    }
}


/* Function:    sdp_attr_sprtmap_payload_valid
 * Description: Returns true or false depending on whether an sprtmap 
 *              attribute was specified with the given payload value
 *              at the given level.  If it was, the instance number of
 *              that attribute is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number of the attribute
 *                          found is returned via this param.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_sprtmap_payload_valid (void *sdp_ptr, u16 level, u8 cap_num, 
                                        u16 *inst_num, u16 payload_type)
{
    u16          i;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    u16          num_instances;

    *inst_num = 0;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if (sdp_attr_num_instances(sdp_ptr, level, cap_num, 
                          SDP_ATTR_SPRTMAP, &num_instances) != SDP_SUCCESS) {
        return (FALSE);
    }

    for (i=1; i <= num_instances; i++) {
        attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, i);
        if ((attr_p != NULL) && 
            (attr_p->attr.transport_map.payload_num == payload_type)) {
            *inst_num = i;
            return (TRUE);
        }
    }   

    return (FALSE);
}

/* Function:    sdp_attr_get_sprtmap_payload_type
 * Description: Returns the value of the sprtmap attribute payload type
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, zero is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Payload type value.
 */
u16 sdp_attr_get_sprtmap_payload_type (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.transport_map.payload_num);
    }
}

/* Function:    sdp_attr_get_sprtmap_encname
 * Description: Returns a pointer to the value of the encoding name
 *              parameter specified for the given attribute.  Value is
 *              returned as a const ptr and so cannot be modified by the
 *              application.  If the given attribute is not defined, NULL
 *              will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Codec value or SDP_CODEC_INVALID.
 */
const char *sdp_attr_get_sprtmap_encname (void *sdp_ptr, u16 level,
                                         u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        return (attr_p->attr.transport_map.encname);
    }
}

/* Function:    sdp_attr_get_sprtmap_clockrate
 * Description: Returns the value of the sprtmap attribute clockrate
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, zero is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Clockrate value.
 */
u32 sdp_attr_get_sprtmap_clockrate (void *sdp_ptr, u16 level,
                                   u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.transport_map.clockrate);
    }
}

/* Function:    sdp_attr_get_sprtmap_num_chan
 * Description: Returns the value of the sprtmap attribute num_chan
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, zero is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of channels param or zero.
 */
u16 sdp_attr_get_sprtmap_num_chan (void *sdp_ptr, u16 level,
                                  u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.transport_map.num_chan);
    }
}

/* Function:    sdp_attr_set_sprtmap_payload_type
 * Description: Sets the value of the sprtmap attribute payload type parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              payload_num New payload type value.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_sprtmap_payload_type (void *sdp_ptr, u16 level, 
                                               u8 cap_num, u16 inst_num, 
                                               u16 payload_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.transport_map.payload_num = payload_num;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_sprtmap_encname
 * Description: Sets the value of the sprtmap attribute encname parameter
 *              for the given attribute.  String is copied into SDP memory.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              encname     Ptr to string encoding name.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_sprtmap_encname (void *sdp_ptr, u16 level, u8 cap_num,
                                          u16 inst_num, const char *encname)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        sstrncpy(attr_p->attr.transport_map.encname, encname, 
                 sizeof(attr_p->attr.transport_map.encname));
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_sprtmap_clockrate
 * Description: Sets the value of the sprtmap attribute clockrate parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              clockrate   New clockrate value.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_sprtmap_clockrate (void *sdp_ptr, u16 level, 
                                            u8 cap_num, u16 inst_num, 
                                            u16 clockrate)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.transport_map.clockrate = clockrate;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_sprtmap_num_chan
 * Description: Sets the value of the sprtmap attribute num_chan parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              num_chan    New number of channels value.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_sprtmap_num_chan (void *sdp_ptr, u16 level, 
                                           u8 cap_num, u16 inst_num, 
                                           u16 num_chan)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.transport_map.num_chan = num_chan;
        return (SDP_SUCCESS);
    }
}

/* Note:  The fmtp attribute formats currently handled are:
 *        fmtp:<payload type> <event>,<event>...
 *        fmtp:<payload_type> [annexa=yes/no] [annexb=yes/no] [bitrate=<value>]
 *        where "value" is a numeric value > 0
 *        where each event is a single number or a range separated
 *        by a '-'.
 *        Example:  fmtp:101 1,3-15,20
 */

/* Function:    tinybool sdp_attr_fmtp_valid(void *sdp_ptr)
 * Description: Returns true or false depending on whether an fmtp
 *              attribute was specified with the given payload value
 *              at the given level.  If it was, the instance number of
 *              that attribute is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_fmtp_payload_valid (void *sdp_ptr, u16 level, u8 cap_num, 
                                      u16 *inst_num, u16 payload_type)
{
    u16          i;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    u16          num_instances;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    if (sdp_attr_num_instances(sdp_ptr, level, cap_num, 
                               SDP_ATTR_FMTP, &num_instances) != SDP_SUCCESS) {
        return (FALSE);
    }

    for (i=1; i <= num_instances; i++) {
        attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, i);
        if ((attr_p != NULL) && 
            (attr_p->attr.fmtp.payload_num == payload_type)) {
            *inst_num = i;
            return (TRUE);
        }
    }   

    return (FALSE);
}

/* Function:    sdp_attr_get_fmtp_payload_type
 * Description: Returns the value of the fmtp attribute payload type
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, zero is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Payload type value.
 */
u16 sdp_attr_get_fmtp_payload_type (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.fmtp.payload_num);
    }
}


/* Function:    sdp_attr_fmtp_is_range_set
 * Description: Determines if a range of events is set in an fmtp attribute.
 *              The overall range for events is 0-255.
 *              This will return either FULL_MATCH, PARTIAL_MATCH, or NO_MATCH
 *              depending on whether all, some, or none of the specified
 *              events are defined. If the given attribute is not defined, 
 *              NO_MATCH will be returned.  It is up to the appl to verify
 *              the validity of the attribute before calling this routine.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              low_val     Low value of the range. Range is 0-255.
 *              high_val    High value of the range.
 * Returns:     SDP_FULL_MATCH, SDP_PARTIAL_MATCH, SDP_NO_MATCH
 */
sdp_ne_res_e sdp_attr_fmtp_is_range_set (void *sdp_ptr, u16 level, u8 cap_num, 
                                         u16 inst_num, u8 low_val, u8 high_val)
{
    u16          i;
    u32          mapword;
    u32          bmap;
    u32          num_vals = 0;
    u32          num_vals_set = 0;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_NO_MATCH);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_NO_MATCH);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    for (i = low_val; i <= high_val; i++) {
        num_vals++;
        mapword = i/SDP_NE_BITS_PER_WORD;
        bmap = SDP_NE_BIT_0 << (i%32);
        if (fmtp_p->bmap[ mapword ] & bmap) {
            num_vals_set++;
        }
    }

    if (num_vals == num_vals_set) {
        return (SDP_FULL_MATCH);
    } else if (num_vals_set == 0) {
        return (SDP_NO_MATCH);
    } else {
        return (SDP_PARTIAL_MATCH);
    }
}

/* Function:    sdp_attr_fmtp_valid
 * Description: Determines the validity of the events in the fmtp.
 *              The overall range for events is 0-255.
 *              The user passes an event list with valid events supported by Appl.
 *              This routine will do a simple AND comparison and report the result.
 *
 *              This will return TRUE if ftmp events are valid, and FALSE otherwise.
 *              It is up to the appl to verify the validity of the attribute 
 *              before calling this routine.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              appl_maxval Max event value supported by Appl. Range is 0-255.
 *              evt_array   Bitmap containing events supported by application.
 * Returns:     TRUE, FALSE
 */
tinybool
sdp_attr_fmtp_valid(void *sdp_ptr, u16 level, u8 cap_num,
                    u16 inst_num, u16 appl_maxval, u32* evt_array)
{
    u16          i;
    u32          mapword;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return FALSE;
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return FALSE;
    }

    fmtp_p = &(attr_p->attr.fmtp);

    /* Do quick test. If application max value is lower than fmtp's then error */
    if (fmtp_p->maxval > appl_maxval)
      return FALSE;

    /* Ok, events are within range. Now check that only 
     * allowed events have been received
     */
    mapword = appl_maxval/SDP_NE_BITS_PER_WORD;
    for (i=0; i<mapword; i++) {
      if (fmtp_p->bmap[i] & ~(evt_array[i])) {
	/* Remote SDP is requesting events not supported by Application */
	return FALSE;
      }
    }    
    return TRUE;
}

/* Function:    sdp_attr_set_fmtp_payload_type
 * Description: Sets the value of the fmtp attribute payload type parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              payload_type New payload type value.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_fmtp_payload_type (void *sdp_ptr, u16 level, 
                                             u8 cap_num, u16 inst_num, 
                                             u16 payload_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.fmtp.payload_num = payload_num;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_fmtp_bitmap
 * Description: Set a range of named events for an fmtp attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              bmap        The 8 word data array holding the bitmap
 * Returns:     SDP_SUCCESS 
 */
sdp_result_e sdp_attr_set_fmtp_bitmap(void *sdp_ptr, u16 level, 
                           u8 cap_num, u16 inst_num, u32 *bmap, u32 maxval)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->maxval = maxval;
    memcpy(fmtp_p->bmap, bmap, SDP_NE_NUM_BMAP_WORDS * sizeof(u32) );

    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_get_fmtp_range
 * Description: Get a range of named events for an fmtp attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              bmap        The 8 word data array holding the bitmap
 * Returns:     SDP_SUCCESS 
 */
sdp_result_e sdp_attr_get_fmtp_range (void *sdp_ptr, u16 level, u8 cap_num, 
                                      u16 inst_num, u32 *bmap)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    memcpy(bmap, fmtp_p->bmap, SDP_NE_NUM_BMAP_WORDS * sizeof(u32) );

    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_range
 * Description: Set a range of named events for an fmtp attribute. The low
 *              value specified must be <= the high value.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              low_val     The low value of the range. Range is 0-255.
 *              high_val    The high value of the range.  May be == low.
 * Returns:     SDP_SUCCESS 
 */
sdp_result_e sdp_attr_set_fmtp_range (void *sdp_ptr, u16 level, u8 cap_num, 
                                      u16 inst_num, u8 low_val, u8 high_val)
{
    u16          i;
    u32          mapword;
    u32          bmap;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    for (i = low_val; i <= high_val; i++) {
        mapword = i/SDP_NE_BITS_PER_WORD;
        bmap = SDP_NE_BIT_0 << (i%32);
        fmtp_p->bmap[ mapword ] |= bmap;
    }
    if (high_val > fmtp_p->maxval) {
        fmtp_p->maxval = high_val;
    }
    fmtp_p->fmtp_format = SDP_FMTP_NTE;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_clear_fmtp_range
 * Description: Clear a range of named events for an fmtp attribute. The low
 *              value specified must be <= the high value.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              low_val     The low value of the range. Range is 0-255
 *              high_val    The high value of the range.  May be == low.
 * Returns:     SDP_SUCCESS 
 */
sdp_result_e sdp_attr_clear_fmtp_range (void *sdp_ptr, u16 level, u8 cap_num, 
                                        u16 inst_num, u8 low_val, u8 high_val)
{
    u16          i;
    u32          mapword;
    u32          bmap;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    for (i = low_val; i <= high_val; i++) {
        mapword = i/SDP_NE_BITS_PER_WORD;
        bmap = SDP_NE_BIT_0 << (i%32);
        fmtp_p->bmap[ mapword ] &= ~bmap;
    }
    if (high_val > fmtp_p->maxval) {
        fmtp_p->maxval = high_val;
    }
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_compare_fmtp_ranges
 * Description: Compare the named events set of two fmtp attributes. If all
 *              events are the same (either set or not), FULL_MATCH will be
 *              returned.  If no events match, NO_MATCH will be returned.
 *              Otherwise PARTIAL_MATCH will be returned.  If either attr is
 *              invalid, NO_MATCH will be returned.
 * Parameters:  src_sdp_ptr The SDP handle returned by sdp_init_description.
 *              dst_sdp_ptr The SDP handle returned by sdp_init_description.
 *              src_level   The level of the src fmtp attribute.
 *              dst_level   The level to the dst fmtp attribute.
 *              src_cap_num The capability number of the src attr.
 *              dst_cap_num The capability number of the dst attr.
 *              src_inst_numh The attribute instance of the src attr.
 *              dst_inst_numh The attribute instance of the dst attr.
 * Returns:     SDP_FULL_MATCH, SDP_PARTIAL_MATCH, SDP_NO_MATCH.
 */
sdp_ne_res_e sdp_attr_compare_fmtp_ranges (void *src_sdp_ptr,void *dst_sdp_ptr,
                                           u16 src_level, u16 dst_level, 
                                           u8 src_cap_num, u8 dst_cap_num,
                                           u16 src_inst_num, u16 dst_inst_num)
{
    u16          i,j;
    u32          bmap;
    u32          num_vals_match = 0;
    sdp_t       *src_sdp_p = (sdp_t *)src_sdp_ptr;
    sdp_t       *dst_sdp_p = (sdp_t *)dst_sdp_ptr;
    sdp_attr_t  *src_attr_p;
    sdp_attr_t  *dst_attr_p;
    sdp_fmtp_t  *src_fmtp_p;
    sdp_fmtp_t  *dst_fmtp_p;

    if ((sdp_verify_sdp_ptr(src_sdp_p) == FALSE) ||
        (sdp_verify_sdp_ptr(dst_sdp_p) == FALSE)) {
        return (SDP_NO_MATCH);
    }

    src_attr_p = sdp_find_attr(src_sdp_p, src_level, src_cap_num, 
                               SDP_ATTR_FMTP, src_inst_num);
    dst_attr_p = sdp_find_attr(dst_sdp_p, dst_level, dst_cap_num, 
                               SDP_ATTR_FMTP, dst_inst_num);
    if ((src_attr_p == NULL) || (dst_attr_p == NULL)) {
        if (src_sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s source or destination fmtp attribute for "
                      "compare not found.", src_sdp_p->debug_str);
        }
        src_sdp_p->conf_p->num_invalid_param++;
        return (SDP_NO_MATCH);
    }

    src_fmtp_p = &(src_attr_p->attr.fmtp);
    dst_fmtp_p = &(dst_attr_p->attr.fmtp);
    for (i = 0; i < SDP_NE_NUM_BMAP_WORDS; i++) {
        for (j = 0; j < SDP_NE_BITS_PER_WORD; j++) {
            bmap = SDP_NE_BIT_0 << j;
            if ((src_fmtp_p->bmap[i] & bmap) && (dst_fmtp_p->bmap[i] & bmap)) {
                num_vals_match++;
            } else if ((!(src_fmtp_p->bmap[i] & bmap)) &&
                       (!(dst_fmtp_p->bmap[i] & bmap))) {
                num_vals_match++;
            }
        }
    }

    if (num_vals_match == (SDP_NE_NUM_BMAP_WORDS * SDP_NE_BITS_PER_WORD)) {
        return (SDP_FULL_MATCH);
    } else if (num_vals_match == 0) {
        return (SDP_NO_MATCH);
    } else {
        return (SDP_PARTIAL_MATCH);
    }
}

/* Function:    sdp_attr_copy_fmtp_ranges
 * Description: Copy the named events set for one fmtp attribute to another. 
 * Parameters:  src_sdp_ptr The SDP handle returned by sdp_init_description.
 *              dst_sdp_ptr The SDP handle returned by sdp_init_description.
 *              src_level   The level of the src fmtp attribute.
 *              dst_level   The level to the dst fmtp attribute.
 *              src_cap_num The capability number of the src attr.
 *              dst_cap_num The capability number of the dst attr.
 *              src_inst_numh The attribute instance of the src attr.
 *              dst_inst_numh The attribute instance of the dst attr.
 * Returns:     SDP_SUCCESS 
 */
sdp_result_e sdp_attr_copy_fmtp_ranges (void *src_sdp_ptr, void *dst_sdp_ptr,
                                        u16 src_level, u16 dst_level, 
                                        u8 src_cap_num, u8 dst_cap_num,
                                        u16 src_inst_num, u16 dst_inst_num)
{
    u16          i;
    sdp_t       *src_sdp_p = (sdp_t *)src_sdp_ptr;
    sdp_t       *dst_sdp_p = (sdp_t *)dst_sdp_ptr;
    sdp_attr_t  *src_attr_p;
    sdp_attr_t  *dst_attr_p;
    sdp_fmtp_t  *src_fmtp_p;
    sdp_fmtp_t  *dst_fmtp_p;

    if ((sdp_verify_sdp_ptr(src_sdp_p) == FALSE) ||
        (sdp_verify_sdp_ptr(dst_sdp_p) == FALSE)) {
        return (SDP_INVALID_SDP_PTR);
    }

    src_attr_p = sdp_find_attr(src_sdp_p, src_level, src_cap_num, 
                               SDP_ATTR_FMTP, src_inst_num);
    dst_attr_p = sdp_find_attr(dst_sdp_p, dst_level, dst_cap_num, 
                               SDP_ATTR_FMTP, dst_inst_num);
    if ((src_attr_p == NULL) || (dst_attr_p == NULL)) {
        if (src_sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s source or destination fmtp attribute for "
                      "copy not found.", src_sdp_p->debug_str);
        }
        src_sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    src_fmtp_p = &(src_attr_p->attr.fmtp);
    dst_fmtp_p = &(dst_attr_p->attr.fmtp);
    dst_fmtp_p->maxval = src_fmtp_p->maxval;
    for (i = 0; i < SDP_NE_NUM_BMAP_WORDS; i++) {
        dst_fmtp_p->bmap[i] = src_fmtp_p->bmap[i];
    }
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_annexa
 * Description: Sets the value of the fmtp attribute annexa type parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              annexa       It is either yes or no.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_fmtp_annexa (void *sdp_ptr, u16 level, 
                                       u8 cap_num, u16 inst_num, 
                                       tinybool annexa)
{

    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->annexa_required = TRUE;
    fmtp_p->annexa = annexa;
    
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_annexb
 * Description: Sets the value of the fmtp attribute annexb type parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              annexb       It is either yes or no.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_fmtp_annexb  (void *sdp_ptr, u16 level, 
                                        u8 cap_num, u16 inst_num, 
                                        tinybool annexb)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

   if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
   }
   
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->annexb_required = TRUE;
    fmtp_p->annexb = annexb;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_get_fmtp_mode
 * Description: Gets the value of the fmtp attribute mode parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              payload_type payload type.
 * Returns:     mode value
 */
u32 sdp_attr_get_fmtp_mode_for_payload_type (void *sdp_ptr, u16 level,
                                             u8 cap_num, u32 payload_type)
{
    u16          num_a_lines = 0;
    int          i;
    sdp_t       *sdp_p = sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }
    /*
     * Get number of FMTP attributes for the AUDIO line
     */
    (void) sdp_attr_num_instances(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                                  &num_a_lines);
    for (i = 0; i < num_a_lines; i++) {
        attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, (uint16_t) (i + 1));
        if ((attr_p != NULL) && 
            (attr_p->attr.fmtp.payload_num == (u16)payload_type)) {
            if (attr_p->attr.fmtp.fmtp_format == SDP_FMTP_MODE) {
                return attr_p->attr.fmtp.mode;
            }
        }
    }
   return 0;
}

/* Function:    sdp_attr_set_fmtp_mode
 * Description: Sets the value of the fmtp attribute mode type parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              mode        in milli seconds
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_fmtp_mode  (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num,
                                      u32 mode)
{
    sdp_t       *sdp_p = sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_MODE;
    fmtp_p->mode = mode;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_bitrate_type
 * Description: Sets the value of the fmtp attribute bitrate type parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              bitrate     Sets the bitrate value.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_fmtp_bitrate_type  (void *sdp_ptr, u16 level, 
                                             u8 cap_num, u16 inst_num, 
                                             u32 bitrate)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (bitrate <= 0) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->bitrate = bitrate;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_cif
 * Description: Sets the value of the fmtp attribute cif parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              cif         Sets the CIF value for a video codec
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_fmtp_cif  (void *sdp_ptr, u16 level, 
                                     u8 cap_num, u16 inst_num, 
                                     u16 cif)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if ((cif < SDP_MIN_CIF_VALUE) || ( cif > SDP_MAX_CIF_VALUE)) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->cif = cif;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_qcif
 * Description: Sets the value of the fmtp attribute qcif parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              qcif        Sets the QCIF value for a video codec
 * Returns:     SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_fmtp_qcif  (void *sdp_ptr, u16 level, 
                                     u8 cap_num, u16 inst_num, 
                                     u16 qcif)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if ((qcif < SDP_MIN_CIF_VALUE) || ( qcif > SDP_MAX_CIF_VALUE)) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->qcif = qcif;
    return (SDP_SUCCESS);
}
/* Function:    sdp_attr_set_fmtp_sqcif
 * Description: Sets the value of the fmtp attribute sqcif parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.  
 *              inst_num    The attribute instance number to check.
 *              sqcif        Sets the SQCIF value for a video codec
 * Returns:     SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_fmtp_sqcif  (void *sdp_ptr, u16 level, 
                                       u8 cap_num, u16 inst_num, 
                                       u16 sqcif)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if ((sqcif < SDP_MIN_CIF_VALUE) || (sqcif > SDP_MAX_CIF_VALUE)) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->sqcif = sqcif;
    return (SDP_SUCCESS);
}


/* Function:    sdp_attr_set_fmtp_cif4
 * Description: Sets the value of the fmtp attribute cif4 parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.  
 *              inst_num    The attribute instance number to check.
 *              sqcif        Sets the cif4 value for a video codec
 * Returns:     SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_fmtp_cif4  (void *sdp_ptr, u16 level, 
                                       u8 cap_num, u16 inst_num, 
                                       u16 cif4)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if ((cif4 < SDP_MIN_CIF_VALUE) || (cif4 > SDP_MAX_CIF_VALUE)) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->cif4 = cif4;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_cif16
 * Description: Sets the value of the fmtp attribute cif16 parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.  
 *              inst_num    The attribute instance number to check.
 *              cif16        Sets the cif16 value for a video codec
 * Returns:     SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_fmtp_cif16  (void *sdp_ptr, u16 level, 
                                       u8 cap_num, u16 inst_num, 
                                       u16 cif16)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if ((cif16 < SDP_MIN_CIF_VALUE) || (cif16 > SDP_MAX_CIF_VALUE)) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->cif16 = cif16;
    return (SDP_SUCCESS);
}


/* Function:    sdp_attr_set_fmtp_maxbr
 * Description: Sets the value of the fmtp attribute maxbr parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              MAXBR        Sets the MAXBR value for a video codec
 * Returns:     SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
*/

sdp_result_e sdp_attr_set_fmtp_maxbr  (void *sdp_ptr, u16 level, 
                                       u8 cap_num, u16 inst_num, 
                                       u16 maxbr)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (maxbr <= 0) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->maxbr = maxbr;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_custom
 * Description: Sets the value of the fmtp attribute custom parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              CUSTOM       Sets the CUSTOM value for a video codec
 * Returns:     SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
*/

sdp_result_e sdp_attr_set_fmtp_custom  (void *sdp_ptr, u16 level, 
                                        u8 cap_num, u16 inst_num, 
                                        u16 custom_x, u16 custom_y, u16 custom_mpi)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if ((custom_x <= 0) || (custom_y <= 0) || (custom_mpi <= 0)) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->custom_x = custom_x;
    fmtp_p->custom_y = custom_y;
    fmtp_p->custom_mpi = custom_mpi;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_par
 * Description: Sets the value of the fmtp attribute PAR parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              PAR         Sets the PAR width/height value
 * Returns:     SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
*/

sdp_result_e sdp_attr_set_fmtp_par  (void *sdp_ptr, u16 level, 
                                     u8 cap_num, u16 inst_num, 
                                     u16 par_width, u16 par_height)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if ((par_width <= 0) || (par_height <= 0)) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->par_width  = par_width;
    fmtp_p->par_height = par_height;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_cpcf
 * Description: Sets the value of the fmtp attribute CPCF parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              CPCF        Sets the CPCF value
 * Returns:     SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
*/

sdp_result_e sdp_attr_set_fmtp_cpcf (void *sdp_ptr, u16 level, 
                                     u8 cap_num, u16 inst_num, 
                                     u16 cpcf)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (cpcf <= 0) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->cpcf  = cpcf;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_bpp
 * Description: Sets the value of the fmtp attribute BPP parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              BPP        Sets the BPP value
 * Returns:     SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
*/

sdp_result_e sdp_attr_set_fmtp_bpp (void *sdp_ptr, u16 level, 
                                    u8 cap_num, u16 inst_num, 
                                    u16 bpp)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (bpp <= 0) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->bpp  = bpp;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_fmtp_hrd
 * Description: Sets the value of the fmtp attribute HRD parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              HRD        Sets the HRD value
 * Returns:     SDP_SUCCESS       Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */

sdp_result_e sdp_attr_set_fmtp_hrd (void *sdp_ptr, u16 level, 
                                    u8 cap_num, u16 inst_num, u16 hrd)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (hrd <= 0) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->hrd  = hrd;
    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_h263_num_params (void *sdp_ptr, int16 level, 
                                                u8 cap_num, u16 inst_num, 
                                                int16 profile,
                                                u16 h263_level,
                                                tinybool interlace)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;

    if ((profile >= SDP_MIN_PROFILE_LEVEL_VALUE) && 
             (profile <= SDP_MAX_PROFILE_VALUE)) {
        fmtp_p->profile  = profile;
    }

    if ((level >= SDP_MIN_PROFILE_LEVEL_VALUE) && 
             (level <= SDP_MAX_LEVEL_VALUE)) { 
        fmtp_p->level  = h263_level;
    }

    if (interlace) {
        fmtp_p->is_interlace  = TRUE;
    } else {
        fmtp_p->is_interlace  = FALSE;
    }

    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_profile_level_id (void *sdp_ptr, u16 level, 
                                                u8 cap_num, u16 inst_num, 
                                                const char *profile_level_id)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    if (profile_level_id) { 
        sstrncpy(fmtp_p->profile_level_id, profile_level_id, 
	   SDP_MAX_STRING_LEN+1);
    }
    
    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_parameter_sets (void *sdp_ptr, u16 level, 
                                               u8 cap_num, u16 inst_num, 
                                               const char *parameter_sets)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    if (parameter_sets) {
        sstrncpy(fmtp_p->parameter_sets, parameter_sets, SDP_MAX_STRING_LEN+1);
    }
    
    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_pack_mode (void *sdp_ptr, u16 level, 
                                          u8 cap_num, u16 inst_num, 
                                          u16 pack_mode)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (pack_mode > SDP_MAX_PACKETIZATION_MODE_VALUE) {
        return (SDP_INVALID_PARAMETER);
    }

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->packetization_mode  = pack_mode;

    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_level_asymmetry_allowed (void *sdp_ptr, u16 level, 
                                          u8 cap_num, u16 inst_num, 
                                          u16 asym_allowed)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->level_asymmetry_allowed  = asym_allowed;

    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_deint_buf_req (void *sdp_ptr, u16 level, 
                                                u8 cap_num, u16 inst_num, 
                                                u32 deint_buf_req)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->deint_buf_req = deint_buf_req;
    fmtp_p->flag |= SDP_DEINT_BUF_REQ_FLAG;

    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_init_buf_time (void *sdp_ptr, u16 level, 
                                                u8 cap_num, u16 inst_num, 
                                                u32 init_buf_time)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->init_buf_time = init_buf_time;
    fmtp_p->flag |= SDP_INIT_BUF_TIME_FLAG;

    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_max_don_diff (void *sdp_ptr, u16 level, 
                                                u8 cap_num, u16 inst_num, 
                                                u32 max_don_diff)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->max_don_diff  = max_don_diff;

    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_interleaving_depth (void *sdp_ptr, u16 level, 
                                                u8 cap_num, u16 inst_num, 
                                                u16 interleaving_depth)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->interleaving_depth  = interleaving_depth;

    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_redundant_pic_cap (void *sdp_ptr, u16 level, 
                                               u8 cap_num, u16 inst_num, 
					       tinybool redundant_pic_cap)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;

    if (redundant_pic_cap > 1) {
        return (SDP_FAILURE);
    } else {
        fmtp_p->redundant_pic_cap = redundant_pic_cap;
        return (SDP_SUCCESS);
    }
}

sdp_result_e sdp_attr_set_fmtp_max_mbps (void *sdp_ptr, u16 level, 
                                         u8 cap_num, u16 inst_num, 
                                         u32 max_mbps)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;

    if (max_mbps > 0) {
        fmtp_p->max_mbps  = max_mbps;
        return (SDP_SUCCESS);
    } else {
        return (SDP_FAILURE);
    }
}

sdp_result_e sdp_attr_set_fmtp_max_fs (void *sdp_ptr, u16 level, 
                                       u8 cap_num, u16 inst_num, 
                                       u32 max_fs)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;

    if (max_fs > 0) {
        fmtp_p->max_fs  = max_fs;
        return (SDP_SUCCESS);
    } else {
        return (SDP_FAILURE);
    }
}

sdp_result_e sdp_attr_set_fmtp_max_br (void *sdp_ptr, u16 level, 
                                       u8 cap_num, u16 inst_num, 
                                       u32 max_br)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;

    if (max_br > 0) {
        fmtp_p->max_br  = max_br;
        return (SDP_SUCCESS);
    } else {
        return (SDP_FAILURE);
    }
}

sdp_result_e sdp_attr_set_fmtp_max_cpb (void *sdp_ptr, u16 level, 
                                        u8 cap_num, u16 inst_num, 
                                        u32 max_cpb)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;

    if (max_cpb > 0) {
        fmtp_p->max_cpb  = max_cpb;
        return (SDP_SUCCESS);
    } else {
        return (SDP_FAILURE);
    }
}

sdp_result_e sdp_attr_set_fmtp_max_dpb (void *sdp_ptr, u16 level, 
                                        u8 cap_num, u16 inst_num, 
                                        u32 max_dpb)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;

    if (max_dpb > 0) {
        fmtp_p->max_dpb  = max_dpb;
        return (SDP_SUCCESS);
    } else {
        return (SDP_FAILURE);
    }
}

sdp_result_e sdp_attr_set_fmtp_max_rcmd_nalu_size (void *sdp_ptr, u16 level, 
                                               u8 cap_num, u16 inst_num, 
					       u32 max_rcmd_nalu_size)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->max_rcmd_nalu_size = max_rcmd_nalu_size;
    fmtp_p->flag |= SDP_MAX_RCMD_NALU_SIZE_FLAG;
 
    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_deint_buf_cap (void *sdp_ptr, u16 level, 
                                               u8 cap_num, u16 inst_num, 
					       u32 deint_buf_cap)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->deint_buf_cap = deint_buf_cap;
    fmtp_p->flag |= SDP_DEINT_BUF_CAP_FLAG;
    
    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_h264_parameter_add (void *sdp_ptr, u16 level, 
                                              u8 cap_num, u16 inst_num, 
                                              tinybool parameter_add)
{

    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;
    fmtp_p->parameter_add = parameter_add;
    
    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_h261_annex_params (void *sdp_ptr, u16 level, 
                                                  u8 cap_num, u16 inst_num,
                                                  tinybool annex_d) {
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;

    fmtp_p->annex_d = annex_d;
    return (SDP_SUCCESS);
}

sdp_result_e sdp_attr_set_fmtp_h263_annex_params (void *sdp_ptr, u16 level, 
                                                  u8 cap_num, u16 inst_num,
                                                  tinybool annex_f,
                                                  tinybool annex_i,
                                                  tinybool annex_j,
                                                  tinybool annex_t,
                                                  u16 annex_k_val,
                                                  u16 annex_n_val,
                                                  u16 annex_p_val_picture_resize, 
                                                  u16 annex_p_val_warp)

{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;
    

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_PARAMETER);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;

    fmtp_p->annex_f = annex_f;
    fmtp_p->annex_i = annex_i;
    fmtp_p->annex_j = annex_j;
    fmtp_p->annex_t = annex_t;
    
    fmtp_p->annex_k_val = annex_k_val;
    fmtp_p->annex_n_val = annex_n_val;
    
    fmtp_p->annex_p_val_picture_resize = annex_p_val_picture_resize;
    fmtp_p->annex_p_val_warp = annex_p_val_warp;
             
    return (SDP_SUCCESS);
}



/* Function:    sdp_attr_fmtp_is_annexb_set
 * Description: Gives the value of the fmtp attribute annexb type parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              
 *
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_fmtp_is_annexb_set (void *sdp_ptr, u16 level, u8 cap_num, 
                                      u16 inst_num)
{    
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annexb);
    }
}

/* Function:    sdp_attr_fmtp_is_annexa_set
 * Description: Gives the value of the fmtp attribute annexa type parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              
 *
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_fmtp_is_annexa_set (void *sdp_ptr, u16 level, u8 cap_num, 
                                      u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

   if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
   }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annexa);
    }
}

/* Function:    sdp_attr_get_fmtp_bitrate_type
 * Description: Gets the value of the fmtp attribute bitrate type parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Bitrate type value.
 */
int32 sdp_attr_get_fmtp_bitrate_type (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num)
{

    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.bitrate);
    }
}

/* Function:    sdp_attr_get_fmtp_qcif
 * Description: Gets the value of the fmtp attribute QCIF type parameter
 *              for a given Video codec.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     QCIF value.
 */
int32 sdp_attr_get_fmtp_qcif (void *sdp_ptr, u16 level,
                            u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.qcif);
    }
}
/* Function:    sdp_attr_get_fmtp_cif
 * Description: Gets the value of the fmtp attribute CIF type parameter
 *              for a given Video codec.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CIF value.
 */
int32 sdp_attr_get_fmtp_cif (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.cif);
    }
}


/* Function:    sdp_attr_get_fmtp_sqcif
 * Description: Gets the value of the fmtp attribute sqcif type parameter
 *              for a given Video codec.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     sqcif value.
 */
int32 sdp_attr_get_fmtp_sqcif (void *sdp_ptr, u16 level,
                               u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.sqcif);
    }
}

/* Function:    sdp_attr_get_fmtp_cif4
 * Description: Gets the value of the fmtp attribute CIF4 type parameter
 *              for a given Video codec.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CIF4 value.
 */
int32 sdp_attr_get_fmtp_cif4 (void *sdp_ptr, u16 level,
                              u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.cif4);
    }
}

/* Function:    sdp_attr_get_fmtp_cif16
 * Description: Gets the value of the fmtp attribute CIF16 type parameter
 *              for a given Video codec.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CIF16 value.
 */

int32 sdp_attr_get_fmtp_cif16 (void *sdp_ptr, u16 level,
                               u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.cif16);
    }
}


/* Function:    sdp_attr_get_fmtp_maxbr
 * Description: Gets the value of the fmtp attribute MAXBR type parameter
 *              for a given Video codec.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     MAXBR value.
 */
int32 sdp_attr_get_fmtp_maxbr (void *sdp_ptr, u16 level,
                               u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.maxbr);
    }
}

/* Function:    sdp_attr_get_fmtp_custom_x
 * Description: Gets the value of the fmtp attribute CUSTOM type parameter
 *              for a given Video codec.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CUSTOM x value.
 */

int32 sdp_attr_get_fmtp_custom_x (void *sdp_ptr, u16 level,
                                  u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.custom_x);
    }
}
/* Function:    sdp_attr_get_fmtp_custom_y
 * Description: Gets the value of the fmtp attribute custom_y type parameter
 *              for a given Video codec.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CUSTOM Y-AXIS value.
 */

int32 sdp_attr_get_fmtp_custom_y (void *sdp_ptr, u16 level,
                                  u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.custom_y);
    }
}

/* Function:    sdp_attr_get_fmtp_custom_mpi
 * Description: Gets the value of the fmtp attribute CUSTOM type parameter
 *              for a given Video codec.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CUSTOM MPI value.
 */

int32 sdp_attr_get_fmtp_custom_mpi (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.custom_mpi);
    }
}

/* Function:    sdp_attr_get_fmtp_par_width
 * Description: Gets the value of the fmtp attribute PAR (width) parameter
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     PAR - width value.
 */
int32 sdp_attr_get_fmtp_par_width (void *sdp_ptr, u16 level,
                                   u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.par_width);
    }
}

/* Function:    sdp_attr_get_fmtp_par_height
 * Description: Gets the value of the fmtp attribute PAR (height) parameter
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     PAR - height value.
 */
int32 sdp_attr_get_fmtp_par_height (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.par_height);
    }
}

/* Function:    sdp_attr_get_fmtp_cpcf
 * Description: Gets the value of the fmtp attribute- CPCF parameter
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CPCF value.
 */
int32 sdp_attr_get_fmtp_cpcf (void *sdp_ptr, u16 level,
                              u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.cpcf);
    }
}

/* Function:    sdp_attr_get_fmtp_bpp
 * Description: Gets the value of the fmtp attribute- BPP parameter
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     BPP value.
 */
int32 sdp_attr_get_fmtp_bpp (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.bpp);
    }
}

/* Function:    sdp_attr_get_fmtp_hrd
 * Description: Gets the value of the fmtp attribute- HRD parameter
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     HRD value.
 */
int32 sdp_attr_get_fmtp_hrd (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.hrd);
    }
}

/* Function:    sdp_attr_get_fmtp_profile
 * Description: Gets the value of the fmtp attribute- PROFILE parameter
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     PROFILE value.
 */
int32 sdp_attr_get_fmtp_profile (void *sdp_ptr, u16 level,
                                 u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.profile);
    }
}

/* Function:    sdp_attr_get_fmtp_level
 * Description: Gets the value of the fmtp attribute- LEVEL parameter
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     LEVEL value.
 */
int32 sdp_attr_get_fmtp_level (void *sdp_ptr, u16 level,
                               u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.level);
    }
}

/* Function:    sdp_attr_get_fmtp_interlace
 * Description: Checks if INTERLACE parameter is set.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     TRUE if INTERLACE is present and FALSE if INTERLACE is absent.
 */
tinybool sdp_attr_get_fmtp_interlace (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return FALSE;
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return FALSE;
    } else {
        return (attr_p->attr.fmtp.is_interlace);
    }
}

/* Function:    sdp_attr_get_fmtp_pack_mode
 * Description: Gets the value of the fmtp attribute- packetization-mode parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     packetization-mode value in the range 0 - 2.
 */

sdp_result_e sdp_attr_get_fmtp_pack_mode (void *sdp_ptr, u16 level,
                                 u8 cap_num, u16 inst_num, u16 *val)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
	*val = attr_p->attr.fmtp.packetization_mode;
	return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_level_asymmetry_allowed
 * Description: Gets the value of the fmtp attribute- level-asymmetry-allowed parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     level asymmetry allowed value (0 or 1).
 */

sdp_result_e sdp_attr_get_fmtp_level_asymmetry_allowed (void *sdp_ptr, u16 level,
                                 u8 cap_num, u16 inst_num, u16 *val)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
	*val = attr_p->attr.fmtp.level_asymmetry_allowed;
	return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_profile_id
 * Description: Gets the value of the fmtp attribute- profile-level-id parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     profile-level-id value.
 */
const char* sdp_attr_get_fmtp_profile_id (void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.fmtp.profile_level_id);
    }
}

/* Function:    sdp_attr_get_fmtp_param_sets
 * Description: Gets the value of the fmtp attribute- parameter-sets parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     parameter-sets value.
 */
const char* sdp_attr_get_fmtp_param_sets (void *sdp_ptr, u16 level,
                                          u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.fmtp.parameter_sets);
    }
}

/* Function:    sdp_attr_get_fmtp_interleaving_depth
 * Description: Gets the value of the fmtp attribute- interleaving_depth parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     interleaving_depth value
 */

sdp_result_e sdp_attr_get_fmtp_interleaving_depth (void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num, u16* val)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
	*val = attr_p->attr.fmtp.interleaving_depth;
	return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_deint_buf_req
 * Description: Gets the value of the fmtp attribute- deint-buf-req parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     deint-buf-req value.
 */

sdp_result_e sdp_attr_get_fmtp_deint_buf_req (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num,
                                             u32 *val)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (attr_p->attr.fmtp.flag & SDP_DEINT_BUF_REQ_FLAG) {
	    *val = attr_p->attr.fmtp.deint_buf_req;
	    return (SDP_SUCCESS);
        } else {
            return (SDP_FAILURE);
        }
    }
}

/* Function:    sdp_attr_get_fmtp_max_don_diff
 * Description: Gets the value of the fmtp attribute- max-don-diff parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-don-diff value.
 */
sdp_result_e sdp_attr_get_fmtp_max_don_diff (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num,
                                      u32 *val)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
	*val = attr_p->attr.fmtp.max_don_diff;
	return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_init_buf_time
 * Description: Gets the value of the fmtp attribute- init-buf-time parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     init-buf-time value.
 */
sdp_result_e sdp_attr_get_fmtp_init_buf_time (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num,
                                             u32 *val)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (attr_p->attr.fmtp.flag & SDP_INIT_BUF_TIME_FLAG) {
	    *val = attr_p->attr.fmtp.init_buf_time;
	    return (SDP_SUCCESS);
        } else {
            return (SDP_FAILURE);
        }
    }
}

/* Function:    sdp_attr_get_fmtp_max_mbps
 * Description: Gets the value of the fmtp attribute- max-mbps parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-mbps value.
 */

sdp_result_e sdp_attr_get_fmtp_max_mbps (void *sdp_ptr, u16 level,
                                u8 cap_num, u16 inst_num,
                                u32 *val)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = attr_p->attr.fmtp.max_mbps;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_max_fs
 * Description: Gets the value of the fmtp attribute- max-fs parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-fs value.
 */

sdp_result_e sdp_attr_get_fmtp_max_fs (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num, u32 *val)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = attr_p->attr.fmtp.max_fs;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_max_cpb
 * Description: Gets the value of the fmtp attribute- max-cpb parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-cpb value.
 */

sdp_result_e sdp_attr_get_fmtp_max_cpb (void *sdp_ptr, u16 level,
                                 u8 cap_num, u16 inst_num, u32 *val)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = attr_p->attr.fmtp.max_cpb;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_max_dpb
 * Description: Gets the value of the fmtp attribute- max-dpb parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-dpb value.
 */

sdp_result_e sdp_attr_get_fmtp_max_dpb (void *sdp_ptr, u16 level,
                               u8 cap_num, u16 inst_num, u32 *val)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = attr_p->attr.fmtp.max_dpb;
        return (SDP_SUCCESS);
    }
}


/* Function:    sdp_attr_get_fmtp_max_br
 * Description: Gets the value of the fmtp attribute- max-br parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-br value.
 */

sdp_result_e sdp_attr_get_fmtp_max_br (void *sdp_ptr, u16 level,
                             u8 cap_num, u16 inst_num, u32* val)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = attr_p->attr.fmtp.max_br;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_fmtp_is_redundant_pic_cap
 * Description: Gets the value of the fmtp attribute- redundant_pic_cap parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     redundant-pic-cap value.
 */
tinybool sdp_attr_fmtp_is_redundant_pic_cap (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.redundant_pic_cap);
    }
}

/* Function:    sdp_attr_get_fmtp_deint_buf_cap
 * Description: Gets the value of the fmtp attribute- deint-buf-cap parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     deint-buf-cap value.
 */

sdp_result_e sdp_attr_get_fmtp_deint_buf_cap (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num,
                                             u32 *val)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (attr_p->attr.fmtp.flag & SDP_DEINT_BUF_CAP_FLAG) {
	    *val = attr_p->attr.fmtp.deint_buf_cap;
	    return (SDP_SUCCESS);
        } else {
            return (SDP_FAILURE);
        }
    }
}

/* Function:    sdp_attr_get_fmtp_max_rcmd_nalu_size
 * Description: Gets the value of the fmtp attribute- max-rcmd-nalu-size parameter for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-rcmd-nalu-size value.
 */
sdp_result_e sdp_attr_get_fmtp_max_rcmd_nalu_size (void *sdp_ptr, u16 level,
                                                  u8 cap_num, u16 inst_num,
                                                  u32 *val)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (attr_p->attr.fmtp.flag & SDP_MAX_RCMD_NALU_SIZE_FLAG) {
	    *val = attr_p->attr.fmtp.max_rcmd_nalu_size;
	    return (SDP_SUCCESS);
        } else {
            return (SDP_FAILURE);
        }
    }
}

/* Function:    sdp_attr_fmtp_is_parameter_add
 * Description: Gets the value of the fmtp attribute- parameter-add for H.264 codec
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     TRUE/FALSE ( parameter-add is boolean)
 */
tinybool sdp_attr_fmtp_is_parameter_add (void *sdp_ptr, u16 level,
                                         u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.parameter_add);
    }
}

/****** Following functions are get routines for Annex values 
 * For each Annex support, the get routine will return the boolean TRUE/FALSE
 * Some Annexures for Video codecs have values defined . In those cases, 
 * (e.g Annex K, P ) , the return values are not boolean.
 * 
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Annex value 
 */

tinybool sdp_attr_get_fmtp_annex_d (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annex_d);
    }
}

tinybool sdp_attr_get_fmtp_annex_f (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annex_f);
    }
}

tinybool sdp_attr_get_fmtp_annex_i (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annex_i);
    }
}

tinybool sdp_attr_get_fmtp_annex_j (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annex_j);
    }
}

tinybool sdp_attr_get_fmtp_annex_t (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annex_t);
    }
}

int32 sdp_attr_get_fmtp_annex_k_val (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.annex_k_val);
    }
}

int32 sdp_attr_get_fmtp_annex_n_val (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.annex_n_val);
    }
}

int32 sdp_attr_get_fmtp_annex_p_picture_resize (void *sdp_ptr, u16 level,
                                                u8 cap_num, u16 inst_num)
{

    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {        
        return (attr_p->attr.fmtp.annex_p_val_picture_resize);
    }
}

int32 sdp_attr_get_fmtp_annex_p_warp (void *sdp_ptr, u16 level,
                                      u8 cap_num, u16 inst_num)
{
    
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.annex_p_val_warp);
    }
}

/* Function:    sdp_attr_fmtp_get_fmtp_format
 * Description: Gives the value of the fmtp attribute fmtp_format
 *              type parameter
 *              for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              
 *
 * Returns:     Enum type sdp_fmtp_format_type_e
 */
sdp_fmtp_format_type_e  sdp_attr_fmtp_get_fmtp_format (void *sdp_ptr, 
                                                       u16 level, u8 cap_num, 
                                                       u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_FMTP_UNKNOWN_TYPE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_FMTP_UNKNOWN_TYPE);
    } else {
        return (attr_p->attr.fmtp.fmtp_format);
    }
}

/* Function:    sdp_attr_get_pccodec_num_payload_types
 * Description: Returns the number of payload types specified for the 
 *              given X-pc-codec attribute.  If the given attribute is not 
 *              defined, zero is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of payload types.
 */
u16 sdp_attr_get_pccodec_num_payload_types (void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_X_PC_CODEC, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-pc-codec attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.pccodec.num_payloads);
    }
}

/* Function:    sdp_attr_get_pccodec_payload_type
 * Description: Returns the value of the specified payload type for the 
 *              given X-pc-codec attribute.  If the given attribute is not 
 *              defined, zero is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              payload_num The payload number to get.  Range is (1 - 
 *                          max num payloads).
 * Returns:     Payload type.
 */
u16 sdp_attr_get_pccodec_payload_type (void *sdp_ptr, u16 level, u8 cap_num, 
                                       u16 inst_num, u16 payload_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_X_PC_CODEC, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-pc-codec attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        if ((payload_num < 1) || 
            (payload_num > attr_p->attr.pccodec.num_payloads)) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s X-pc-codec attribute, level %u instance %u, "
                          "invalid payload number %u requested.", 
                          sdp_p->debug_str, level, inst_num, payload_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (0);
        } else {
            return (attr_p->attr.pccodec.payload_type[payload_num-1]);
        }
    }
}

/* Function:    sdp_attr_add_pccodec_payload_type
 * Description: Add a new value to the list of payload types specified for
 *              the given X-pc-codec attribute.  The payload type will be
 *              added to the end of the list so these values should be added
 *              in the order they will be displayed within the attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              payload_type The payload type to add.
 * Returns:     SDP_SUCCESS            Payload type was added successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_add_pccodec_payload_type (void *sdp_ptr, u16 level, 
                                                u8 cap_num, u16 inst_num,
                                                u16 payload_type)
{
    u16          payload_num;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_X_PC_CODEC, 
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-pc-codec attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        payload_num = attr_p->attr.pccodec.num_payloads++;
        attr_p->attr.pccodec.payload_type[payload_num] = payload_type;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_xcap_first_cap_num
 * Description: Gets the first capability number valid for the specified
 *              X-cap attribute instance.  If the capability is not 
 *              defined, zero is returned.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the capability.  
 *              inst_num    The X-cap instance number to check.
 * Returns:     Capability number or zero.
 */
u16 sdp_attr_get_xcap_first_cap_num (void *sdp_ptr, u16 level, u16 inst_num)
{
    u16          cap_num=1;
    u16          attr_count=0;
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    if (level == SDP_SESSION_LEVEL) {
        for (attr_p = sdp_p->sess_attrs_p; attr_p != NULL;
             attr_p = attr_p->next_p) {
            if (attr_p->type == SDP_ATTR_X_CAP) {
                attr_count++;
                if (attr_count == inst_num) {
                    return (cap_num);
                } else {
                    cap_num += attr_p->attr.cap_p->num_payloads;
                }
            }
        }
    } else {  /* Capability is at a media level */
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (0);
        }
        for (attr_p = mca_p->media_attrs_p; attr_p != NULL;
             attr_p = attr_p->next_p) {
            if (attr_p->type == SDP_ATTR_X_CAP) {
                attr_count++;
                if (attr_count == inst_num) {
                    return (cap_num);
                } else {
                    cap_num += attr_p->attr.cap_p->num_payloads;
                }
            }
        }
    }  /* Attr is at a media level */

    if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
        SDP_ERROR("%s X-cap attribute, level %u instance %u "
                  "not found.", sdp_p->debug_str, level, inst_num);
    }
    sdp_p->conf_p->num_invalid_param++;
    return (0);
}

/* Function:    sdp_attr_get_xcap_media_type
 * Description: Returns the media type specified for the given X-cap  
 *              attribute.  If the given attribute is not defined,
 *              SDP_MEDIA_INVALID is returned.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 * Returns:     Media type or SDP_MEDIA_INVALID.
 */
sdp_media_e sdp_attr_get_xcap_media_type (void *sdp_ptr, u16 level,
                                          u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_MEDIA_INVALID);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_MEDIA_INVALID);
    } else {
        cap_p = attr_p->attr.cap_p;
        return (cap_p->media);
    }
}

/* Function:    sdp_attr_get_xcap_transport_type
 * Description: Returns the transport type specified for the given X-cap  
 *              attribute.  If the given attribute is not defined,
 *              SDP_TRANSPORT_INVALID is returned.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 * Returns:     Media type or SDP_TRANSPORT_INVALID.
 */
sdp_transport_e sdp_attr_get_xcap_transport_type (void *sdp_ptr, u16 level,
                                                  u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_TRANSPORT_INVALID);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP, 
                           inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_TRANSPORT_INVALID);
    } else {
        cap_p = attr_p->attr.cap_p;
        return (cap_p->transport);
    }
}

/* Function:    sdp_attr_get_xcap_num_payload_types
 * Description: Returns the number of payload types associated with the  
 *              specified X-cap attribute.  If the attribute is invalid, 
 *              zero will be returned.  Application must validate the 
 *              attribute line before using this routine.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of payload types or zero.
 */
u16 sdp_attr_get_xcap_num_payload_types (void *sdp_ptr, u16 level,
                                         u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        cap_p = attr_p->attr.cap_p;
        return (cap_p->num_payloads);
    }
}

/* Function:    sdp_attr_get_xcap_payload_type
 * Description: Returns the payload type of the specified payload for the 
 *              X-cap attribute line.  If the attr line or payload number is 
 *              invalid, zero will be returned.  Application must validate 
 *              the X-cap attr before using this routine.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 *              payload_num The payload number to retrieve.  Range is 
 *                          (1 - max num payloads).
 * Returns:     Payload type or zero.
 */
u16 sdp_attr_get_xcap_payload_type (void *sdp_ptr, u16 level,
                                    u16 inst_num, u16 payload_num,
                                    sdp_payload_ind_e *indicator)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        cap_p = attr_p->attr.cap_p;
        if ((payload_num < 1) ||
            (payload_num > cap_p->num_payloads)) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s X-cap attribute, level %u instance %u, "
                          "payload num %u invalid.", sdp_p->debug_str,
                          level, inst_num, payload_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (0);
        } else {
            *indicator = cap_p->payload_indicator[payload_num-1];
            return (cap_p->payload_type[payload_num-1]);
        }
    }
}


/* Function:    sdp_attr_set_xcap_media_type
 * Description: Sets the value of the media type parameter for the X-cap
 *              attribute line.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 *              media       Media type for the X-cap attribute.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_attr_set_xcap_media_type (void *sdp_ptr, u16 level, 
                                           u16 inst_num, sdp_media_e media)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    cap_p = attr_p->attr.cap_p;
    cap_p->media = media;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_xcap_transport_type
 * Description: Sets the value of the transport type parameter for the X-cap
 *              attribute line.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 *              transport   Transport type for the X-cap attribute.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_attr_set_xcap_transport_type(void *sdp_ptr, u16 level, 
                                              u16 inst_num, 
                                              sdp_transport_e transport)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    cap_p = attr_p->attr.cap_p;
    cap_p->transport = transport;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_add_xcap_payload_type
 * Description: Add a new payload type for the X-cap attribute line  
 *              specified. The new payload type will be added at the end
 *              of the payload type list.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 *              payload_type The new payload type.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_attr_add_xcap_payload_type(void *sdp_ptr, u16 level,
                                            u16 inst_num, u16 payload_type,
                                            sdp_payload_ind_e indicator)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    cap_p = attr_p->attr.cap_p;
    cap_p->payload_indicator[cap_p->num_payloads] = indicator;
    cap_p->payload_type[cap_p->num_payloads++] = payload_type;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_get_cdsc_first_cap_num
 * Description: Gets the first capability number valid for the specified
 *              CDSC attribute instance.  If the capability is not 
 *              defined, zero is returned.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the capability.  
 *              inst_num    The CDSC instance number to check.
 * Returns:     Capability number or zero.
 */
u16 sdp_attr_get_cdsc_first_cap_num(void *sdp_ptr, u16 level, u16 inst_num)
{
    u16          cap_num=1;
    u16          attr_count=0;
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *mca_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    if (level == SDP_SESSION_LEVEL) {
        for (attr_p = sdp_p->sess_attrs_p; attr_p != NULL;
             attr_p = attr_p->next_p) {
            if (attr_p->type == SDP_ATTR_CDSC) {
                attr_count++;
                if (attr_count == inst_num) {
                    return (cap_num);
                } else {
                    cap_num += attr_p->attr.cap_p->num_payloads;
                }
            }
        }
    } else {  /* Capability is at a media level */
        mca_p = sdp_find_media_level(sdp_p, level);
        if (mca_p == NULL) {
            sdp_p->conf_p->num_invalid_param++;
            return (0);
        }
        for (attr_p = mca_p->media_attrs_p; attr_p != NULL;
             attr_p = attr_p->next_p) {
            if (attr_p->type == SDP_ATTR_CDSC) {
                attr_count++;
                if (attr_count == inst_num) {
                    return (cap_num);
                } else {
                    cap_num += attr_p->attr.cap_p->num_payloads;
                }
            }
        }
    }  /* Attr is at a media level */

    if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
        SDP_ERROR("%s CDSC attribute, level %u instance %u "
                  "not found.", sdp_p->debug_str, level, inst_num);
    }
    sdp_p->conf_p->num_invalid_param++;
    return (0);
}

/* Function:    sdp_attr_get_cdsc_media_type
 * Description: Returns the media type specified for the given CDSC
 *              attribute.  If the given attribute is not defined,
 *              SDP_MEDIA_INVALID is returned.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 * Returns:     Media type or SDP_MEDIA_INVALID.
 */
sdp_media_e sdp_attr_get_cdsc_media_type(void *sdp_ptr, u16 level,
                                         u16 inst_num)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_MEDIA_INVALID);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_MEDIA_INVALID);
    } else {
        cdsc_p = attr_p->attr.cap_p;
        return (cdsc_p->media);
    }
}

/* Function:    sdp_attr_get_cdsc_transport_type
 * Description: Returns the transport type specified for the given CDSC  
 *              attribute.  If the given attribute is not defined,
 *              SDP_TRANSPORT_INVALID is returned.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 * Returns:     Media type or SDP_TRANSPORT_INVALID.
 */
sdp_transport_e sdp_attr_get_cdsc_transport_type(void *sdp_ptr, u16 level,
                                                 u16 inst_num)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_TRANSPORT_INVALID);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC, 
                           inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_TRANSPORT_INVALID);
    } else {
        cdsc_p = attr_p->attr.cap_p;
        return (cdsc_p->transport);
    }
}

/* Function:    sdp_attr_get_cdsc_num_payload_types
 * Description: Returns the number of payload types associated with the  
 *              specified CDSC attribute.  If the attribute is invalid, 
 *              zero will be returned.  Application must validate the 
 *              attribute line before using this routine.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of payload types or zero.
 */
u16 sdp_attr_get_cdsc_num_payload_types (void *sdp_ptr, u16 level,
                                         u16 inst_num)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        cdsc_p = attr_p->attr.cap_p;
        return (cdsc_p->num_payloads);
    }
}

/* Function:    sdp_attr_get_cdsc_payload_type
 * Description: Returns the payload type of the specified payload for the 
 *              CDSC attribute line.  If the attr line or payload number is 
 *              invalid, zero will be returned.  Application must validate 
 *              the CDSC attr before using this routine.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 *              payload_num The payload number to retrieve.  Range is 
 *                          (1 - max num payloads).
 * Returns:     Payload type or zero.
 */
u16 sdp_attr_get_cdsc_payload_type (void *sdp_ptr, u16 level,
                                    u16 inst_num, u16 payload_num,
                                    sdp_payload_ind_e *indicator)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        cdsc_p = attr_p->attr.cap_p;
        if ((payload_num < 1) ||
            (payload_num > cdsc_p->num_payloads)) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s CDSC attribute, level %u instance %u, "
                          "payload num %u invalid.", sdp_p->debug_str,
                          level, inst_num, payload_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (0);
        } else {
            *indicator = cdsc_p->payload_indicator[payload_num-1];
            return (cdsc_p->payload_type[payload_num-1]);
        }
    }
}

/* Function:    sdp_attr_set_cdsc_media_type
 * Description: Sets the value of the media type parameter for the CDSC
 *              attribute line.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 *              media       Media type for the CDSC attribute.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_attr_set_cdsc_media_type (void *sdp_ptr, u16 level, 
                                           u16 inst_num, sdp_media_e media)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    cdsc_p = attr_p->attr.cap_p;
    cdsc_p->media = media;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_cdsc_transport_type
 * Description: Sets the value of the transport type parameter for the CDSC
 *              attribute line.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 *              transport   Transport type for the CDSC attribute.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_attr_set_cdsc_transport_type (void *sdp_ptr, u16 level, 
                                      u16 inst_num, sdp_transport_e transport)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    cdsc_p = attr_p->attr.cap_p;
    cdsc_p->transport = transport;
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_add_cdsc_payload_type
 * Description: Add a new payload type for the CDSC attribute line  
 *              specified. The new payload type will be added at the end
 *              of the payload type list.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              inst_num    The attribute instance number to check.
 *              payload_type The new payload type.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_attr_add_cdsc_payload_type (void *sdp_ptr, u16 level,
                                             u16 inst_num, u16 payload_type,
                                             sdp_payload_ind_e indicator)
{
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    cdsc_p = attr_p->attr.cap_p;
    cdsc_p->payload_indicator[cdsc_p->num_payloads] = indicator;
    cdsc_p->payload_type[cdsc_p->num_payloads++] = payload_type;
    return (SDP_SUCCESS);
}

/* Function:    sdp_media_dynamic_payload_valid
 * Description: Checks if the dynamic payload type passed in is defined
 *              on the media line m_line
 * Parameters:  sdp_ptr      The SDP handle returned by sdp_init_description.
 *              payload_type  Payload type to be checked
 *
 * Returns:     TRUE or FALSE. Returns TRUE if payload type is defined on the
 *              media line, else returns FALSE
 */
 
tinybool sdp_media_dynamic_payload_valid (void *sdp_ptr, u16 payload_type,
                                          u16 m_line)
{
   u16 p_type,m_ptype;
   ushort num_payload_types;
   sdp_payload_ind_e ind;
   tinybool payload_matches = FALSE;
   tinybool result = TRUE;
   sdp_t *sdp_p = (sdp_t *)sdp_ptr;
   
   if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
   }
   
   if ((payload_type < SDP_MIN_DYNAMIC_PAYLOAD) || 
       (payload_type > SDP_MAX_DYNAMIC_PAYLOAD)) {
       return FALSE;
   }
    
   num_payload_types =
       sdp_get_media_num_payload_types(sdp_p, m_line);
		
   for(p_type=1; p_type <=num_payload_types;p_type++){
	  
       m_ptype = (u16)sdp_get_media_payload_type(sdp_p,
                                            m_line, p_type, &ind);
       if (payload_type == m_ptype) {
           payload_matches = TRUE;
	   break;
       }

   }
   
   if (!payload_matches) {
       return FALSE;
   }
       
   return (result);
   
}

/* Function:    sdp_attr_set_rtr_confirm
 * Description: Sets the rtr confirm value  a= rtr:confirm.
 *              If this parameter is TRUE, the confirm parameter will be
 *              specified when the SDP description is built.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 *              confirm     New qos confirm parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_rtr_confirm (void *sdp_ptr, u16 level, u8 cap_num, 
                                       u16 inst_num,
                                       tinybool confirm)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTR, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(SDP_ATTR_RTR), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.rtr.confirm = confirm;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_rtr_confirm
 * Description: Returns the value of the rtr attribute confirm
 *              parameter specified for the given attribute.  Returns TRUE if
 *              the confirm parameter is specified.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Boolean value.
 */
tinybool sdp_attr_get_rtr_confirm (void *sdp_ptr, u16 level,
                                u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTR, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, 
                      sdp_get_attr_name(SDP_ATTR_RTR), level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.rtr.confirm);
    }
}



sdp_mediadir_role_e sdp_attr_get_comediadir_role (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_MEDIADIR_ROLE_UNKNOWN);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_DIRECTION, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Comediadir role attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_MEDIADIR_ROLE_UNKNOWN);
    } else {
        return (attr_p->attr.comediadir.role);
    }
}

/* Function:    sdp_attr_set_comediadir_role
 * Description: Sets the value of the comediadir role parameter
 *              for the direction attribute. 
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              comediadir_role The role of the comedia direction attribute
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_comediadir_role (void *sdp_ptr, u16 level, 
                                       u8 cap_num, u16 inst_num,
                                       sdp_mediadir_role_e comediadir_role)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_DIRECTION, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Comediadir role attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.comediadir.role = comediadir_role;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_silencesupp_enabled
 * Description: Returns the value of the silencesupp attribute enable
 *              parameter specified for the given attribute.  Returns TRUE if
 *              the confirm parameter is specified.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Boolean value.
 */
tinybool sdp_attr_get_silencesupp_enabled (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (FALSE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s silenceSuppEnable attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.silencesupp.enabled);
    }
}

/* Function:    sdp_attr_get_silencesupp_timer
 * Description: Returns the value of the silencesupp attribute timer
 *              parameter specified for the given attribute.  null_ind
 *              is set to TRUE if no value was specified, but instead the
 *              null "-" value was specified.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     16-bit timer value
 *              boolean null_ind
 */
u16 sdp_attr_get_silencesupp_timer (void *sdp_ptr, u16 level,
                                    u8 cap_num, u16 inst_num,
                                    tinybool *null_ind)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s silenceTimer attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        *null_ind = attr_p->attr.silencesupp.timer_null;
        return (attr_p->attr.silencesupp.timer);
    }
}

/* Function:    sdp_attr_get_silencesupp_pref
 * Description: Sets the silencesupp supppref value
 *              If this parameter is TRUE, the confirm parameter will be
 *              specified when the SDP description is built.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              confirm     New qos confirm parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_silencesupp_pref_e sdp_attr_get_silencesupp_pref (void *sdp_ptr,
                                                      u16 level, u8 cap_num, 
                                                      u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_SILENCESUPP_PREF_UNKNOWN);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s silence suppPref attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_SILENCESUPP_PREF_UNKNOWN);
    } else {
        return (attr_p->attr.silencesupp.pref);
    }
}

/* Function:    sdp_attr_get_silencesupp_siduse
 * Description: Returns the value of the silencesupp attribute siduse
 *              parameter specified for the given attribute.  If the given 
 *              attribute is not defined, SDP_QOS_STRENGTH_UNKNOWN is 
 *              returned.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     silencesupp siduse enum.
 */
sdp_silencesupp_siduse_e sdp_attr_get_silencesupp_siduse (void *sdp_ptr,
                                                          u16 level,
                                                          u8 cap_num,
                                                          u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_SILENCESUPP_SIDUSE_UNKNOWN);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s silence sidUse attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_SILENCESUPP_SIDUSE_UNKNOWN);
    } else {
        return (attr_p->attr.silencesupp.siduse);
    }
}

/* Function:    sdp_attr_get_silencesupp_fxnslevel
 * Description: Returns the value of the silencesupp attribute fxns
 *              (fixed noise) parameter specified for the given attribute.
 *              null_ind is set to TRUE if no value was specified,
 *              but instead the null "-" value was specified.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     7-bit fxns value
 *              boolean null_ind
 */
u8 sdp_attr_get_silencesupp_fxnslevel (void *sdp_ptr, u16 level,
                                       u8 cap_num, u16 inst_num,
                                       tinybool *null_ind)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s silence fxnslevel attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        *null_ind = attr_p->attr.silencesupp.fxnslevel_null;
        return (attr_p->attr.silencesupp.fxnslevel);
    }
}

/* Function:    sdp_attr_set_silencesupp_enabled
 * Description: Sets the silencesupp enable value
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              confirm     New silencesupp enable parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_silencesupp_enabled (void *sdp_ptr, u16 level,
                                               u8 cap_num, u16 inst_num,
                                               tinybool enable)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s silenceSuppEnable attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.silencesupp.enabled = enable;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_silencesupp_timer
 * Description: Sets the silencesupp timer value
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              value       New silencesupp timer parameter.
 *              null_ind    if TRUE, timer value is set to "-" instead of
 *                          the 16 bit numeric value
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_silencesupp_timer (void *sdp_ptr, u16 level,
                                             u8 cap_num, u16 inst_num,
                                             u16 value, tinybool null_ind)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s silenceTimer attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.silencesupp.timer = value;
        attr_p->attr.silencesupp.timer_null = null_ind;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_silencesupp_pref
 * Description: Sets the silencesupp supppref value
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              pref        New silencesupp supppref parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_silencesupp_pref (void *sdp_ptr, u16 level,
                                            u8 cap_num, u16 inst_num,
                                            sdp_silencesupp_pref_e pref)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s silence SuppPref attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.silencesupp.pref = pref;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_silencesupp_siduse
 * Description: Sets the silencesupp supppref value
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              siduse      New silencesupp siduse parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_silencesupp_siduse (void *sdp_ptr, u16 level,
                                              u8 cap_num, u16 inst_num,
                                              sdp_silencesupp_siduse_e siduse)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s silence sidUse attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.silencesupp.siduse = siduse;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_set_silencesupp_fxnslevel
 * Description: Sets the silencesupp timer value
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              value       New silencesupp timer parameter.
 *              null_ind    if TRUE, timer value is set to "-" instead of
 *                          the 16 bit numeric value
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_silencesupp_fxnslevel (void *sdp_ptr, u16 level,
                                                 u8 cap_num, u16 inst_num,
                                                 u16 value, tinybool null_ind)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s silenceTimer attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.silencesupp.fxnslevel = (u8)value;
        attr_p->attr.silencesupp.fxnslevel_null = null_ind;
        return (SDP_SUCCESS);
    }
}


/* Function:    sdp_attr_get_mptime_num_intervals
 * Description: Returns the number of intervals specified for the 
 *              given mptime attribute.  If the given attribute is not 
 *              defined, zero is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of intervals.
 */
u16 sdp_attr_get_mptime_num_intervals (
    void *sdp_ptr,
    u16 level,
    u8 cap_num,
    u16 inst_num) {
    
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return 0;
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_MPTIME, inst_num);
    if (attr_p != NULL) {
        return attr_p->attr.mptime.num_intervals;
    }
        
    if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
        SDP_ERROR("%s mptime attribute, level %u instance %u not found.",
                  sdp_p->debug_str, level, inst_num);
    }
    sdp_p->conf_p->num_invalid_param++;
    return 0;
}

/* Function:    sdp_attr_get_mptime_interval
 * Description: Returns the value of the specified interval for the 
 *              given mptime attribute.  If the given attribute is not 
 *              defined, zero is returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              interval_num The interval number to get.  Range is (1 - 
 *                          max num payloads).
 * Returns:     Interval.
 */
u16 sdp_attr_get_mptime_interval (
    void *sdp_ptr,
    u16 level,
    u8 cap_num,
    u16 inst_num,
    u16 interval_num) {
    
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return 0;
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_MPTIME, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s mptime attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return 0;
    }
        
    if ((interval_num<1) || (interval_num>attr_p->attr.mptime.num_intervals)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s mptime attribute, level %u instance %u, "
                      "invalid interval number %u requested.", 
                      sdp_p->debug_str, level, inst_num, interval_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return 0;
    }

    return attr_p->attr.mptime.intervals[interval_num-1];
}

/* Function:    sdp_attr_add_mptime_interval
 * Description: Add a new value to the list of intervals specified for
 *              the given mptime attribute.  The interval will be
 *              added to the end of the list so these values should be added
 *              in the order they will be displayed within the attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              mp_interval The interval to add.
 * Returns:     SDP_SUCCESS            Interval was added successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 *              SDP_INVALID_SDP_PTR    Supplied SDP pointer is invalid
 */
sdp_result_e sdp_attr_add_mptime_interval (
    void *sdp_ptr,
    u16 level,
    u8 cap_num,
    u16 inst_num,
    u16 mp_interval) {
    
    u16 interval_num;
    sdp_t *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_INVALID_SDP_PTR;
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_MPTIME, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s mptime attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }

    interval_num = attr_p->attr.mptime.num_intervals;
    if (interval_num>=SDP_MAX_PAYLOAD_TYPES) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s mptime attribute, level %u instance %u "
                      "exceeds maximum length.",
                      sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }
    
    attr_p->attr.mptime.intervals[interval_num] = mp_interval;
    ++attr_p->attr.mptime.num_intervals;
    return SDP_SUCCESS;
}



/* Function:    sdp_get_group_attr
 * Description: Returns the attribute parameter from the a=group:<>  
 *              line.  If no attrib has been set ,
 *              SDP_GROUP_ATTR_UNSUPPORTED will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       SDP_SESSION_LEVEL
 * Returns:     Valid attrib value or SDP_GROUP_ATTR_UNSUPPORTED.
 */
sdp_group_attr_e sdp_get_group_attr (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t          *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_GROUP_ATTR_UNSUPPORTED);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_GROUP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Group (a= group line) attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_GROUP_ATTR_UNSUPPORTED);
    } else {
       if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
           SDP_PRINT("%s Stream data group attr field is :%s ", 
		     sdp_p->debug_str, 
		     sdp_get_group_attr_name(attr_p->attr.stream_data.group_attr) );
        }
        return (attr_p->attr.stream_data.group_attr);
    }
}

/* Function:    sdp_set_group_attr
 * Description: Sets the value of the group attribute for the 
 *              a=group:<val> line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       SDP_SESSION_LEVEL
 *              group_attr  group attribute value ( LS/FID ).
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER/SDP_INVALID_SDP_PTR
*/

sdp_result_e sdp_set_group_attr (void *sdp_ptr, u16 level, 
                                 u8 cap_num, u16 inst_num,
                                 sdp_group_attr_e group_attr)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_GROUP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Group attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.stream_data.group_attr = group_attr;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_get_group_num_id
 * Description: Returns the number of ids from the a=group:<>  line.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       SDP_SESSION_LEVEL
 * Returns:    Num of group ids present or 0 if there is an error. 
 */
u16 sdp_get_group_num_id (void *sdp_ptr, u16 level,
                          u8 cap_num, u16 inst_num)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t          *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (0);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_GROUP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s a=group level attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Stream data group attr - num of ids is :%d ", 
                      sdp_p->debug_str, 
                      attr_p->attr.stream_data.num_group_id);
        }
    }
    return (attr_p->attr.stream_data.num_group_id);
}

/* Function:    sdp_set_group_num_id
 * Description: Sets the number og group ids that would be added on
 *              a=group:<val> <id> <id> ...line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       SDP_SESSION_LEVEL
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER/SDP_INVALID_SDP_PTR
 * Note:        The application must call this API to set the number of group
 *              ids to be provided before actually setting the group ids on 
 *              the a=group line.
*/

sdp_result_e sdp_set_group_num_id (void *sdp_ptr, u16 level, 
                                 u8 cap_num, u16 inst_num,
                                 u16 group_num_id)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_GROUP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Group attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else if ((group_num_id == 0) || (group_num_id > SDP_MAX_GROUP_STREAM_ID)){
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Number of group id value provided - %u is invalid\n",
                      sdp_p->debug_str, group_num_id);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.stream_data.num_group_id = group_num_id;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_get_group_id
 * Description: Returns the number of ids from the a=group:<>  line.  
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       SDP_SESSION_LEVEL
 *              id_num      Number of the id to retrieve. The range is (1 - 
 *                          SDP_MAX_GROUP_STREAM_ID)
 * Returns:    Value of the group id at the index specified or 
 *             SDP_INVALID_VALUE if an error
 */
int32 sdp_get_group_id (void *sdp_ptr, u16 level,
                        u8 cap_num, u16 inst_num, u16 id_num)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t          *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_GROUP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s a=group level attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Stream data group attr - num of ids is :%d ", 
                      sdp_p->debug_str, 
                      attr_p->attr.stream_data.num_group_id);
        }
        if ((id_num < 1) || (id_num > attr_p->attr.stream_data.num_group_id)) {
            return (SDP_INVALID_VALUE);
        }
    }
    return (attr_p->attr.stream_data.group_id_arr[id_num-1]);
}

/* Function:    sdp_set_group_id
 * Description: Sets the number og group ids that would be added on
 *              a=group:<val> <id> <id> ...line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       SDP_SESSION_LEVEL
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER/SDP_INVALID_SDP_PTR
*/

sdp_result_e sdp_set_group_id (void *sdp_ptr, u16 level, 
                               u8 cap_num, u16 inst_num,
                               u16 group_id)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    u16 num_group_id;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_GROUP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Group attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
	num_group_id = attr_p->attr.stream_data.num_group_id;
	if (num_group_id == SDP_MAX_GROUP_STREAM_ID) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s Max number of Group Ids already defined "
                      "for this group line %u", sdp_p->debug_str, level);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
        }
        attr_p->attr.stream_data.group_id_arr[num_group_id] = group_id;
        attr_p->attr.stream_data.num_group_id++;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_x_sidin
 * Description: Returns the attribute parameter from the a=X-sidin:<>  
 *              line.  If no attrib has been set NULL will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       media level index
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Pointer to sidin or NULL.
 */
const char* sdp_attr_get_x_sidin (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t          *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_X_SIDIN, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-sidin attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Stream X-sidin attr field is :%s ", 
	       	      sdp_p->debug_str, 
		      attr_p->attr.stream_data.x_sidin);
        }
        return (attr_p->attr.stream_data.x_sidin);
    }
}

/* Function:    sdp_attr_set_x_sidin
 * Description: Sets the value of the X-sidin parameter
 *              for the given attribute. The address is copied into the 
 *              SDP structure so application memory will not be 
 *              referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              sidin       Ptr to the sidin string 
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_x_sidin (void *sdp_ptr, u16 level, 
                                   u8 cap_num, u16 inst_num,
                                   const char *sidin)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_X_SIDIN, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-sidin attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        sstrncpy(attr_p->attr.stream_data.x_sidin, sidin, 
                 sizeof(attr_p->attr.stream_data.x_sidin)) ;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_x_sidout
 * Description: Returns the attribute parameter from the a=X-sidout:<>  
 *              line.  If no attrib has been set NULL will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       media level index
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Pointer to sidout or NULL.
 */
const char* sdp_attr_get_x_sidout (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t          *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_X_SIDOUT, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-sidout attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Stream X-sidout attr field is :%s ", 
	 	      sdp_p->debug_str, 
		      attr_p->attr.stream_data.x_sidout);
        }
        return (attr_p->attr.stream_data.x_sidout);
    }
}

/* Function:    sdp_attr_set_x_sidout
 * Description: Sets the value of the x-sidout parameter
 *              for the given attribute. The address is copied into the 
 *              SDP structure so application memory will not be 
 *              referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              sidout      Ptr to the sidout string.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_x_sidout (void *sdp_ptr, u16 level, 
                                   u8 cap_num, u16 inst_num,
                                   const char *sidout)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_X_SIDOUT, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-sidout attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        sstrncpy(attr_p->attr.stream_data.x_sidout, sidout, 
                 sizeof(attr_p->attr.stream_data.x_sidout)) ;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_x_confid
 * Description: Returns the attribute parameter from the a=X-confid:<>  
 *              line.  If no attrib has been set NULL will be returned.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       media level index
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Pointer to confid or NULL.
 */
const char* sdp_attr_get_x_confid (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num)
{
    sdp_t               *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t          *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (NULL);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_X_CONFID, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-confid attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Stream X-confid attr field is :%s ", 
		      sdp_p->debug_str, 
		      attr_p->attr.stream_data.x_confid);
        }
        return (attr_p->attr.stream_data.x_confid);
    }
}

/* Function:    sdp_attr_set_x_confid
 * Description: Sets the value of the X-confid parameter
 *              for the given attribute. The address is copied into the 
 *              SDP structure so application memory will not be 
 *              referenced by the SDP lib.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              confid      Ptr to the confid string.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_x_confid (void *sdp_ptr, u16 level, 
                                   u8 cap_num, u16 inst_num,
                                   const char *confid)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_X_CONFID, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s X-confid attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        sstrncpy(attr_p->attr.stream_data.x_confid, confid, 
                 sizeof(attr_p->attr.stream_data.x_confid)) ;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_set_source_filter
 * Description: Sets the value of the source filter attribute for the
 *              a=source-filter:<val> line.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       SDP_SESSION_LEVEL/Media level
 *              mode        Filter-mode (incl/excl)
 *              nettype     Network type
 *              addrtype    Address type of the destination
 *              dest_addr   Destination unicast/multicast address 
 *                          (ip-addr/ fqdn/ *)
 *              src_addr    One of the source address to which the filter
 *                          applies, i.e. process/drop the packets.
 *                          (More source addresses are added using 
 *                           sdp_include_new_filter_src_addr)
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER/SDP_INVALID_SDP_PTR
 */
sdp_result_e 
sdp_set_source_filter (void *sdp_ptr, u16 level, u8 cap_num, 
                       u16 inst_num, sdp_src_filter_mode_e mode,
                       sdp_nettype_e nettype, sdp_addrtype_e addrtype, 
                       const char *dest_addr, const char *src_addr)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    u16 index;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SOURCE_FILTER, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Source filter attribute, level %u instance %u "
                      "not found", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.source_filter.mode = mode;
    attr_p->attr.source_filter.nettype = nettype;
    attr_p->attr.source_filter.addrtype = addrtype;
    sstrncpy(attr_p->attr.source_filter.dest_addr, dest_addr,
             SDP_MAX_STRING_LEN+1);
    if (src_addr) {
        index = attr_p->attr.source_filter.num_src_addr;	     
        sstrncpy(attr_p->attr.source_filter.src_list[index], 
	         src_addr,SDP_MAX_STRING_LEN+1);
        /* Increment source list count if the api was invoked for
         * first time or else we're basically replacing the index 0
         * element in src-list.
         */
        ++attr_p->attr.source_filter.num_src_addr;
	SDP_PRINT("%s Source address (%s) number %d added to source filter", 
		  sdp_p->debug_str,src_addr, 
		  attr_p->attr.source_filter.num_src_addr);
        
    }

    return (SDP_SUCCESS);
}

/* Function:    sdp_include_new_filter_src_addr
 * Description: Adds source addresses to the list to which the filter applies
 *              This is to be invoked only as follow-up to
 *              sdp_set_source_filter() to include more source addresses
 * Parameters:  sdp_ptr     The SDP handle to which the filter attributes
 *                          were added using sdp_set_source_filter
 *              level       SDP_SESSION_LEVEL/Media level
 *              src_addr    Source address to be added to the list
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER/SDP_INVALID_SDP_PTR
 */

sdp_result_e 
sdp_include_new_filter_src_addr (void *sdp_ptr, u16 level, u8 cap_num, 
                                 u16 inst_num, const char *src_addr)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SOURCE_FILTER, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Source filter attribute, level %u instance %u "
                      "not found", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    if (attr_p->attr.source_filter.num_src_addr >= SDP_MAX_SRC_ADDR_LIST) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Max number of source addresses included for "
                      "filter for the instance %u", sdp_p->debug_str,
                       inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_FAILURE);
    }
    sstrncpy(attr_p->attr.source_filter.src_list[
                 attr_p->attr.source_filter.num_src_addr],
                 src_addr, SDP_MAX_STRING_LEN+1);
    ++attr_p->attr.source_filter.num_src_addr;

    return (SDP_SUCCESS);
}

/* Function:    sdp_get_source_filter_mode
 * Description: Gets the filter mode in internal representation
 * Parameters:  sdp_ptr   The SDP handle which contains the attributes
 *              level     SDP_SESSION_LEVEL/m-line number
 *              inst_num  The attribute instance number
 * Returns:     Filter mode (incl/excl/not present)
 */
sdp_src_filter_mode_e 
sdp_get_source_filter_mode (void *sdp_ptr, u16 level, u8 cap_num, 
                            u16 inst_num)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_FILTER_MODE_NOT_PRESENT);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SOURCE_FILTER, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Source filter attribute, level %u, "
                      "instance %u not found", sdp_p->debug_str,
                      level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_FILTER_MODE_NOT_PRESENT);
    }
    return (attr_p->attr.source_filter.mode);
}

/* Function:    sdp_get_filter_destination_attributes
 * Description: Gets the destination address parameters
 * Parameters:  Network type (optional), destination address type 
 *              (optional), and destination address (mandatory) variables 
 *              which gets updated.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER/SDP_INVALID_SDP_PTR
 */
sdp_result_e
sdp_get_filter_destination_attributes (void *sdp_ptr, u16 level, u8 cap_num,
                                       u16 inst_num, sdp_nettype_e *nettype,
                                       sdp_addrtype_e *addrtype, 
                                       char *dest_addr)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SOURCE_FILTER, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Source filter attribute, level %u instance %u "
                      "not found", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    if (nettype) {
        *nettype = attr_p->attr.source_filter.nettype;
    }
    if (addrtype) {
        *addrtype = attr_p->attr.source_filter.addrtype;
    }
    sstrncpy(dest_addr, attr_p->attr.source_filter.dest_addr,
             SDP_MAX_STRING_LEN+1);

    return (SDP_SUCCESS);
}

/* Function:    sdp_get_filter_source_address_count
 * Description: Gets the number of source addresses in the list
 * Parameters:  sdp_ptr   The SDP handle which contains the attributes
 *              level     SDP_SESSION_LEVEL/m-line number
 *              inst_num  The attribute instance number
 * Returns:     Source-list count
 */

int32
sdp_get_filter_source_address_count (void *sdp_ptr, u16 level, 
                                     u8 cap_num, u16 inst_num)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_VALUE);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SOURCE_FILTER, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Source filter attribute, level %u instance %u "
                      "not found", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    }
    return (attr_p->attr.source_filter.num_src_addr);
}

/* Function:    sdp_get_filter_source_address
 * Description: Gets one of the source address that is indexed by the user
 * Parameters:  sdp_ptr   The SDP handle which contains the attributes
 *              level     SDP_SESSION_LEVEL/m-line number
 *              inst_num  The attribute instance number
 *              src_addr_id User provided index (value in range between
 *                          0 to (SDP_MAX_SRC_ADDR_LIST-1) which obtains
 *                          the source addr corresponding to it.
 *              src_addr  The user provided variable which gets updated
 *                        with source address corresponding to the index
 */
sdp_result_e
sdp_get_filter_source_address (void *sdp_ptr, u16 level, u8 cap_num,
                               u16 inst_num, u16 src_addr_id, 
                               char *src_addr)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t *attr_p;

    src_addr[0] = '\0';

    if (src_addr_id >= SDP_MAX_SRC_ADDR_LIST) {
        return (SDP_INVALID_PARAMETER);
    }
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SOURCE_FILTER, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s Source filter attribute, level %u instance %u "
                      "not found", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    if (src_addr_id >= attr_p->attr.source_filter.num_src_addr) {
        return (SDP_INVALID_PARAMETER);
    }
    sstrncpy(src_addr, attr_p->attr.source_filter.src_list[src_addr_id],
             SDP_MAX_STRING_LEN+1);

    return (SDP_SUCCESS);
}

sdp_result_e
sdp_set_rtcp_unicast_mode (void *sdp_ptr, u16 level, u8 cap_num, 
                           u16 inst_num, sdp_rtcp_unicast_mode_e mode)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }
    if (mode >= SDP_RTCP_MAX_UNICAST_MODE) {
        return (SDP_INVALID_PARAMETER);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_RTCP_UNICAST, inst_num);

    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s RTCP Unicast attribute, level %u instance %u "
                      "not found", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.u32_val = mode;

    return (SDP_SUCCESS);
}

sdp_rtcp_unicast_mode_e 
sdp_get_rtcp_unicast_mode(void *sdp_ptr, u16 level, u8 cap_num,
                          u16 inst_num)
{
    sdp_t      *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_RTCP_UNICAST_MODE_NOT_PRESENT);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_RTCP_UNICAST, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s RTCP Unicast attribute, level %u, "
                      "instance %u not found", sdp_p->debug_str,
                      level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_RTCP_UNICAST_MODE_NOT_PRESENT);
    }
    return ((sdp_rtcp_unicast_mode_e)attr_p->attr.u32_val);
}


/* Function:    sdp_attr_get_sdescriptions_tag
 * Description: Returns the value of the sdescriptions tag
 *              parameter specified for the given attribute.
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Tag value or SDP_INVALID_VALUE (-2) if error encountered.
 */
 
int32 
sdp_attr_get_sdescriptions_tag (void *sdp_ptr, u16 level,
                                u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_INVALID_VALUE;
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SDESCRIPTIONS, inst_num);
    
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s srtp attribute tag, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_VALUE;
    } else {
        return attr_p->attr.srtp_context.tag;
    }
}

/* Function:    sdp_attr_get_sdescriptions_crypto_suite
 * Description: Returns the value of the sdescriptions crypto suite
 *              parameter specified for the given attribute. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, return the suite. If it's not,
 *              try to find the version 9. This assumes you cannot have both
 *              versions in the same SDP.
 *              
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     SDP_SRTP_UNKNOWN_CRYPTO_SUITE is returned if an error was 
 *              encountered otherwise the crypto suite is returned.
 */
 
sdp_srtp_crypto_suite_t
sdp_attr_get_sdescriptions_crypto_suite (void *sdp_ptr, u16 level,
                                         u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_SRTP_UNKNOWN_CRYPTO_SUITE;
    }

   
    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    
    if (attr_p == NULL) {
        /* There's no version 2 so now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
	if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute suite, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_SRTP_UNKNOWN_CRYPTO_SUITE;
	}
    }
    
    return attr_p->attr.srtp_context.suite;
    
}

/* Function:    sdp_attr_get_sdescriptions_key
 * Description: Returns the value of the sdescriptions master key
 *              parameter specified for the given attribute. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, return the key. If it's not,
 *              try to find the version 9. This assumes you cannot have both
 *              versions in the same SDP.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     NULL if error encountered or master key salt string
 */
 
const char*
sdp_attr_get_sdescriptions_key (void *sdp_ptr, u16 level,
                                u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return NULL;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    
    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
	attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
			       
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute key, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return NULL;
	}
    } 
    
    return (char*)attr_p->attr.srtp_context.master_key;
}


/* Function:    sdp_attr_get_sdescriptions_salt
 * Description: Returns the value of the sdescriptions master salt
 *              parameter specified for the given attribute. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, return the salt. If it's not,
 *              try to find the version 9. This assumes you cannot have both
 *              versions in the same SDP.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     NULL if error encountered or master key salt string
 */
 
const char*
sdp_attr_get_sdescriptions_salt (void *sdp_ptr, u16 level,
                                 u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return NULL;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    
    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
        
	if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute salt, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return NULL;
	}
    } 
        
    return (char*) attr_p->attr.srtp_context.master_salt;
    
}



/* Function:    sdp_attr_get_sdescriptions_lifetime
 * Description: Returns the value of the sdescriptions lifetime
 *              parameter specified for the given attribute.Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, return the lifetime. If it's
 *              not, try to find the version 9. This assumes you cannot have
 *              both versions in the same SDP.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     NULL if error encountered or lifetime string
 */
 
const char*
sdp_attr_get_sdescriptions_lifetime (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return NULL;
    }

    /* Try version 2 first. */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    
    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
			       
	if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute lifetime, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return NULL;
	}
    } 
    
    return (char*)attr_p->attr.srtp_context.master_key_lifetime;
    
}

/* Function:    sdp_attr_get_sdescriptions_mki
 * Description: Returns the value of the sdescriptions MKI value and length
 *              parameter of the specified attribute instance. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, return the MKI. If it's
 *              not, try to find version 9. This assumes you cannot have
 *              both versions in the same SDP.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              mki_value   application provided pointer that on exit
 *                          is set to the MKI value string if one exists.
 *              mki_length application provided pointer that on exit 
 *                         is set to the MKI length if one exists.
 * Returns:     SDP_SUCCESS no errors encountered otherwise sdp error 
 *              based upon the specific error.
 */
 
sdp_result_e
sdp_attr_get_sdescriptions_mki (void *sdp_ptr, u16 level,
                                u8 cap_num, u16 inst_num,
				const char **mki_value,
				u16 *mki_length)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    *mki_value = NULL;
    *mki_length = 0;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_INVALID_SDP_PTR;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    
    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
	if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute MKI, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_INVALID_PARAMETER;
	}
    }
    
    *mki_value = (char*)attr_p->attr.srtp_context.mki;
    *mki_length = attr_p->attr.srtp_context.mki_size_bytes;
    return SDP_SUCCESS;
   
}


/* Function:    sdp_attr_get_sdescriptions_session_params
 * Description: Returns the unparsed session parameters string. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, return session parameters. If
 *              it's not, try to find version 9. This assumes you cannot have
 *              both versions in the same SDP.
 *     
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     NULL if no session parameters were received in the sdp,
 *              otherwise returns a pointer to the start of the session
 *              parameters string. Note that the calling function should
 *              not free the returned pointer.
 */
 
const char*
sdp_attr_get_sdescriptions_session_params (void *sdp_ptr, u16 level,
                                           u8 cap_num, u16 inst_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return NULL;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    
    if (attr_p == NULL) {
        /* Couldn't find version 2 try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SDESCRIPTIONS, inst_num);
	if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute session params, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return NULL;
        }
    } 
    
    return attr_p->attr.srtp_context.session_parameters;
}


/* Function:    sdp_attr_get_sdescriptions_key_size
 * Description: Returns the master key size. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, return key size. If
 *              it's not, try to find version 9. This assumes you cannot have
 *              both versions in the same SDP.
 *     
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     0 (SDP_SDESCRIPTIONS_KEY_SIZE_UNKNOWN) if error was 
 *              encountered, otherwise key size.
 */
 
unsigned char 
sdp_attr_get_sdescriptions_key_size (void *sdp_ptr, 
                                     u16 level, 
				     u8 cap_num, 
				     u16 inst_num)
{

    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_SDESCRIPTIONS_KEY_SIZE_UNKNOWN;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    
    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
	if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute MKI, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_SDESCRIPTIONS_KEY_SIZE_UNKNOWN;
	}
    }
    
    return attr_p->attr.srtp_context.master_key_size_bytes;

}


/* Function:    sdp_attr_get_sdescriptions_salt_size
 * Description: Returns the salt key size. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, return salt size. If
 *              it's not, try to find version 9. This assumes you cannot have
 *              both versions in the same SDP.
 *     
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     0 (SDP_SDESCRIPTIONS_KEY_SIZE_UNKNOWN) if error was 
 *              encountered, otherwise salt size.
 */
 
unsigned char 
sdp_attr_get_sdescriptions_salt_size (void *sdp_ptr, 
                                      u16 level, 
				      u8 cap_num, 
				      u16 inst_num)
{

    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_SDESCRIPTIONS_KEY_SIZE_UNKNOWN;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    
    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
	if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute MKI, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_SDESCRIPTIONS_KEY_SIZE_UNKNOWN;
	}
    }
    
    return attr_p->attr.srtp_context.master_salt_size_bytes;

}


/* Function:    sdp_attr_get_srtp_crypto_selection_flags
 * Description: Returns the selection flags. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, return selection flags. If
 *              it's not, try to find version 9. This assumes you cannot have
 *              both versions in the same SDP.
 *              Currently only necessary for MGCP.
 *     
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     0 (SDP_SRTP_CRYPTO_SELECTION_FLAGS_UNKNOWN) if error was 
 *              encountered, otherwise selection flags.
 */
 
unsigned long 
sdp_attr_get_srtp_crypto_selection_flags (void *sdp_ptr, 
                                          u16 level, 
					  u8 cap_num, 
					  u16 inst_num)
{


    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    
    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_SRTP_CRYPTO_SELECTION_FLAGS_UNKNOWN;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    
    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
	if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute MKI, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_SRTP_CRYPTO_SELECTION_FLAGS_UNKNOWN;
	}
    }
    
    return attr_p->attr.srtp_context.selection_flags;
    
}



/* Function:    sdp_attr_set_sdescriptions_tag
 * Description: Sets the sdescriptions tag parameter
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              tag_num     tag
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
 
sdp_result_e 
sdp_attr_set_sdescriptions_tag (void *sdp_ptr, u16 level,
                                u8 cap_num, u16 inst_num,
                                int32 tag_num)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return (SDP_INVALID_SDP_PTR);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SDESCRIPTIONS, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            SDP_ERROR("%s srtp attribute tag, level %u instance %u "
                      "not found.", sdp_p->debug_str, level, inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.srtp_context.tag = tag_num;
        return (SDP_SUCCESS);
    }
}


/* Function:    sdp_attr_set_sdescriptions_crypto_suite
 * Description: Sets the sdescriptions crypto suite parameter. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, it will set the crypto suite
 *              for version 2. If it's not, try to find version 9. This assumes 
 *              you cannot have both versions in the same SDP.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              crypto_suite crypto suite
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
 
sdp_result_e 
sdp_attr_set_sdescriptions_crypto_suite (void *sdp_ptr, u16 level,
                                         u8 cap_num, u16 inst_num,
                                         sdp_srtp_crypto_suite_t crypto_suite)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;
    int         i;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_INVALID_SDP_PTR;
    }

    /* Try to find version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    if (attr_p == NULL) {
    
        /* Version 2 not found, try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
        if (attr_p == NULL) {
	
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute suite, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_INVALID_PARAMETER;
	}
    } 
    
    attr_p->attr.srtp_context.suite = crypto_suite;
    for (i=0; i < SDP_SRTP_MAX_NUM_CRYPTO_SUITES; i++) {
         /* For the specified crypto suite, get the size of the
	  * key and salt.
	  */
	 if (sdp_srtp_crypto_suite_array[i].crypto_suite_val == 
	                                    crypto_suite) {
					       
              attr_p->attr.srtp_context.master_key_size_bytes =
	      sdp_srtp_crypto_suite_array[i].key_size_bytes;
	      
	      attr_p->attr.srtp_context.master_salt_size_bytes = 
	      sdp_srtp_crypto_suite_array[i].salt_size_bytes;
					       
	 }
   }
   
   return SDP_SUCCESS;
    
}


/* Function:    sdp_attr_set_sdescriptions_key
 * Description: Sets the sdescriptions key parameter. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, it will set the key for
 *              version 2. If it's not, try to find version 9. This assumes 
 *              you cannot have both versions in the same SDP.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              key         buffer containing the key assumes null terminated
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
 
sdp_result_e 
sdp_attr_set_sdescriptions_key (void *sdp_ptr, u16 level,
                                u8 cap_num, u16 inst_num,
                                char *key)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;


    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_INVALID_SDP_PTR;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    if (attr_p == NULL) {
        /* Couldn't find version 2 try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SDESCRIPTIONS, inst_num);
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute key, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_INVALID_PARAMETER;
        }
	
    } 
    
    bcopy(key, attr_p->attr.srtp_context.master_key, 
	  SDP_SRTP_MAX_KEY_SIZE_BYTES);
	     
    return SDP_SUCCESS;
    
}


/* Function:    sdp_attr_set_sdescriptions_salt
 * Description: Sets the sdescriptions salt parameter. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, it will set the salt for
 *              version 2. If it's not, try to find version 9. This assumes 
 *              you cannot have both versions in the same SDP.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              salt        buffer containing the salt assumes null terminated
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
 
sdp_result_e 
sdp_attr_set_sdescriptions_salt (void *sdp_ptr, u16 level,
                                 u8 cap_num, u16 inst_num,
                                 char *salt)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;


    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_INVALID_SDP_PTR;
    }

    /* Try to find version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    if (attr_p == NULL) {
        /* Couldn't find version 2, try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
	if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp attribute salt, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_PARAMETER);
	}
	
    } 
    
    bcopy(salt, attr_p->attr.srtp_context.master_salt,
	  SDP_SRTP_MAX_SALT_SIZE_BYTES);
	     
    return SDP_SUCCESS;
}


/* Function:    sdp_attr_set_sdescriptions_lifetime
 * Description: Sets the sdescriptions lifetime parameter. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, it will set the lifetime for
 *              version 2. If it's not, try to find version 9. This assumes 
 *              you cannot have both versions in the same SDP.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              lifetime    buffer containing the lifetime assumes null terminated
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
 
sdp_result_e 
sdp_attr_set_sdescriptions_lifetime (void *sdp_ptr, u16 level,
                                     u8 cap_num, u16 inst_num,
                                     char *lifetime)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_INVALID_SDP_PTR;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    if (attr_p == NULL) {
        /* Couldn't find version 2, try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
	if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp lifetime attribute, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_INVALID_PARAMETER;
	}
	
    } 
    
    sstrncpy((char*)attr_p->attr.srtp_context.master_key_lifetime, lifetime, 
	     SDP_SRTP_MAX_LIFETIME_BYTES);
    return SDP_SUCCESS;
   
}


/* Function:    sdp_attr_set_sdescriptions_mki
 * Description: Sets the sdescriptions mki parameter compose of the MKI 
 *              value and length. Note that this is a common api for both
 *              version 2 and version 9 sdescriptions. It has no knowledge
 *              which version is being used so it will first try to find if 
 *              a version 2 sdescriptions attribute is present. If it is, it will 
 *              set the lifetime for version 2. If it's not, try to find version 9. 
 *              This assumes you cannot have both versions in the same SDP.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              mki_value   buffer containing the mki value. Assumes null 
 *                          terminated buffer.
 *              mki_length  length of the MKI
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
 
sdp_result_e 
sdp_attr_set_sdescriptions_mki (void *sdp_ptr, u16 level,
                                u8 cap_num, u16 inst_num,
                                char *mki_value,
				u16 mki_length)
{
    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_INVALID_SDP_PTR;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    if (attr_p == NULL) {
        /* Couldn't find version 2, try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp MKI attribute, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_INVALID_PARAMETER;
	}
    }
    
    sstrncpy((char*)attr_p->attr.srtp_context.mki, mki_value, 
	     SDP_SRTP_MAX_MKI_SIZE_BYTES);
    attr_p->attr.srtp_context.mki_size_bytes = mki_length;
    return SDP_SUCCESS;
  
}


/* Function:    sdp_attr_set_sdescriptions_key_size
 * Description: Sets the sdescriptions key size parameter. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, it will set the key for
 *              version 2. If it's not, try to find version 9. This assumes 
 *              you cannot have both versions in the same SDP.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              key_size    key size
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
 
sdp_result_e
sdp_attr_set_sdescriptions_key_size (void *sdp_ptr, 
                                     u16 level, 
				     u8 cap_num, 
				     u16 inst_num, 
				     unsigned char key_size)
				     
{

    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_INVALID_SDP_PTR;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    if (attr_p == NULL) {
        /* Couldn't find version 2, try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp MKI attribute, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_INVALID_PARAMETER;
	}
    }
    
    attr_p->attr.srtp_context.master_key_size_bytes = key_size;
    return SDP_SUCCESS;

}


/* Function:    sdp_attr_set_sdescriptions_key_size
 * Description: Sets the sdescriptions salt size parameter. Note that
 *              this is a common api for both version 2 and version 9
 *              sdescriptions. It has no knowledge which version is being
 *              used so it will first try to find if a version 2 sdescriptions
 *              attribute is present. If it is, it will set the salt for
 *              version 2. If it's not, try to find version 9. This assumes 
 *              you cannot have both versions in the same SDP.
 *
 * Parameters:  sdp_ptr     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.  
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              salt_size   salt size
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
 
sdp_result_e
sdp_attr_set_sdescriptions_salt_size (void *sdp_ptr, 
                                      u16 level, 
				      u8 cap_num, 
				      u16 inst_num, 
				      unsigned char salt_size)
{

    sdp_t       *sdp_p = (sdp_t *)sdp_ptr;
    sdp_attr_t  *attr_p;

    if (sdp_verify_sdp_ptr(sdp_p) == FALSE) {
        return SDP_INVALID_SDP_PTR;
    }

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                           SDP_ATTR_SRTP_CONTEXT, inst_num);
    if (attr_p == NULL) {
        /* Couldn't find version 2, try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num, 
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                SDP_ERROR("%s srtp MKI attribute, level %u instance %u "
                          "not found.", sdp_p->debug_str, level, inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_INVALID_PARAMETER;
	}
    }
    
    attr_p->attr.srtp_context.master_salt_size_bytes = salt_size;
    return SDP_SUCCESS;

}

