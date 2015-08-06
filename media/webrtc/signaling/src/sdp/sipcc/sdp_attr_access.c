/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "sdp_os_defs.h"
#include "sdp.h"
#include "sdp_private.h"

#include "CSFLog.h"

static const char* logTag = "sdp_attr_access";

/* Attribute access routines are all defined by the following parameters.
 *
 * sdp_p     The SDP handle returned by sdp_init_description.
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              attr_type   The type of attribute to add.
 *              inst_num    Pointer to a uint16_t in which to return the instance
 *                          number of the newly added attribute.
 * Returns:     SDP_SUCCESS            Attribute was added successfully.
 *              SDP_NO_RESOURCE        No memory avail for new attribute.
 *              SDP_INVALID_PARAMETER  Specified media line is not defined.
 */
sdp_result_e sdp_add_new_attr (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                               sdp_attr_e attr_type, uint16_t *inst_num)
{
    uint16_t          i;
    sdp_mca_t   *mca_p;
    sdp_mca_t   *cap_p;
    sdp_attr_t  *attr_p;
    sdp_attr_t  *new_attr_p;
    sdp_attr_t  *prev_attr_p=NULL;
    sdp_fmtp_t  *fmtp_p;
    sdp_comediadir_t  *comediadir_p;

    *inst_num = 0;

    if ((cap_num != 0) &&
        ((attr_type == SDP_ATTR_X_CAP) || (attr_type == SDP_ATTR_X_CPAR) ||
         (attr_type == SDP_ATTR_X_SQN) || (attr_type == SDP_ATTR_CDSC) ||
         (attr_type == SDP_ATTR_CPAR) || (attr_type == SDP_ATTR_SQN))) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            CSFLogDebug(logTag, "%s Warning: Invalid attribute type for X-cpar/cdsc "
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
        fmtp_p->packetization_mode = SDP_INVALID_PACKETIZATION_MODE_VALUE;
        fmtp_p->level_asymmetry_allowed = SDP_INVALID_LEVEL_ASYMMETRY_ALLOWED_VALUE;
        fmtp_p->annexb_required = FALSE;
        fmtp_p->annexa_required = FALSE;
        fmtp_p->maxval = 0;
        fmtp_p->bitrate = 0;
        fmtp_p->cif = 0;
        fmtp_p->qcif = 0;
        fmtp_p->profile = SDP_INVALID_VALUE;
        fmtp_p->level = SDP_INVALID_VALUE;
        fmtp_p->parameter_add = SDP_FMTP_UNUSED;
        fmtp_p->usedtx = SDP_FMTP_UNUSED;
        fmtp_p->stereo = SDP_FMTP_UNUSED;
        fmtp_p->useinbandfec = SDP_FMTP_UNUSED;
        fmtp_p->cbr = SDP_FMTP_UNUSED;
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

/* Function:    sdp_attr_num_instances
 * Description: Get the number of attributes of the specified type at
 *              the given level and capability level.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              attr_type   The type of attribute to add.
 *              num_attr_inst Pointer to a uint16_t in which to return the
 *                          number of attributes.
 * Returns:     SDP_SUCCESS            Attribute was added successfully.
 *              SDP_INVALID_PARAMETER  Specified media line is not defined.
 */
sdp_result_e sdp_attr_num_instances (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                                     sdp_attr_e attr_type, uint16_t *num_attr_inst)
{
    sdp_attr_t  *attr_p;
    sdp_result_e rc;
    static char  fname[] = "attr_num_instances";

    *num_attr_inst = 0;

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
    int          i;

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


    if (attr_p->type == SDP_ATTR_GROUP) {
        for (i = 0; i < attr_p->attr.stream_data.num_group_id; i++) {
            SDP_FREE(attr_p->attr.stream_data.group_ids[i]);
        }
    } else if (attr_p->type == SDP_ATTR_MSID_SEMANTIC) {
        for (i = 0; i < SDP_MAX_MEDIA_STREAMS; ++i) {
            SDP_FREE(attr_p->attr.msid_semantic.msids[i]);
        }
    }

    /* Now free the actual attribute memory. */
    SDP_FREE(attr_p);

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
sdp_result_e sdp_find_attr_list (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
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
                CSFLogError(logTag, "%s %s, invalid capability %u at "
                          "level %u specified.", sdp_p->debug_str, fname,
                          (unsigned)cap_num, (unsigned)level);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (SDP_INVALID_CAPABILITY);
        }
        cap_p = cap_attr_p->attr.cap_p;
        *attr_p = cap_p->media_attrs_p;
    }

    return (SDP_SUCCESS);
}

/* Find fmtp inst_num with correct payload value or -1 for failure */
int sdp_find_fmtp_inst (sdp_t *sdp_p, uint16_t level, uint16_t payload_num)
{
    uint16_t          attr_count=0;
    sdp_mca_t   *mca_p;
    sdp_attr_t  *attr_p;

    /* Attr is at a media level */
    mca_p = sdp_find_media_level(sdp_p, level);
    if (mca_p == NULL) {
      return (-1);
    }
    for (attr_p = mca_p->media_attrs_p; attr_p != NULL;
         attr_p = attr_p->next_p) {
      if (attr_p->type == SDP_ATTR_FMTP) {
        attr_count++;
        if (attr_p->attr.fmtp.payload_num == payload_num) {
          return (attr_count);
        }
      }
    }

    return (-1);

}

/* Function:    sdp_find_attr
 * Description: Find the specified attribute in an SDP structure.
 *              Note: This is not an API for the application but an internal
 *              routine used by the SDP library.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              attr_type   The type of attribute to find.
 *              inst_num    The instance num of the attribute to find.
 *                          Range should be (1 - max num insts of this
 *                          particular type of attribute at this level).
 * Returns:     Pointer to the attribute or NULL if not found.
 */
sdp_attr_t *sdp_find_attr (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                           sdp_attr_e attr_type, uint16_t inst_num)
{
    uint16_t          attr_count=0;
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
sdp_attr_t *sdp_find_capability (sdp_t *sdp_p, uint16_t level, uint8_t cap_num)
{
    uint8_t           cur_cap_num=0;
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
        CSFLogError(logTag, "%s Unable to find specified capability (level %u, "
                  "cap_num %u).", sdp_p->debug_str, (unsigned)level, (unsigned)cap_num);
    }
    sdp_p->conf_p->num_invalid_param++;
    return (NULL);
}

/* Function:    sdp_attr_valid(sdp_t *sdp_p)
 * Description: Returns true or false depending on whether the specified
 *              instance of the given attribute has been defined at the
 *              given level.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              attr_type   The attribute type to validate.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_valid (sdp_t *sdp_p, sdp_attr_e attr_type, uint16_t level,
                         uint8_t cap_num, uint16_t inst_num)
{
    if (sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num) == NULL) {
        return (FALSE);
    } else {
        return (TRUE);
    }
}

/* Function:    sdp_attr_line_number(sdp_t *sdp_p)
 * Description: Returns the line number this attribute appears on.
 *              Only works if the SDP was parsed rather than created
 *              locally.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              attr_type   The attribute type to validate.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     line number, or 0 if an error
 */
uint32_t sdp_attr_line_number (sdp_t *sdp_p, sdp_attr_e attr_type, uint16_t level,
                          uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num);
    if (attr_p == NULL) {
        return 0;
    } else {
        return attr_p->line_number;
    }
}

static boolean sdp_attr_is_simple_string(sdp_attr_e attr_type) {
    if ((attr_type != SDP_ATTR_BEARER) &&
        (attr_type != SDP_ATTR_CALLED) &&
        (attr_type != SDP_ATTR_CONN_TYPE) &&
        (attr_type != SDP_ATTR_DIALED) &&
        (attr_type != SDP_ATTR_DIALING) &&
        (attr_type != SDP_ATTR_FRAMING) &&
        (attr_type != SDP_ATTR_MID) &&
        (attr_type != SDP_ATTR_X_SIDIN) &&
        (attr_type != SDP_ATTR_X_SIDOUT)&&
        (attr_type != SDP_ATTR_X_CONFID) &&
        (attr_type != SDP_ATTR_LABEL) &&
        (attr_type != SDP_ATTR_IDENTITY) &&
        (attr_type != SDP_ATTR_ICE_OPTIONS) &&
        (attr_type != SDP_ATTR_IMAGEATTR)) {
      return FALSE;
    }
    return TRUE;
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              attr_type   The simple string attribute type.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Pointer to the parameter value.
 */
const char *sdp_attr_get_simple_string (sdp_t *sdp_p, sdp_attr_e attr_type,
                                        uint16_t level, uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    if (!sdp_attr_is_simple_string(attr_type)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Attribute type is not a simple string (%s)",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type),
                      (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        return (attr_p->attr.string_val);
    }
}

static boolean sdp_attr_is_simple_u32(sdp_attr_e attr_type) {
    if ((attr_type != SDP_ATTR_EECID) &&
        (attr_type != SDP_ATTR_PTIME) &&
        (attr_type != SDP_ATTR_MAXPTIME) &&
        (attr_type != SDP_ATTR_T38_VERSION) &&
        (attr_type != SDP_ATTR_T38_MAXBITRATE) &&
        (attr_type != SDP_ATTR_T38_MAXBUFFER) &&
        (attr_type != SDP_ATTR_T38_MAXDGRAM) &&
        (attr_type != SDP_ATTR_X_SQN) &&
        (attr_type != SDP_ATTR_TC1_PAYLOAD_BYTES) &&
        (attr_type != SDP_ATTR_TC1_WINDOW_SIZE) &&
        (attr_type != SDP_ATTR_TC2_PAYLOAD_BYTES) &&
        (attr_type != SDP_ATTR_TC2_WINDOW_SIZE) &&
        (attr_type != SDP_ATTR_FRAMERATE)) {
        return FALSE;
    }

    return TRUE;
}

/* Function:    sdp_attr_get_simple_u32
 * Description: Returns an unsigned 32-bit attribute parameter.  This
 *              routine can only be called for attributes that have just
 *              one uint32_t parameter.  If the given attribute is not defined,
 *              zero will be returned.  There is no way for the application
 *              to determine if zero is the actual value or the attribute
 *              wasn't defined, so the application must use the
 *              sdp_attr_valid function to determine this.
 *              Attributes with a simple uint32_t parameter currently include:
 *              eecid, ptime, T38FaxVersion, T38maxBitRate, T38FaxMaxBuffer,
 *              T38FaxMaxDatagram, X-sqn, TC1PayloadBytes, TC1WindowSize,
 *              TC2PayloadBytes, TC2WindowSize, rtcp.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              attr_type   The simple uint32_t attribute type.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     uint32_t parameter value.
 */
uint32_t sdp_attr_get_simple_u32 (sdp_t *sdp_p, sdp_attr_e attr_type, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    if (!sdp_attr_is_simple_u32(attr_type)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Attribute type is not a simple uint32_t (%s)",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type),
                      (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.u32_val);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              attr_type   The simple boolean attribute type.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Boolean value.
 */
tinybool sdp_attr_get_simple_boolean (sdp_t *sdp_p, sdp_attr_e attr_type,
                                      uint16_t level, uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    if ((attr_type != SDP_ATTR_T38_FILLBITREMOVAL) &&
        (attr_type != SDP_ATTR_T38_TRANSCODINGMMR) &&
        (attr_type != SDP_ATTR_T38_TRANSCODINGJBIG) &&
        (attr_type != SDP_ATTR_TMRGWXID)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Attribute type is not a simple boolean (%s)",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type));
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    }

    attr_p = sdp_find_attr(sdp_p, level, cap_num, attr_type, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(attr_type),
                      (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.boolean_val);
    }
}

/*
 * sdp_attr_get_maxprate
 *
 * This function is used by the application layer to get the packet-rate
 * within the maxprate attribute.
 *
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              inst_num    The attribute instance number to set.
 *
 * Returns a pointer to a constant char array that stores the packet-rate,
 * OR null if the attribute does not exist.
 */
const char*
sdp_attr_get_maxprate (sdp_t *sdp_p, uint16_t level, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_MAXPRATE, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Attribute %s, level %u instance %u not found.",
                      sdp_p->debug_str, sdp_get_attr_name(SDP_ATTR_MAXPRATE),
                      (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        return (attr_p->attr.string_val);
    }
}

/* Function:    sdp_attr_get_t38ratemgmt
 * Description: Returns the value of the t38ratemgmt attribute
 *              parameter specified for the given attribute.  If the given
 *              attribute is not defined, SDP_T38_UNKNOWN_RATE is returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Ratemgmt value.
 */
sdp_t38_ratemgmt_e sdp_attr_get_t38ratemgmt (sdp_t *sdp_p, uint16_t level,
                                             uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_T38_RATEMGMT, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s t38ratemgmt attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_T38_UNKNOWN_RATE);
    } else {
        return (attr_p->attr.t38ratemgmt);
    }
}

/* Function:    sdp_attr_get_t38udpec
 * Description: Returns the value of the t38udpec attribute
 *              parameter specified for the given attribute.  If the given
 *              attribute is not defined, SDP_T38_UDPEC_UNKNOWN is returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     UDP EC value.
 */
sdp_t38_udpec_e sdp_attr_get_t38udpec (sdp_t *sdp_p, uint16_t level,
                                       uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_T38_UDPEC, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s t38udpec attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_T38_UDPEC_UNKNOWN);
    } else {
        return (attr_p->attr.t38udpec);
    }
}

/* Function:    sdp_get_media_direction
 * Description: Determines the direction defined for a given level.  The
 *              direction will be inactive, sendonly, recvonly, or sendrecv
 *              as determined by the last of these attributes specified at
 *              the given level.  If none of these attributes are specified,
 *              the direction will be sendrecv by default.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 * Returns:     An SDP direction enum value.
 */
sdp_direction_e sdp_get_media_direction (sdp_t *sdp_p, uint16_t level,
                                         uint8_t cap_num)
{
    sdp_mca_t   *mca_p;
    sdp_attr_t  *attr_p;
    sdp_direction_e direction = SDP_DIRECTION_SENDRECV;

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
            CSFLogDebug(logTag, "%s Warning: Invalid cap_num for media direction.",
                     sdp_p->debug_str);
        }
    }

    return (direction);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     Qos strength value.
 */
sdp_qos_strength_e sdp_attr_get_qos_strength (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, sdp_attr_e qos_attr, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            CSFLogDebug(logTag, "%s Warning: Invalid QOS attribute specified for"
                     "get qos strength.", sdp_p->debug_str);
        }
        return (SDP_QOS_STRENGTH_UNKNOWN);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     Qos direction value.
 */
sdp_qos_dir_e sdp_attr_get_qos_direction (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, sdp_attr_e qos_attr, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            CSFLogDebug(logTag, "%s Warning: Invalid QOS attribute specified "
                     "for get qos direction.", sdp_p->debug_str);
        }
        return (SDP_QOS_DIR_UNKNOWN);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     Qos direction value.
 */
sdp_qos_status_types_e sdp_attr_get_qos_status_type (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, sdp_attr_e qos_attr, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            CSFLogDebug(logTag, "%s Warning: Invalid QOS attribute specified "
                     "for get qos status_type.", sdp_p->debug_str);
        }
        return (SDP_QOS_STATUS_TYPE_UNKNOWN);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     Boolean value.
 */
tinybool sdp_attr_get_qos_confirm (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, sdp_attr_e qos_attr, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    if (sdp_validate_qos_attr(qos_attr) == FALSE) {
        if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
            CSFLogDebug(logTag, "%s Warning: Invalid QOS attribute specified "
                     "for get qos confirm.", sdp_p->debug_str);
        }
        return (FALSE);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.qos.confirm);
    }
}

/* Function:    sdp_attr_get_curr_type
 * Description: Returns the value of the curr attribute status_type
 *              parameter specified for the given attribute.  If the given
 *              attribute is not defined, SDP_CURR_UNKNOWN_TYPE is
 *              returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     Curr type value.
 */
sdp_curr_type_e sdp_attr_get_curr_type (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, sdp_attr_e qos_attr, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     DES type value.
 */
sdp_des_type_e sdp_attr_get_des_type (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, sdp_attr_e qos_attr, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              qos_attr    The specific type of qos attribute.  May be
 *                          qos, secure, X-pc-qos, or X-qos.
 *              inst_num    The attribute instance number to check.
 * Returns:     CONF type value.
 */
sdp_conf_type_e sdp_attr_get_conf_type (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, sdp_attr_e qos_attr, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, qos_attr, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(qos_attr), (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_CONF_UNKNOWN_TYPE);
    } else {
        return (attr_p->attr.conf.type);
    }
}

/* Function:    sdp_attr_get_subnet_nettype
 * Description: Returns the value of the subnet attribute network type
 *              parameter specified for the given attribute.  If the given
 *              attribute is not defined, SDP_NT_INVALID is returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Nettype value.
 */
sdp_nettype_e sdp_attr_get_subnet_nettype (sdp_t *sdp_p, uint16_t level,
                                           uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Addrtype value.
 */
sdp_addrtype_e sdp_attr_get_subnet_addrtype (sdp_t *sdp_p, uint16_t level,
                                             uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Pointer to address or NULL.
 */
const char *sdp_attr_get_subnet_addr (sdp_t *sdp_p, uint16_t level,
                                      uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Prefix value or SDP_INVALID_PARAM.
 */
int32_t sdp_attr_get_subnet_prefix (sdp_t *sdp_p, uint16_t level,
                                  uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SUBNET, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Subnet attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.subnet.prefix);
    }
}

/* Function:    sdp_attr_rtpmap_payload_valid
 * Description: Returns true or false depending on whether an rtpmap
 *              attribute was specified with the given payload value
 *              at the given level.  If it was, the instance number of
 *              that attribute is returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number of the attribute
 *                          found is returned via this param.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_rtpmap_payload_valid (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                                        uint16_t *inst_num, uint16_t payload_type)
{
    uint16_t          i;
    sdp_attr_t  *attr_p;
    uint16_t          num_instances;

    *inst_num = 0;

    if (sdp_attr_num_instances(sdp_p, level, cap_num,
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Payload type value.
 */
uint16_t sdp_attr_get_rtpmap_payload_type (sdp_t *sdp_p, uint16_t level,
                                      uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtpmap attribute, level %u instance %u "
                                "not found.",
                                sdp_p->debug_str,
                                (unsigned)level,
                                (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Codec value or SDP_CODEC_INVALID.
 */
const char *sdp_attr_get_rtpmap_encname (sdp_t *sdp_p, uint16_t level,
                                         uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtpmap attribute, level %u instance %u "
                                "not found.",
                                sdp_p->debug_str,
                                (unsigned)level,
                                (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Clockrate value.
 */
uint32_t sdp_attr_get_rtpmap_clockrate (sdp_t *sdp_p, uint16_t level,
                                   uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtpmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of channels param or zero.
 */
uint16_t sdp_attr_get_rtpmap_num_chan (sdp_t *sdp_p, uint16_t level,
                                  uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTPMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtpmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.transport_map.num_chan);
    }
}

/* Function:    sdp_attr_get_ice_attribute
 * Description: Returns the value of an ice attribute at a given level
 *
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              ice_attrib  Returns an ice attrib string
 * Returns:
 *              SDP_SUCCESS           Attribute param was set successfully.
 *              SDP_INVALID_SDP_PTR   SDP pointer invalid
 *              SDP_INVALID_PARAMETER Specified attribute is not defined.
 */

sdp_result_e sdp_attr_get_ice_attribute (sdp_t *sdp_p, uint16_t level,
                                  uint8_t cap_num, sdp_attr_e sdp_attr, uint16_t inst_num,
                                  char **out)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, sdp_attr, inst_num);
    if (attr_p != NULL) {
        *out = attr_p->attr.ice_attr;
        return (SDP_SUCCESS);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s ice attribute, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
}

/* Function:    sdp_attr_is_present
 * Description: Returns a boolean value based on attribute being present or
 *              not
 *
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              attr_type   The attribute type.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 * Returns:
 *              Boolean value.
 */

tinybool sdp_attr_is_present (sdp_t *sdp_p, sdp_attr_e attr_type, uint16_t level,
                              uint8_t cap_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, attr_type, 1);
    if (attr_p != NULL) {
        return (TRUE);
    }
    if (sdp_p->debug_flag[SDP_DEBUG_WARNINGS]) {
        CSFLogDebug(logTag, "%s Attribute %s, level %u not found.",
                    sdp_p->debug_str, sdp_get_attr_name(attr_type), level);
    }

    return (FALSE);
}



/* Function:    sdp_attr_get_rtcp_mux_attribute
 * Description: Returns the value of an rtcp-mux attribute at a given level
 *
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              rtcp_mux    Returns an rtcp-mux attrib bool
 * Returns:
 *              SDP_SUCCESS           Attribute param was set successfully.
 *              SDP_INVALID_SDP_PTR   SDP pointer invalid
 *              SDP_INVALID_PARAMETER Specified attribute is not defined.
 */
sdp_result_e sdp_attr_get_rtcp_mux_attribute (sdp_t *sdp_p, uint16_t level,
                                  uint8_t cap_num, sdp_attr_e sdp_attr, uint16_t inst_num,
                                  tinybool *rtcp_mux)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, sdp_attr, inst_num);
    if (attr_p != NULL) {
        *rtcp_mux = attr_p->attr.boolean_val;
        return (SDP_SUCCESS);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s rtcp-mux attribute, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
}

/* Function:    sdp_attr_get_setup_attribute
 * Description: Returns the value of a setup attribute at a given level
 *
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              setup_type  Returns sdp_setup_type_e enum
 * Returns:
 *              SDP_SUCCESS           Attribute param was set successfully.
 *              SDP_INVALID_SDP_PTR   SDP pointer invalid
 *              SDP_INVALID_PARAMETER Specified attribute is not defined.
 */
sdp_result_e sdp_attr_get_setup_attribute (sdp_t *sdp_p, uint16_t level,
    uint8_t cap_num, uint16_t inst_num, sdp_setup_type_e *setup_type)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SETUP, inst_num);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag,
                "%s setup attribute, level %u instance %u not found.",
                sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }

    *setup_type = attr_p->attr.setup;
    return SDP_SUCCESS;
}

/* Function:    sdp_attr_get_connection_attribute
 * Description: Returns the value of a connection attribute at a given level
 *
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              connection_type  Returns sdp_connection_type_e enum
 * Returns:
 *              SDP_SUCCESS           Attribute param was set successfully.
 *              SDP_INVALID_SDP_PTR   SDP pointer invalid
 *              SDP_INVALID_PARAMETER Specified attribute is not defined.
 */
sdp_result_e sdp_attr_get_connection_attribute (sdp_t *sdp_p, uint16_t level,
    uint8_t cap_num, uint16_t inst_num, sdp_connection_type_e *connection_type)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_CONNECTION,
        inst_num);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag,
                "%s setup attribute, level %u instance %u not found.",
                sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }

    *connection_type = attr_p->attr.connection;
    return SDP_SUCCESS;
}

/* Function:    sdp_attr_get_dtls_fingerprint_attribute
 * Description: Returns the value of dtls fingerprint attribute at a given level
 *
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              dtls_fingerprint  Returns an dtls fingerprint attrib string
 * Returns:
 *              SDP_SUCCESS           Attribute param was set successfully.
 *              SDP_INVALID_SDP_PTR   SDP pointer invalid
 *              SDP_INVALID_PARAMETER Specified attribute is not defined.
 */
sdp_result_e sdp_attr_get_dtls_fingerprint_attribute (sdp_t *sdp_p, uint16_t level,
                                  uint8_t cap_num, sdp_attr_e sdp_attr, uint16_t inst_num,
                                  char **out)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, sdp_attr, inst_num);
    if (attr_p != NULL) {
        *out = attr_p->attr.string_val;
        return (SDP_SUCCESS);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s dtls fingerprint attribute, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
}

/* Function:    sdp_attr_sprtmap_payload_valid
 * Description: Returns true or false depending on whether an sprtmap
 *              attribute was specified with the given payload value
 *              at the given level.  If it was, the instance number of
 *              that attribute is returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number of the attribute
 *                          found is returned via this param.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_sprtmap_payload_valid (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                                        uint16_t *inst_num, uint16_t payload_type)
{
    uint16_t          i;
    sdp_attr_t  *attr_p;
    uint16_t          num_instances;

    *inst_num = 0;

    if (sdp_attr_num_instances(sdp_p, level, cap_num,
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Payload type value.
 */
uint16_t sdp_attr_get_sprtmap_payload_type (sdp_t *sdp_p, uint16_t level,
                                      uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Codec value or SDP_CODEC_INVALID.
 */
const char *sdp_attr_get_sprtmap_encname (sdp_t *sdp_p, uint16_t level,
                                         uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Clockrate value.
 */
uint32_t sdp_attr_get_sprtmap_clockrate (sdp_t *sdp_p, uint16_t level,
                                   uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of channels param or zero.
 */
uint16_t sdp_attr_get_sprtmap_num_chan (sdp_t *sdp_p, uint16_t level,
                                  uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SPRTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s sprtmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.transport_map.num_chan);
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

/* Function:    tinybool sdp_attr_fmtp_valid(sdp_t *sdp_p)
 * Description: Returns true or false depending on whether an fmtp
 *              attribute was specified with the given payload value
 *              at the given level.  If it was, the instance number of
 *              that attribute is returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_fmtp_payload_valid (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                                      uint16_t *inst_num, uint16_t payload_type)
{
    uint16_t          i;
    sdp_attr_t  *attr_p;
    uint16_t          num_instances;

    if (sdp_attr_num_instances(sdp_p, level, cap_num,
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Payload type value.
 */
uint16_t sdp_attr_get_fmtp_payload_type (sdp_t *sdp_p, uint16_t level,
                                    uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              low_val     Low value of the range. Range is 0-255.
 *              high_val    High value of the range.
 * Returns:     SDP_FULL_MATCH, SDP_PARTIAL_MATCH, SDP_NO_MATCH
 */
sdp_ne_res_e sdp_attr_fmtp_is_range_set (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                                         uint16_t inst_num, uint8_t low_val, uint8_t high_val)
{
    uint16_t          i;
    uint32_t          mapword;
    uint32_t          bmap;
    uint32_t          num_vals = 0;
    uint32_t          num_vals_set = 0;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              appl_maxval Max event value supported by Appl. Range is 0-255.
 *              evt_array   Bitmap containing events supported by application.
 * Returns:     TRUE, FALSE
 */
tinybool
sdp_attr_fmtp_valid(sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                    uint16_t inst_num, uint16_t appl_maxval, uint32_t* evt_array)
{
    uint16_t          i;
    uint32_t          mapword;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              payload_type New payload type value.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_set_fmtp_payload_type (sdp_t *sdp_p, uint16_t level,
                                             uint8_t cap_num, uint16_t inst_num,
                                             uint16_t payload_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        attr_p->attr.fmtp.payload_num = payload_num;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_range
 * Description: Get a range of named events for an fmtp attribute.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              bmap        The 8 word data array holding the bitmap
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_attr_get_fmtp_range (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                                      uint16_t inst_num, uint32_t *bmap)
{
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    memcpy(bmap, fmtp_p->bmap, SDP_NE_NUM_BMAP_WORDS * sizeof(uint32_t) );

    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_clear_fmtp_range
 * Description: Clear a range of named events for an fmtp attribute. The low
 *              value specified must be <= the high value.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              low_val     The low value of the range. Range is 0-255
 *              high_val    The high value of the range.  May be == low.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_attr_clear_fmtp_range (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                                        uint16_t inst_num, uint8_t low_val, uint8_t high_val)
{
    uint16_t          i;
    uint32_t          mapword;
    uint32_t          bmap;
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  src_sdp_p The SDP handle returned by sdp_init_description.
 *              dst_sdp_p The SDP handle returned by sdp_init_description.
 *              src_level   The level of the src fmtp attribute.
 *              dst_level   The level to the dst fmtp attribute.
 *              src_cap_num The capability number of the src attr.
 *              dst_cap_num The capability number of the dst attr.
 *              src_inst_numh The attribute instance of the src attr.
 *              dst_inst_numh The attribute instance of the dst attr.
 * Returns:     SDP_FULL_MATCH, SDP_PARTIAL_MATCH, SDP_NO_MATCH.
 */
sdp_ne_res_e sdp_attr_compare_fmtp_ranges (sdp_t *src_sdp_p,sdp_t *dst_sdp_p,
                                           uint16_t src_level, uint16_t dst_level,
                                           uint8_t src_cap_num, uint8_t dst_cap_num,
                                           uint16_t src_inst_num, uint16_t dst_inst_num)
{
    uint16_t          i,j;
    uint32_t          bmap;
    uint32_t          num_vals_match = 0;
    sdp_attr_t  *src_attr_p;
    sdp_attr_t  *dst_attr_p;
    sdp_fmtp_t  *src_fmtp_p;
    sdp_fmtp_t  *dst_fmtp_p;

    src_attr_p = sdp_find_attr(src_sdp_p, src_level, src_cap_num,
                               SDP_ATTR_FMTP, src_inst_num);
    dst_attr_p = sdp_find_attr(dst_sdp_p, dst_level, dst_cap_num,
                               SDP_ATTR_FMTP, dst_inst_num);
    if ((src_attr_p == NULL) || (dst_attr_p == NULL)) {
        if (src_sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s source or destination fmtp attribute for "
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
 * Parameters:  src_sdp_p The SDP handle returned by sdp_init_description.
 *              dst_sdp_p The SDP handle returned by sdp_init_description.
 *              src_level   The level of the src fmtp attribute.
 *              dst_level   The level to the dst fmtp attribute.
 *              src_cap_num The capability number of the src attr.
 *              dst_cap_num The capability number of the dst attr.
 *              src_inst_numh The attribute instance of the src attr.
 *              dst_inst_numh The attribute instance of the dst attr.
 * Returns:     SDP_SUCCESS
 */
sdp_result_e sdp_attr_copy_fmtp_ranges (sdp_t *src_sdp_p, sdp_t *dst_sdp_p,
                                        uint16_t src_level, uint16_t dst_level,
                                        uint8_t src_cap_num, uint8_t dst_cap_num,
                                        uint16_t src_inst_num, uint16_t dst_inst_num)
{
    uint16_t          i;
    sdp_attr_t  *src_attr_p;
    sdp_attr_t  *dst_attr_p;
    sdp_fmtp_t  *src_fmtp_p;
    sdp_fmtp_t  *dst_fmtp_p;

    if (!src_sdp_p || !dst_sdp_p) {
        return (SDP_INVALID_SDP_PTR);
    }

    src_attr_p = sdp_find_attr(src_sdp_p, src_level, src_cap_num,
                               SDP_ATTR_FMTP, src_inst_num);
    dst_attr_p = sdp_find_attr(dst_sdp_p, dst_level, dst_cap_num,
                               SDP_ATTR_FMTP, dst_inst_num);
    if ((src_attr_p == NULL) || (dst_attr_p == NULL)) {
        if (src_sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s source or destination fmtp attribute for "
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

/* Function:    sdp_attr_get_fmtp_mode
 * Description: Gets the value of the fmtp attribute mode parameter
 *              for the given attribute.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              payload_type payload type.
 * Returns:     mode value
 */
uint32_t sdp_attr_get_fmtp_mode_for_payload_type (sdp_t *sdp_p, uint16_t level,
                                             uint8_t cap_num, uint32_t payload_type)
{
    uint16_t          num_a_lines = 0;
    int          i;
    sdp_attr_t  *attr_p;

    /*
     * Get number of FMTP attributes for the AUDIO line
     */
    (void) sdp_attr_num_instances(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                                  &num_a_lines);
    for (i = 0; i < num_a_lines; i++) {
        attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, (uint16_t) (i + 1));
        if ((attr_p != NULL) &&
            (attr_p->attr.fmtp.payload_num == (uint16_t)payload_type)) {
            if (attr_p->attr.fmtp.fmtp_format == SDP_FMTP_MODE) {
                return attr_p->attr.fmtp.mode;
            }
        }
    }
   return 0;
}

sdp_result_e sdp_attr_set_fmtp_max_fs (sdp_t *sdp_p, uint16_t level,
                                       uint8_t cap_num, uint16_t inst_num,
                                       uint32_t max_fs)
{
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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

sdp_result_e sdp_attr_set_fmtp_max_fr (sdp_t *sdp_p, uint16_t level,
                                       uint8_t cap_num, uint16_t inst_num,
                                       uint32_t max_fr)
{
    sdp_attr_t  *attr_p;
    sdp_fmtp_t  *fmtp_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    fmtp_p = &(attr_p->attr.fmtp);
    fmtp_p->fmtp_format = SDP_FMTP_CODEC_INFO;

    if (max_fr > 0) {
        fmtp_p->max_fr  = max_fr;
        return (SDP_SUCCESS);
    } else {
        return (SDP_FAILURE);
    }
}

/* Function:    sdp_attr_get_fmtp_max_average_bitrate
 * Description: Gets the value of the fmtp attribute- maxaveragebitrate parameter for the OPUS codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-br value.
 */

sdp_result_e sdp_attr_get_fmtp_max_average_bitrate (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, uint32_t* val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP, 1);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = attr_p->attr.fmtp.maxaveragebitrate;
        return (SDP_SUCCESS);
    }
}


/* Function:    sdp_attr_get_fmtp_usedtx
 * Description: Gets the value of the fmtp attribute- usedtx parameter for the OPUS codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     usedtx value.
 */

sdp_result_e sdp_attr_get_fmtp_usedtx (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, tinybool* val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = (tinybool)attr_p->attr.fmtp.usedtx;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_usedtx
 * Description: Gets the value of the fmtp attribute- usedtx parameter for the OPUS codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     stereo value.
 */

sdp_result_e sdp_attr_get_fmtp_stereo (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, tinybool* val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = (tinybool)attr_p->attr.fmtp.stereo;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_useinbandfec
 * Description: Gets the value of the fmtp attribute useinbandfec parameter for the OPUS codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     useinbandfec value.
 */

sdp_result_e sdp_attr_get_fmtp_useinbandfec (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, tinybool* val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = (tinybool)attr_p->attr.fmtp.useinbandfec;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_maxcodedaudiobandwidth
 * Description: Gets the value of the fmtp attribute maxcodedaudiobandwidth parameter for OPUS codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     maxcodedaudiobandwidth value.
 */
char* sdp_attr_get_fmtp_maxcodedaudiobandwidth (sdp_t *sdp_p, uint16_t level,
                                          uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.fmtp.maxcodedaudiobandwidth);
    }
}

/* Function:    sdp_attr_get_fmtp_cbr
 * Description: Gets the value of the fmtp attribute cbr parameter for the OPUS codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     cbr value.
 */

sdp_result_e sdp_attr_get_fmtp_cbr (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, tinybool* val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = (tinybool)attr_p->attr.fmtp.cbr;
        return (SDP_SUCCESS);
    }
}

uint16_t sdp_attr_get_sctpmap_port(sdp_t *sdp_p, uint16_t level,
                              uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SCTPMAP, inst_num);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s sctpmap port, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return 0;
    } else {
        return attr_p->attr.sctpmap.port;
    }
}

sdp_result_e sdp_attr_get_sctpmap_streams (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, uint32_t* val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SCTPMAP, inst_num);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s sctpmap streams, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = attr_p->attr.sctpmap.streams;
        return (SDP_SUCCESS);
    }
}

sdp_result_e sdp_attr_get_sctpmap_protocol (sdp_t *sdp_p, uint16_t level,
                                            uint8_t cap_num, uint16_t inst_num,
                                            char* protocol)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_SCTPMAP,
                           inst_num);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s sctpmap, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        sstrncpy(protocol, attr_p->attr.sctpmap.protocol, SDP_MAX_STRING_LEN+1);
    }
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_fmtp_is_annexb_set
 * Description: Gives the value of the fmtp attribute annexb type parameter
 *              for the given attribute.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *
 *
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_fmtp_is_annexb_set (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                                      uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *
 *
 * Returns:     TRUE or FALSE.
 */
tinybool sdp_attr_fmtp_is_annexa_set (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                                      uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Bitrate type value.
 */
int32_t sdp_attr_get_fmtp_bitrate_type (sdp_t *sdp_p, uint16_t level,
                                      uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     QCIF value.
 */
int32_t sdp_attr_get_fmtp_qcif (sdp_t *sdp_p, uint16_t level,
                            uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CIF value.
 */
int32_t sdp_attr_get_fmtp_cif (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     sqcif value.
 */
int32_t sdp_attr_get_fmtp_sqcif (sdp_t *sdp_p, uint16_t level,
                               uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CIF4 value.
 */
int32_t sdp_attr_get_fmtp_cif4 (sdp_t *sdp_p, uint16_t level,
                              uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CIF16 value.
 */

int32_t sdp_attr_get_fmtp_cif16 (sdp_t *sdp_p, uint16_t level,
                               uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     MAXBR value.
 */
int32_t sdp_attr_get_fmtp_maxbr (sdp_t *sdp_p, uint16_t level,
                               uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CUSTOM x value.
 */

int32_t sdp_attr_get_fmtp_custom_x (sdp_t *sdp_p, uint16_t level,
                                  uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CUSTOM Y-AXIS value.
 */

int32_t sdp_attr_get_fmtp_custom_y (sdp_t *sdp_p, uint16_t level,
                                  uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CUSTOM MPI value.
 */

int32_t sdp_attr_get_fmtp_custom_mpi (sdp_t *sdp_p, uint16_t level,
                                    uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.custom_mpi);
    }
}

/* Function:    sdp_attr_get_fmtp_par_width
 * Description: Gets the value of the fmtp attribute PAR (width) parameter
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     PAR - width value.
 */
int32_t sdp_attr_get_fmtp_par_width (sdp_t *sdp_p, uint16_t level,
                                   uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.par_width);
    }
}

/* Function:    sdp_attr_get_fmtp_par_height
 * Description: Gets the value of the fmtp attribute PAR (height) parameter
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     PAR - height value.
 */
int32_t sdp_attr_get_fmtp_par_height (sdp_t *sdp_p, uint16_t level,
                                    uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.par_height);
    }
}

/* Function:    sdp_attr_get_fmtp_cpcf
 * Description: Gets the value of the fmtp attribute- CPCF parameter
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     CPCF value.
 */
int32_t sdp_attr_get_fmtp_cpcf (sdp_t *sdp_p, uint16_t level,
                              uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.cpcf);
    }
}

/* Function:    sdp_attr_get_fmtp_bpp
 * Description: Gets the value of the fmtp attribute- BPP parameter
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     BPP value.
 */
int32_t sdp_attr_get_fmtp_bpp (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.bpp);
    }
}

/* Function:    sdp_attr_get_fmtp_hrd
 * Description: Gets the value of the fmtp attribute- HRD parameter
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     HRD value.
 */
int32_t sdp_attr_get_fmtp_hrd (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.hrd);
    }
}

/* Function:    sdp_attr_get_fmtp_profile
 * Description: Gets the value of the fmtp attribute- PROFILE parameter
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     PROFILE value.
 */
int32_t sdp_attr_get_fmtp_profile (sdp_t *sdp_p, uint16_t level,
                                 uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.profile);
    }
}

/* Function:    sdp_attr_get_fmtp_level
 * Description: Gets the value of the fmtp attribute- LEVEL parameter
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     LEVEL value.
 */
int32_t sdp_attr_get_fmtp_level (sdp_t *sdp_p, uint16_t level,
                               uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.level);
    }
}

/* Function:    sdp_attr_get_fmtp_interlace
 * Description: Checks if INTERLACE parameter is set.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     TRUE if INTERLACE is present and FALSE if INTERLACE is absent.
 */
tinybool sdp_attr_get_fmtp_interlace (sdp_t *sdp_p, uint16_t level,
                                      uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return FALSE;
    } else {
        return (attr_p->attr.fmtp.is_interlace);
    }
}

/* Function:    sdp_attr_get_fmtp_pack_mode
 * Description: Gets the value of the fmtp attribute- packetization-mode parameter for H.264 codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     packetization-mode value in the range 0 - 2.
 */

sdp_result_e sdp_attr_get_fmtp_pack_mode (sdp_t *sdp_p, uint16_t level,
                                 uint8_t cap_num, uint16_t inst_num, uint16_t *val)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        if (SDP_INVALID_PACKETIZATION_MODE_VALUE == attr_p->attr.fmtp.packetization_mode) {
            /* packetization mode unspecified (optional) */
            *val = SDP_DEFAULT_PACKETIZATION_MODE_VALUE;
        } else {
            *val = attr_p->attr.fmtp.packetization_mode;
        }
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_level_asymmetry_allowed
 * Description: Gets the value of the fmtp attribute- level-asymmetry-allowed parameter for H.264 codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     level asymmetry allowed value (0 or 1).
 */

sdp_result_e sdp_attr_get_fmtp_level_asymmetry_allowed (sdp_t *sdp_p, uint16_t level,
                                 uint8_t cap_num, uint16_t inst_num, uint16_t *val)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     profile-level-id value.
 */
const char* sdp_attr_get_fmtp_profile_id (sdp_t *sdp_p, uint16_t level,
                                          uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.fmtp.profile_level_id);
    }
}

/* Function:    sdp_attr_get_fmtp_param_sets
 * Description: Gets the value of the fmtp attribute- parameter-sets parameter for H.264 codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     parameter-sets value.
 */
const char* sdp_attr_get_fmtp_param_sets (sdp_t *sdp_p, uint16_t level,
                                          uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        return (attr_p->attr.fmtp.parameter_sets);
    }
}

/* Function:    sdp_attr_get_fmtp_interleaving_depth
 * Description: Gets the value of the fmtp attribute- interleaving_depth parameter for H.264 codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     interleaving_depth value
 */

sdp_result_e sdp_attr_get_fmtp_interleaving_depth (sdp_t *sdp_p, uint16_t level,
                                            uint8_t cap_num, uint16_t inst_num, uint16_t* val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     deint-buf-req value.
 */

sdp_result_e sdp_attr_get_fmtp_deint_buf_req (sdp_t *sdp_p, uint16_t level,
                                             uint8_t cap_num, uint16_t inst_num,
                                             uint32_t *val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-don-diff value.
 */
sdp_result_e sdp_attr_get_fmtp_max_don_diff (sdp_t *sdp_p, uint16_t level,
                                      uint8_t cap_num, uint16_t inst_num,
                                      uint32_t *val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     init-buf-time value.
 */
sdp_result_e sdp_attr_get_fmtp_init_buf_time (sdp_t *sdp_p, uint16_t level,
                                             uint8_t cap_num, uint16_t inst_num,
                                             uint32_t *val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-mbps value.
 */

sdp_result_e sdp_attr_get_fmtp_max_mbps (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, uint16_t inst_num,
                                uint32_t *val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-fs value.
 */

sdp_result_e sdp_attr_get_fmtp_max_fs (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, uint32_t *val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = attr_p->attr.fmtp.max_fs;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_max_fr
 * Description: Gets the value of the fmtp attribute- max-fr parameter
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-fr value.
 */

sdp_result_e sdp_attr_get_fmtp_max_fr (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, uint32_t *val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    } else {
        *val = attr_p->attr.fmtp.max_fr;
        return (SDP_SUCCESS);
    }
}

/* Function:    sdp_attr_get_fmtp_max_cpb
 * Description: Gets the value of the fmtp attribute- max-cpb parameter for H.264 codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-cpb value.
 */

sdp_result_e sdp_attr_get_fmtp_max_cpb (sdp_t *sdp_p, uint16_t level,
                                 uint8_t cap_num, uint16_t inst_num, uint32_t *val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-dpb value.
 */

sdp_result_e sdp_attr_get_fmtp_max_dpb (sdp_t *sdp_p, uint16_t level,
                               uint8_t cap_num, uint16_t inst_num, uint32_t *val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-br value.
 */

sdp_result_e sdp_attr_get_fmtp_max_br (sdp_t *sdp_p, uint16_t level,
                             uint8_t cap_num, uint16_t inst_num, uint32_t* val)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     redundant-pic-cap value.
 */
tinybool sdp_attr_fmtp_is_redundant_pic_cap (sdp_t *sdp_p, uint16_t level,
                                             uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.redundant_pic_cap);
    }
}

/* Function:    sdp_attr_get_fmtp_deint_buf_cap
 * Description: Gets the value of the fmtp attribute- deint-buf-cap parameter for H.264 codec
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     deint-buf-cap value.
 */

sdp_result_e sdp_attr_get_fmtp_deint_buf_cap (sdp_t *sdp_p, uint16_t level,
                                             uint8_t cap_num, uint16_t inst_num,
                                             uint32_t *val)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     max-rcmd-nalu-size value.
 */
sdp_result_e sdp_attr_get_fmtp_max_rcmd_nalu_size (sdp_t *sdp_p, uint16_t level,
                                                  uint8_t cap_num, uint16_t inst_num,
                                                  uint32_t *val)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     TRUE/FALSE ( parameter-add is boolean)
 */
tinybool sdp_attr_fmtp_is_parameter_add (sdp_t *sdp_p, uint16_t level,
                                         uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        /* Both 1 and SDP_FMTP_UNUSED (parameter not present) should be
         * treated as TRUE, per RFC 3984, page 45 */
        return (attr_p->attr.fmtp.parameter_add != 0);
    }
}

/****** Following functions are get routines for Annex values
 * For each Annex support, the get routine will return the boolean TRUE/FALSE
 * Some Annexures for Video codecs have values defined . In those cases,
 * (e.g Annex K, P ) , the return values are not boolean.
 *
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Annex value
 */

tinybool sdp_attr_get_fmtp_annex_d (sdp_t *sdp_p, uint16_t level,
                                    uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annex_d);
    }
}

tinybool sdp_attr_get_fmtp_annex_f (sdp_t *sdp_p, uint16_t level,
                                    uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annex_f);
    }
}

tinybool sdp_attr_get_fmtp_annex_i (sdp_t *sdp_p, uint16_t level,
                                    uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annex_i);
    }
}

tinybool sdp_attr_get_fmtp_annex_j (sdp_t *sdp_p, uint16_t level,
                                    uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annex_j);
    }
}

tinybool sdp_attr_get_fmtp_annex_t (sdp_t *sdp_p, uint16_t level,
                                    uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.fmtp.annex_t);
    }
}

int32_t sdp_attr_get_fmtp_annex_k_val (sdp_t *sdp_p, uint16_t level,
                                     uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.annex_k_val);
    }
}

int32_t sdp_attr_get_fmtp_annex_n_val (sdp_t *sdp_p, uint16_t level,
                                     uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.annex_n_val);
    }
}

int32_t sdp_attr_get_fmtp_annex_p_picture_resize (sdp_t *sdp_p, uint16_t level,
                                                uint8_t cap_num, uint16_t inst_num)
{


    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    } else {
        return (attr_p->attr.fmtp.annex_p_val_picture_resize);
    }
}

int32_t sdp_attr_get_fmtp_annex_p_warp (sdp_t *sdp_p, uint16_t level,
                                      uint8_t cap_num, uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *
 *
 * Returns:     Enum type sdp_fmtp_format_type_e
 */
sdp_fmtp_format_type_e  sdp_attr_fmtp_get_fmtp_format (sdp_t *sdp_p,
                                                       uint16_t level, uint8_t cap_num,
                                                       uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_FMTP,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s fmtp attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of payload types.
 */
uint16_t sdp_attr_get_pccodec_num_payload_types (sdp_t *sdp_p, uint16_t level,
                                            uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_X_PC_CODEC,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s X-pc-codec attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              payload_num The payload number to get.  Range is (1 -
 *                          max num payloads).
 * Returns:     Payload type.
 */
uint16_t sdp_attr_get_pccodec_payload_type (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                                       uint16_t inst_num, uint16_t payload_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_X_PC_CODEC,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s X-pc-codec attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        if ((payload_num < 1) ||
            (payload_num > attr_p->attr.pccodec.num_payloads)) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s X-pc-codec attribute, level %u instance %u, "
                          "invalid payload number %u requested.",
                          sdp_p->debug_str, (unsigned)level, (unsigned)inst_num, (unsigned)payload_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              payload_type The payload type to add.
 * Returns:     SDP_SUCCESS            Payload type was added successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e sdp_attr_add_pccodec_payload_type (sdp_t *sdp_p, uint16_t level,
                                                uint8_t cap_num, uint16_t inst_num,
                                                uint16_t payload_type)
{
    uint16_t          payload_num;
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_X_PC_CODEC,
                           inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s X-pc-codec attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the capability.
 *              inst_num    The X-cap instance number to check.
 * Returns:     Capability number or zero.
 */
uint16_t sdp_attr_get_xcap_first_cap_num (sdp_t *sdp_p, uint16_t level, uint16_t inst_num)
{
    uint16_t          cap_num=1;
    uint16_t          attr_count=0;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *mca_p;

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
        CSFLogError(logTag, "%s X-cap attribute, level %u instance %u "
                  "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
    }
    sdp_p->conf_p->num_invalid_param++;
    return (0);
}

/* Function:    sdp_attr_get_xcap_media_type
 * Description: Returns the media type specified for the given X-cap
 *              attribute.  If the given attribute is not defined,
 *              SDP_MEDIA_INVALID is returned.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              inst_num    The attribute instance number to check.
 * Returns:     Media type or SDP_MEDIA_INVALID.
 */
sdp_media_e sdp_attr_get_xcap_media_type (sdp_t *sdp_p, uint16_t level,
                                          uint16_t inst_num)
{
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              inst_num    The attribute instance number to check.
 * Returns:     Media type or SDP_TRANSPORT_INVALID.
 */
sdp_transport_e sdp_attr_get_xcap_transport_type (sdp_t *sdp_p, uint16_t level,
                                                  uint16_t inst_num)
{
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP,
                           inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of payload types or zero.
 */
uint16_t sdp_attr_get_xcap_num_payload_types (sdp_t *sdp_p, uint16_t level,
                                         uint16_t inst_num)
{
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              inst_num    The attribute instance number to check.
 *              payload_num The payload number to retrieve.  Range is
 *                          (1 - max num payloads).
 * Returns:     Payload type or zero.
 */
uint16_t sdp_attr_get_xcap_payload_type (sdp_t *sdp_p, uint16_t level,
                                    uint16_t inst_num, uint16_t payload_num,
                                    sdp_payload_ind_e *indicator)
{
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        cap_p = attr_p->attr.cap_p;
        if ((payload_num < 1) ||
            (payload_num > cap_p->num_payloads)) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s X-cap attribute, level %u instance %u, "
                          "payload num %u invalid.", sdp_p->debug_str,
                          (unsigned)level, (unsigned)inst_num, (unsigned)payload_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (0);
        } else {
            *indicator = cap_p->payload_indicator[payload_num-1];
            return (cap_p->payload_type[payload_num-1]);
        }
    }
}


/* Function:    sdp_attr_add_xcap_payload_type
 * Description: Add a new payload type for the X-cap attribute line
 *              specified. The new payload type will be added at the end
 *              of the payload type list.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              inst_num    The attribute instance number to check.
 *              payload_type The new payload type.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_attr_add_xcap_payload_type(sdp_t *sdp_p, uint16_t level,
                                            uint16_t inst_num, uint16_t payload_type,
                                            sdp_payload_ind_e indicator)
{
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cap_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_X_CAP, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s X-cap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the capability.
 *              inst_num    The CDSC instance number to check.
 * Returns:     Capability number or zero.
 */
uint16_t sdp_attr_get_cdsc_first_cap_num(sdp_t *sdp_p, uint16_t level, uint16_t inst_num)
{
    uint16_t          cap_num=1;
    uint16_t          attr_count=0;
    sdp_attr_t  *attr_p;
    sdp_mca_t   *mca_p;

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
        CSFLogError(logTag, "%s CDSC attribute, level %u instance %u "
                  "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
    }
    sdp_p->conf_p->num_invalid_param++;
    return (0);
}

/* Function:    sdp_attr_get_cdsc_media_type
 * Description: Returns the media type specified for the given CDSC
 *              attribute.  If the given attribute is not defined,
 *              SDP_MEDIA_INVALID is returned.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              inst_num    The attribute instance number to check.
 * Returns:     Media type or SDP_MEDIA_INVALID.
 */
sdp_media_e sdp_attr_get_cdsc_media_type(sdp_t *sdp_p, uint16_t level,
                                         uint16_t inst_num)
{
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              inst_num    The attribute instance number to check.
 * Returns:     Media type or SDP_TRANSPORT_INVALID.
 */
sdp_transport_e sdp_attr_get_cdsc_transport_type(sdp_t *sdp_p, uint16_t level,
                                                 uint16_t inst_num)
{
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC,
                           inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of payload types or zero.
 */
uint16_t sdp_attr_get_cdsc_num_payload_types (sdp_t *sdp_p, uint16_t level,
                                         uint16_t inst_num)
{
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              inst_num    The attribute instance number to check.
 *              payload_num The payload number to retrieve.  Range is
 *                          (1 - max num payloads).
 * Returns:     Payload type or zero.
 */
uint16_t sdp_attr_get_cdsc_payload_type (sdp_t *sdp_p, uint16_t level,
                                    uint16_t inst_num, uint16_t payload_num,
                                    sdp_payload_ind_e *indicator)
{
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        cdsc_p = attr_p->attr.cap_p;
        if ((payload_num < 1) ||
            (payload_num > cdsc_p->num_payloads)) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s CDSC attribute, level %u instance %u, "
                          "payload num %u invalid.", sdp_p->debug_str,
                          (unsigned)level, (unsigned)inst_num, (unsigned)payload_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return (0);
        } else {
            *indicator = cdsc_p->payload_indicator[payload_num-1];
            return (cdsc_p->payload_type[payload_num-1]);
        }
    }
}

/* Function:    sdp_attr_add_cdsc_payload_type
 * Description: Add a new payload type for the CDSC attribute line
 *              specified. The new payload type will be added at the end
 *              of the payload type list.
 *              Note: cap_num is not specified.  It must be zero.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              inst_num    The attribute instance number to check.
 *              payload_type The new payload type.
 * Returns:     SDP_SUCCESS or SDP_INVALID_PARAMETER
 */
sdp_result_e sdp_attr_add_cdsc_payload_type (sdp_t *sdp_p, uint16_t level,
                                             uint16_t inst_num, uint16_t payload_type,
                                             sdp_payload_ind_e indicator)
{
    sdp_attr_t  *attr_p;
    sdp_mca_t   *cdsc_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_CDSC, inst_num);
    if ((attr_p == NULL) || (attr_p->attr.cap_p == NULL)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s CDSC attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p      The SDP handle returned by sdp_init_description.
 *              payload_type  Payload type to be checked
 *
 * Returns:     TRUE or FALSE. Returns TRUE if payload type is defined on the
 *              media line, else returns FALSE
 */

tinybool sdp_media_dynamic_payload_valid (sdp_t *sdp_p, uint16_t payload_type,
                                          uint16_t m_line)
{
   uint16_t p_type,m_ptype;
   ushort num_payload_types;
   sdp_payload_ind_e ind;
   tinybool payload_matches = FALSE;
   tinybool result = TRUE;

   if ((payload_type < SDP_MIN_DYNAMIC_PAYLOAD) ||
       (payload_type > SDP_MAX_DYNAMIC_PAYLOAD)) {
       return FALSE;
   }

   num_payload_types =
       sdp_get_media_num_payload_types(sdp_p, m_line);

   for(p_type=1; p_type <=num_payload_types;p_type++){

       m_ptype = (uint16_t)sdp_get_media_payload_type(sdp_p,
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

/* Function:    sdp_attr_get_rtr_confirm
 * Description: Returns the value of the rtr attribute confirm
 *              parameter specified for the given attribute.  Returns TRUE if
 *              the confirm parameter is specified.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Boolean value.
 */
tinybool sdp_attr_get_rtr_confirm (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_RTR, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s %s attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str,
                      sdp_get_attr_name(SDP_ATTR_RTR), (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (FALSE);
    } else {
        return (attr_p->attr.rtr.confirm);
    }
}



sdp_mediadir_role_e sdp_attr_get_comediadir_role (sdp_t *sdp_p, uint16_t level,
                                             uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_DIRECTION, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Comediadir role attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_MEDIADIR_ROLE_UNKNOWN);
    } else {
        return (attr_p->attr.comediadir.role);
    }
}

/* Function:    sdp_attr_get_silencesupp_enabled
 * Description: Returns the value of the silencesupp attribute enable
 *              parameter specified for the given attribute.  Returns TRUE if
 *              the confirm parameter is specified.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Boolean value.
 */
tinybool sdp_attr_get_silencesupp_enabled (sdp_t *sdp_p, uint16_t level,
                                           uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s silenceSuppEnable attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     16-bit timer value
 *              boolean null_ind
 */
uint16_t sdp_attr_get_silencesupp_timer (sdp_t *sdp_p, uint16_t level,
                                    uint8_t cap_num, uint16_t inst_num,
                                    tinybool *null_ind)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s silenceTimer attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              confirm     New qos confirm parameter.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_silencesupp_pref_e sdp_attr_get_silencesupp_pref (sdp_t *sdp_p,
                                                      uint16_t level, uint8_t cap_num,
                                                      uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s silence suppPref attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     silencesupp siduse enum.
 */
sdp_silencesupp_siduse_e sdp_attr_get_silencesupp_siduse (sdp_t *sdp_p,
                                                          uint16_t level,
                                                          uint8_t cap_num,
                                                          uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s silence sidUse attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     7-bit fxns value
 *              boolean null_ind
 */
uint8_t sdp_attr_get_silencesupp_fxnslevel (sdp_t *sdp_p, uint16_t level,
                                       uint8_t cap_num, uint16_t inst_num,
                                       tinybool *null_ind)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SILENCESUPP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s silence fxnslevel attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        *null_ind = attr_p->attr.silencesupp.fxnslevel_null;
        return (attr_p->attr.silencesupp.fxnslevel);
    }
}

/* Function:    sdp_attr_get_mptime_num_intervals
 * Description: Returns the number of intervals specified for the
 *              given mptime attribute.  If the given attribute is not
 *              defined, zero is returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Number of intervals.
 */
uint16_t sdp_attr_get_mptime_num_intervals (
    sdp_t *sdp_p,
    uint16_t level,
    uint8_t cap_num,
    uint16_t inst_num) {

    sdp_attr_t *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_MPTIME, inst_num);
    if (attr_p != NULL) {
        return attr_p->attr.mptime.num_intervals;
    }

    if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
        CSFLogError(logTag, "%s mptime attribute, level %u instance %u not found.",
                  sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
    }
    sdp_p->conf_p->num_invalid_param++;
    return 0;
}

/* Function:    sdp_attr_get_mptime_interval
 * Description: Returns the value of the specified interval for the
 *              given mptime attribute.  If the given attribute is not
 *              defined, zero is returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 *              interval_num The interval number to get.  Range is (1 -
 *                          max num payloads).
 * Returns:     Interval.
 */
uint16_t sdp_attr_get_mptime_interval (
    sdp_t *sdp_p,
    uint16_t level,
    uint8_t cap_num,
    uint16_t inst_num,
    uint16_t interval_num) {

    sdp_attr_t *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_MPTIME, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s mptime attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return 0;
    }

    if ((interval_num<1) || (interval_num>attr_p->attr.mptime.num_intervals)) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s mptime attribute, level %u instance %u, "
                      "invalid interval number %u requested.",
                      sdp_p->debug_str, (unsigned)level, (unsigned)inst_num, (unsigned)interval_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
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
    sdp_t *sdp_p,
    uint16_t level,
    uint8_t cap_num,
    uint16_t inst_num,
    uint16_t mp_interval) {

    uint16_t interval_num;
    sdp_attr_t *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num, SDP_ATTR_MPTIME, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s mptime attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return SDP_INVALID_PARAMETER;
    }

    interval_num = attr_p->attr.mptime.num_intervals;
    if (interval_num>=SDP_MAX_PAYLOAD_TYPES) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s mptime attribute, level %u instance %u "
                      "exceeds maximum length.",
                      sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       SDP_SESSION_LEVEL
 * Returns:     Valid attrib value or SDP_GROUP_ATTR_UNSUPPORTED.
 */
sdp_group_attr_e sdp_get_group_attr (sdp_t *sdp_p, uint16_t level,
                                     uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t          *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_GROUP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Group (a= group line) attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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

/* Function:    sdp_get_group_num_id
 * Description: Returns the number of ids from the a=group:<>  line.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       SDP_SESSION_LEVEL
 * Returns:    Num of group ids present or 0 if there is an error.
 */
uint16_t sdp_get_group_num_id (sdp_t *sdp_p, uint16_t level,
                          uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t          *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_GROUP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s a=group level attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (0);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Stream data group attr - num of ids is :%u ",
                      sdp_p->debug_str,
                      (unsigned)attr_p->attr.stream_data.num_group_id);
        }
    }
    return (attr_p->attr.stream_data.num_group_id);
}

/* Function:    sdp_get_group_id
 * Description: Returns the group id from the a=group:<>  line.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       SDP_SESSION_LEVEL
 *              id_num      Number of the id to retrieve. The range is (1 -
 *                          SDP_MAX_GROUP_STREAM_ID)
 * Returns:    Value of the group id at the index specified or
 *             NULL if an error
 */
const char* sdp_get_group_id (sdp_t *sdp_p, uint16_t level,
                        uint8_t cap_num, uint16_t inst_num, uint16_t id_num)
{
    sdp_attr_t          *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_GROUP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s a=group level attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        if (sdp_p->debug_flag[SDP_DEBUG_TRACE]) {
            SDP_PRINT("%s Stream data group attr - num of ids is :%u ",
                      sdp_p->debug_str,
                      (unsigned)attr_p->attr.stream_data.num_group_id);
        }
        if ((id_num < 1) || (id_num > attr_p->attr.stream_data.num_group_id)) {
            return (NULL);
        }
    }
    return (attr_p->attr.stream_data.group_ids[id_num-1]);
}

/* Function:    sdp_attr_get_x_sidin
 * Description: Returns the attribute parameter from the a=X-sidin:<>
 *              line.  If no attrib has been set NULL will be returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       media level index
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Pointer to sidin or NULL.
 */
const char* sdp_attr_get_x_sidin (sdp_t *sdp_p, uint16_t level,
                                     uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t          *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_X_SIDIN, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s X-sidin attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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

/* Function:    sdp_attr_get_x_sidout
 * Description: Returns the attribute parameter from the a=X-sidout:<>
 *              line.  If no attrib has been set NULL will be returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       media level index
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Pointer to sidout or NULL.
 */
const char* sdp_attr_get_x_sidout (sdp_t *sdp_p, uint16_t level,
                                     uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t          *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_X_SIDOUT, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s X-sidout attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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

/* Function:    sdp_attr_get_x_confid
 * Description: Returns the attribute parameter from the a=X-confid:<>
 *              line.  If no attrib has been set NULL will be returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       media level index
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Pointer to confid or NULL.
 */
const char* sdp_attr_get_x_confid (sdp_t *sdp_p, uint16_t level,
                                     uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t          *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_X_CONFID, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s X-confid attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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

/* Function:    sdp_get_source_filter_mode
 * Description: Gets the filter mode in internal representation
 * Parameters:  sdp_p   The SDP handle which contains the attributes
 *              level     SDP_SESSION_LEVEL/m-line number
 *              inst_num  The attribute instance number
 * Returns:     Filter mode (incl/excl/not present)
 */
sdp_src_filter_mode_e
sdp_get_source_filter_mode (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                            uint16_t inst_num)
{
    sdp_attr_t *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SOURCE_FILTER, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Source filter attribute, level %u, "
                      "instance %u not found", sdp_p->debug_str,
                      (unsigned)level, (unsigned)inst_num);
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
sdp_get_filter_destination_attributes (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                                       uint16_t inst_num, sdp_nettype_e *nettype,
                                       sdp_addrtype_e *addrtype,
                                       char *dest_addr)
{
    sdp_attr_t *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SOURCE_FILTER, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Source filter attribute, level %u instance %u "
                      "not found", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p   The SDP handle which contains the attributes
 *              level     SDP_SESSION_LEVEL/m-line number
 *              inst_num  The attribute instance number
 * Returns:     Source-list count
 */

int32_t
sdp_get_filter_source_address_count (sdp_t *sdp_p, uint16_t level,
                                     uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SOURCE_FILTER, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Source filter attribute, level %u instance %u "
                      "not found", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_VALUE);
    }
    return (attr_p->attr.source_filter.num_src_addr);
}

/* Function:    sdp_get_filter_source_address
 * Description: Gets one of the source address that is indexed by the user
 * Parameters:  sdp_p   The SDP handle which contains the attributes
 *              level     SDP_SESSION_LEVEL/m-line number
 *              inst_num  The attribute instance number
 *              src_addr_id User provided index (value in range between
 *                          0 to (SDP_MAX_SRC_ADDR_LIST-1) which obtains
 *                          the source addr corresponding to it.
 *              src_addr  The user provided variable which gets updated
 *                        with source address corresponding to the index
 */
sdp_result_e
sdp_get_filter_source_address (sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                               uint16_t inst_num, uint16_t src_addr_id,
                               char *src_addr)
{
    sdp_attr_t *attr_p;

    src_addr[0] = '\0';

    if (src_addr_id >= SDP_MAX_SRC_ADDR_LIST) {
        return (SDP_INVALID_PARAMETER);
    }
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SOURCE_FILTER, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s Source filter attribute, level %u instance %u "
                      "not found", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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

sdp_rtcp_unicast_mode_e
sdp_get_rtcp_unicast_mode(sdp_t *sdp_p, uint16_t level, uint8_t cap_num,
                          uint16_t inst_num)
{
    sdp_attr_t *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_RTCP_UNICAST, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s RTCP Unicast attribute, level %u, "
                      "instance %u not found", sdp_p->debug_str,
                      (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_RTCP_UNICAST_MODE_NOT_PRESENT);
    }
    return ((sdp_rtcp_unicast_mode_e)attr_p->attr.u32_val);
}


/* Function:    sdp_attr_get_sdescriptions_tag
 * Description: Returns the value of the sdescriptions tag
 *              parameter specified for the given attribute.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     Tag value or SDP_INVALID_VALUE (-2) if error encountered.
 */

int32_t
sdp_attr_get_sdescriptions_tag (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SDESCRIPTIONS, inst_num);

    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s srtp attribute tag, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     SDP_SRTP_UNKNOWN_CRYPTO_SUITE is returned if an error was
 *              encountered otherwise the crypto suite is returned.
 */

sdp_srtp_crypto_suite_t
sdp_attr_get_sdescriptions_crypto_suite (sdp_t *sdp_p, uint16_t level,
                                         uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;


    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SRTP_CONTEXT, inst_num);

    if (attr_p == NULL) {
        /* There's no version 2 so now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num,
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s srtp attribute suite, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     NULL if error encountered or master key salt string
 */

const char*
sdp_attr_get_sdescriptions_key (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SRTP_CONTEXT, inst_num);

    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num,
                               SDP_ATTR_SDESCRIPTIONS, inst_num);

        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s srtp attribute key, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     NULL if error encountered or master key salt string
 */

const char*
sdp_attr_get_sdescriptions_salt (sdp_t *sdp_p, uint16_t level,
                                 uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SRTP_CONTEXT, inst_num);

    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num,
                               SDP_ATTR_SDESCRIPTIONS, inst_num);

        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s srtp attribute salt, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     NULL if error encountered or lifetime string
 */

const char*
sdp_attr_get_sdescriptions_lifetime (sdp_t *sdp_p, uint16_t level,
                                     uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    /* Try version 2 first. */
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SRTP_CONTEXT, inst_num);

    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num,
                               SDP_ATTR_SDESCRIPTIONS, inst_num);

        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s srtp attribute lifetime, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
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
sdp_attr_get_sdescriptions_mki (sdp_t *sdp_p, uint16_t level,
                                uint8_t cap_num, uint16_t inst_num,
                                const char **mki_value,
                                uint16_t *mki_length)
{
    sdp_attr_t  *attr_p;

    *mki_value = NULL;
    *mki_length = 0;

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SRTP_CONTEXT, inst_num);

    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num,
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s srtp attribute MKI, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
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
sdp_attr_get_sdescriptions_session_params (sdp_t *sdp_p, uint16_t level,
                                           uint8_t cap_num, uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SRTP_CONTEXT, inst_num);

    if (attr_p == NULL) {
        /* Couldn't find version 2 try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SDESCRIPTIONS, inst_num);
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s srtp attribute session params, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     0 (SDP_SDESCRIPTIONS_KEY_SIZE_UNKNOWN) if error was
 *              encountered, otherwise key size.
 */

unsigned char
sdp_attr_get_sdescriptions_key_size (sdp_t *sdp_p,
                                     uint16_t level,
                                     uint8_t cap_num,
                                     uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SRTP_CONTEXT, inst_num);

    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num,
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s srtp attribute MKI, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     0 (SDP_SDESCRIPTIONS_KEY_SIZE_UNKNOWN) if error was
 *              encountered, otherwise salt size.
 */

unsigned char
sdp_attr_get_sdescriptions_salt_size (sdp_t *sdp_p,
                                      uint16_t level,
                                      uint8_t cap_num,
                                      uint16_t inst_num)
{

    sdp_attr_t  *attr_p;

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SRTP_CONTEXT, inst_num);

    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num,
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s srtp attribute MKI, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
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
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              cap_num     The capability number associated with the
 *                          attribute if any.  If none, should be zero.
 *              inst_num    The attribute instance number to check.
 * Returns:     0 (SDP_SRTP_CRYPTO_SELECTION_FLAGS_UNKNOWN) if error was
 *              encountered, otherwise selection flags.
 */

unsigned long
sdp_attr_get_srtp_crypto_selection_flags (sdp_t *sdp_p,
                                          uint16_t level,
                                          uint8_t cap_num,
                                          uint16_t inst_num)
{


    sdp_attr_t  *attr_p;

    /* Try version 2 first */
    attr_p = sdp_find_attr(sdp_p, level, cap_num,
                           SDP_ATTR_SRTP_CONTEXT, inst_num);

    if (attr_p == NULL) {
        /* Couldn't find version 2 now try version 9 */
        attr_p = sdp_find_attr(sdp_p, level, cap_num,
                               SDP_ATTR_SDESCRIPTIONS, inst_num);
        if (attr_p == NULL) {
            if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
                CSFLogError(logTag, "%s srtp attribute MKI, level %u instance %u "
                          "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
            }
            sdp_p->conf_p->num_invalid_param++;
            return SDP_SRTP_CRYPTO_SELECTION_FLAGS_UNKNOWN;
        }
    }

    return attr_p->attr.srtp_context.selection_flags;

}



/* Function:    sdp_find_rtcp_fb_attr
 * Description: Helper to find the nth instance of a rtcp-fb attribute of
 *              the specified feedback type.
 * Parameters:  sdp_p        The SDP handle returned by sdp_init_description.
 *              level        The level to check for the attribute.
 *              payload_type The payload to get the attribute for
 *              fb_type      The feedback type to look for.
 *              inst_num     The attribute instance number to check.
 * Returns:     Pointer to the attribute, or NULL if not found.
 */

sdp_attr_t *
sdp_find_rtcp_fb_attr (sdp_t *sdp_p,
                       uint16_t level,
                       uint16_t payload_type,
                       sdp_rtcp_fb_type_e fb_type,
                       uint16_t inst_num)
{
    uint16_t          attr_count=0;
    sdp_mca_t   *mca_p;
    sdp_attr_t  *attr_p;

    mca_p = sdp_find_media_level(sdp_p, level);
    if (!mca_p) {
        return (NULL);
    }
    for (attr_p = mca_p->media_attrs_p; attr_p; attr_p = attr_p->next_p) {
        if (attr_p->type == SDP_ATTR_RTCP_FB &&
            (attr_p->attr.rtcp_fb.payload_num == payload_type ||
             attr_p->attr.rtcp_fb.payload_num == SDP_ALL_PAYLOADS) &&
            attr_p->attr.rtcp_fb.feedback_type == fb_type) {
            attr_count++;
            if (attr_count == inst_num) {
                return (attr_p);
            }
        }
    }
    return NULL;
}

/* Function:    sdp_attr_get_rtcp_fb_ack
 * Description: Returns the value of the rtcp-fb:...ack attribute
 * Parameters:  sdp_p      The SDP handle returned by sdp_init_description.
 *              level        The level to check for the attribute.
 *              payload_type The payload to get the attribute for
 *              inst_num    The attribute instance number to check.
 * Returns:     ACK type (SDP_RTCP_FB_ACK_NOT_FOUND if not present)
 */
sdp_rtcp_fb_ack_type_e
sdp_attr_get_rtcp_fb_ack(sdp_t *sdp_p, uint16_t level, uint16_t payload_type, uint16_t inst)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_rtcp_fb_attr(sdp_p, level, payload_type,
                                   SDP_RTCP_FB_ACK, inst);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtcp-fb attribute, level %u, pt %u, "
                      "instance %u not found.", sdp_p->debug_str, (unsigned)level,
                      (unsigned)payload_type, (unsigned)inst);
        }
        sdp_p->conf_p->num_invalid_param++;
        return SDP_RTCP_FB_ACK_NOT_FOUND;
    }
    return (attr_p->attr.rtcp_fb.param.ack);
}

/* Function:    sdp_attr_get_rtcp_fb_nack
 * Description: Returns the value of the rtcp-fb:...nack attribute
 * Parameters:  sdp_p      The SDP handle returned by sdp_init_description.
 *              level        The level to check for the attribute.
 *              payload_type The payload to get the attribute for
 *              inst_num    The attribute instance number to check.
 * Returns:     NACK type (SDP_RTCP_FB_NACK_NOT_FOUND if not present)
 */
sdp_rtcp_fb_nack_type_e
sdp_attr_get_rtcp_fb_nack(sdp_t *sdp_p, uint16_t level, uint16_t payload_type, uint16_t inst)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_rtcp_fb_attr(sdp_p, level, payload_type,
                                   SDP_RTCP_FB_NACK, inst);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtcp-fb attribute, level %u, pt %u, "
                      "instance %u not found.", sdp_p->debug_str, (unsigned)level,
                      (unsigned)payload_type, (unsigned)inst);
        }
        sdp_p->conf_p->num_invalid_param++;
        return SDP_RTCP_FB_NACK_NOT_FOUND;
    }
    return (attr_p->attr.rtcp_fb.param.nack);
}

/* Function:    sdp_attr_get_rtcp_fb_trr_int
 * Description: Returns the value of the rtcp-fb:...trr-int attribute
 * Parameters:  sdp_p      The SDP handle returned by sdp_init_description.
 *              level        The level to check for the attribute.
 *              payload_type The payload to get the attribute for
 *              inst_num    The attribute instance number to check.
 * Returns:     trr-int interval (0xFFFFFFFF if not found)
 */
uint32_t
sdp_attr_get_rtcp_fb_trr_int(sdp_t *sdp_p, uint16_t level,
                             uint16_t payload_type, uint16_t inst)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_rtcp_fb_attr(sdp_p, level, payload_type,
                                   SDP_RTCP_FB_TRR_INT, inst);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtcp-fb attribute, level %u, pt %u, "
                      "instance %u not found.", sdp_p->debug_str, (unsigned)level,
                      (unsigned)payload_type, (unsigned)inst);
        }
        sdp_p->conf_p->num_invalid_param++;
        return 0xFFFFFFFF;
    }
    return (attr_p->attr.rtcp_fb.param.trr_int);
}

/* Function:    sdp_attr_get_rtcp_fb_ccm
 * Description: Returns the value of the rtcp-fb:...ccm attribute
 * Parameters:  sdp_p      The SDP handle returned by sdp_init_description.
 *              level        The level to check for the attribute.
 *              payload_type The payload to get the attribute for
 *              inst_num    The attribute instance number to check.
 * Returns:     CCM type (SDP_RTCP_FB_CCM_NOT_FOUND if not present)
 */
sdp_rtcp_fb_ccm_type_e
sdp_attr_get_rtcp_fb_ccm(sdp_t *sdp_p, uint16_t level, uint16_t payload_type, uint16_t inst)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_rtcp_fb_attr(sdp_p, level, payload_type,
                                   SDP_RTCP_FB_CCM, inst);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtcp-fb attribute, level %u, pt %u, "
                      "instance %u not found.", sdp_p->debug_str, (unsigned)level,
                      (unsigned)payload_type, (unsigned)inst);
        }
        sdp_p->conf_p->num_invalid_param++;
        return SDP_RTCP_FB_CCM_NOT_FOUND;
    }
    return (attr_p->attr.rtcp_fb.param.ccm);
}

/* Function:    sdp_attr_set_rtcp_fb_ack
 * Description: Sets the value of an rtcp-fb:...ack attribute
 * Parameters:  sdp_p        The SDP handle returned by sdp_init_description.
 *              level          The level to set the attribute.
 *              payload_type   The value to set the payload type to for
 *                             this attribute. Can be SDP_ALL_PAYLOADS.
 *              inst_num       The attribute instance number to check.
 *              type           The ack type to indicate
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e
sdp_attr_set_rtcp_fb_ack(sdp_t *sdp_p, uint16_t level, uint16_t payload_type, uint16_t inst,
                         sdp_rtcp_fb_ack_type_e type)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_RTCP_FB, inst);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtcp_fb ack attribute, level %u "
                      "instance %u not found.", sdp_p->debug_str, (unsigned)level,
                      (unsigned)inst);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    attr_p->attr.rtcp_fb.payload_num = payload_type;
    attr_p->attr.rtcp_fb.feedback_type = SDP_RTCP_FB_ACK;
    attr_p->attr.rtcp_fb.param.ack = type;
    attr_p->attr.rtcp_fb.extra[0] = '\0';
    return (SDP_SUCCESS);
}


/* Function:    sdp_attr_set_rtcp_fb_nack
 * Description: Sets the value of an rtcp-fb:...nack attribute
 * Parameters:  sdp_p        The SDP handle returned by sdp_init_description.
 *              level          The level to set the attribute.
 *              payload_type   The value to set the payload type to for
 *                             this attribute. Can be SDP_ALL_PAYLOADS.
 *              inst_num       The attribute instance number to check.
 *              type           The nack type to indicate
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e
sdp_attr_set_rtcp_fb_nack(sdp_t *sdp_p, uint16_t level, uint16_t payload_type, uint16_t inst,
                          sdp_rtcp_fb_nack_type_e type)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_RTCP_FB, inst);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtcp_fb nack attribute, level %u "
                      "instance %u not found.", sdp_p->debug_str, (unsigned)level,
                      (unsigned)inst);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    attr_p->attr.rtcp_fb.payload_num = payload_type;
    attr_p->attr.rtcp_fb.feedback_type = SDP_RTCP_FB_NACK;
    attr_p->attr.rtcp_fb.param.nack = type;
    attr_p->attr.rtcp_fb.extra[0] = '\0';
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_rtcp_fb_trr_int
 * Description: Sets the value of an rtcp-fb:...trr-int attribute
 * Parameters:  sdp_p        The SDP handle returned by sdp_init_description.
 *              level          The level to set the attribute.
 *              payload_type   The value to set the payload type to for
 *                             this attribute. Can be SDP_ALL_PAYLOADS.
 *              inst_num       The attribute instance number to check.
 *              interval       The interval time to indicate
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e
sdp_attr_set_rtcp_fb_trr_int(sdp_t *sdp_p, uint16_t level, uint16_t payload_type,
                             uint16_t inst, uint32_t interval)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_RTCP_FB, inst);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtcp_fb trr-int attribute, level %u "
                      "instance %u not found.", sdp_p->debug_str, (unsigned)level,
                      (unsigned)inst);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    attr_p->attr.rtcp_fb.payload_num = payload_type;
    attr_p->attr.rtcp_fb.feedback_type = SDP_RTCP_FB_TRR_INT;
    attr_p->attr.rtcp_fb.param.trr_int = interval;
    attr_p->attr.rtcp_fb.extra[0] = '\0';
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_set_rtcp_fb_ccm
 * Description: Sets the value of an rtcp-fb:...ccm attribute
 * Parameters:  sdp_p        The SDP handle returned by sdp_init_description.
 *              level          The level to set the attribute.
 *              payload_type   The value to set the payload type to for
 *                             this attribute. Can be SDP_ALL_PAYLOADS.
 *              inst_num       The attribute instance number to check.
 *              type           The ccm type to indicate
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e
sdp_attr_set_rtcp_fb_ccm(sdp_t *sdp_p, uint16_t level, uint16_t payload_type, uint16_t inst,
                         sdp_rtcp_fb_ccm_type_e type)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_RTCP_FB, inst);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s rtcp_fb ccm attribute, level %u "
                      "instance %u not found.", sdp_p->debug_str, (unsigned)level,
                      (unsigned)inst);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }
    attr_p->attr.rtcp_fb.payload_num = payload_type;
    attr_p->attr.rtcp_fb.feedback_type = SDP_RTCP_FB_CCM;
    attr_p->attr.rtcp_fb.param.ccm = type;
    attr_p->attr.rtcp_fb.extra[0] = '\0';
    return (SDP_SUCCESS);
}

/* Function:    sdp_attr_get_extmap_uri
 * Description: Returns a pointer to the value of the encoding name
 *              parameter specified for the given attribute.  Value is
 *              returned as a const ptr and so cannot be modified by the
 *              application.  If the given attribute is not defined, NULL
 *              will be returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              inst_num    The attribute instance number to check.
 * Returns:     Codec value or SDP_CODEC_INVALID.
 */
const char *sdp_attr_get_extmap_uri(sdp_t *sdp_p, uint16_t level,
                                    uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_EXTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s extmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (NULL);
    } else {
        return (attr_p->attr.extmap.uri);
    }
}

/* Function:    sdp_attr_get_extmap_id
 * Description: Returns the id of the  extmap specified for the given
 *              attribute.  If the given attribute is not defined, 0xFFFF
 *              will be returned.
 * Parameters:  sdp_p     The SDP handle returned by sdp_init_description.
 *              level       The level to check for the attribute.
 *              inst_num    The attribute instance number to check.
 * Returns:     The id of the extmap attribute.
 */
uint16_t sdp_attr_get_extmap_id(sdp_t *sdp_p, uint16_t level,
                           uint16_t inst_num)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_EXTMAP, inst_num);
    if (attr_p == NULL) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s extmap attribute, level %u instance %u "
                      "not found.", sdp_p->debug_str, (unsigned)level, (unsigned)inst_num);
        }
        sdp_p->conf_p->num_invalid_param++;
        return 0xFFFF;
    } else {
        return (attr_p->attr.extmap.id);
    }
}

/* Function:    sdp_attr_set_extmap
 * Description: Sets the value of an rtcp-fb:...ccm attribute
 * Parameters:  sdp_p        The SDP handle returned by sdp_init_description.
 *              level          The level to set the attribute.
 *              id             The id to set the attribute.
 *              uri            The uri to set the attribute.
 *              inst           The attribute instance number to check.
 * Returns:     SDP_SUCCESS            Attribute param was set successfully.
 *              SDP_INVALID_PARAMETER  Specified attribute is not defined.
 */
sdp_result_e
sdp_attr_set_extmap(sdp_t *sdp_p, uint16_t level, uint16_t id, const char* uri, uint16_t inst)
{
    sdp_attr_t  *attr_p;

    attr_p = sdp_find_attr(sdp_p, level, 0, SDP_ATTR_EXTMAP, inst);
    if (!attr_p) {
        if (sdp_p->debug_flag[SDP_DEBUG_ERRORS]) {
            CSFLogError(logTag, "%s extmap attribute, level %u "
                      "instance %u not found.", sdp_p->debug_str, (unsigned)level,
                      (unsigned)inst);
        }
        sdp_p->conf_p->num_invalid_param++;
        return (SDP_INVALID_PARAMETER);
    }

    attr_p->attr.extmap.id = id;
    sstrncpy(attr_p->attr.extmap.uri, uri, SDP_MAX_STRING_LEN+1);
    return (SDP_SUCCESS);
}

const char *sdp_attr_get_msid_identifier(sdp_t *sdp_p, uint16_t level,
                                       uint8_t cap_num, uint16_t inst)
{
    sdp_attr_t  *attr_p = sdp_find_attr(sdp_p, level, cap_num,
                                        SDP_ATTR_MSID, inst);
    if (!attr_p) {
      return NULL;
    }
    return attr_p->attr.msid.identifier;
}

const char *sdp_attr_get_msid_appdata(sdp_t *sdp_p, uint16_t level,
                                      uint8_t cap_num, uint16_t inst)
{
    sdp_attr_t  *attr_p = sdp_find_attr(sdp_p, level, cap_num,
                                        SDP_ATTR_MSID, inst);
    if (!attr_p) {
      return NULL;
    }
    return attr_p->attr.msid.appdata;
}
