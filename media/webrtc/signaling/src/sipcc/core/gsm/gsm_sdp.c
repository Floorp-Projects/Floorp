/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "cpr_in.h"
#include "cpr_rand.h"
#include "cpr_stdlib.h"
#include "lsm.h"
#include "fsm.h"
#include "ccapi.h"
#include "ccsip_sdp.h"
#include "sdp.h"
#include "gsm.h"
#include "gsm_sdp.h"
#include "util_string.h"
#include "rtp_defs.h"
#include "debug.h"
#include "dtmf.h"
#include "prot_configmgr.h"
#include "dns_utils.h"
#include "sip_interface_regmgr.h"
#include "platform_api.h"
#include "vcm.h"
#include "prlog.h"
#include "plstr.h"
#include "sdp_private.h"

static const char* logTag = "gsm_sdp";

//TODO Need to place this in a portable location
#define MULTICAST_START_ADDRESS 0xe1000000
#define MULTICAST_END_ADDRESS   0xefffffff

/* Only first octet contains codec type */
#define GET_CODEC_TYPE(a)    ((uint8_t)((a) & 0XFF))

#define GSMSDP_SET_MEDIA_DIABLE(media) \
     (media->src_port = 0)

#define CAST_DEFAULT_BITRATE 320000
/*
 * The maximum number of media lines per call. This puts the upper limit
 * on * the maximum number of media lines per call to resource hogging.
 * The value of 8 is intended up to 2 audio and 2 video streams with
 * each stream can offer IPV4 and IPV6 alternate network address type
 * in ANAT group (RFC-4091).
 */
#define GSMSDP_MAX_MLINES_PER_CALL  (8)

/*
 * Permanent number of free media structure elements for media structure
 * that represents media line in the SDP. The maximum number of elements
 * is set to equal number of call or LSM_MAX_CALLS. This should be enough
 * to minimumly allow typical a single audio media stream per call scenario
 * without using dynamic memory.
 *
 * If more media structures are needed than this number, the addition
 * media structures are allocated from heap and they will be freed back
 * from heap after thehy are not used. The only time where the heap
 * is used when phone reaches the maximum call capacity and each one
 * of the call is using more than one media lines.
 */
#define GSMSDP_PERM_MEDIA_ELEMS   (LSM_MAX_CALLS)

/*
 * The permanent free media structure elements use static array.
 * It is to ensure a low overhead for this a typical single audio call.
 */
static fsmdef_media_t gsmsdp_free_media_chunk[GSMSDP_PERM_MEDIA_ELEMS];
static sll_lite_list_t gsmsdp_free_media_list;

typedef enum {
    MEDIA_TABLE_GLOBAL,
    MEDIA_TABLE_SESSION
} media_table_e;

/* Forward references */
static cc_causes_t
gsmsdp_init_local_sdp (const char *peerconnection, cc_sdp_t **sdp_pp);

static void
gsmsdp_set_media_capability(fsmdef_media_t *media,
                            const cc_media_cap_t *media_cap);
static fsmdef_media_t *
gsmsdp_add_media_line(fsmdef_dcb_t *dcb_p, const cc_media_cap_t *media_cap,
                      uint8_t cap_index, uint16_t level,
                      cpr_ip_type addr_type, boolean offer);
static boolean
gsmsdp_add_remote_stream(uint16_t idx, int pc_stream_id,
                         fsmdef_dcb_t *dcb_p);

static boolean
gsmsdp_add_remote_track(uint16_t idx, uint16_t track,
                         fsmdef_dcb_t *dcb_p, fsmdef_media_t *media);



extern cc_media_cap_table_t g_media_table;

extern boolean g_disable_mass_reg_debug_print;

/**
 * A wraper function to return the media capability supported by
 * the platform and session. This is a convient place if policy
 * to get the capability table as it applies to the session
 * updates the media_cap_tbl ptr in dcb
 *
 * @param[in]dcb     - pointer to the fsmdef_dcb_t

 *
 * @return           - pointer to the the media capability table for session
 */
static const cc_media_cap_table_t *gsmsdp_get_media_capability (fsmdef_dcb_t *dcb_p)
{
    static const char *fname = "gsmsdp_get_media_capability";
    int                sdpmode = 0;

    if (g_disable_mass_reg_debug_print == FALSE) {
        GSM_DEBUG(DEB_F_PREFIX"dcb video pref %x",
                               DEB_F_PREFIX_ARGS(GSM, fname), dcb_p->video_pref);
    }

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    if ( dcb_p->media_cap_tbl == NULL ) {
         dcb_p->media_cap_tbl = (cc_media_cap_table_t*) cpr_calloc(1, sizeof(cc_media_cap_table_t));
         if ( dcb_p->media_cap_tbl == NULL ) {
             GSM_ERR_MSG(GSM_L_C_F_PREFIX"media table malloc failed.",
                    dcb_p->line, dcb_p->call_id, fname);
             return NULL;
         }
    }

    if (sdpmode) {
        /* Here we are copying only what we need from the g_media_table
           in order to avoid a data race with its values being set in
           media_cap_tbl.c */
        dcb_p->media_cap_tbl->id = g_media_table.id;

        /* This needs to change when we handle more than one stream
           of each media type at a time. */

        dcb_p->media_cap_tbl->cap[CC_AUDIO_1].name = CC_AUDIO_1;
        dcb_p->media_cap_tbl->cap[CC_VIDEO_1].name = CC_VIDEO_1;
        dcb_p->media_cap_tbl->cap[CC_DATACHANNEL_1].name = CC_DATACHANNEL_1;

        dcb_p->media_cap_tbl->cap[CC_AUDIO_1].type = SDP_MEDIA_AUDIO;
        dcb_p->media_cap_tbl->cap[CC_VIDEO_1].type = SDP_MEDIA_VIDEO;
        dcb_p->media_cap_tbl->cap[CC_DATACHANNEL_1].type = SDP_MEDIA_APPLICATION;

        dcb_p->media_cap_tbl->cap[CC_AUDIO_1].enabled = FALSE;
        dcb_p->media_cap_tbl->cap[CC_VIDEO_1].enabled = FALSE;
        dcb_p->media_cap_tbl->cap[CC_DATACHANNEL_1].enabled = FALSE;

        dcb_p->media_cap_tbl->cap[CC_AUDIO_1].support_security = TRUE;
        dcb_p->media_cap_tbl->cap[CC_VIDEO_1].support_security = TRUE;
        dcb_p->media_cap_tbl->cap[CC_DATACHANNEL_1].support_security = TRUE;

        /* By default, all channels are "bundle only" */
        dcb_p->media_cap_tbl->cap[CC_AUDIO_1].bundle_only = TRUE;
        dcb_p->media_cap_tbl->cap[CC_VIDEO_1].bundle_only = TRUE;
        dcb_p->media_cap_tbl->cap[CC_DATACHANNEL_1].bundle_only = TRUE;

        /* We initialize as RECVONLY to allow the application to
           display incoming media streams, even if it doesn't
           plan to send media for those streams. This will be
           upgraded to SENDRECV when and if a stream is added. */

        dcb_p->media_cap_tbl->cap[CC_AUDIO_1].support_direction =
          SDP_DIRECTION_RECVONLY;

        dcb_p->media_cap_tbl->cap[CC_VIDEO_1].support_direction =
          SDP_DIRECTION_RECVONLY;

        dcb_p->media_cap_tbl->cap[CC_DATACHANNEL_1].support_direction =
          SDP_DIRECTION_SENDRECV;
    } else {
        *(dcb_p->media_cap_tbl) = g_media_table;

        dcb_p->media_cap_tbl->cap[CC_DATACHANNEL_1].enabled = FALSE;

        if ( dcb_p->video_pref == SDP_DIRECTION_INACTIVE) {
            // do not enable video
            dcb_p->media_cap_tbl->cap[CC_VIDEO_1].enabled = FALSE;
        }

        if ( dcb_p->video_pref == SDP_DIRECTION_RECVONLY ) {
            if ( dcb_p->media_cap_tbl->cap[CC_VIDEO_1].support_direction == SDP_DIRECTION_SENDRECV ) {
                dcb_p->media_cap_tbl->cap[CC_VIDEO_1].support_direction = dcb_p->video_pref;
            }

            if ( dcb_p->media_cap_tbl->cap[CC_VIDEO_1].support_direction == SDP_DIRECTION_SENDONLY ) {
                dcb_p->media_cap_tbl->cap[CC_VIDEO_1].support_direction = SDP_DIRECTION_INACTIVE;
                DEF_DEBUG(GSM_L_C_F_PREFIX"video capability disabled to SDP_DIRECTION_INACTIVE from sendonly",
                dcb_p->line, dcb_p->call_id, fname);
            }
        } else if ( dcb_p->video_pref == SDP_DIRECTION_SENDONLY ) {
            if ( dcb_p->media_cap_tbl->cap[CC_VIDEO_1].support_direction == SDP_DIRECTION_SENDRECV ) {
                dcb_p->media_cap_tbl->cap[CC_VIDEO_1].support_direction = dcb_p->video_pref;
            }

            if ( dcb_p->media_cap_tbl->cap[CC_VIDEO_1].support_direction == SDP_DIRECTION_RECVONLY ) {
               dcb_p->media_cap_tbl->cap[CC_VIDEO_1].support_direction = SDP_DIRECTION_INACTIVE;
                DEF_DEBUG(GSM_L_C_F_PREFIX"video capability disabled to SDP_DIRECTION_INACTIVE from recvonly",
                    dcb_p->line, dcb_p->call_id, fname);
            }
        } // else if requested is SENDRECV just go by capability
    }

    return (dcb_p->media_cap_tbl);
}

/*
 * Process a single constraint for one media capablity
 */
void gsmsdp_process_cap_constraint(cc_media_cap_t *cap,
                                   cc_boolean constraint) {
  if (!constraint) {
    cap->support_direction &= ~SDP_DIRECTION_FLAG_RECV;
  } else {
    cap->support_direction |= SDP_DIRECTION_FLAG_RECV;
    cap->enabled = TRUE;
  }
}

/*
 * Process options only related to media capabilities., i.e
 * OfferToReceiveAudio, OfferToReceiveVideo
 */
void gsmsdp_process_cap_options(fsmdef_dcb_t *dcb,
                                    cc_media_options_t* options) {
  if (options->offer_to_receive_audio.was_passed) {
    gsmsdp_process_cap_constraint(&dcb->media_cap_tbl->cap[CC_AUDIO_1],
                                  options->offer_to_receive_audio.value);
  }
  if (options->offer_to_receive_video.was_passed) {
    gsmsdp_process_cap_constraint(&dcb->media_cap_tbl->cap[CC_VIDEO_1],
                                  options->offer_to_receive_video.value);
  }
  if (options->moz_dont_offer_datachannel.was_passed) {
    /* Hack to suppress data channel */
    if (options->moz_dont_offer_datachannel.value) {
      dcb->media_cap_tbl->cap[CC_DATACHANNEL_1].enabled = FALSE;
    }
  }
}

/**
 * Copy an fsmdef_media_t's payload list to its previous_sdp's payload list
 *
 * @param[in]media   - pointer to the fsmdef_media_t to update
 */
void gsmsdp_copy_payloads_to_previous_sdp (fsmdef_media_t *media)
{
    static const char *fname = "gsmsdp_copy_payloads_to_previous_sdp";

    if ((!media->payloads) && (NULL != media->previous_sdp.payloads))
    {
      cpr_free(media->previous_sdp.payloads);
      media->previous_sdp.payloads = NULL;
      media->previous_sdp.num_payloads = 0;
    }

    /* Ensure that there is enough space to hold all the payloads */
    if (media->num_payloads > media->previous_sdp.num_payloads)
    {
      media->previous_sdp.payloads =
        cpr_realloc(media->previous_sdp.payloads,
            media->num_payloads * sizeof(vcm_payload_info_t));
    }

    /* Copy the payloads over */
    media->previous_sdp.num_payloads = media->num_payloads;
    memcpy(media->previous_sdp.payloads, media->payloads,
        media->num_payloads * sizeof(vcm_payload_info_t));
    media->previous_sdp.num_payloads = media->num_payloads;
}

/**
 * Find an entry for the specified codec type in the vcm_payload_info_t
 * list array.
 *
 * @param[in]codec            - Codec type to find
 * @param[in]payload_info     - Array of payload info entries
 * @param[in]num_payload_info - Total number of elements in payload_info
 * @param[in]instance         - Which instance of the codec to find
 *                              (e.g., if more than one entry for a codec type)
 *
 * @return           Pointer to the payload info if found;
 *                   NULL when no match is found.
 */
vcm_payload_info_t *gsmsdp_find_info_for_codec(rtp_ptype codec,
                                         vcm_payload_info_t *payload_info,
                                         int num_payload_info,
                                         int instance) {
    int i;
    for (i = 0; i < num_payload_info; i++) {
        if (payload_info[i].codec_type == codec)
        {
            instance--;
            if (instance == 0) {
                return &(payload_info[i]);
            }
        }
    }
    return NULL;
}


/**
 * Sets up the media track table
 *
 * @param[in]dcb     - pointer to the fsmdef_dcb_t
 *
 * @return           - pointer to the the media track table for session
 */
static const cc_media_remote_stream_table_t *gsmsdp_get_media_stream_table (fsmdef_dcb_t *dcb_p)
{
    static const char *fname = "gsmsdp_get_media_stream_table";
    if ( dcb_p->remote_media_stream_tbl == NULL ) {
      dcb_p->remote_media_stream_tbl = (cc_media_remote_stream_table_t*) cpr_malloc(sizeof(cc_media_remote_stream_table_t));
      if ( dcb_p->remote_media_stream_tbl == NULL ) {
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"media track table malloc failed.",
                    dcb_p->line, dcb_p->call_id, fname);
        return NULL;
      }
    }

    memset(dcb_p->remote_media_stream_tbl, 0, sizeof(cc_media_remote_stream_table_t));

    return (dcb_p->remote_media_stream_tbl);
}

/**
 * The function creates a free media structure elements list. The
 * free list is global for all calls. The function must be called once
 * during GSM initializtion.
 *
 * @param            None.
 *
 * @return           TRUE  - free media structure list is created
 *                           successfully.
 *                   FALSE - failed to create free media structure
 *                           list.
 */
boolean
gsmsdp_create_free_media_list (void)
{
    uint32_t i;
    fsmdef_media_t *media;

    /* initialize free media_list structure */
    (void)sll_lite_init(&gsmsdp_free_media_list);

    /*
     * Populate the free list:
     * Break the entire chunk into multiple free elements and link them
     * onto to the free media list.
     */
    media = &gsmsdp_free_media_chunk[0];    /* first element */
    for (i = 0; i < GSMSDP_PERM_MEDIA_ELEMS; i++) {
        (void)sll_lite_link_head(&gsmsdp_free_media_list,
                                 (sll_lite_node_t *)media);
        media = media + 1; /* next element */
    }

    /* Successful create media free list */
    return (TRUE);
}

/**
 * The function destroys the free media structure list. It should be
 * call during GSM shutdown.
 *
 * @param            None.
 *
 * @return           None.
 */
void
gsmsdp_destroy_free_media_list (void)
{
    /*
     * Although the free chunk is not allocated but,
     * NULL out the free list header to indicate that the
     * there is not thing from the free chunk.
     */
    (void)sll_lite_init(&gsmsdp_free_media_list);
}

/**
 * The function allocates a media structure. The function
 * attempts to obtain a free media structure from the free media
 * structure list first. If free list is empty then the media structure
 * is allocated from a memory pool.
 *
 * @param            None.
 *
 * @return           pointer to the fsmdef_media_t if successful or
 *                   NULL when there is no free media structure
 *                   is available.
 */
static fsmdef_media_t *
gsmsdp_alloc_media (void)
{
    static const char fname[] = "gsmsdp_alloc_media";
    fsmdef_media_t *media = NULL;

    /* Get a media element from the free list */
    media = (fsmdef_media_t *)sll_lite_unlink_head(&gsmsdp_free_media_list);
    if (media == NULL) {
        /* no free element from cache, allocate it from the pool */
        media = cpr_malloc(sizeof(fsmdef_media_t));
        GSM_DEBUG(DEB_F_PREFIX"get from dynamic pool, media %p",
                               DEB_F_PREFIX_ARGS(GSM, fname), media);
    }
    return (media);
}

/**
 * The function frees a media structure back to the free list or
 * heap. If the media structure is from the free list then it
 * is put back to the free list otherwise it will be freed
 * back to the dynamic pool.
 *
 * @param[in]media   - pointer to fsmdef_media_t to free back to
 *                   free list.
 *
 * @return           pointer to the fsmdef_media_t if successful
 *                   NULL when there is no free media structure
 *                   is available.
 */
static void
gsmsdp_free_media (fsmdef_media_t *media)
{
    static const char fname[] = "gsmsdp_free_media";

    if (media == NULL) {
        return;
    }

    if (media-> video != NULL ) {
      vcmFreeMediaPtr(media->video);
    }

    if(media->payloads != NULL) {
        cpr_free(media->payloads);
        media->payloads = NULL;
        media->num_payloads = 0;
    }
    /*
     * Check to see if the element is part of the
     * free chunk space.
     */
    if ((media >= &gsmsdp_free_media_chunk[0]) &&
        (media <= &gsmsdp_free_media_chunk[GSMSDP_PERM_MEDIA_ELEMS-1])) {
        /* the element is part of free chunk, put it back to the list */
        (void)sll_lite_link_head(&gsmsdp_free_media_list,
                                 (sll_lite_node_t *)media);
    } else {
        /* this element is from the dynamic pool, free it back */
        cpr_free(media);
        GSM_DEBUG(DEB_F_PREFIX"free media %p to dynamic pool",
                  DEB_F_PREFIX_ARGS(GSM, fname), media);
    }
}

/**
 * Initialize the media entry. The function initializes media
 * entry.
 *
 * @param[in]media - pointer to fsmdef_media_t of the media entry to be
 *                   initialized.
 *
 * @return  none
 *
 * @pre     (media not_eq NULL)
 */
static void
gsmsdp_init_media (fsmdef_media_t *media)
{
    media->refid = CC_NO_MEDIA_REF_ID;
    media->type = SDP_MEDIA_INVALID; /* invalid (free entry) */
    media->packetization_period = ATTR_PTIME;
    media->max_packetization_period = ATTR_MAXPTIME;
    media->mode = (uint16_t)vcmGetILBCMode();
    media->vad = VCM_VAD_OFF;
    /* Default to audio codec */
    media->level = 0;
    media->dest_port = 0;
    media->dest_addr = ip_addr_invalid;
    media->is_multicast = FALSE;
    media->multicast_port = 0;
    media->avt_payload_type = RTP_NONE;
    media->src_port = 0;
    media->src_addr = ip_addr_invalid;
    media->rcv_chan = FALSE;
    media->xmit_chan = FALSE;

    media->direction = SDP_DIRECTION_INACTIVE;
    media->direction_set = FALSE;
    media->transport = SDP_TRANSPORT_INVALID;
    media->tias_bw = SDP_INVALID_VALUE;
    media->profile_level = 0;

    media->previous_sdp.avt_payload_type = RTP_NONE;
    media->previous_sdp.dest_addr = ip_addr_invalid;
    media->previous_sdp.dest_port = 0;
    media->previous_sdp.direction = SDP_DIRECTION_INACTIVE;
    media->previous_sdp.packetization_period = media->packetization_period;
    media->previous_sdp.max_packetization_period = media->max_packetization_period;
    media->previous_sdp.payloads = NULL;
    media->previous_sdp.num_payloads = 0;
    media->previous_sdp.tias_bw = SDP_INVALID_VALUE;
    media->previous_sdp.profile_level = 0;
    media->hold  = FSM_HOLD_NONE;
    media->flags = 0;                    /* clear all flags      */
    media->cap_index = CC_MAX_MEDIA_CAP; /* max is invalid value */
    media->video = NULL;
    media->candidate_ct = 0;
    media->rtcp_mux = FALSE;
    media->audio_level = TRUE;
    media->audio_level_id = 1;
    /* ACTPASS is the value we put in every offer */
    media->setup = SDP_SETUP_ACTPASS;
    media->local_datachannel_port = 0;
    media->remote_datachannel_port = 0;
    media->datachannel_streams = WEBRTC_DATACHANNEL_STREAMS_DEFAULT;
    sstrncpy(media->datachannel_protocol, WEBRTC_DATA_CHANNEL_PROT, SDP_MAX_STRING_LEN);

    media->payloads = NULL;
    media->num_payloads = 0;
}

/**
 *
 * Returns a pointer to a new the fsmdef_media_t for a given dcb.
 * The default media parameters will be intialized for the known or
 * supported media types. The new media is also added to the media list
 * in the dcb.
 *
 * @param[in]dcb_p      - pointer to the fsmdef_dcb_t
 * @param[in]media_type - sdp_media_e.
 * @param[in]level      - uint16_t for media line level.
 *
 * @return           pointer to the fsmdef_media_t of the corresponding
 *                   media entry in the dcb.
 * @pre              (dcb not_eq NULL)
 */
static fsmdef_media_t *
gsmsdp_get_new_media (fsmdef_dcb_t *dcb_p, sdp_media_e media_type,
                      uint16_t level)
{
    static const char fname[] = "gsmsdp_get_new_media";
    fsmdef_media_t *media;
    static media_refid_t media_refid = CC_NO_MEDIA_REF_ID;
    sll_lite_return_e sll_lite_ret;

    /* check to ensue we do not handle too many media lines */
    if (GSMSDP_MEDIA_COUNT(dcb_p) >= GSMSDP_MAX_MLINES_PER_CALL) {
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"exceeding media lines per call",
                    dcb_p->line, dcb_p->call_id, fname);
        return (NULL);
    }

    /* allocate new media entry */
    media = gsmsdp_alloc_media();
    if (media != NULL) {
        /* initialize the media entry */
        gsmsdp_init_media(media);

        /* assigned media reference id */
        if (++media_refid == CC_NO_MEDIA_REF_ID) {
            media_refid = 1;
        }
        media->refid = media_refid;
        media->type  = media_type;
        media->level = level;

        /* append the media to the active list */
        sll_lite_ret = sll_lite_link_tail(&dcb_p->media_list,
                           (sll_lite_node_t *)media);
        if (sll_lite_ret != SLL_LITE_RET_SUCCESS) {
            /* fails to put the new media entry on to the list */
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"error %d when add media to list",
                        dcb_p->line, dcb_p->call_id, fname, sll_lite_ret);
            gsmsdp_free_media(media);
            media = NULL;
        }
    }
    return (media);
}

/**
 * The function removes the media entry from the list of a given call and
 * then deallocates the media entry.
 *
 * @param[in]dcb   - pointer to fsmdef_def_t for the dcb whose
 *                   media to be removed from.
 * @param[in]media - pointer to fsmdef_media_t for the media
 *                   entry to be removed.
 *
 * @return  none
 *
 * @pre     (dcb not_eq NULL)
 */
static void gsmsdp_remove_media (fsmdef_dcb_t *dcb_p, fsmdef_media_t *media)
{
    static const char fname[] = "gsmsdp_remove_media";
    cc_action_data_t data;

    if (media == NULL) {
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"removing NULL media",
                    dcb_p->line, dcb_p->call_id, fname);
        return;
    }

    if (media->rcv_chan || media->xmit_chan) {
        /* stop media, if it is opened */
        data.stop_media.media_refid = media->refid;
        (void)cc_call_action(dcb_p->call_id, dcb_p->line, CC_ACTION_STOP_MEDIA,
                             &data);
    }
    /* remove this media off the list */
    (void)sll_lite_remove(&dcb_p->media_list, (sll_lite_node_t *)media);

    /* Release the port */
    vcmRxReleasePort(media->cap_index, dcb_p->group_id, media->refid,
                 lsm_get_ms_ui_call_handle(dcb_p->line, dcb_p->call_id, CC_NO_CALL_ID), media->src_port);

    /* free media structure */
    gsmsdp_free_media(media);
}

/**
 * The function performs cleaning media list of a given call. It walks
 * through the list and deallocates each media entries.
 *
 * @param[in]dcb   - pointer to fsmdef_def_t for the dcb whose
 *                   media list to be cleaned.
 *
 * @return  none
 *
 * @pre     (dcb not_eq NULL)
 */
void gsmsdp_clean_media_list (fsmdef_dcb_t *dcb_p)
{
    fsmdef_media_t *media = NULL;

    while (TRUE) {
        /* unlink head and free the media */
        media = (fsmdef_media_t *)sll_lite_unlink_head(&dcb_p->media_list);
        if (media != NULL) {
            gsmsdp_free_media(media);
        } else {
            break;
        }
    }
}

/**
 *
 * The function is used for per call media list initialization. It is
 * an interface function to other module for initializing the media list
 * used during a call.
 *
 * @param[in]dcb_p   - pointer to the fsmdef_dcb_t where the media list
 *                   will be attached to.
 *
 * @return           None.
 * @pre              (dcb not_eq NULL)
 */
void gsmsdp_init_media_list (fsmdef_dcb_t *dcb_p)
{
    const cc_media_cap_table_t *media_cap_tbl;
    const cc_media_remote_stream_table_t *media_track_tbl;
    const char                 fname[] = "gsmsdp_init_media_list";

    /* do the actual media element list initialization */
    (void)sll_lite_init(&dcb_p->media_list);

    media_cap_tbl = gsmsdp_get_media_capability(dcb_p);

    if (media_cap_tbl == NULL) {
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"no media capbility available",
                    dcb_p->line, dcb_p->call_id, fname);
    }

    media_track_tbl = gsmsdp_get_media_stream_table(dcb_p);

    if (media_track_tbl == NULL) {
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"no media tracks available",
                    dcb_p->line, dcb_p->call_id, fname);
    }
}

/**
 *
 * Returns a pointer to the fsmdef_media_t in the dcb for the
 * correspoinding media level in the SDP.
 *
 * @param[in]dcb_p      - pointer to the fsmdef_dcb_t
 * @param[in]level      - uint16_t for media line level.
 *
 * @return           pointer to the fsmdef_media_t of the corresponding
 *                   media entry in the dcb.
 * @pre              (dcb not_eq NULL)
 */
static fsmdef_media_t *
gsmsdp_find_media_by_level (fsmdef_dcb_t *dcb_p, uint16_t level)
{
    fsmdef_media_t *media = NULL;

    /*
     * search the all entries that has a valid media and matches
     * the level.
     */
    GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
        if (media->level == level) {
            /* found a match */
            return (media);
        }
    }
    return (NULL);
}

/**
 *
 * Returns a pointer to the fsmdef_media_t in the dcb for the
 * correspoinding reference ID.
 *
 * @param[in]dcb_p      - pointer to the fsmdef_dcb_t
 * @param[in]refid      - media reference ID to look for.
 *
 * @return           pointer to the fsmdef_media_t of the corresponding
 *                   media entry in the dcb.
 * @pre              (dcb not_eq NULL)
 */
fsmdef_media_t *
gsmsdp_find_media_by_refid (fsmdef_dcb_t *dcb_p, media_refid_t refid)
{
    fsmdef_media_t *media = NULL;

    /*
     * search the all entries that has a valid media and matches
     * the reference ID.
     */
    GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
        if (media->refid == refid) {
            /* found a match */
            return (media);
        }
    }
    return (NULL);
}

/**
 *
 * Returns a pointer to the fsmdef_media_t in the dcb for the
 * correspoinding capability index.
 *
 * @param[in]dcb_p      - pointer to the fsmdef_dcb_t
 * @param[in]cap_index  - capability table index.
 *
 * @return           pointer to the fsmdef_media_t of the corresponding
 *                   media entry in the dcb.
 * @pre              (dcb not_eq NULL)
 */
static fsmdef_media_t *
gsmsdp_find_media_by_cap_index (fsmdef_dcb_t *dcb_p, uint8_t cap_index)
{
    fsmdef_media_t *media = NULL;

    /*
     * search the all entries that has a valid media and matches
     * the reference ID.
     */
    GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
        if (media->cap_index == cap_index) {
            /* found a match */
            return (media);
        }
    }
    return (NULL);

}

/**
 *
 * Returns a pointer to the fsmdef_media_t in the dcb for the
 * first audio type in the SDP.
 *
 * @param[in]dcb_p      - pointer to the fsmdef_dcb_t.
 *
 * @return           pointer to the fsmdef_media_t of the corresponding
 *                   media entry in the dcb.
 * @pre              (dcb not_eq NULL)
 */
fsmdef_media_t *gsmsdp_find_audio_media (fsmdef_dcb_t *dcb_p)
{
    fsmdef_media_t *media = NULL;

    /*
     * search the all entries that has a valid media and matches
     * SDP_MEDIA_AUDIO type.
     */
    GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
        if (media->type == SDP_MEDIA_AUDIO) {
            /* found a match */
            return (media);
        }
    }
    return (NULL);
}

/**
 *
 * The function finds an unused media line given type.
 *
 * @param[in]sdp        - void pointer of thd SDP libray handle.
 * @param[in]media_type - media type of the unused line.
 *
 * @return           level (line) of the unused one if found or
 *                   0 if there is no unused one found.
 */
static uint16_t
gsmsdp_find_unused_media_line_with_type (void *sdp, sdp_media_e media_type)
{
    uint16_t num_m_lines, level;
    int32_t  port;

    num_m_lines  = sdp_get_num_media_lines(sdp);
    for (level = 1; level <= num_m_lines; level++) {
        port = sdp_get_media_portnum(sdp, level);
        if (port == 0) {
            /* This slot is not used, check the type */
            if (sdp_get_media_type(sdp, level) == media_type) {
                /* Found an empty slot that has the same media type */
                return (level);
            }
        }
    }
    /* no unused media line of the given type found */
    return (0);
}

/**
 *
 * The function returns the media cap entry pointer to the caller based
 * on the index.
 *
 * @param[in]cap_index  - uint8_t for index of the media cap table.
 *
 * @return           pointer to the media cap entry if one is available.
 *                   NULL if none is available.
 *
 */
static const cc_media_cap_t *
gsmsdp_get_media_cap_entry_by_index (uint8_t cap_index, fsmdef_dcb_t *dcb_p)
{
    const cc_media_cap_table_t *media_cap_tbl;

    media_cap_tbl = dcb_p->media_cap_tbl;

    if (media_cap_tbl == NULL) {
        return (NULL);
    }

    if (cap_index >= CC_MAX_MEDIA_CAP) {
        return (NULL);
    }
    return (&media_cap_tbl->cap[cap_index]);
}

/**
 *
 * Returns a pointer to the fsmdef_media_t in the dcb for the
 * corresponding media line. It looks for another media line
 * with the same type and cap_index but different level
 *
 * @param[in]dcb_p      - pointer to the fsmdef_dcb_t
 * @param[in]media      - current media level.
 *
 * @return           pointer to the fsmdef_media_t of the corresponding
 *                   media entry in the dcb.
 * @pre              (dcb not_eq NULL)
 */
fsmdef_media_t *
gsmsdp_find_anat_pair (fsmdef_dcb_t *dcb_p, fsmdef_media_t *media)
{
    fsmdef_media_t *searched_media = NULL;

    /*
     * search the all entries that has a the same capability index
     * but at a different level. The only time that this is true is
     * both media are in the same ANAT group.
     */
    GSMSDP_FOR_ALL_MEDIA(searched_media, dcb_p) {
        if ((searched_media->cap_index == media->cap_index) &&
            (searched_media->level != media->level)) {
            /* found a match */
            return (searched_media);
        }
    }
    return (NULL);
}

/**
 *
 * The function queries platform to see if the platform is capable
 * of handle mixing additional media or not.
 *
 * P2: This may go away when integrate with the platform.
 *
 * @param[in]dcb_p      - pointer to the fsmdef_dcb_t structure.
 * @param[in]media_type - media type to be mixed.
 *
 * @return          TRUE the media can be mixed.
 *                  FALSE the media can not be mixed
 *
 * @pre            (dcb_p not_eq NULL)
 */
static boolean
gsmsdp_platform_addition_mix (fsmdef_dcb_t *dcb_p, sdp_media_e media_type)
{
    return (FALSE);
}


/**
 *
 * The function updates the local time stamp during SDP offer/answer
 * processing.
 *
 * @param[in]dcb_p         - pointer to the fsmdef_dcb_t
 * @param[in]offer         - boolean indicates this is procssing an offered
 *                           SDP
 * @param[in]initial_offer - boolean indicates this is processin an
 *                           initial offered SDP.
 *
 * @return           none.
 * @pre              (dcb not_eq NULL)
 */
static void
gsmsdp_update_local_time_stamp (fsmdef_dcb_t *dcb_p, boolean offer,
                                boolean initial_offer)
{
    const char                 fname[] = "gsmsdp_update_local_time_stamp";
    void           *local_sdp_p;
    void           *remote_sdp_p;

    local_sdp_p  = dcb_p->sdp->src_sdp;
    remote_sdp_p = dcb_p->sdp->dest_sdp;

    /*
     * If we are processing an offer sdp, need to set the
     * start time and stop time based on the remote SDP
     */
    if (initial_offer) {
        /*
         * Per RFC3264, time description of answer must equal that
         * of the offer.
         */
        (void) sdp_set_time_start(local_sdp_p,
                                  sdp_get_time_start(remote_sdp_p));
        (void) sdp_set_time_stop(local_sdp_p, sdp_get_time_stop(remote_sdp_p));
    } else if (offer) {
        /*
         * Set t= line based on remote SDP
         */
        if (sdp_timespec_valid(remote_sdp_p) != TRUE) {
            GSM_DEBUG(DEB_L_C_F_PREFIX"\nTimespec is invalid.",
                      DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
            (void) sdp_set_time_start(local_sdp_p, "0");
            (void) sdp_set_time_stop(local_sdp_p, "0");
        } else {
            if (sdp_get_time_start(local_sdp_p) !=
                sdp_get_time_start(remote_sdp_p)) {
                (void) sdp_set_time_start(local_sdp_p,
                                          sdp_get_time_start(remote_sdp_p));
            }
            if (sdp_get_time_stop(local_sdp_p) !=
                sdp_get_time_stop(remote_sdp_p)) {
                (void) sdp_set_time_stop(local_sdp_p,
                                         sdp_get_time_stop(remote_sdp_p));
            }
        }
    }
}

/**
 *
 * The function gets the local source address address and puts it into
 * the media entry.
 *
 * @param[in]media   - pointer to fsmdef_media_t structure to
 *                     get the local address into.
 *
 * @return           none.
 * @pre              (media not_eq NULL)
 */
static void
gsmsdp_get_local_source_v4_address (fsmdef_media_t *media)
{
    int              nat_enable = 0;
    char             curr_media_ip[MAX_IPADDR_STR_LEN];
    cpr_ip_addr_t    addr;
    const char       fname[] = "gsmsdp_get_local_source_v4_address";

    /*
     * Get device address.
     */;
    config_get_value(CFGID_NAT_ENABLE, &nat_enable, sizeof(nat_enable));
    if (nat_enable == 0) {
        init_empty_str(curr_media_ip);
        config_get_value(CFGID_MEDIA_IP_ADDR, curr_media_ip,
                        MAX_IPADDR_STR_LEN);
        if (is_empty_str(curr_media_ip) == FALSE) {

        	 str2ip(curr_media_ip, &addr);
             util_ntohl(&addr, &addr);
             if (util_check_if_ip_valid(&media->src_addr) == FALSE)  {
                 // Update the media Src address only if it is invalid
                 media->src_addr = addr;
                 GSM_ERR_MSG("%s:  Update IP %s", fname, curr_media_ip);
             }
        } else {
            sip_config_get_net_device_ipaddr(&media->src_addr);
        }
    } else {
        sip_config_get_nat_ipaddr(&media->src_addr);
    }
}

/*
 *
 * The function gets the local source address address and puts it into
 * the media entry.
 *
 * @param[in]media   - pointer to fsmdef_media_t structure to
 *                     get the local address into.
 *
 * @return           none.
 * @pre              (media not_eq NULL)
 */
static void
gsmsdp_get_local_source_v6_address (fsmdef_media_t *media)
{
    int      nat_enable = 0;

    /*
     * Get device address.
     */
    config_get_value(CFGID_NAT_ENABLE, &nat_enable, sizeof(nat_enable));
    if (nat_enable == 0) {
        sip_config_get_net_ipv6_device_ipaddr(&media->src_addr);
    } else {
        sip_config_get_nat_ipaddr(&media->src_addr);
    }
}

/**
 * Set the connection address into the SDP.
 *
 * @param[in]sdp_p   - pointer to SDP (type void)
 * @param[in]level   - media level or line.
 * @param[in]addr    - string representation of IP address.
 *                     Assumed to be IPV6 if larger than 15.
 *
 * @return           none.
 * @pre              (sdp_p not_eq NULL) and (addr not_eq NULL)
 *
 */
static void
gsmsdp_set_connection_address (void *sdp_p, uint16_t level, char *addr)
{
    /*
     * c= line <network type><address type><connection address>
     */

    (void) sdp_set_conn_nettype(sdp_p, level, SDP_NT_INTERNET);

    if (addr && (strlen(addr) > strlen("123.123.123.123")))
    {
      // Long IP address, must be IPV6
      (void) sdp_set_conn_addrtype(sdp_p, level, SDP_AT_IP6);
    }
    else
    {
      (void) sdp_set_conn_addrtype(sdp_p, level, SDP_AT_IP4);
    }

    (void) sdp_set_conn_address(sdp_p, level, addr);
}

/*
 * gsmsdp_set_2543_hold_sdp
 *
 * Description:
 *
 * Manipulates the local SDP of the specified DCB to indicate hold
 * to the far end using 2543 style signaling.
 *
 * Parameters:
 *
 * dcb_p - Pointer to the DCB whose SDP is to be manipulated.
 *
 */
static void
gsmsdp_set_2543_hold_sdp (fsmdef_dcb_t *dcb_p, uint16 level)
{
    (void) sdp_set_conn_nettype(dcb_p->sdp->src_sdp, level, SDP_NT_INTERNET);
    (void) sdp_set_conn_addrtype(dcb_p->sdp->src_sdp, level, SDP_AT_IP4);
    (void) sdp_set_conn_address(dcb_p->sdp->src_sdp, level, "0.0.0.0");
}


/*
 * gsmsdp_set_video_media_attributes
 *
 * Description:
 *
 * Add the specified video media format to the SDP.
 *
 * Parameters:
 *
 * media_type - The media type (format) to add to the specified SDP.
 * sdp_p - Pointer to the SDP the media attribute is to be added to.
 * level - The media level of the SDP where the media attribute is to be added.
 * payload_number - AVT payload type if the media attribute being added is
 *                  RTP_AVT.
 *
 */
static void
gsmsdp_set_video_media_attributes (uint32_t media_type, void *cc_sdp_p, uint16_t level,
                             uint16_t payload_number)
{
    uint16_t a_inst;
    int added_fmtp = 0;
    void *sdp_p = ((cc_sdp_t*)cc_sdp_p)->src_sdp;
    int max_fs = 0;
    int max_fr = 0;

    switch (media_type) {
        case RTP_H263:
        case RTP_H264_P0:
        case RTP_H264_P1:
        case RTP_VP8:
        /*
         * add a=rtpmap line
         */
        if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_RTPMAP, &a_inst)
                != SDP_SUCCESS) {
            return;
        }

        (void) sdp_attr_set_rtpmap_payload_type(sdp_p, level, 0, a_inst,
                                                payload_number);

        switch (media_type) {
        case RTP_H263:
            (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                               SIPSDP_ATTR_ENCNAME_H263v2);
            (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
                                             RTPMAP_VIDEO_CLOCKRATE);
            break;
        case RTP_H264_P0:
        case RTP_H264_P1:
            (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                               SIPSDP_ATTR_ENCNAME_H264);
            (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
                                             RTPMAP_VIDEO_CLOCKRATE);
            // we know we haven't added it yet
            if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_FMTP, &a_inst)
                != SDP_SUCCESS) {
                GSM_ERR_MSG("Failed to add attribute");
                return;
            }
            added_fmtp = 1;
            (void) sdp_attr_set_fmtp_payload_type(sdp_p, level, 0, a_inst,
                                                  payload_number);
            {
                char buffer[32];
                uint32_t profile_level_id = vcmGetVideoH264ProfileLevelID();
                snprintf(buffer, sizeof(buffer), "0x%x", profile_level_id);
                (void) sdp_attr_set_fmtp_profile_level_id(sdp_p, level, 0, a_inst,
                                                          buffer);
            }
            if (media_type == RTP_H264_P1) {
                (void) sdp_attr_set_fmtp_pack_mode(sdp_p, level, 0, a_inst,
                                                   1);
            }
            // TODO: other parameters we may want/need to set for H.264
        //(void) sdp_attr_set_fmtp_max_mbps(sdp_p, level, 0, a_inst, max_mbps);
        //(void) sdp_attr_set_fmtp_max_fs(sdp_p, level, 0, a_inst, max_fs);
        //(void) sdp_attr_set_fmtp_max_cpb(sdp_p, level, 0, a_inst, max_cpb);
        //(void) sdp_attr_set_fmtp_max_dpb(sdp_p, level, 0, a_inst, max_dpb);
        //(void) sdp_attr_set_fmtp_max_br(sdp_p, level, 0, a_inst, max_br);
        //(void) sdp_add_new_bw_line(sdp_p, level, &a_inst);
        //(void) sdp_set_bw(sdp_p, level, a_inst, SDP_BW_MODIFIER_TIAS, tias_bw);
            break;
        case RTP_VP8:
            (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                               SIPSDP_ATTR_ENCNAME_VP8);
            (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
                                             RTPMAP_VIDEO_CLOCKRATE);
            break;
        }

        switch (media_type) {
        case RTP_H264_P0:
        case RTP_H264_P1:
        case RTP_VP8:
            max_fs = config_get_video_max_fs((rtp_ptype) media_type);
            max_fr = config_get_video_max_fr((rtp_ptype) media_type);

            if (max_fs || max_fr) {
                if (!added_fmtp) {
                    if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_FMTP, &a_inst)
                        != SDP_SUCCESS) {
                        GSM_ERR_MSG("Failed to add attribute");
                        return;
                    }
                    added_fmtp = 1;

                    (void) sdp_attr_set_fmtp_payload_type(sdp_p, level, 0, a_inst,
                                                          payload_number);
                }

                if (max_fs) {
                    (void) sdp_attr_set_fmtp_max_fs(sdp_p, level, 0, a_inst,
                                                    max_fs);
                }

                if (max_fr) {
                    (void) sdp_attr_set_fmtp_max_fr(sdp_p, level, 0, a_inst,
                                                    max_fr);
                }
            }
            break;
        }
        break;

        default:
            break;
    }
}

/*
 * gsmsdp_set_media_attributes
 *
 * Description:
 *
 * Add the specified media format to the SDP.
 *
 * Parameters:
 *
 * media_type - The media type (format) to add to the specified SDP.
 * sdp_p - Pointer to the SDP the media attribute is to be added to.
 * level - The media level of the SDP where the media attribute is to be added.
 * payload_number - AVT payload type if the media attribute being added is
 *                  RTP_AVT.
 *
 */
static void
gsmsdp_set_media_attributes (uint32_t media_type, void *sdp_p, uint16_t level,
                             uint16_t payload_number)
{
    uint16_t a_inst, a_inst2, a_inst3, a_inst4;
    int      maxavbitrate = 0;
    int      maxcodedaudiobw = 0;
    int      usedtx = 0;
    int      stereo = 0;
    int      useinbandfec = 0;
    int      cbr = 0;
    int      maxptime = 0;


    config_get_value(CFGID_MAXAVBITRATE, &maxavbitrate, sizeof(maxavbitrate));
    config_get_value(CFGID_MAXCODEDAUDIOBW, &maxcodedaudiobw, sizeof(maxcodedaudiobw));
    config_get_value(CFGID_USEDTX, &usedtx, sizeof(usedtx));
    config_get_value(CFGID_STEREO, &stereo, sizeof(stereo));
    config_get_value(CFGID_USEINBANDFEC, &useinbandfec, sizeof(useinbandfec));
    config_get_value(CFGID_CBR, &cbr, sizeof(cbr));
    config_get_value(CFGID_MAXPTIME, &maxptime, sizeof(maxptime));



    switch (media_type) {
    case RTP_PCMU:             // type 0
    case RTP_PCMA:             // type 8
    case RTP_G729:             // type 18
    case RTP_G722:             // type 9
    case RTP_ILBC:
    case RTP_L16:
    case RTP_ISAC:
    case RTP_OPUS:
        /*
         * add a=rtpmap line
         */
        if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_RTPMAP, &a_inst)
                != SDP_SUCCESS) {
            return;
        }

        (void) sdp_attr_set_rtpmap_payload_type(sdp_p, level, 0, a_inst,
                                                payload_number);

        switch (media_type) {
        case RTP_PCMU:
            (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                               SIPSDP_ATTR_ENCNAME_PCMU);
            (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
                                                 RTPMAP_CLOCKRATE);
            break;
        case RTP_PCMA:
            (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                               SIPSDP_ATTR_ENCNAME_PCMA);
            (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
                                                 RTPMAP_CLOCKRATE);
            break;
        case RTP_G729:
            {

            (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                               SIPSDP_ATTR_ENCNAME_G729);
            if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_FMTP, &a_inst2)
                    != SDP_SUCCESS) {
                return;
            }
            (void) sdp_attr_set_fmtp_payload_type(sdp_p, level, 0, a_inst2,
                                                  payload_number);
            (void) sdp_attr_set_fmtp_annexb(sdp_p, level, 0, a_inst2, FALSE);
            (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
                                                 RTPMAP_CLOCKRATE);
            }
            break;

        case RTP_G722:
            (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                               SIPSDP_ATTR_ENCNAME_G722);
            (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
                                                 RTPMAP_CLOCKRATE);
            break;

        case RTP_L16:
            (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                               SIPSDP_ATTR_ENCNAME_L16_256K);

            (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
                                                 RTPMAP_L16_CLOCKRATE);
            break;

        case RTP_ILBC:
            (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                               SIPSDP_ATTR_ENCNAME_ILBC);
            if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_FMTP, &a_inst2)
                    != SDP_SUCCESS) {
                return;
            }
            (void) sdp_attr_set_fmtp_payload_type(sdp_p, level, 0, a_inst2,
                                                  payload_number);
            (void) sdp_attr_set_fmtp_mode(sdp_p, level, 0, a_inst2, vcmGetILBCMode());

            (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
                                                RTPMAP_CLOCKRATE);
            break;

        case RTP_ISAC:
            (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                               SIPSDP_ATTR_ENCNAME_ISAC);

            (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
                                                 RTPMAP_ISAC_CLOCKRATE);
            break;

        case RTP_OPUS:
            (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                               SIPSDP_ATTR_ENCNAME_OPUS);

            (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
            		RTPMAP_OPUS_CLOCKRATE);
            (void) sdp_attr_set_rtpmap_num_chan (sdp_p, level, 0, a_inst, 2);

            /* a=fmtp options */
            if (maxavbitrate || maxcodedaudiobw || usedtx || stereo || useinbandfec || cbr) {
                if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_FMTP, &a_inst2)
                    != SDP_SUCCESS) {
                    return;
                }

                (void) sdp_attr_set_fmtp_payload_type (sdp_p, level, 0, a_inst2, payload_number);

                if (maxavbitrate)
                    sdp_attr_set_fmtp_max_average_bitrate (sdp_p, level, 0, a_inst2, FMTP_MAX_AVERAGE_BIT_RATE);

                if(usedtx)
                    sdp_attr_set_fmtp_usedtx (sdp_p, level, 0, a_inst2, FALSE);

                if(stereo)
                    sdp_attr_set_fmtp_stereo (sdp_p, level, 0, a_inst2, FALSE);

                if(useinbandfec)
                    sdp_attr_set_fmtp_useinbandfec (sdp_p, level, 0, a_inst2, FALSE);

                if(maxcodedaudiobw) {
                    sdp_attr_set_fmtp_maxcodedaudiobandwidth (sdp_p, level, 0, a_inst2,
            		     max_coded_audio_bandwidth_table[opus_fb].name);
                }

                if(cbr)
                    sdp_attr_set_fmtp_cbr (sdp_p, level, 0, a_inst2, FALSE);
            }

            /* a=ptime attribute */
            if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_PTIME, &a_inst3)
                    != SDP_SUCCESS) {
                return;
            }

            sdp_attr_set_simple_u32(sdp_p, SDP_ATTR_PTIME, level, 0, a_inst3, ATTR_PTIME);

            if(maxptime) {
                /* a=maxptime attribute */
                if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_MAXPTIME, &a_inst4)
                        != SDP_SUCCESS) {
                    return;
                }

                sdp_attr_set_simple_u32(sdp_p, SDP_ATTR_MAXPTIME, level, 0, a_inst4, ATTR_MAXPTIME);
            }

            break;
        }
        break;

    case RTP_AVT:
        /*
         * add a=rtpmap line
         */
        if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_RTPMAP, &a_inst)
                != SDP_SUCCESS) {
            return;
        }
        (void) sdp_attr_set_rtpmap_encname(sdp_p, level, 0, a_inst,
                                           SIPSDP_ATTR_ENCNAME_TEL_EVENT);
        (void) sdp_attr_set_rtpmap_payload_type(sdp_p, level, 0, a_inst,
                                                payload_number);
        (void) sdp_attr_set_rtpmap_clockrate(sdp_p, level, 0, a_inst,
                                             RTPMAP_CLOCKRATE);

        /*
         * Malloc the mediainfo structure
         */
        if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_FMTP, &a_inst)
                != SDP_SUCCESS) {
            return;
        }
        (void) sdp_attr_set_fmtp_payload_type(sdp_p, level, 0, a_inst,
                                              payload_number);
        (void) sdp_attr_set_fmtp_range(sdp_p, level, 0, a_inst,
                                       SIPSDP_NTE_DTMF_MIN,
                                       SIPSDP_NTE_DTMF_MAX);


        break;

    default:
        /* The remaining coded types aren't supported, but are listed below
         * as a reminder
         *   RTP_CELP         = 1,
         *   RTP_GSM          = 3,
         *   RTP_G726         = 2,
         *   RTP_G723         = 4,
         *   RTP_DVI4         = 5,
         *   RTP_DVI4_II      = 6,
         *   RTP_LPC          = 7,
         *   RTP_G722         = 9,
         *   RTP_G728         = 15,
         *   RTP_JPEG         = 26,
         *   RTP_NV           = 28,
         *   RTP_H261         = 31
         */

        break;
    }
}

/*
 * gsmsdp_set_sctp_attributes
 *
 * Description:
 *
 * Add the specified SCTP media format to the SDP.
 *
 * Parameters:
 *
 * sdp_p - Pointer to the SDP the media attribute is to be added to.
 * level - The media level of the SDP where the media attribute is to be added.
 */
static void
gsmsdp_set_sctp_attributes (void *sdp_p, uint16_t level, fsmdef_media_t *media)
{
    uint16_t a_inst;

    if (sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_SCTPMAP, &a_inst)
        != SDP_SUCCESS) {
            return;
    }

    sdp_attr_set_sctpmap_port(sdp_p, level, 0, a_inst,
        media->local_datachannel_port);

    sdp_attr_set_sctpmap_protocol (sdp_p, level, 0, a_inst,
        media->datachannel_protocol);

    sdp_attr_set_sctpmap_streams (sdp_p, level, 0, a_inst,
        media->datachannel_streams);
}

/*
 * gsmsdp_set_remote_sdp
 *
 * Description:
 *
 * Sets the specified SDP as the remote SDP in the DCB.
 *
 * Parameters:
 *
 * dcb_p - Pointer to the DCB.
 * sdp_p - Pointer to the SDP to be set as the remote SDP in the DCB.
 */
static void
gsmsdp_set_remote_sdp (fsmdef_dcb_t *dcb_p, cc_sdp_t *sdp_p)
{
    dcb_p->remote_sdp_present = TRUE;
}

/*
 * gsmsdp_get_sdp_direction_attr
 *
 * Description:
 *
 * Given a sdp_direction_e enumerated type, returns a sdp_attr_e
 * enumerated type.
 *
 * Parameters:
 *
 * direction - The SDP direction used to determine which sdp_attr_e to return.
 *
 */
static sdp_attr_e
gsmsdp_get_sdp_direction_attr (sdp_direction_e direction)
{
    sdp_attr_e sdp_attr = SDP_ATTR_SENDRECV;

    switch (direction) {
    case SDP_DIRECTION_INACTIVE:
        sdp_attr = SDP_ATTR_INACTIVE;
        break;
    case SDP_DIRECTION_SENDONLY:
        sdp_attr = SDP_ATTR_SENDONLY;
        break;
    case SDP_DIRECTION_RECVONLY:
        sdp_attr = SDP_ATTR_RECVONLY;
        break;
    case SDP_DIRECTION_SENDRECV:
        sdp_attr = SDP_ATTR_SENDRECV;
        break;
    default:
        GSM_ERR_MSG("\nFSMDEF ERROR: replace with formal error text");
    }

    return sdp_attr;
}

/*
 * gsmsdp_set_sdp_direction
 *
 * Description:
 *
 * Adds a direction attribute to the given media line in the
 * specified SDP.
 *
 * Parameters:
 *
 * media     - pointer to the fsmdef_media_t for the media entry.
 * direction - The direction to use when setting the direction attribute.
 * sdp_p - Pointer to the SDP to set the direction attribute against.
 */
static void
gsmsdp_set_sdp_direction (fsmdef_media_t *media,
                          sdp_direction_e direction, void *sdp_p)
{
    sdp_attr_e    sdp_attr = SDP_ATTR_SENDRECV;
    uint16_t      a_instance = 0;

    /*
     * Convert the direction to an SDP direction attribute.
     */
    sdp_attr = gsmsdp_get_sdp_direction_attr(direction);
    if (media->level) {
       (void) sdp_add_new_attr(sdp_p, media->level, 0, sdp_attr, &a_instance);
    } else {
       /* Just in case that there is no level defined, add to the session */
       (void) sdp_add_new_attr(sdp_p, SDP_SESSION_LEVEL, 0, sdp_attr,
                               &a_instance);
    }
}

/*
 * gsmsdp_get_ice_attributes
 *
 * Description:
 *
 * Returns the ice attribute strings at a given level
 *
 * Parameters:
 *
 * session         - true = session level attributes, false = media line attribs
 * level           - The media level of the SDP where the media attribute exists.
 * sdp_p           - Pointer to the SDP whose ice candidates are being searched.
 * ice_attribs     - return ice attribs at this level in an array
 * attributes_ctp  - count of array of media line attributes
 */

static cc_causes_t
gsmsdp_get_ice_attributes (sdp_attr_e sdp_attr, uint16_t level, void *sdp_p, char ***ice_attribs, int *attributes_ctp)
{
    uint16_t        num_a_lines = 0;
    uint16_t        i;
    sdp_result_e    result;
    char*           ice_attrib;

    result = sdp_attr_num_instances(sdp_p, level, 0, sdp_attr, &num_a_lines);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("enumerating ICE attributes failed");
        return result;
    }

    if (num_a_lines < 1) {
        GSM_DEBUG("enumerating ICE attributes returned 0 attributes");
        return CC_CAUSE_OK;
    }

    *ice_attribs = (char **)cpr_malloc(num_a_lines * sizeof(char *));

    if (!(*ice_attribs))
      return CC_CAUSE_OUT_OF_MEM;

    *attributes_ctp = 0;

    for (i = 0; i < num_a_lines; i++) {
        result = sdp_attr_get_ice_attribute (sdp_p, level, 0, sdp_attr, (uint16_t) (i + 1),
          &ice_attrib);
        if (result != SDP_SUCCESS) {
            GSM_ERR_MSG("Failed to retrieve ICE attribute");
            cpr_free(*ice_attribs);
            return result == SDP_INVALID_SDP_PTR ?
                             CC_CAUSE_INVALID_SDP_POINTER :
                   result == SDP_INVALID_PARAMETER ?
                             CC_CAUSE_BAD_ICE_ATTRIBUTE :
                   /* otherwise */
                             CC_CAUSE_ERROR;
        }
        (*ice_attribs)[i] = (char *) cpr_calloc(1, strlen(ice_attrib) + 1);
        if(!(*ice_attribs)[i])
            return CC_CAUSE_OUT_OF_MEM;

        sstrncpy((*ice_attribs)[i], ice_attrib, strlen(ice_attrib) + 1);
        (*attributes_ctp)++;
    }

    return CC_CAUSE_OK;
}

/*
 * gsmsdp_set_ice_attribute
 *
 * Description:
 *
 * Adds an ice attribute attributes to the specified SDP.
 *
 * Parameters:
 *
 * session      - true = session level attribute, false = media line attribute
 * level        - The media level of the SDP where the media attribute exists.
 * sdp_p        - Pointer to the SDP to set the ice candidate attribute against.
 * ice_attrib   - ice attribute to set
 */
void
gsmsdp_set_ice_attribute (sdp_attr_e sdp_attr, uint16_t level, void *sdp_p, char *ice_attrib)
{
    uint16_t      a_instance = 0;
    sdp_result_e  result;

    result = sdp_add_new_attr(sdp_p, level, 0, sdp_attr, &a_instance);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to add attribute");
        return;
    }

    result = sdp_attr_set_ice_attribute(sdp_p, level, 0, sdp_attr, a_instance, ice_attrib);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to set attribute");
    }
}

/*
 * gsmsdp_set_rtcp_fb_ack_attribute
 *
 * Description:
 *
 * Adds an rtcp-fb:...ack attribute attributes to the specified SDP.
 *
 * Parameters:
 *
 * level        - The media level of the SDP where the media attribute exists.
 * sdp_p        - Pointer to the SDP to set the ice candidate attribute against.
 * ack_type     - Type of ack feedback mechanism in use
 */
void
gsmsdp_set_rtcp_fb_ack_attribute (uint16_t level,
                                  void *sdp_p,
                                  u16 payload_type,
                                  sdp_rtcp_fb_ack_type_e ack_type)
{
    uint16_t      a_instance = 0;
    sdp_result_e  result;

    result = sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_RTCP_FB, &a_instance);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to add attribute");
        return;
    }
    result = sdp_attr_set_rtcp_fb_ack(sdp_p, level, payload_type,
                                      a_instance, ack_type);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to set attribute");
    }
}

/*
 * gsmsdp_set_audio_level_attribute
 *
 * Description:
 *
 * Adds an audio level extension attributesto the specified SDP.
 *
 * Parameters:
 *
 * level        - The media level of the SDP where the media attribute exists.
 * sdp_p        - Pointer to the SDP to set the attribute against.
 */
void
gsmsdp_set_extmap_attribute (uint16_t level,
                             void *sdp_p,
                             u16 id,
                             const char* uri)
{
    uint16_t      a_instance = 0;
    sdp_result_e  result;

    result = sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_EXTMAP, &a_instance);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to add attribute");
        return;
    }

    result = sdp_attr_set_extmap(sdp_p, level, id, uri, a_instance);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to set attribute");
    }
}

/*
 * gsmsdp_set_rtcp_fb_nack_attribute
 *
 * Description:
 *
 * Adds an rtcp-fb:...nack attribute attributes to the specified SDP.
 *
 * Parameters:
 *
 * level        - The media level of the SDP where the media attribute exists.
 * sdp_p        - Pointer to the SDP to set the ice candidate attribute against.
 * nack_type    - Type of nack feedback mechanism in use
 */
void
gsmsdp_set_rtcp_fb_nack_attribute (uint16_t level,
                                   void *sdp_p,
                                   u16 payload_type,
                                   sdp_rtcp_fb_nack_type_e nack_type)
{
    uint16_t      a_instance = 0;
    sdp_result_e  result;

    result = sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_RTCP_FB, &a_instance);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to add attribute");
        return;
    }

    result = sdp_attr_set_rtcp_fb_nack(sdp_p, level, payload_type,
                                       a_instance, nack_type);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to set attribute");
    }
}

/*
 * gsmsdp_set_rtcp_fb_trr_int_attribute
 *
 * Description:
 *
 * Adds an rtcp-fb:...trr-int attribute attributes to the specified SDP.
 *
 * Parameters:
 *
 * level        - The media level of the SDP where the media attribute exists.
 * sdp_p        - Pointer to the SDP to set the ice candidate attribute against.
 * trr_interval - Interval to set trr-int value to
 */
void
gsmsdp_set_rtcp_fb_trr_int_attribute (uint16_t level,
                                      void *sdp_p,
                                      u16 payload_type,
                                      u32 trr_interval)
{
    uint16_t      a_instance = 0;
    sdp_result_e  result;

    result = sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_RTCP_FB, &a_instance);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to add attribute");
        return;
    }

    result = sdp_attr_set_rtcp_fb_trr_int(sdp_p, level, payload_type,
                                          a_instance, trr_interval);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to set attribute");
    }
}

/*
 * gsmsdp_set_rtcp_fb_ccm_attribute
 *
 * Description:
 *
 * Adds an rtcp-fb:...ccm attribute attributes to the specified SDP.
 *
 * Parameters:
 *
 * level        - The media level of the SDP where the media attribute exists.
 * sdp_p        - Pointer to the SDP to set the ice candidate attribute against.
 * ccm_type     - Type of ccm feedback mechanism in use
 */
void
gsmsdp_set_rtcp_fb_ccm_attribute (uint16_t level,
                                  void *sdp_p,
                                  u16 payload_type,
                                  sdp_rtcp_fb_ccm_type_e ccm_type)
{
    uint16_t      a_instance = 0;
    sdp_result_e  result;

    result = sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_RTCP_FB, &a_instance);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to add attribute");
        return;
    }

    result = sdp_attr_set_rtcp_fb_ccm(sdp_p, level, payload_type,
                                      a_instance, ccm_type);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to set attribute");
    }
}

/*
 * gsmsdp_set_rtcp_mux_attribute
 *
 * Description:
 *
 * Adds an ice attribute attributes to the specified SDP.
 *
 * Parameters:
 *
 * session      - true = session level attribute, false = media line attribute
 * level        - The media level of the SDP where the media attribute exists.
 * sdp_p        - Pointer to the SDP to set the ice candidate attribute against.
 * rtcp_mux     - ice attribute to set
 */
static void
gsmsdp_set_rtcp_mux_attribute (sdp_attr_e sdp_attr, uint16_t level, void *sdp_p, boolean rtcp_mux)
{
    uint16_t      a_instance = 0;
    sdp_result_e  result;

    result = sdp_add_new_attr(sdp_p, level, 0, sdp_attr, &a_instance);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to add attribute");
        return;
    }

    result = sdp_attr_set_rtcp_mux_attribute(sdp_p, level, 0, sdp_attr, a_instance, rtcp_mux);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to set attribute");
    }
}

/*
 * gsmsdp_set_setup_attribute
 *
 * Description:
 *
 * Adds a setup attribute to the specified SDP.
 *
 * Parameters:
 *
 * level        - The media level of the SDP where the media attribute exists.
 * sdp_p        - Pointer to the SDP to set the ice candidate attribute against.
 * setup_type   - Value for the a=setup line
 */
static void
gsmsdp_set_setup_attribute(uint16_t level,
  void *sdp_p, sdp_setup_type_e setup_type) {
    uint16_t a_instance = 0;
    sdp_result_e result;

    result = sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_SETUP, &a_instance);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to add attribute");
        return;
    }

    result = sdp_attr_set_setup_attribute(sdp_p, level, 0,
      a_instance, setup_type);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to set attribute");
    }
}

/*
 * gsmsdp_set_dtls_fingerprint_attribute
 *
 * Description:
 *
 * Adds an dtls fingerprint attribute attributes to the specified SDP.
 *
 * Parameters:
 *
 * sdp_attr     - The attribute to set
 * level        - The media level of the SDP where the media attribute exists.
 * sdp_p        - Pointer to the SDP to set the attribute against.
 * hash_func    - hash function string, e.g. "sha-256"
 * fingerprint     - fingerprint attribute to set
 */
static void
gsmsdp_set_dtls_fingerprint_attribute (sdp_attr_e sdp_attr, uint16_t level, void *sdp_p,
  char *hash_func, char *fingerprint)
{
    uint16_t      a_instance = 0;
    sdp_result_e  result;
    char hash_and_fingerprint[FSMDEF_MAX_DIGEST_ALG_LEN + FSMDEF_MAX_DIGEST_LEN + 2];

    snprintf(hash_and_fingerprint, sizeof(hash_and_fingerprint),
         "%s %s", hash_func, fingerprint);

    result = sdp_add_new_attr(sdp_p, level, 0, sdp_attr, &a_instance);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to add attribute");
        return;
    }

    result = sdp_attr_set_dtls_fingerprint_attribute(sdp_p, level, 0, sdp_attr,
        a_instance, hash_and_fingerprint);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to set dtls fingerprint attribute");
    }
}

/*
 * gsmsdp_set_identity_attribute
 *
 * Description:
 *
 * Adds an identity attribute to the specified SDP.
 *
 * Parameters:
 *
 * level        - The media level of the SDP where the media attribute exists.
 * sdp_p        - Pointer to the SDP to set the ice candidate attribute against.
 * identity     - attribute value to set
 * identity_len - string len of fingerprint
 */
static void
gsmsdp_set_identity_attribute (uint16_t level, void *sdp_p,
  char *identity)
{
    uint16_t      a_instance = 0;
    sdp_result_e  result;

    result = sdp_add_new_attr(sdp_p, level, 0, SDP_ATTR_IDENTITY, &a_instance);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to add attribute");
        return;
    }

    result = sdp_attr_set_simple_string(sdp_p, level, 0, SDP_ATTR_IDENTITY,
        a_instance, identity);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG("Failed to set identity attribute");
    }
}

/*
 * gsmsdp_remove_sdp_direction
 *
 * Description:
 *
 * Removes the direction attribute corresponding to the passed in direction
 * from the media line of the specified SDP.
 *
 * Parameters:
 *
 * media     - pointer to the fsmdef_media_t for the media entry.
 * direction - The direction whose corresponding direction attribute
 *             is to be removed.
 * sdp_p - Pointer to the SDP where the direction attribute is to be
 *         removed.
 */
static void
gsmsdp_remove_sdp_direction (fsmdef_media_t *media,
                             sdp_direction_e direction, void *sdp_p)
{
    sdp_attr_e    sdp_attr = SDP_ATTR_SENDRECV;

    sdp_attr = gsmsdp_get_sdp_direction_attr(direction);
    (void) sdp_delete_attr(sdp_p, media->level, 0, sdp_attr, 1);
}

/*
 * gsmsdp_set_local_sdp_direction
 *
 * Description:
 *
 * Sets the direction attribute for the local SDP.
 *
 * Parameters:
 *
 * dcb_p - The DCB where the local SDP is located.
 * media - Pointer to fsmdef_media_t for the media entry of the SDP.
 * direction - The media direction to set into the local SDP.
 */
void
gsmsdp_set_local_sdp_direction (fsmdef_dcb_t *dcb_p,
                                fsmdef_media_t *media,
                                sdp_direction_e direction)
{
    /*
     * If media direction was previously set, remove the direction attribute
     * before adding the specified direction. Save the direction in previous
     * direction before clearing it.
     */
    if (media->type != SDP_MEDIA_APPLICATION) {
      if (media->direction_set) {
          media->previous_sdp.direction = media->direction;
          gsmsdp_remove_sdp_direction(media, media->direction,
                                      dcb_p->sdp ? dcb_p->sdp->src_sdp : NULL );
          media->direction_set = FALSE;
      }
      gsmsdp_set_sdp_direction(media, direction, dcb_p->sdp ? dcb_p->sdp->src_sdp : NULL);
    }

    /*
     * We could just get the direction from the local SDP when we need it in
     * GSM, but setting the direction in the media structure gives a quick way
     * to access the media direction.
     */
    media->direction = direction;
    media->direction_set = TRUE;
}

/*
 * gsmsdp_get_remote_sdp_direction
 *
 * Description:
 *
 * Returns the media direction from the specified SDP. We will check for the
 * media direction attribute at the session level and the first AUDIO media line.
 * If the direction attribute is specified at the media level, the media level setting
 * overrides the session level attribute.
 *
 * Parameters:
 *
 * dcb_p - pointer to the fsmdef_dcb_t.
 * level - media line level.
 * dest_addr - pointer to the remote address.
 */
static sdp_direction_e
gsmsdp_get_remote_sdp_direction (fsmdef_dcb_t *dcb_p, uint16_t level,
                                 cpr_ip_addr_t *dest_addr)
{
    sdp_direction_e direction = SDP_DIRECTION_SENDRECV;
    cc_sdp_t       *sdp_p = dcb_p->sdp;
    uint16_t       media_attr;
    uint16_t       i;
    uint32         port;
    int            sdpmode = 0;
    static const sdp_attr_e  dir_attr_array[] = {
        SDP_ATTR_INACTIVE,
        SDP_ATTR_RECVONLY,
        SDP_ATTR_SENDONLY,
        SDP_ATTR_SENDRECV,
        SDP_MAX_ATTR_TYPES
    };

    if (!sdp_p->dest_sdp) {
        return direction;
    }

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    media_attr = 0; /* media level attr. count */
    /*
     * Now check for direction as a media attribute. If found, the
     * media attribute takes precedence over direction specified
     * as a session attribute.
     *
     * In order to find out whether there is a direction attribute
     * associated with a media line (or even at the
     * session level) or not is to get the number of instances of
     * that attribute via the sdp_attr_num_instances() first. The is
     * because the sdp_get_media_direction() always returns a valid
     * direction value even when there is no direction attribute with
     * the media line (or session level).
     *
     * Note: there is no single attribute value to pass to the
     * sdp_attr_num_instances() to get number a direction attribute that
     * represents inactive, recvonly, sendonly, sendrcv. We have to
     * look for each one of them individually.
     */
    for (i = 0; (dir_attr_array[i] != SDP_MAX_ATTR_TYPES); i++) {
        if (sdp_attr_num_instances(sdp_p->dest_sdp, level, 0,
                                   dir_attr_array[i], &media_attr) ==
            SDP_SUCCESS) {
            if (media_attr) {
                /* There is direction attribute in the media line */
                direction = sdp_get_media_direction(sdp_p->dest_sdp,
                                                    level, 0);
                break;
            }
        }
    }

    /*
     * Check for the direction attribute. The direction can be specified
     * as a session attribute or a media stream attribute. If the direction
     * is specified as a session attribute, the direction is applicable to
     * all media streams in the SDP.
     */
    if (media_attr == 0) {
        /* no media level direction, get the direction from session */
        direction = sdp_get_media_direction(sdp_p->dest_sdp,
                                            SDP_SESSION_LEVEL, 0);
    }

    /*
     * To support legacy way of signaling remote hold, we will interpret
     * c=0.0.0.0 to be a=inactive
     */
    if (dest_addr->type == CPR_IP_ADDR_IPV4 &&
        dest_addr->u.ip4 == 0) {

        /*
         * For WebRTC, we allow active media sections with IP=0.0.0.0, iff
         * port != 0. This is to allow interop with existing Trickle ICE
         * implementations. TODO: This may need to be updated to match the
         * spec once the Trickle ICE spec is finalized.
         */
        port = sdp_get_media_portnum(sdp_p->dest_sdp, level);
        if (sdpmode && port != 0) {
            return direction;
        }

        direction = SDP_DIRECTION_INACTIVE;
    } else {

        //todo IPv6: reject the request.
    }
    return direction;
}

/**
 *
 * The function overrides direction for some special feature
 * processing.
 *
 * @param[in]dcb_p  - pointer to the fsmdef_dcb_t
 * @param[in]media  - pointer to fsmdef_media_t for the media to
 *                    override the direction.
 *
 * @return           None.
 * @pre              (dcb_p not_eq NULL) and (media not_eq NULL)
 */
static void
gsmsdp_feature_overide_direction (fsmdef_dcb_t *dcb_p, fsmdef_media_t *media)
{
    /*
     * Disable video if this is a BARGE with video
     */
    if ( CC_IS_VIDEO(media->cap_index) &&
                   dcb_p->join_call_id != CC_NO_CALL_ID ){
        media->support_direction = SDP_DIRECTION_INACTIVE;
    }

    if (CC_IS_VIDEO(media->cap_index) && media->support_direction == SDP_DIRECTION_INACTIVE) {
        DEF_DEBUG(GSM_F_PREFIX"video capability disabled to SDP_DIRECTION_INACTIVE", "gsmsdp_feature_overide_direction");
    }
}

/*
 * gsmsdp_negotiate_local_sdp_direction
 *
 * Description:
 *
 * Given an offer SDP, return the corresponding answer SDP direction.
 *
 * local hold   remote direction support direction new local direction
 * enabled      inactive            any                 inactive
 * enabled      sendrecv          sendonly              sendonly
 * enabled      sendrecv          recvonly              inactive
 * enabled      sendrecv          sendrecv              sendonly
 * enabled      sendrecv          inactive              inactive
 * enabled      sendonly            any                 inactive
 * enabled      recvonly          sendrecv              sendonly
 * enabled      recvonly          sendonly              sendonly
 * enabled      recvonly          recvonly              inactive
 * enabled      recvonly          inactive              inactive
 * disabled     inactive            any                 inactive
 * disabled     sendrecv          sendrecv              sendrecv
 * disabled     sendrecv          sendonly              sendonly
 * disabled     sendrecv          recvonly              recvonly
 * disabled     sendrecv          inactive              inactive
 * disabled     sendonly          sendrecv              recvonly
 * disabled     sendonly          sendonly              inactive
 * disabled     sendonly          recvonly              recvonly
 * disabled     sendonly          inactive              inactive
 * disabled     recvonly          sendrecv              sendonly
 * disabled     recvonly          sendonly              sendonly
 * disabled     recvonly          recvonly              inactive
 * disabled     recvonly          inactive              inactive
 *
 * Parameters:
 *
 * dcb_p - pointer to the fsmdef_dcb_t.
 * media - pointer to the fsmdef_media_t for the current media entry.
 * local_hold - Boolean indicating if local hold feature is enabled
 */
static sdp_direction_e
gsmsdp_negotiate_local_sdp_direction (fsmdef_dcb_t *dcb_p,
                                      fsmdef_media_t *media,
                                      boolean local_hold)
{
    sdp_direction_e direction = SDP_DIRECTION_SENDRECV;
    sdp_direction_e remote_direction = gsmsdp_get_remote_sdp_direction(dcb_p,
                                           media->level, &media->dest_addr);

    if (remote_direction == SDP_DIRECTION_SENDRECV) {
        if (local_hold) {
            if ((media->support_direction == SDP_DIRECTION_SENDRECV) ||
                (media->support_direction == SDP_DIRECTION_SENDONLY)) {
                direction = SDP_DIRECTION_SENDONLY;
            } else {
                direction = SDP_DIRECTION_INACTIVE;
            }
        } else {
            direction = media->support_direction;
        }
    } else if (remote_direction == SDP_DIRECTION_SENDONLY) {
        if (local_hold) {
            direction = SDP_DIRECTION_INACTIVE;
        } else {
            if ((media->support_direction == SDP_DIRECTION_SENDRECV) ||
                (media->support_direction == SDP_DIRECTION_RECVONLY)) {
                direction = SDP_DIRECTION_RECVONLY;
            } else {
                direction = SDP_DIRECTION_INACTIVE;
            }
        }
    } else if (remote_direction == SDP_DIRECTION_INACTIVE) {
        direction = SDP_DIRECTION_INACTIVE;
    } else if (remote_direction == SDP_DIRECTION_RECVONLY) {
        if ((media->support_direction == SDP_DIRECTION_SENDRECV) ||
            (media->support_direction == SDP_DIRECTION_SENDONLY)) {
            direction = SDP_DIRECTION_SENDONLY;
        } else {
            direction = SDP_DIRECTION_INACTIVE;
        }
    }

    return direction;
}

/*
 * gsmsdp_add_default_audio_formats_to_local_sdp
 *
 * Description:
 *
 * Add all supported media formats to the local SDP of the specified DCB
 * at the specified media level. If the call is involved in a conference
 * call, only add G.711 formats.
 *
 * Parameters
 *
 * dcb_p - The DCB whose local SDP is to be updated with the default media formats.
 * sdp_p - Pointer to the local sdp structure. This is added so call to this
 *         routine can be made irrespective of whether we have a dcb or not(To
 *         handle out-of-call options request for example)
 * media - Pointer to fsmdef_media_t for the media entry of the SDP.
 *
 */
static void
gsmsdp_add_default_audio_formats_to_local_sdp (fsmdef_dcb_t *dcb_p,
                                               cc_sdp_t * sdp_p,
                                               fsmdef_media_t *media)
{
    static const char fname[] = "gsmsdp_add_default_audio_formats_to_local_sdp";
    int             local_media_types[CC_MAX_MEDIA_TYPES];
    int16_t         local_avt_payload_type = RTP_NONE;
    DtmfOutOfBandTransport_t transport = DTMF_OUTOFBAND_NONE;
    int             type_cnt;
    void           *local_sdp_p = NULL;
    uint16_t        media_format_count;
    uint16_t        level;
    int i;

    if (media) {
        level = media->level;
    } else {
        level = 1;
    }
    local_sdp_p = (void *) sdp_p->src_sdp;

    /*
     * Create list of supported codecs. Get all the codecs that the phone
     * supports.
     */
    media_format_count = sip_config_local_supported_codecs_get(
                                (rtp_ptype *) local_media_types,
                                CC_MAX_MEDIA_TYPES);
    /*
     * If there are no media payloads, it's because we are making an
     * initial offer. We will be opening our receive port so we need to specify
     * the media payload type to be used initially. We set the media payload
     * type in the dcb to do this. Until we receive an answer from the far
     * end, we will use our first choice payload type. i.e. the first payload
     * type sent in our AUDIO media line.
     */

    if (dcb_p && media && media->num_payloads == 0) {

        if (media->payloads &&
            (media->num_payloads < media_format_count)) {
            cpr_free(media->payloads);
            media->payloads = NULL;
        }

        if (!media->payloads) {
            media->payloads = cpr_calloc(media_format_count,
                                         sizeof(vcm_payload_info_t));
        }

        media->num_payloads = 0;
        for (i = 0; i < media_format_count; i++) {
            if (local_media_types[i] > RTP_NONE) {
                media->payloads[i].codec_type = local_media_types[i];
                media->payloads[i].local_rtp_pt = local_media_types[i];
                media->payloads[i].remote_rtp_pt = local_media_types[i];
                media->num_payloads++;
            }
        }
        gsmsdp_copy_payloads_to_previous_sdp(media);
    }

    /*
     * Get configured OOB DTMF setting and avt payload type if applicable
     */
    config_get_value(CFGID_DTMF_OUTOFBAND, &transport, sizeof(transport));

    if ((transport == DTMF_OUTOFBAND_AVT) ||
        (transport == DTMF_OUTOFBAND_AVT_ALWAYS)) {
        int temp_payload_type = RTP_NONE;

        config_get_value(CFGID_DTMF_AVT_PAYLOAD,
                         &(temp_payload_type),
                         sizeof(temp_payload_type));
        local_avt_payload_type = (uint16_t) temp_payload_type;
    }

    /*
     * add all the audio media types
     */
    for (type_cnt = 0;
         (type_cnt < media_format_count) &&
         (local_media_types[type_cnt] > RTP_NONE);
         type_cnt++) {

        if (sdp_add_media_payload_type(local_sdp_p, level,
                                       (uint16_t)local_media_types[type_cnt],
                                       SDP_PAYLOAD_NUMERIC) != SDP_SUCCESS) {
            GSM_ERR_MSG(DEB_L_C_F_PREFIX"Adding media payload type failed",
                        DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));

        }

        if (media->support_direction != SDP_DIRECTION_INACTIVE) {
            gsmsdp_set_media_attributes(local_media_types[type_cnt], local_sdp_p,
                                    level, (uint16_t)local_media_types[type_cnt]);
        }
    }

    /*
     * add the avt media type
     */
    if (local_avt_payload_type > RTP_NONE) {
        if (sdp_add_media_payload_type(local_sdp_p, level,
                                       local_avt_payload_type,
                                       SDP_PAYLOAD_NUMERIC) != SDP_SUCCESS) {
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"Adding AVT payload type failed",
                        dcb_p->line, dcb_p->call_id, fname);
        }

        if (media->support_direction != SDP_DIRECTION_INACTIVE) {
            gsmsdp_set_media_attributes(RTP_AVT, local_sdp_p, level,
                                        local_avt_payload_type);
            if (media) {
                media->avt_payload_type = local_avt_payload_type;
            }
        }
    }
}

/*
 * gsmsdp_add_default_video_formats_to_local_sdp
 *
 * Description:
 *
 * Add all supported media formats to the local SDP of the specified DCB
 * at the specified media level. If the call is involved in a conference
 * call, only add G.711 formats.
 *
 * Parameters
 *
 * dcb_p - The DCB whose local SDP is to be updated with the default media formats.
 * sdp_p - Pointer to the local sdp structure. This is added so call to this
 *         routine can be made irrespective of whether we have a dcb or not(To
 *         handle out-of-call options request for example)
 * media - Pointer to fsmdef_media_t for the media entry of the SDP.
 *
 */
static void
gsmsdp_add_default_video_formats_to_local_sdp (fsmdef_dcb_t *dcb_p,
                                               cc_sdp_t * sdp_p,
                                               fsmdef_media_t *media)
{
    static const char fname[] = "gsmsdp_add_default_video_formats_to_local_sdp";
    int             video_media_types[CC_MAX_MEDIA_TYPES];
    int             type_cnt;
    void           *local_sdp_p = NULL;
    uint16_t        video_format_count;
    uint16_t        level;
    line_t          line = 0;
    callid_t        call_id = 0;
    int             i;

    if (dcb_p && media) {
        line = dcb_p->line;
        call_id = dcb_p->call_id;
    }
    GSM_DEBUG(DEB_L_C_F_PREFIX"", DEB_L_C_F_PREFIX_ARGS(GSM, line, call_id, fname));

    if (media) {
        level = media->level;
    } else {
        level = 2;
    }
    local_sdp_p = (void *) sdp_p->src_sdp;

    /*
     * Create list of supported codecs. Get all the codecs that the phone supports.
     */

    video_format_count = sip_config_video_supported_codecs_get( (rtp_ptype *) video_media_types,
                                                 CC_MAX_MEDIA_TYPES, TRUE /*offer*/);

    GSM_DEBUG(DEB_L_C_F_PREFIX"video_count=%d", DEB_L_C_F_PREFIX_ARGS(GSM, line, call_id, fname), video_format_count);
    /*
     * If the there are no media payloads, its because we are making an
     * initial offer. We will be opening our receive port so we need to specify
     * the media payload type to be used initially. We set the media payload
     * type in the dcb to do this. Until we receive an answer from the far
     * end, we will use our first choice payload type. i.e. the first payload
     * type sent in our video media line.
     */
    if (dcb_p && media && media->num_payloads == 0) {
        if (media->payloads &&
            (media->num_payloads < video_format_count)) {
            cpr_free(media->payloads);
            media->payloads = NULL;
        }

        if (!media->payloads) {
            media->payloads = cpr_calloc(video_format_count,
                                         sizeof(vcm_payload_info_t));
        }

        media->num_payloads = 0;
        for (i = 0; i < video_format_count; i++) {
            if (video_media_types[i] > RTP_NONE) {
                media->payloads[i].codec_type = video_media_types[i];
                media->payloads[i].local_rtp_pt = video_media_types[i];
                media->payloads[i].remote_rtp_pt = video_media_types[i];
                media->num_payloads++;
            }
        }
        gsmsdp_copy_payloads_to_previous_sdp(media);
    }


    /*
     * add all the video media types
     */
    for (type_cnt = 0;
         (type_cnt < video_format_count) &&
         (video_media_types[type_cnt] > RTP_NONE);
         type_cnt++) {

        if (sdp_add_media_payload_type(local_sdp_p, level,
                                       (uint16_t)video_media_types[type_cnt],
                                       SDP_PAYLOAD_NUMERIC) != SDP_SUCCESS) {
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"SDP ERROR(1)",
                        line, call_id, fname);
        }

        if (media->support_direction != SDP_DIRECTION_INACTIVE) {
            gsmsdp_set_video_media_attributes(video_media_types[type_cnt], sdp_p,
                               level, (uint16_t)video_media_types[type_cnt]);
        }
    }
}

/**
 * This function sets the mid attr at the media level
 * and labels the relevant media streams when phone is
 * operating in a dual stack mode
 *
 * @param[in]src_sdp_p  - Our source sdp.
 * @param[in]level      - The level of the media that we are operating on.
 *
 * @return           - None
 *
 */
static void gsmsdp_set_mid_attr (void *src_sdp_p, uint16_t level)
{
    uint16         inst_num;

    if (platform_get_ip_address_mode() == CPR_IP_MODE_DUAL) {
        /*
         * add a=mid line
         */
        (void) sdp_add_new_attr(src_sdp_p, level, 0, SDP_ATTR_MID, &inst_num);

        (void) sdp_attr_set_simple_u32(src_sdp_p, SDP_ATTR_MID, level, 0,
                                       inst_num, level);
    }
}

/**
 * This function sets the anat attr to the session level
 * and labels the relevant media streams
 *
 * @param[in]media   - The media line that we are operating on.
 * @param[in]dcb_p   - Pointer to the DCB whose local SDP is to be updated.
 *
 * @return           - None
 *
 */
static void gsmsdp_set_anat_attr (fsmdef_dcb_t *dcb_p, fsmdef_media_t *media)
{
    void           *src_sdp_p = (void *) dcb_p->sdp->src_sdp;
    void           *dest_sdp_p = (void *) dcb_p->sdp->dest_sdp;
    uint16         inst_num;
    uint16_t       num_group_lines= 0;
    uint16_t       num_anat_lines = 0;
    u32            group_id_1, group_id_2;
    uint16_t       i;
    fsmdef_media_t *group_media;


    if (dest_sdp_p == NULL) {
        /* If this is our initial offer */
        if (media->addr_type == SDP_AT_IP4) {
            group_media = gsmsdp_find_anat_pair(dcb_p, media);
            if (group_media != NULL) {
                /*
                 * add a=group line
                 */
                 (void) sdp_add_new_attr(src_sdp_p, SDP_SESSION_LEVEL, 0, SDP_ATTR_GROUP, &inst_num);

                 (void) sdp_set_group_attr(src_sdp_p, SDP_SESSION_LEVEL, 0, inst_num, SDP_GROUP_ATTR_ANAT);

                 (void) sdp_set_group_num_id(src_sdp_p, SDP_SESSION_LEVEL, 0, inst_num, 2);
                 (void) sdp_set_group_id(src_sdp_p, SDP_SESSION_LEVEL, 0, inst_num, group_media->level);
                 (void) sdp_set_group_id(src_sdp_p, SDP_SESSION_LEVEL, 0, inst_num, media->level);
            }
        }
    } else {
        /* This is an answer, check if the offer rcvd had anat grouping */
        (void) sdp_attr_num_instances(dest_sdp_p, SDP_SESSION_LEVEL, 0, SDP_ATTR_GROUP,
                                  &num_group_lines);

        for (i = 1; i <= num_group_lines; i++) {
             if (sdp_get_group_attr(dest_sdp_p, SDP_SESSION_LEVEL, 0, i) == SDP_GROUP_ATTR_ANAT) {
                 num_anat_lines++;
             }
        }

        for (i = 1; i <= num_anat_lines; i++) {
             group_id_1 = sdp_get_group_id(dest_sdp_p, SDP_SESSION_LEVEL, 0, i, 1);
             group_id_2 = sdp_get_group_id(dest_sdp_p, SDP_SESSION_LEVEL, 0, i, 2);

             if ((media->level == group_id_1)  || (media->level == group_id_2)) {

                 group_media = gsmsdp_find_anat_pair(dcb_p, media);
                 if (group_media != NULL) {
                     if (sdp_get_group_attr(src_sdp_p, SDP_SESSION_LEVEL, 0, i) != SDP_GROUP_ATTR_ANAT) {
                         /*
                          * add a=group line
                          */
                         (void) sdp_add_new_attr(src_sdp_p, SDP_SESSION_LEVEL, 0, SDP_ATTR_GROUP, &inst_num);
                         (void) sdp_set_group_attr(src_sdp_p, SDP_SESSION_LEVEL, 0, inst_num, SDP_GROUP_ATTR_ANAT);

                     }
                     (void) sdp_set_group_num_id(src_sdp_p, SDP_SESSION_LEVEL, 0, i, 2);
                     (void) sdp_set_group_id(src_sdp_p, SDP_SESSION_LEVEL, 0, i, group_media->level);
                     (void) sdp_set_group_id(src_sdp_p, SDP_SESSION_LEVEL, 0, i, media->level);

                 } else {
                     /*
                      * add a=group line
                      */
                     (void) sdp_add_new_attr(src_sdp_p, SDP_SESSION_LEVEL, 0, SDP_ATTR_GROUP, &inst_num);
                     (void) sdp_set_group_attr(src_sdp_p, SDP_SESSION_LEVEL, 0, inst_num, SDP_GROUP_ATTR_ANAT);

                     (void) sdp_set_group_num_id(src_sdp_p, SDP_SESSION_LEVEL, 0, inst_num, 1);
                     (void) sdp_set_group_id(src_sdp_p, SDP_SESSION_LEVEL, 0, inst_num, media->level);
                 }

             }
        }
    }
    gsmsdp_set_mid_attr (src_sdp_p, media->level);
}

/*
 * gsmsdp_update_local_sdp_media
 *
 * Description:
 *
 * Adds an AUDIO media line to the local SDP of the specified DCB. If all_formats
 * is TRUE, sets all media formats supported by the phone into the local SDP, else
 * only add the single negotiated media format. If an AUDIO media line already
 * exists in the local SDP, remove it as this function completely rebuilds the
 * AUDIO media line and will do so at the same media level as the pre-existing
 * AUDIO media line.
 *
 * Parameters:
 *
 * dcb_p - Pointer to the DCB whose local SDP is to be updated.
 * cc_sdp_p - Pointer to the SDP being updated.
 * all_formats - If true, all supported media formats will be added to the
 *               AUDIO media line of the SDP. Otherwise, only the single
 *               negotiated media format is added.
 * media     - Pointer to fsmdef_media_t for the media entry of the SDP.
 * transport - transport type to for this media line.
 *
 */
static void
gsmsdp_update_local_sdp_media (fsmdef_dcb_t *dcb_p, cc_sdp_t *cc_sdp_p,
                              boolean all_formats, fsmdef_media_t *media,
                              sdp_transport_e transport)
{
    static const char fname[] = "gsmsdp_update_local_sdp_media";
    uint16_t        port;
    sdp_result_e    result;
    uint16_t        level;
    void           *sdp_p;
    int             sdpmode = 0;
    int             i = 0;

    if (!dcb_p || !media)  {
        GSM_ERR_MSG(get_debug_string(FSMDEF_DBG_INVALID_DCB), fname);
        return;
    }
    level = media->level;
    port  = media->src_port;

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    sdp_p = cc_sdp_p ? (void *) cc_sdp_p->src_sdp : NULL;

    if (sdp_p == NULL) {

        gsmsdp_init_local_sdp(dcb_p->peerconnection, &(dcb_p->sdp));

        cc_sdp_p = dcb_p->sdp;
        if ((cc_sdp_p == NULL) || (cc_sdp_p->src_sdp == NULL)) {
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"sdp is NULL and init failed",
                    dcb_p->line, dcb_p->call_id, fname);
            return;
        }
        sdp_p = (void *) cc_sdp_p->src_sdp;
    } else {

    /*
     * Remove the audio stream. Reset direction_set flag since
     * all media attributes have just been removed.
     */
    sdp_delete_media_line(sdp_p, level);
    media->direction_set = FALSE;
    }

    result = sdp_insert_media_line(sdp_p, level);
    if (result != SDP_SUCCESS) {
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"Inserting media line to Sdp failed",
                    dcb_p->line, dcb_p->call_id, fname);
        return;
    }

    gsmsdp_set_connection_address(sdp_p, media->level, dcb_p->ice_default_candidate_addr);

    (void) sdp_set_media_type(sdp_p, level, media->type);


    (void) sdp_set_media_portnum(sdp_p, level, port, media->local_datachannel_port);

    /* Set media transport and crypto attributes if it is for SRTP */
    gsmsdp_update_local_sdp_media_transport(dcb_p, sdp_p, media, transport,
                                            all_formats);

    if (all_formats) {
        /*
         * Add all supported media formats to the local sdp.
         */
        switch (media->type) {
        case SDP_MEDIA_AUDIO:
            gsmsdp_add_default_audio_formats_to_local_sdp(dcb_p, cc_sdp_p,
                                                          media);
            break;
        case SDP_MEDIA_VIDEO:
            gsmsdp_add_default_video_formats_to_local_sdp(dcb_p, cc_sdp_p,
                                                          media);
            break;
        case SDP_MEDIA_APPLICATION:
            gsmsdp_set_sctp_attributes (sdp_p, level, media);
            break;
        default:
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"SDP ERROR media %d for level %d is not"
                        " supported\n",
                        dcb_p->line, dcb_p->call_id, fname, media->type,
                        media->level);
            break;
        }
    } else {
        /*
         * Add negotiated codec list to the sdp
         */
        for(i = 0; i < media->num_payloads; i++) {
          result =
            sdp_add_media_payload_type(sdp_p, level,
                (uint16_t)(media->payloads[i].local_rtp_pt),
                SDP_PAYLOAD_NUMERIC);

          if (result != SDP_SUCCESS) {
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"Adding dynamic payload type failed",
                        dcb_p->line, dcb_p->call_id, fname);
          }

          switch (media->type) {
            case SDP_MEDIA_AUDIO:
              gsmsdp_set_media_attributes(media->payloads[i].codec_type,
                  sdp_p, level,
                  (uint16_t)(media->payloads[i].local_rtp_pt));
              break;
            case SDP_MEDIA_VIDEO:
              gsmsdp_set_video_media_attributes(media->payloads[i].codec_type,
                  cc_sdp_p, level,
                  (uint16_t)(media->payloads[i].local_rtp_pt));
              break;
            case SDP_MEDIA_APPLICATION:
              gsmsdp_set_sctp_attributes (sdp_p, level, media);
              break;
            default:
              GSM_ERR_MSG(GSM_L_C_F_PREFIX"SDP ERROR media %d for level %d is"
                        " not supported\n",
                        dcb_p->line, dcb_p->call_id, fname, media->type,
                        media->level);
              break;
          }

        }//end for

        /*
         * add the avt media type
         */
        if (media->avt_payload_type > RTP_NONE) {
            result = sdp_add_media_payload_type(sdp_p, level,
                         (uint16_t)media->avt_payload_type,
                         SDP_PAYLOAD_NUMERIC);
            if (result != SDP_SUCCESS) {
                GSM_ERR_MSG(GSM_L_C_F_PREFIX"Adding AVT payload type failed",
                            dcb_p->line, dcb_p->call_id, fname);
            }
            gsmsdp_set_media_attributes(RTP_AVT, sdp_p, level,
                (uint16_t) media->avt_payload_type);
        }
    }

    if (!sdpmode)
        gsmsdp_set_anat_attr(dcb_p, media);
}

/*
 * gsmsdp_update_local_sdp
 *
 * Description:
 *
 * Updates the local SDP of the DCB based on the remote SDP.
 *
 * Parameters:
 *
 * dcb_p - Pointer to the DCB whose local SDP is to be updated.
 * offer - Indicates whether the remote SDP was received in an offer
 *               or an answer.
 * initial_offer - this media line is initial offer.
 * media - Pointer to fsmdef_media_t for the media entry of the SDP.
 *
 * Return:
 *    TRUE  - update the local SDP was successfull.
 *    FALSE - update the local SDP failed.
 */
static boolean
gsmsdp_update_local_sdp (fsmdef_dcb_t *dcb_p, boolean offer,
                         boolean initial_offer,
                         fsmdef_media_t *media)
{
    static const char fname[] = "gsmsdp_update_local_sdp";
    cc_action_data_t data;
    sdp_direction_e direction;
    boolean         local_hold = (boolean)FSM_CHK_FLAGS(media->hold, FSM_HOLD_LCL);
    sdp_result_e    sdp_res;

    if (media->src_port == 0) {
        GSM_DEBUG(DEB_L_C_F_PREFIX"allocate receive port for media line",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
        /*
         * Source port has not been allocated, this could mean we
         * processing an initial offer SDP, SDP that requets to insert
         * a media line or re-insert a media line.
         */
        data.open_rcv.is_multicast = FALSE;
        data.open_rcv.listen_ip = ip_addr_invalid;
        data.open_rcv.port = 0;
        data.open_rcv.keep = FALSE;
        /*
         * Indicate type of media (audio/video etc) becase some for supporting
         * video over vieo, the port is obtained from other entity.
         */
        data.open_rcv.media_type = media->type;
        data.open_rcv.media_refid = media->refid;
        if (cc_call_action(dcb_p->call_id, dcb_p->line, CC_ACTION_OPEN_RCV,
                           &data) == CC_RC_SUCCESS) {
            /* allocate port successful, save the port */
            media->src_port = data.open_rcv.port;
            media->rcv_chan = FALSE;  /* mark no RX chan yet */
        } else {
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"allocate rx port failed",
                        dcb_p->line, dcb_p->call_id, fname);
            return (FALSE);
        }
    }

    /*
     * Negotiate direction based on remote SDP.
     */
    direction = gsmsdp_negotiate_local_sdp_direction(dcb_p, media, local_hold);

    /*
     * Update Transmit SRTP transmit key if this SRTP session.
     */
    if (media->transport == SDP_TRANSPORT_RTPSAVP) {
        gsmsdp_update_crypto_transmit_key(dcb_p, media, offer,
                                       initial_offer, direction);
    }

    if (offer == TRUE) {
        gsmsdp_update_local_sdp_media(dcb_p, dcb_p->sdp, FALSE, media,
                                      media->transport);
    }

    /*
     * Set local sdp direction.
     */
    if (media->direction_set) {
        if (media->direction != direction) {
            gsmsdp_set_local_sdp_direction(dcb_p, media, direction);
        }
    } else {
        gsmsdp_set_local_sdp_direction(dcb_p, media, direction);
    }
    return (TRUE);
}

/*
 * gsmsdp_update_local_sdp_for_multicast
 *
 * Description:
 *
 * Updates the local SDP of the DCB based on the remote SDP for
 * multicast. Populates the local sdp with the same addr:port as
 * in the offer and the same direction as in the offer (as per
 * rfc3264).
 *
 * Parameters:
 *
 * dcb_p         - Pointer to the DCB whose local SDP is to be updated.
 * portnum       - Remote port.
 * media         - Pointer to fsmdef_media_t for the media entry of the SDP.
 * offer         - boolean indicating an offer SDP if true.
 * initial_offer - boolean indicating an initial offer SDP if true.
 *
 */
static boolean
gsmsdp_update_local_sdp_for_multicast (fsmdef_dcb_t *dcb_p,
                                      uint16_t portnum,
                                      fsmdef_media_t *media,
                                      boolean offer,
                                      boolean initial_offer)
{
   static const char fname[] = "gsmsdp_update_local_sdp_for_multicast";
    sdp_direction_e direction;
    char            addr_str[MAX_IPADDR_STR_LEN];
    uint16_t        level;
    char            *p_addr_str;
    char            *strtok_state;

    level = media->level;

    GSM_DEBUG(DEB_L_C_F_PREFIX"%d %d %d",
			  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname),
			  portnum, level, initial_offer);

    direction = gsmsdp_get_remote_sdp_direction(dcb_p, media->level,
                                                &media->dest_addr);
    GSM_DEBUG(DEB_L_C_F_PREFIX"sdp direction: %d",
              DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), direction);
    /*
     * Update Transmit SRTP transmit key any way to clean up the
     * tx condition that we may have offered prior.
     */
    gsmsdp_update_crypto_transmit_key(dcb_p, media, offer, initial_offer,
                                      direction);

    gsmsdp_update_local_sdp_media(dcb_p, dcb_p->sdp, FALSE,
                                  media, media->transport);

    /*
     * Set local sdp direction same as on remote SDP for multicast
     */
    if ((direction == SDP_DIRECTION_RECVONLY) || (direction == SDP_DIRECTION_INACTIVE)) {
        if ((media->support_direction == SDP_DIRECTION_SENDRECV) ||
            (media->support_direction == SDP_DIRECTION_RECVONLY)) {
            /*
             * Echo same direction back in our local SDP but set the direction
             * in DCB to recvonly so that LSM operations on rcv port work
             * without modification.
             */
        } else {
            direction = SDP_DIRECTION_INACTIVE;
            GSM_DEBUG(DEB_L_C_F_PREFIX"media line"
                      " does not support receive stream\n",
                      DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
        }
        gsmsdp_set_local_sdp_direction(dcb_p, media, direction);
        media->direction_set = TRUE;
    } else {
        /*
         * return FALSE indicating error
         */
        return (FALSE);
    }

    /*
     * Set the ip addr to the multicast ip addr.
     */
    ipaddr2dotted(addr_str, &media->dest_addr);
    p_addr_str = PL_strtok_r(addr_str, "[ ]", &strtok_state);

    /*
     * Set the local SDP port number to match far ends port number.
     */
    (void) sdp_set_media_portnum(dcb_p->sdp->src_sdp, level, portnum, 0);

    /*
     * c= line <network type><address type><connection address>
     */
    (void) sdp_set_conn_nettype(dcb_p->sdp->src_sdp, level, SDP_NT_INTERNET);
    (void) sdp_set_conn_addrtype(dcb_p->sdp->src_sdp, level, media->addr_type);
    (void) sdp_set_conn_address(dcb_p->sdp->src_sdp, level, p_addr_str);

    return (TRUE);
}

/*
 * gsmsdp_get_remote_avt_payload_type
 *
 * Description:
 *
 * Returns the AVT payload type of the given audio line in the specified SDP.
 *
 * Parameters:
 *
 * level - The media level of the SDP where the media attribute is to be found.
 * sdp_p - Pointer to the SDP whose AVT payload type is being searched.
 *
 */
static int
gsmsdp_get_remote_avt_payload_type (uint16_t level, void *sdp_p)
{
    uint16_t        i;
    uint16_t        ptype;
    int             remote_avt_payload_type = RTP_NONE;
    uint16_t        num_a_lines = 0;
    const char     *encname = NULL;

    /*
     * Get number of RTPMAP attributes for the media line
     */
    (void) sdp_attr_num_instances(sdp_p, level, 0, SDP_ATTR_RTPMAP,
                                  &num_a_lines);

    /*
     * Loop through AUDIO media line RTPMAP attributes. The last
     * NET dynamic payload type will be returned.
     */
    for (i = 0; i < num_a_lines; i++) {
        ptype = sdp_attr_get_rtpmap_payload_type(sdp_p, level, 0,
                                                 (uint16_t) (i + 1));
        if (sdp_media_dynamic_payload_valid(sdp_p, ptype, level)) {
            encname = sdp_attr_get_rtpmap_encname(sdp_p, level, 0,
                                                  (uint16_t) (i + 1));
            if (encname) {
                if (cpr_strcasecmp(encname, SIPSDP_ATTR_ENCNAME_TEL_EVENT) == 0) {
                    remote_avt_payload_type = ptype;
                }
            }
        }
    }
    return (remote_avt_payload_type);
}


#define MIX_NEAREND_STRING  "X-mix-nearend"

/*
 *  gsmsdp_negotiate_codec
 *
 *  Description:
 *
 *  Negotiates an acceptable codec from the local and remote SDPs
 *
 *  Parameters:
 *
 *  dcb_p - Pointer to DCB whose codec is being negotiated
 *  sdp_p - Pointer to local and remote SDP
 *  media - Pointer to the fsmdef_media_t for a given media entry whose
 *          codecs are being negotiated.
 *  offer - Boolean indicating if the remote SDP came in an OFFER.
 *
 *  Returns:
 *
 *  codec  >  0: most preferred negotiated codec
 *         <= 0: negotiation failed
 */
static int
gsmsdp_negotiate_codec (fsmdef_dcb_t *dcb_p, cc_sdp_t *sdp_p,
                        fsmdef_media_t *media, boolean offer,
                        boolean initial_offer, uint16 media_level)
{
    static const char fname[] = "gsmsdp_negotiate_codec";
    rtp_ptype       pref_codec = RTP_NONE;
    uint16_t        i;
    uint16_t        j;
    int            *master_list_p = NULL;
    int            *slave_list_p = NULL;
    DtmfOutOfBandTransport_t transport = DTMF_OUTOFBAND_NONE;
    int             avt_payload_type;
    uint16_t        num_remote_types;
    uint16_t        num_local_types;
    uint16_t        num_master_types;
    uint16_t        num_slave_types;
    int             remote_codecs[CC_MAX_MEDIA_TYPES];
    int             remote_payload_types[CC_MAX_MEDIA_TYPES];
    int             local_codecs[CC_MAX_MEDIA_TYPES];
    sdp_payload_ind_e pt_indicator;
    uint32          ptime = 0;
    uint32          maxptime = 0;
    const char*     attr_label;
    uint16_t        level;
    boolean         explicit_reject = FALSE;
    boolean         found_codec = FALSE;
    int32_t         num_match_payloads = 0;
    int             codec = RTP_NONE;
    int             remote_pt = RTP_NONE;
    int32_t         payload_types_count = 0; /* count for allocating right amout
                                                of memory for media->payloads */
    int             temp;
    u16             a_inst;
    vcm_payload_info_t *payload_info = NULL;
    vcm_payload_info_t *previous_payload_info;

    if (!dcb_p || !sdp_p || !media) {
        return (RTP_NONE);
    }

    level = media_level;
    attr_label = sdp_attr_get_simple_string(sdp_p->dest_sdp,
                                            SDP_ATTR_LABEL, level, 0, 1);

    if (attr_label != NULL) {
        if (strcmp(attr_label, MIX_NEAREND_STRING) == 0) {
            dcb_p->session = WHISPER_COACHING;
        }
    }
    /*
     * Obtain list of payload types from the remote SDP
     */
    num_remote_types = sdp_get_media_num_payload_types(sdp_p->dest_sdp, level);

    if (num_remote_types > CC_MAX_MEDIA_TYPES) {
        num_remote_types = CC_MAX_MEDIA_TYPES;
    }

    for (i = 0; i < num_remote_types; i++) {
        temp = sdp_get_media_payload_type(sdp_p->dest_sdp, level,
                                          (uint16_t) (i + 1), &pt_indicator);
        remote_codecs[i] = GET_CODEC_TYPE(temp);
        remote_payload_types[i] = GET_DYN_PAYLOAD_TYPE_VALUE(temp);
    }

    /*
     * Get all the codecs that the phone supports.
     */
    if (media->type == SDP_MEDIA_AUDIO) {
        num_local_types = sip_config_local_supported_codecs_get(
                                    (rtp_ptype *)local_codecs,
                                    CC_MAX_MEDIA_TYPES);
    } else if (media->type == SDP_MEDIA_VIDEO) {
        num_local_types = sip_config_video_supported_codecs_get(
            (rtp_ptype *)local_codecs, CC_MAX_MEDIA_TYPES, offer);
    } else {
        GSM_DEBUG(DEB_L_C_F_PREFIX"unsupported media type %d",
            DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname),
            media->type);
        return (RTP_NONE);
    }

    /*
     * Set the AVT payload type to whatever value the farend wants to use,
     * but only if we have AVT turned on and the farend wants it
     * or if we are configured to always send it
     */
    config_get_value(CFGID_DTMF_OUTOFBAND, &transport, sizeof(transport));

    /*
     * Save AVT payload type for use by gsmsdp_compare_to_previous_sdp
     */
    media->previous_sdp.avt_payload_type = media->avt_payload_type;

    switch (transport) {
        case DTMF_OUTOFBAND_AVT:
            avt_payload_type = gsmsdp_get_remote_avt_payload_type(
                                   media->level, sdp_p->dest_sdp);
            if (avt_payload_type > RTP_NONE) {
                media->avt_payload_type = avt_payload_type;
            } else {
                media->avt_payload_type = RTP_NONE;
            }
            break;

        case DTMF_OUTOFBAND_AVT_ALWAYS:
            avt_payload_type = gsmsdp_get_remote_avt_payload_type(
                                   media->level, sdp_p->dest_sdp);
            if (avt_payload_type > RTP_NONE) {
                media->avt_payload_type = avt_payload_type;
            } else {
                /*
                 * If we are AVT_ALWAYS and the remote end is not using AVT,
                 * then send DTMF as out-of-band and use our configured
                 * payload type.
                 */
                config_get_value(CFGID_DTMF_AVT_PAYLOAD,
                                 &media->avt_payload_type,
                                 sizeof(media->avt_payload_type));

                GSM_DEBUG(DEB_L_C_F_PREFIX"AVT_ALWAYS forcing out-of-band DTMF,"
                          " payload_type = %d\n",
                          DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line,
                                                dcb_p->call_id, fname),
                          media->avt_payload_type);
            }
            break;

        case DTMF_OUTOFBAND_NONE:
        default:
            media->avt_payload_type = RTP_NONE;
            break;
    }

    /*
     * Find a matching codec in our local list with the remote list.
     * The local list was created with our preferred codec first in the list,
     * so this will ensure that we will match the preferred codec with the
     * remote list first, before matching other codecs.
     */
    pref_codec = sip_config_preferred_codec();
    if (pref_codec != RTP_NONE) {
        /*
         * If a preferred codec was configured and the platform
         * currently can do this codec, then it would be the
         * first element of the local_codecs because of the
         * logic in sip_config_local_supported_codec_get().
         */
        if (local_codecs[0] != pref_codec) {
            /*
             * preferred codec is configured but it is not avaible
             * currently, treat it as there is no codec available.
             */
            pref_codec = RTP_NONE;
        }
    }

    if (pref_codec == RTP_NONE) {
        master_list_p = remote_codecs;
        slave_list_p = local_codecs;
        num_master_types = num_remote_types;
        num_slave_types = num_local_types;
        GSM_DEBUG(DEB_L_C_F_PREFIX"Remote Codec list is Master",
            DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
    } else {
        master_list_p = local_codecs;
        slave_list_p = remote_codecs;
        num_master_types = num_local_types;
        num_slave_types = num_remote_types;
        GSM_DEBUG(DEB_L_C_F_PREFIX"Local Codec list is Master",
           DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
    }

    /*
     * Save payload information for use by gsmspd_compare_to_previous_sdp
     */
    gsmsdp_copy_payloads_to_previous_sdp(media);

    /*
     * Setup payload info structure list to store matched payload details
     * in the SDP.
     */
    media->num_payloads = 0;
    if(num_master_types <= num_slave_types ) {
      payload_types_count = num_master_types;
    } else {
      payload_types_count = num_slave_types;
    }

    /* Remove any previously allocated lists */
    if (media->payloads) {
        cpr_free(media->payloads);
    }

    /* Allocate memory for PT value, local PT and remote PT. */
    media->payloads = cpr_calloc(payload_types_count,
                                 sizeof(vcm_payload_info_t));

    /* Store the ptime and maxptime parameter for this m= section */
    if (media->type == SDP_MEDIA_AUDIO) {
        ptime = sdp_attr_get_simple_u32(sdp_p->dest_sdp,
                                    SDP_ATTR_PTIME, level, 0, 1);
        if (ptime != 0) {
            media->packetization_period = (uint16_t) ptime;
        }
        maxptime = sdp_attr_get_simple_u32(sdp_p->dest_sdp,
                      SDP_ATTR_MAXPTIME, level, 0, 1);
        if (maxptime != 0) {
            media->max_packetization_period = (uint16_t) maxptime;
        }
    }

    for (i = 0; i < num_master_types; i++) {
        for (j = 0; j < num_slave_types; j++) {
            if (master_list_p[i] == slave_list_p[j]) {

                /* We've found a codec in common. Configure the coresponding
                   payload information structure */
                codec = slave_list_p[j];
                payload_info = &(media->payloads[media->num_payloads]);

                if (master_list_p == remote_codecs) {
                    remote_pt = remote_payload_types[i];
                } else {
                    remote_pt = remote_payload_types[j];
                }

                payload_info->codec_type = codec;
                payload_info->local_rtp_pt = remote_pt;
                payload_info->remote_rtp_pt = remote_pt;

                /* If the negotiated payload type in an answer is different from
                   what we sent in our offer, we set the local payload type to
                   our default (since that's what we put in our offer) */
                if (!offer) {
                    previous_payload_info =
                        gsmsdp_find_info_for_codec(codec,
                           media->previous_sdp.payloads,
                           media->previous_sdp.num_payloads, 0);
                    if ((previous_payload_info == NULL) ||
                        (previous_payload_info->local_rtp_pt
                            != payload_info->local_rtp_pt)) {
                        payload_info->local_rtp_pt = codec;
                    }
                }

                if (media->type == SDP_MEDIA_AUDIO) {

                    if (sdp_attr_rtpmap_payload_valid(sdp_p->dest_sdp, level, 0,
                        &a_inst, remote_pt) ) {
                        /* Set the number of channels -- if omitted,
                           default is 1 */
                        payload_info->audio.channels =
                            sdp_attr_get_rtpmap_num_chan(sdp_p->dest_sdp,
                                                         level, 0, a_inst);
                        if (payload_info->audio.channels == 0) {
                            payload_info->audio.channels = 1;
                        }

                        /* Set frequency = clock rate. This is generally
                           correct.  Codecs can override this assumption on a
                           case-by-case basis in the switch construct below. */
                        payload_info->audio.frequency =
                            sdp_attr_get_rtpmap_clockrate(sdp_p->dest_sdp,
                                      level, 0, a_inst);
                    } else {
                        GSM_DEBUG(DEB_L_C_F_PREFIX"Could not find rtpmap "
                            "entry for payload %d -- setting defaults\n",
                            DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line,
                            dcb_p->call_id, fname), codec);
                        payload_info->audio.channels = 1;
                        /* See http://www.iana.org/assignments/rtp-parameters/
                           rtp-parameters.xml */
                        switch (codec) {
                            case STATIC_RTP_AVP_DVI4_16000_1:
                                codec = RTP_DVI4;
                                payload_info->audio.frequency = 16000;
                                break;
                            case STATIC_RTP_AVP_L16_44100_2:
                                codec = RTP_L16;
                                payload_info->audio.frequency = 44100;
                                payload_info->audio.channels = 2;
                                break;
                            case STATIC_RTP_AVP_L16_44100_1:
                                codec = RTP_L16;
                                payload_info->audio.frequency = 44100;
                                break;
                            case STATIC_RTP_AVP_DVI4_11025_1:
                                codec = RTP_DVI4;
                                payload_info->audio.frequency = 11025;
                                break;
                            case STATIC_RTP_AVP_DVI4_22050_1:
                                codec = RTP_DVI4;
                                payload_info->audio.frequency = 22050;
                                break;
                            default:
                                payload_info->audio.frequency = 8000;
                        }
                    }


                    switch (codec) {
                        case RTP_PCMA:
                        case RTP_PCMU:
                            /* 20 ms = 1/50th of a second */
                            payload_info->audio.packet_size =
                                payload_info->audio.frequency / 50;

                            payload_info->audio.bitrate = 8 *
                                payload_info->audio.frequency *
                                payload_info->audio.channels;
                            break;


                        case RTP_OPUS:
                            if (!sdp_attr_rtpmap_payload_valid(sdp_p->dest_sdp,
                                  level, 0, &a_inst, remote_pt) ||
                                (payload_info->audio.frequency
                                  != RTPMAP_OPUS_CLOCKRATE) ||
                                (payload_info->audio.channels != 2)) {

                                /* Be conservative in what we accept: any
                                   implementation that does not use a 48 kHz
                                   clockrate or 2 channels is broken. */
                                explicit_reject = TRUE;
                                continue; // keep looking
                            }

                            /* ********************************************* */
                            /* TODO !! FIXME !! XXX
                             * We can't support two-channel Opus until we merge
                             * in webrtc.org upstream, rev 3050 or later. See
                         http://code.google.com/p/webrtc/issues/detail?id=1013
                             * for details. We also need to have proper
                             * handling of the sprop-stereo and stereo SDP
                             * values before we use stereo encoding/decoding.
                             * Details in Mozilla Bug 818618.
                             */
                            payload_info->audio.channels = 1;
                            /* ********************************************* */

                            /* Store fmtp options */
                            sdp_attr_get_fmtp_max_average_bitrate (
                                sdp_p->dest_sdp, level, 0, 1,
                                &payload_info->opus.max_average_bitrate);

                            payload_info->opus.maxcodedaudiobandwidth =
                                sdp_attr_get_fmtp_maxcodedaudiobandwidth(
                                    sdp_p->dest_sdp, level, 0, 1);

                            sdp_attr_get_fmtp_usedtx (sdp_p->dest_sdp, level, 0,
                                1, &payload_info->opus.usedtx);

                            sdp_attr_get_fmtp_stereo (sdp_p->dest_sdp, level, 0,
                                1, &payload_info->opus.stereo);

                            sdp_attr_get_fmtp_useinbandfec (sdp_p->dest_sdp,
                                level, 0, 1, &payload_info->opus.useinbandfec);

                            sdp_attr_get_fmtp_cbr (sdp_p->dest_sdp, level, 0, 1,
                                &payload_info->opus.cbr);

                            /* Copied from media/webrtc/trunk/src/modules/
                               audio_coding/main/source/acm_codec_database.cc */
                            payload_info->audio.frequency = 48000;
                            payload_info->audio.packet_size = 960;
                            payload_info->audio.bitrate = 16000; // Increase when we have higher capture rates
                            break;

                        case RTP_ISAC:
                            /* TODO: Update these from proper SDP constructs */
                            payload_info->audio.frequency = 16000;
                            payload_info->audio.packet_size = 480;
                            payload_info->audio.bitrate = 32000;
                            break;

                        case RTP_ILBC:
                            payload_info->ilbc.mode =
                              (uint16_t)sdp_attr_get_fmtp_mode_for_payload_type(
                                  sdp_p->dest_sdp, level, 0, remote_pt);

                            /* TODO -- These should be updated to reflect the
                               actual frequency */
                            if (payload_info->ilbc.mode == SIPSDP_ILBC_MODE20)
                            {
                                payload_info->audio.packet_size = 160;
                                payload_info->audio.bitrate = 15200;
                            }
                            else /* mode = 30 */
                            {
                                payload_info->audio.packet_size = 240;
                                payload_info->audio.bitrate = 13300;
                            }
                            break;

                          default:
                              GSM_DEBUG(DEB_L_C_F_PREFIX"codec=%d not setting "
                                  "codec parameters (not implemented)\n",
                                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line,
                                  dcb_p->call_id, fname), codec);
                            payload_info->audio.packet_size = -1;
                            payload_info->audio.bitrate = -1;
                        } /* end switch */


                } else if (media->type == SDP_MEDIA_VIDEO) {
                    if ( media->video != NULL ) {
                       vcmFreeMediaPtr(media->video);
                       media->video = NULL;
                    }

                    if (!vcmCheckAttribs(codec, sdp_p, level, remote_pt,
                                         &media->video)) {
                          GSM_DEBUG(DEB_L_C_F_PREFIX"codec= %d ignored - "
                               "attribs not accepted\n",
                               DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line,
                               dcb_p->call_id, fname), codec);
                          explicit_reject = TRUE;
                          continue; /* keep looking */
                    }

                    /* cache the negotiated profile_level and bandwidth */
                    media->previous_sdp.tias_bw = media->tias_bw;
                    media->tias_bw =  ccsdpGetBandwidthValue(sdp_p,level, 1);
                    if ( (attr_label =
                        ccsdpAttrGetFmtpProfileLevelId(sdp_p,level,0,1))
                            != NULL ) {
                        media->previous_sdp.profile_level =
                            media->profile_level;
                        sscanf(attr_label,"%x", &media->profile_level);
                    }

                    /* This should ultimately use RFC 6236 a=imageattr
                       if present */

                    payload_info->video.width = 0;
                    payload_info->video.height = 0;

                    /* Set maximum frame size */
                    payload_info->video.max_fs = 0;
                    sdp_attr_get_fmtp_max_fs(sdp_p->dest_sdp, level, 0, 1,
                                             &payload_info->video.max_fs);

                    /* Set maximum frame rate */
                    payload_info->video.max_fr = 0;
                    sdp_attr_get_fmtp_max_fr(sdp_p->dest_sdp, level, 0, 1,
                                             &payload_info->video.max_fr);
                } /* end video */

                GSM_DEBUG(DEB_L_C_F_PREFIX"codec= %d",
                      DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line,
                                            dcb_p->call_id, fname), codec);


                found_codec = TRUE;

                /* Incrementing this number serves as a "commit" for the
                   payload_info. If we bail out of the loop before this
                   happens, then the collected information is abandoned. */
                media->num_payloads++;
                if(media->num_payloads >= payload_types_count) {
                    /* We maxed our allocated memory -- processing is done. */
                    return codec;
                }

                if(offer) {
                    /* If we are creating an answer, return after the first match.
                       TODO -- Eventually, we'll (probably) want to answer with
                       all the codecs we can receive. See bug 814227. */
                    return codec;
                }
            }
        }
    }

    /* Return the most preferred codec */
    if(found_codec) {
        return (media->payloads[0].codec_type);
    }

    /*
     * CSCsv84705 - we could not negotiate a common codec because
     * the local list is empty. This condition could happen when
     * using g729 in locally mixed conference in which another call
     * to vcm_get_codec_list() would return 0 or no codec.  So if
     * this is a not an init offer, we should just go ahead and use
     * the last negotiated codec if the remote list matches with
     * currently used.
     */
    if (!initial_offer && !explicit_reject) {
        for (i = 0; i < num_remote_types; i++) {
            if (media->num_payloads != 0 && media->payloads[0].codec_type ==
                remote_payload_types[i]) {
                GSM_DEBUG(DEB_L_C_F_PREFIX"local codec list was empty codec= %d"
                          " local=%d remote =%d\n", DEB_L_C_F_PREFIX_ARGS(GSM,
                          dcb_p->line, dcb_p->call_id, fname),
                          media->payloads[0].codec_type,
                          media->payloads[0].local_rtp_pt,
                          media->payloads[0].remote_rtp_pt);
                return (media->payloads[0].codec_type);
            }
        }
    }

    return (RTP_NONE);
}

/*
 * gsmsdp_negotiate_datachannel_attribs
 *
 * dcb_p - a pointer to the current dcb
 * sdp_p - the sdp we are analyzing
 * media - the media info
 * offer - Boolean indicating if the remote SDP came in an OFFER.
 */
static void
gsmsdp_negotiate_datachannel_attribs(fsmdef_dcb_t* dcb_p, cc_sdp_t* sdp_p, uint16_t level,
    fsmdef_media_t* media, boolean offer)
{
    uint32          num_streams;
    char           *protocol;

    sdp_attr_get_sctpmap_streams (sdp_p->dest_sdp, level, 0, 1, &num_streams);

    media->datachannel_streams = num_streams;

    sdp_attr_get_sctpmap_protocol(sdp_p->dest_sdp, level, 0, 1, media->datachannel_protocol);

    media->remote_datachannel_port = sdp_get_media_sctp_port(sdp_p->dest_sdp, level);
}

/*
 * gsmsdp_add_unsupported_stream_to_local_sdp
 *
 * Description:
 *
 * Adds a rejected media line to the local SDP. If there is already a media line at
 * the specified level, check to see if it matches the corresponding media line in the
 * remote SDP. If it does not, remove the media line from the local SDP so that
 * the corresponding remote SDP media line can be added. Note that port will be set
 * to zero indicating the media line is rejected.
 *
 * Parameters:
 *
 * scp_p - Pointer to the local and remote SDP.
 * level - The media line level being rejected.
 */
static void
gsmsdp_add_unsupported_stream_to_local_sdp (cc_sdp_t *sdp_p,
                                            uint16_t level)
{
    static const char fname[] = "gsmsdp_add_unsupported_stream_to_local_sdp";
    uint32_t          remote_pt;
    sdp_payload_ind_e remote_pt_indicator;
    cpr_ip_addr_t     addr;

    if (sdp_p == NULL) {
        GSM_ERR_MSG(GSM_F_PREFIX"sdp is null.", fname);
        return;
    }

    if (sdp_get_media_type(sdp_p->src_sdp, level) != SDP_MEDIA_INVALID) {
        sdp_delete_media_line(sdp_p->src_sdp, level);
    }

    if (sdp_p->dest_sdp == NULL) {
        GSM_ERR_MSG(GSM_F_PREFIX"no remote SDP available", fname);
        return;
    }

    /*
     * Insert media line at the specified level.
     */
    if (sdp_insert_media_line(sdp_p->src_sdp, level) != SDP_SUCCESS) {
        GSM_ERR_MSG(GSM_F_PREFIX"failed to insert a media line", fname);
        return;
    }

    /*
     * Set the attributes of the media line. Specify port = 0 to
     * indicate media line is rejected.
     */
    (void) sdp_set_media_type(sdp_p->src_sdp, level,
                              sdp_get_media_type(sdp_p->dest_sdp, level));
    (void) sdp_set_media_portnum(sdp_p->src_sdp, level, 0, 0);
    (void) sdp_set_media_transport(sdp_p->src_sdp, level,
                    sdp_get_media_transport(sdp_p->dest_sdp, level));

    remote_pt = sdp_get_media_payload_type(sdp_p->dest_sdp, level, 1,
                                           &remote_pt_indicator);
    /*
     * Don't like having to cast the payload type but sdp_get_media_payload_type
     * returns a uint32_t but sdp_add_media_payload_type takes a uint16_t payload type.
     * This needs to be fixed in Rootbeer.
     */
    (void) sdp_add_media_payload_type(sdp_p->src_sdp, level,
                                      (uint16_t) remote_pt,
                                      remote_pt_indicator);
    /*
     * The rejected media line needs to have "c=" line since
     * we currently do not include the "c=" at the session level.
     * The sdp parser in other end point such as the SDP parser
     * in the CUCM and in the phone ensures that there
     * is at least one "c=" line that can be used with each media
     * line. Such the parser will flag unsupported media line without
     * "c=" in that media line and at the session as error.
     *
     * The solution to have "c=" at the session level and
     * omitting "c=" at the media level all together can also
     * resolve this problem. Since the phone is also supporting
     * ANAT group for IPV4/IPV6 offering therefore selecting
     * session level and determining not to include "c=" line
     * at the media level can become complex. For this reason, the
     * unsupported media line will have "c=" with 0.0.0.0 address instead.
     */
    gsmsdp_set_connection_address(sdp_p->src_sdp, level, "0.0.0.0");
}

/*
 * gsmsdp_get_remote_media_address
 *
 * Description:
 *
 * Extract the remote address from the given sdp.
 *
 * Parameters:
 *
 * fcb_p - Pointer to the FCB containing thhe DCB whose media lines are
 *         being negotiated
 * sdp_p - Pointer to the the remote SDP
 * level - media line level.
 * dest_addr - pointer to the cpr_ip_addr_t structure to return
 *             remote address.
 *
 *  Returns:
 *  FALSE - fails.
 *  TRUE  - success.
 *
 */
static boolean
gsmsdp_get_remote_media_address (fsmdef_dcb_t *dcb_p,
                                 cc_sdp_t * sdp_p, uint16_t level,
                                 cpr_ip_addr_t *dest_addr)
{
   const char fname[] = "gsmsdp_get_remote_media_address";
    const char     *addr_str = NULL;
    int             dns_err_code;
    boolean         stat;

    *dest_addr = ip_addr_invalid;

    stat = sdp_connection_valid(sdp_p->dest_sdp, level);
    if (stat) {
        addr_str = sdp_get_conn_address(sdp_p->dest_sdp, level);
    } else {
        /* Address not at the media level. Try the session level. */
        stat = sdp_connection_valid(sdp_p->dest_sdp, SDP_SESSION_LEVEL);
        if (stat) {
            addr_str = sdp_get_conn_address(sdp_p->dest_sdp, SDP_SESSION_LEVEL);
        }
    }

    if (stat && addr_str) {
        /* Assume that this is dotted address */
        if (str2ip(addr_str, dest_addr) != 0) {
            /* It could be Fully Qualify DN, need to add DNS look up here */
            dns_err_code = dnsGetHostByName(addr_str, dest_addr, 100, 1);
            if (dns_err_code) {
                *dest_addr = ip_addr_invalid;
                stat = FALSE;
                GSM_ERR_MSG(GSM_L_C_F_PREFIX"DNS remote address error %d"
                            " with media at %d\n", dcb_p->line, dcb_p->call_id,
                            fname, dns_err_code, level);
            }
        }
    } else {
        /*
         * No address the media level or the session level.
         */
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"No remote address from SDP with at %d",
                    dcb_p->line, dcb_p->call_id, fname, level);
    }
    /*
     * Convert the remote address to host address to be used. It was
     * found out that without doing so, the softphone can crash when
     * in attempt to setup remote address to transmit API of DSP
     * implementation on win32.
     */
    util_ntohl(dest_addr, dest_addr);
    return (stat);
}

/* Function     gsmsdp_is_multicast_address
 *
 * Inputs:      - IP Address
 *
 * Returns:     YES if is multicast address, no if otherwise
 *
 * Purpose:     This is a utility function that tests to see if the passed
 *      address is a multicast address.  It does so by verifying that the
 *      address is between 225.0.0.0 and 239.255.255.255.
 *
 * Note:        Addresses passed are not in network byte order.
 *
 * Note2:       Addresses between 224.0.0.0 and 224.255.255.225 are also multicast
 *      addresses, but 224.0.0.0 to 224.0.0.255 are reserved and it is recommended
 *      to start at 225.0.0.0.  We need to research to see if this is a reasonable
 *      restriction.
 *
 */
int
gsmsdp_is_multicast_address (cpr_ip_addr_t theIpAddress)
{
    if  (theIpAddress.type == CPR_IP_ADDR_IPV4) {
    /*
     * Address already in host format
     */
        if ((theIpAddress.u.ip4 >= MULTICAST_START_ADDRESS) &&
            (theIpAddress.u.ip4 <= MULTICAST_END_ADDRESS)) {
        return (TRUE);
    }
    } else {
        //todo IPv6: Check IPv6 multicast address here.

    }
    return (FALSE);
}

/**
 *
 * The function assigns or associate the new media line in the
 * offered SDP to an entry in the media capability table.
 *
 * @param[in]dcb_p       - pointer to the fsmdef_dcb_t
 * @param[in]sdp_p       - pointer to cc_sdp_t that contains the retmote SDP.
 * @param[in]level       - uint16_t for media line level.
 *
 * @return           Pointer to the fsmdef_media_t if successfully
 *                   found the anat pair media line otherwise return NULL.
 *
 * @pre              (dcb not_eq NULL)
 * @pre              (sdp_p not_eq NULL)
 * @pre              (media not_eq NULL)
 */
static fsmdef_media_t*
gsmsdp_find_anat_media_line (fsmdef_dcb_t *dcb_p, cc_sdp_t *sdp_p, uint16_t level)
{
    fsmdef_media_t *anat_media = NULL;
    u32            group_id_1, group_id_2;
    u32            dst_mid, group_mid;
    uint16_t       num_group_lines= 0;
    uint16_t       num_anat_lines = 0;
    uint16_t       i;

    /*
     * Get number of ANAT groupings at the session level for the media line
     */
    (void) sdp_attr_num_instances(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0, SDP_ATTR_GROUP,
                                  &num_group_lines);

    for (i = 1; i <= num_group_lines; i++) {
         if (sdp_get_group_attr(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0, i) == SDP_GROUP_ATTR_ANAT) {
             num_anat_lines++;
         }
    }

    for (i = 1; i <= num_anat_lines; i++) {

        dst_mid = sdp_attr_get_simple_u32(sdp_p->dest_sdp, SDP_ATTR_MID, level, 0, 1);
        group_id_1 = sdp_get_group_id(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0, i, 1);
        group_id_2 = sdp_get_group_id(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0, i, 2);

        if (dst_mid == group_id_1) {
            GSMSDP_FOR_ALL_MEDIA(anat_media, dcb_p) {
                group_mid = sdp_attr_get_simple_u32(sdp_p->src_sdp,
                                                    SDP_ATTR_MID, (uint16_t) group_id_2, 0, 1);
                if (group_mid == group_id_2) {
                    /* found a match */
                    return (anat_media);
                }
            }
        } else if (dst_mid == group_id_2) {
            GSMSDP_FOR_ALL_MEDIA(anat_media, dcb_p) {
                group_mid = sdp_attr_get_simple_u32(sdp_p->src_sdp,
                                                    SDP_ATTR_MID, (uint16_t) group_id_1, 0, 1);
                if (group_mid == group_id_1) {
                    /* found a match */
                    return (anat_media);
                }
            }
        }
    }
    return (anat_media);
}

/**
 *
 * The function validates if all the anat groupings
 * have the right number of ids and their media type
 * is not the same
 *
 * @param[in]sdp_p      - pointer to the cc_sdp_t.
 *
 * @return           TRUE - anat validation passes
 *                   FALSE - anat validation fails
 *
 * @pre              (dcb not_eq NULL)
 * @pre              (sdp_p not_eq NULL)
 */
static boolean
gsmsdp_validate_anat (cc_sdp_t *sdp_p)
{
    u16          i, num_group_id;
    u32          group_id_1, group_id_2;
    sdp_media_e  media_type_gid1, media_type_gid2;
    uint16_t     num_group_lines= 0;
    uint16_t     num_anat_lines = 0;

    /*
     * Get number of ANAT groupings at the session level for the media line
     */
    (void) sdp_attr_num_instances(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0, SDP_ATTR_GROUP,
                                  &num_group_lines);

    for (i = 1; i <= num_group_lines; i++) {
         if (sdp_get_group_attr(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0, i) == SDP_GROUP_ATTR_ANAT) {
             num_anat_lines++;
         }
    }

    for (i = 1; i <= num_anat_lines; i++) {
         num_group_id = sdp_get_group_num_id (sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0, i);
         if ((num_group_id <=0) || (num_group_id > 2)) {
             /* This anat line has zero or more than two grouping, this is invalid */
             return (FALSE);
         } else if (num_group_id == 2) {
            /* Make sure that these anat groupings are not of same type */
            group_id_1 = sdp_get_group_id(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0, i, 1);
            group_id_2 = sdp_get_group_id(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0, i, 2);
            media_type_gid1 = sdp_get_media_type(sdp_p->dest_sdp, (u16) group_id_1);
            media_type_gid2 = sdp_get_media_type(sdp_p->dest_sdp, (u16) group_id_2);
            if (media_type_gid1 != media_type_gid2) {
                /* Group id types do not match */
                return (FALSE);
            }
            if (group_id_1 != sdp_attr_get_simple_u32(sdp_p->dest_sdp, SDP_ATTR_MID, (u16) group_id_1, 0, 1)) {
                /* Group id does not match the mid at the corresponding line */
                return (FALSE);
            }
            if (group_id_2 != sdp_attr_get_simple_u32(sdp_p->dest_sdp, SDP_ATTR_MID, (u16) group_id_2, 0, 1)) {
                return (FALSE);
            }
         }
    }

    return (TRUE);
}

/**
 *
 * The function validates if all the destination
 * Sdp m lines have mid values and if those mid values match
 * the source Sdp mid values
 *
 * @param[in]sdp_p      - pointer to the cc_sdp_t
 * @param[in]level      - uint16_t for media line level.
 *
 * @return           TRUE - mid validation passes
 *                   FALSE - mid validation fails
 *
 * @pre              (dcb not_eq NULL)
 * @pre              (sdp_p not_eq NULL)
 */
static boolean
gsmsdp_validate_mid (cc_sdp_t *sdp_p, uint16_t level)
{
    int32     src_mid, dst_mid;
    u16       i;
    uint16_t  num_group_lines= 0;
    uint16_t  num_anat_lines = 0;

    /*
     * Get number of ANAT groupings at the session level for the media line
     */
    (void) sdp_attr_num_instances(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0, SDP_ATTR_GROUP,
                                  &num_group_lines);

    for (i = 1; i <= num_group_lines; i++) {
         if (sdp_get_group_attr(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0, i) == SDP_GROUP_ATTR_ANAT) {
             num_anat_lines++;
         }
    }


    if (num_anat_lines > 0) {
        dst_mid = sdp_attr_get_simple_u32(sdp_p->dest_sdp, SDP_ATTR_MID, level, 0, 1);
        if (dst_mid == 0) {
            return (FALSE);
        }
        if (sdp_get_group_attr(sdp_p->src_sdp, SDP_SESSION_LEVEL, 0, 1) == SDP_GROUP_ATTR_ANAT) {
            src_mid = sdp_attr_get_simple_u32(sdp_p->src_sdp, SDP_ATTR_MID, level, 0, 1);
            if (dst_mid != src_mid) {
                return (FALSE);
             }
        }

    }
    return (TRUE);
}

/**
 *
 * The function negotiates the type of the media lines
 * based on anat attributes and ipv4/ipv6 settinsg.
 *
 * @param[in]dcb_p      - pointer to the fsmdef_dcb_t
 * @param[in]media      - pointer to the fsmdef_media_t
 *
 * @return           TRUE - this media line can be kept
 *                   FALSE - this media line can not be kept
 *
 * @pre              (dcb not_eq NULL)
 * @pre              (sdp_p not_eq NULL)
 */
static boolean
gsmsdp_negotiate_addr_type (fsmdef_dcb_t *dcb_p, fsmdef_media_t *media)
{
    static const char fname[] = "gsmsdp_negotiate_addr_type";
    cpr_ip_type     media_addr_type;
    cpr_ip_mode_e   ip_mode;
    fsmdef_media_t  *group_media;

    media_addr_type = media->dest_addr.type;
    if ((media_addr_type != CPR_IP_ADDR_IPV4) &&
        (media_addr_type != CPR_IP_ADDR_IPV6)) {
        /* Unknown/unsupported address type */
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"address type is not IPv4 or IPv6",
                    dcb_p->line, dcb_p->call_id, fname);
        return (FALSE);
    }
    ip_mode = platform_get_ip_address_mode();
    /*
     * find out whether this media line is part of an ANAT group or not.
     */
    group_media = gsmsdp_find_anat_pair(dcb_p, media);

    /*
     * It is possible that we have a media sink/source device that
     * attached to the phone, then we only accept IPV4 for these device.
     *
     * The code below is using FSM_MEDIA_F_SUPPORT_SECURITY as indication
     * whether this media line is mapped to the off board device or not.
     * When we get a better API to find out then use the better API than
     * checking the FSM_MEDIA_F_SUPPORT_SECURITY.
     */
    if (!FSM_CHK_FLAGS(media->flags, FSM_MEDIA_F_SUPPORT_SECURITY)) {
        if (media_addr_type != CPR_IP_ADDR_IPV4) {
            /* off board device we do not allow other address type but IPV4 */
            GSM_DEBUG(DEB_L_C_F_PREFIX"offboard device does not support IPV6",
                      DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
            return (FALSE);
        }

        /*
         * P2:
         * Need an API to get address from the off board device. For now,
         * use our local address.
         */
        if ((ip_mode == CPR_IP_MODE_DUAL) || (ip_mode == CPR_IP_MODE_IPV4)) {
            if (group_media != NULL) {
                /*
                 * this media line is part of ANAT group, keep the previous
                 * one negotiated media line.
                 */
                return (FALSE);
            }
            gsmsdp_get_local_source_v4_address(media);
            return (TRUE);
        }
        /* phone is IPV6 only mode */
        return (FALSE);
    }

    if (ip_mode == CPR_IP_MODE_DUAL) {
         /*
          * In dual mode, IPV6 is preferred address type. If there is an
          * ANAT then select the media line that has IPV6 address.
          */
         if (group_media == NULL) {
             /*
              * no pair media line found, this can be the first media
              * line negotiate. Keep this media line for now.
              */
             if (media_addr_type == CPR_IP_ADDR_IPV4) {
                 gsmsdp_get_local_source_v4_address(media);
             } else {
                 gsmsdp_get_local_source_v6_address(media);
             }
             return (TRUE);
         }

         /*
          * Found a ANAT pair media structure that this media line
          * is part of.
          */
         if (media_addr_type == CPR_IP_ADDR_IPV4) {
             /*
              * This media line is IPV4, keep the other line that have
              * been accepted before i.e. it shows up first therefore
              * the other one has preference.
              */
             return (FALSE);
         }

         /* This media line is IPV6 */
         if (group_media->src_addr.type == CPR_IP_ADDR_IPV4) {
             /*
              * The previous media line part of ANAT group is IPV4. The
              * phone policy is to select IPV6 for media stream. Remove
              * the previous media line and keep this media line (IPV6).
              */
              gsmsdp_add_unsupported_stream_to_local_sdp(dcb_p->sdp,
                                                         group_media->level);
              gsmsdp_remove_media(dcb_p, group_media);
              /* set this media line source address to IPV6 */
              gsmsdp_get_local_source_v6_address(media);
              return (TRUE);
         }
         /*
          * keep the previous one is also IPV6, remove this one i.e.
          * the one found has higher preferecne although this is not
          * a valid ANAT grouping.
          */
         return (FALSE);
    }

    /*
     * The phone is not in dual mode, the address type must be from the media
     * line must match the address type that the phone is supporting.
     */
    if ((ip_mode == CPR_IP_MODE_IPV6) &&
        (media_addr_type == CPR_IP_ADDR_IPV4)) {
        /* incompatible address type */
        return (FALSE);
    }
    if ((ip_mode == CPR_IP_MODE_IPV4) &&
        (media_addr_type == CPR_IP_ADDR_IPV6)) {
        /* incompatible address type */
        return (FALSE);
    }

    if (group_media != NULL) {
        /*
         * This meida line is part of an ANAT group, keep the previous
         *  media line and throw away this line.
         */
        return (FALSE);
    }

    /*
     * We have a compatible address type, set the source address based on
     * the address type from the remote media line.
     */
    if (media_addr_type == CPR_IP_ADDR_IPV4) {
        gsmsdp_get_local_source_v4_address(media);
    } else {
        gsmsdp_get_local_source_v6_address(media);
    }
    /* keep this media line */
    return (TRUE);
}

/**
 *
 * The function finds the best media capability that matches the offer
 * media line according to the media table specified.
 *
 * @param[in]dcb_p       - pointer to the fsmdef_dcb_t
 * @param[in]sdp_p       - pointer to cc_sdp_t that contains the retmote SDP.
 * @param[in]media       - pointer to the fsmdef_media_t.
 * @param[in]media_table - media table to use (global or session)
 *
 * @return     cap_index - the best match for the offer
 *
 * @pre              (dcb_p not_eq NULL)
 * @pre              (sdp_p not_eq NULL)
 * @pre              (media not_eq NULL)
 */
static uint8_t
gsmdsp_find_best_match_media_cap_index (fsmdef_dcb_t    *dcb_p,
                                        cc_sdp_t        *sdp_p,
                                        fsmdef_media_t  *media,
                                        media_table_e   media_table)
{
    const cc_media_cap_t *media_cap;
    uint8_t              cap_index, candidate_cap_index;
    boolean              srtp_fallback;
    sdp_direction_e      remote_direction, support_direction;
    sdp_transport_e      remote_transport;
    sdp_media_e          media_type;

    remote_transport = sdp_get_media_transport(sdp_p->dest_sdp, media->level);
    remote_direction = gsmsdp_get_remote_sdp_direction(dcb_p, media->level,
                                                       &media->dest_addr);
    srtp_fallback    = sip_regmgr_srtp_fallback_enabled(dcb_p->line);
    media_type       = media->type;


    /*
     * Select the best suitable media capability entry that
     * match this media line.
     *
     * The following rules are used:
     *
     * 1) rule out entry that is invalid or not enabled or with
     *    different media type.
     * 2) rule out entry that has been used by other existing
     *    media line.
     *
     * After the above rules applies look for the better match for
     * direction support and security support. The platform should
     * arrange the capability table in preference order with
     * higher prefered entry placed at the lower index in the table.
     */
    candidate_cap_index = CC_MAX_MEDIA_CAP;
    for (cap_index = 0; cap_index < CC_MAX_MEDIA_CAP; cap_index++) {
        /* Find the cap entry that has the same media type and enabled */
        if (media_table == MEDIA_TABLE_GLOBAL) {
            media_cap = &g_media_table.cap[cap_index];
        } else {
            media_cap = gsmsdp_get_media_cap_entry_by_index(cap_index,dcb_p);
        }
        if ((media_cap == NULL) || !media_cap->enabled ||
            (media_cap->type != media_type)) {
            /* does not exist, not enabled or not the same type */
            continue;
        }

        /* Check for already in used */
        if (gsmsdp_find_media_by_cap_index(dcb_p, cap_index) != NULL) {
            /* this capability entry has been used */
            continue;
        }

        /*
         * Check for security support. The rules below attempts to
         * use entry that support security unless there is no entry
         * and the SRTP fallback is enabled. If the remote offer is not
         * SRTP just ignore the supported security and proceed on i.e.
         * any entry is ok.
         */
        if (remote_transport == SDP_TRANSPORT_RTPSAVP) {
            if (!media_cap->support_security && !srtp_fallback) {
                /*
                 * this entry does not support security and SRTP fallback
                 * is not enabled.
                 */
                continue;
            }
            if (!media_cap->support_security) {
                /*
                 * this entry is not support security but srtp fallback
                 * is enabled, it potentially can be used
                 */
                candidate_cap_index = cap_index;
            }
        }

        /*
         * Check for suitable direction support. The rules for matching
         * directions are not exact rules. Try to match the closely
         * offer as much as possible. This is the best we know. We can
         * not guess what the real capability of the offer may have or will
         * change in the future (re-invite).
         */
        support_direction = media_cap->support_direction;
        if (remote_direction == SDP_DIRECTION_INACTIVE) {
            if (support_direction != SDP_DIRECTION_SENDRECV) {
                /* prefer send and receive for inactive */
                candidate_cap_index = cap_index;
            }
        } else if (remote_direction == SDP_DIRECTION_RECVONLY) {
            if ((support_direction != SDP_DIRECTION_SENDRECV) &&
                (support_direction != SDP_DIRECTION_SENDONLY)) {
                /* incompatible direction */
                continue;
            } else if (support_direction != SDP_DIRECTION_SENDONLY) {
                candidate_cap_index = cap_index;
            }
        } else if (remote_direction == SDP_DIRECTION_SENDONLY) {
            if ((support_direction != SDP_DIRECTION_SENDRECV) &&
                (support_direction != SDP_DIRECTION_RECVONLY)) {
                /* incompatible direction */
                continue;
            } else if (support_direction != SDP_DIRECTION_RECVONLY) {
                candidate_cap_index = cap_index;
            }
        } else if (remote_direction == SDP_DIRECTION_SENDRECV) {
            if (support_direction != SDP_DIRECTION_SENDRECV) {
                candidate_cap_index = cap_index;
            }
        }

        if (candidate_cap_index == cap_index) {
            /* this entry is not exactly best match, try other ones */
            continue;
        }
        /* this is the first best match found, use it */
        break;
    }

    if (cap_index == CC_MAX_MEDIA_CAP) {
        if (candidate_cap_index != CC_MAX_MEDIA_CAP) {
            /* We have a candidate entry to use */
            cap_index = candidate_cap_index;
        }
    }

    return cap_index;
}

/**
 *
 * The function finds the best media capability that matches the offer
 * media line.
 *
 * @param[in]dcb_p       - pointer to the fsmdef_dcb_t
 * @param[in]sdp_p       - pointer to cc_sdp_t that contains the retmote SDP.
 * @param[in]media       - pointer to the fsmdef_media_t.
 *
 * @return           TRUE - successful assigning a capability entry
 *                          to the media line.
 *                   FALSE - failed to assign a capability entry to the
 *                           media line.
 *
 * @pre              (dcb_p not_eq NULL)
 * @pre              (sdp_p not_eq NULL)
 * @pre              (media not_eq NULL)
 */
static boolean
gsmsdp_assign_cap_entry_to_incoming_media (fsmdef_dcb_t    *dcb_p,
                                           cc_sdp_t        *sdp_p,
                                           fsmdef_media_t  *media)
{
    static const char fname[] = "gsmsdp_assign_cap_entry_to_incoming_media";
    const cc_media_cap_t *media_cap;
    uint8_t              cap_index;
    fsmdef_media_t       *anat_media;

    /*
     * Find an existing media line that this media line belongs to the
     * same media group. If found, the same cap_index will be used.
     */
    anat_media = gsmsdp_find_anat_media_line(dcb_p, sdp_p, media->level);
    if (anat_media != NULL) {
        media_cap = gsmsdp_get_media_cap_entry_by_index(anat_media->cap_index, dcb_p);
        if (media_cap == NULL) {
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"no media capability",
                        dcb_p->line, dcb_p->call_id, fname);
            return (FALSE);
        }
        gsmsdp_set_media_capability(media, media_cap);
        /* found the existing media line in the same ANAT group */
        media->cap_index = anat_media->cap_index;
        return (TRUE);
    }


    cap_index  = gsmdsp_find_best_match_media_cap_index(dcb_p,
                                                        sdp_p,
                                                        media,
                                                        MEDIA_TABLE_SESSION);

    if (cap_index == CC_MAX_MEDIA_CAP) {
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"reached max streams supported or"
                      " no suitable media capability\n",
                      dcb_p->line, dcb_p->call_id, fname);
            return (FALSE);
        }

    /* set the capabilities to the media and associate with it */
    media_cap = gsmsdp_get_media_cap_entry_by_index(cap_index,dcb_p);
    if (media_cap == NULL) {
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"no media cap",
                    dcb_p->line, dcb_p->call_id, fname);
        return (FALSE);
    }
    gsmsdp_set_media_capability(media, media_cap);

    /* override the direction for special feature */
    gsmsdp_feature_overide_direction(dcb_p, media);
    if (media->support_direction == SDP_DIRECTION_INACTIVE) {
        GSM_DEBUG(DEB_L_C_F_PREFIX"feature overrides direction to inactive,"
                  " no capability assigned\n",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
        return (FALSE);
    }

    media->cap_index = cap_index;
    GSM_DEBUG(DEB_L_C_F_PREFIX"assign media cap index %d",
              DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), cap_index);
    return (TRUE);
}

/**
 *
 * The function handles negotiate adding of a media line.
 *
 * @param[in]dcb_p       - pointer to the fsmdef_dcb_t
 * @param[in]media_type  - media type.
 * @param[in]level       - media line.
 * @param[in]remote_port - remote port
 * @param[in]offer       - boolean indicates offer or answer.
 *
 * @return           pointer to fsmdef_media_t if media is successfully
 *                   added or return NULL.
 *
 * @pre              (dcb_p not_eq NULL)
 * @pre              (sdp_p not_eq NULL)
 * @pre              (remote_addr not_eq NULL)
 */
static fsmdef_media_t *
gsmsdp_negotiate_add_media_line (fsmdef_dcb_t  *dcb_p,
                                 sdp_media_e   media_type,
                                 uint16_t      level,
                                 uint16_t      remote_port,
                                 boolean       offer)
{
    static const char fname[] = "gsmsdp_negotiate_add_media_line";
    fsmdef_media_t       *media;

    if (remote_port == 0) {
        /*
         * This media line is new but marked as disbaled.
         */
        return (NULL);
    }

    if (!offer) {
        /*
         * This is not an offer, the remote end wants to add
         * a new media line in the answer.
         */
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"remote trying add media in answer SDP",
                    dcb_p->line, dcb_p->call_id, fname);
        return (NULL);
    }

    /*
     * Allocate a new raw media structure but not filling completely yet.
     */
    media = gsmsdp_get_new_media(dcb_p, media_type, level);
    if (media == NULL) {
        /* unable to add another media */
        return (NULL);
    }

    /*
     * If this call is locally held, mark the media with local hold so
     * that the negotiate direction will have the correct direction.
     */
    if ((dcb_p->fcb->state == FSMDEF_S_HOLDING) ||
        (dcb_p->fcb->state == FSMDEF_S_HOLD_PENDING)) {
        /* the call is locally held, set the local held status */
        FSM_SET_FLAGS(media->hold, FSM_HOLD_LCL);
    }
    return (media);
}

/**
 *
 * The function handles negotiate remove of a media line. Note the
 * removal of a media line does not actaully removed from the offer/answer
 * SDP.
 *
 * @param[in]dcb_p    - pointer to the fsmdef_dcb_t
 * @param[in]media    - pointer to the fsmdef_media_t for the media entry
 *                      to deactivate.
 * @param[in]remote_port - remote port from the remote's SDP.
 * @param[in]offer    - boolean indicates offer or answer.
 *
 * @return           TRUE  - when line is inactive.
 *                   FALSE - when line remains to be further processed.
 *
 * @pre              (dcb not_eq NULL) and (media not_eq NULL)
 */
static boolean
gsmsdp_negotiate_remove_media_line (fsmdef_dcb_t *dcb_p,
                                    fsmdef_media_t *media,
                                    uint16_t remote_port,
                                    boolean offer)
{
    static const char fname[] = "gsmsdp_negotiate_remove_media_line";

    if (offer) {
        /* This is an offer SDP from the remote */
        if (remote_port != 0) {
            /* the remote quests media is not for removal */
            return (FALSE);
        }
        /*
         * Remote wants to remove the media line or to keep the media line
         * disabled. Fall through.
         */
    } else {
        /* This is an answer SDP from the remote */
        if ((media->src_port != 0) && (remote_port != 0)) {
            /* the media line is not for removal */
            return (FALSE);
        }
        /*
         * There are 3 possible causes:
         * 1) our offered port is 0 and remote's port is 0
         * 2) our offered port is 0 and remote's port is not 0.
         * 3) our offered port is not 0 and remote's port is 0.
         *
         * In any of these cases, the media line will not be used.
         */
        if ((media->src_port == 0) && (remote_port != 0)) {
            /* we offer media line removal but the remote does not comply */
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"remote insists on keeping media line",
                        dcb_p->line, dcb_p->call_id, fname);
        }
    }

    /*
     * This media line is to be removed.
     */
    return (TRUE);
}

/*
 * Find a media line based on the media type.
 */
fsmdef_media_t* gsmsdp_find_media_by_media_type(fsmdef_dcb_t *dcb_p, sdp_media_e media_type) {

    fsmdef_media_t *media = NULL;

    /*
     * search the all entries that has a valid media and matches the media type
     */
    GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
        if (media->type == media_type) {
            /* found a match */
            return (media);
        }
    }
    return (NULL);
}

/*
 * gsmsdp_add_rtcp_fb
 *
 * Description:
 *  Adds a=rtcp-fb attributes to local SDP for supported video codecs
 *
 * Parameters:
 *   level - SDP media level (between 1 and the # of m-lines)
 *   sdp_p - pointer to local SDP
 *   codec - the codec type that the attributes should be added for
 *   types - a bitmask of rtcp-fb types to add, taken from
 *           sdp_rtcp_fb_bitmask_e
 *
 * returns
 *  CC_CAUSE_OK - success
 *  any other code - failure
 */
cc_causes_t
gsmsdp_add_rtcp_fb (int level, sdp_t *sdp_p,
                    rtp_ptype codec, unsigned int types)
{
    int num_pts;
    int pt_codec;
    sdp_payload_ind_e indicator;

    int pt_index;
    unsigned int j;
    num_pts = sdp_get_media_num_payload_types(sdp_p, level);
    for (pt_index = 1; pt_index <= num_pts; pt_index++) {
        pt_codec = sdp_get_media_payload_type (sdp_p, level, pt_index,
                                               &indicator);
        if (codec == RTP_NONE || (pt_codec & 0xFF) == codec) {
            int pt = GET_DYN_PAYLOAD_TYPE_VALUE(pt_codec);

            /* Add requested a=rtcp-fb:nack attributes */
            for (j = 0; j < SDP_MAX_RTCP_FB_NACK; j++) {
                if (types & sdp_rtcp_fb_nack_to_bitmap(j)) {
                    gsmsdp_set_rtcp_fb_nack_attribute(level, sdp_p, pt, j);
                }
            }

            /* Add requested a=rtcp-fb:ack attributes */
            for (j = 0; j < SDP_MAX_RTCP_FB_ACK; j++) {
                if (types & sdp_rtcp_fb_ack_to_bitmap(j)) {
                    gsmsdp_set_rtcp_fb_nack_attribute(level, sdp_p, pt, j);
                }
            }

            /* Add requested a=rtcp-fb:ccm attributes */
            for (j = 0; j < SDP_MAX_RTCP_FB_CCM; j++) {
                if (types & sdp_rtcp_fb_ccm_to_bitmap(j)) {
                    gsmsdp_set_rtcp_fb_ccm_attribute(level, sdp_p, pt, j);
                }
            }

        }
    }
    return CC_CAUSE_OK;
}


/*
 * gsmsdp_negotiate_rtcp_fb
 *
 * Description:
 *  Negotiates a=rtcp-fb attributes to local SDP for supported video codecs
 *
 * Parameters:
 *   cc_sdp_p - local and remote SDP
 *   media    - The media structure for the current level to be negotiated
 *   offer    - True if the remote SDP is an offer
 *
 * returns
 *  CC_CAUSE_OK - success
 *  any other code - failure
 */
cc_causes_t
gsmsdp_negotiate_rtcp_fb (cc_sdp_t *cc_sdp_p,
                          fsmdef_media_t *media,
                          boolean offer)
{
    int level = media->level;
    int pt_codec;
    int remote_pt;
    sdp_payload_ind_e indicator;
    int pt_index, i;
    sdp_rtcp_fb_nack_type_e nack_type;
    sdp_rtcp_fb_ack_type_e ack_type;
    sdp_rtcp_fb_ccm_type_e ccm_type;
    uint32_t fb_types = 0;

    int num_pts = sdp_get_media_num_payload_types(cc_sdp_p->dest_sdp, level);

    /*
     * Remove any previously negotiated rtcp-fb attributes from the
     * local SDP
     */
    sdp_result_e result = SDP_SUCCESS;
    while (result == SDP_SUCCESS) {
        result = sdp_delete_attr (cc_sdp_p->src_sdp, level, 0,
                                  SDP_ATTR_RTCP_FB, 1);
    }

    /*
     * For each remote payload type, determine what feedback types are
     * requested.
     */
    for (pt_index = 1; pt_index <= num_pts; pt_index++) {
        int pt_codec = sdp_get_media_payload_type (cc_sdp_p->dest_sdp,
                                                   level, pt_index, &indicator);
        int codec = pt_codec & 0xFF;
        remote_pt = GET_DYN_PAYLOAD_TYPE_VALUE(pt_codec);
        fb_types = 0;

        /* a=rtcp-fb:nack */
        i = 1;
        do {
            nack_type = sdp_attr_get_rtcp_fb_nack(cc_sdp_p->dest_sdp,
                                                  level, remote_pt, i);
            if (nack_type >= 0 && nack_type < SDP_MAX_RTCP_FB_NACK) {
                fb_types |= sdp_rtcp_fb_nack_to_bitmap(nack_type);
            }
            i++;
        } while (nack_type != SDP_RTCP_FB_NACK_NOT_FOUND);

        /* a=rtcp-fb:ack */
        i = 1;
        do {
            ack_type = sdp_attr_get_rtcp_fb_ack(cc_sdp_p->dest_sdp,
                                                level, remote_pt, i);
            if (ack_type >= 0 && ack_type < SDP_MAX_RTCP_FB_ACK) {
                fb_types |= sdp_rtcp_fb_ack_to_bitmap(ack_type);
            }
            i++;
        } while (ack_type != SDP_RTCP_FB_ACK_NOT_FOUND);

        /* a=rtcp-fb:ccm */
        i = 1;
        do {
            ccm_type = sdp_attr_get_rtcp_fb_ccm(cc_sdp_p->dest_sdp,
                                                level, remote_pt, i);
            if (ccm_type >= 0 && ccm_type < SDP_MAX_RTCP_FB_CCM) {
                fb_types |= sdp_rtcp_fb_ccm_to_bitmap(ccm_type);
            }
            i++;
        } while (ccm_type != SDP_RTCP_FB_CCM_NOT_FOUND);

        /*
         * Mask out the types that we do not support
         */
        switch (codec) {
            /* Really should be all video codecs... */
            case RTP_VP8:
            case RTP_H263:
            case RTP_H264_P0:
            case RTP_H264_P1:
            case RTP_I420:
                fb_types &=
                  sdp_rtcp_fb_nack_to_bitmap(SDP_RTCP_FB_NACK_BASIC) |
                  sdp_rtcp_fb_nack_to_bitmap(SDP_RTCP_FB_NACK_PLI) |
                  sdp_rtcp_fb_ccm_to_bitmap(SDP_RTCP_FB_CCM_FIR);
                break;
            default:
                fb_types = 0;
                break;
        }

        /*
         * Now, in our local SDP, set rtcp-fb types that both we and the
         * remote party support
         */
        if (fb_types) {
            gsmsdp_add_rtcp_fb (level, cc_sdp_p->src_sdp, codec, fb_types);
        }

        /*
         * Finally, update the media record for this payload type to
         * reflect the expected feedback types
         */
        for (i = 0; i < media->num_payloads; i++) {
            if (media->payloads[i].remote_rtp_pt == remote_pt) {
                media->payloads[i].video.rtcp_fb_types = fb_types;
            }
        }
    }
    return CC_CAUSE_OK;
}

/*
 * gsmsdp_negotiate_extmap
 *
 * Description:
 *  Negotiates extmaps header extension to local SDP for supported audio codecs
 *
 * Parameters:
 *   cc_sdp_p - local and remote SDP
 *   media    - The media structure for the current level to be negotiated
 *   offer    - True if the remote SDP is an offer
 *
 * returns
 *  CC_CAUSE_OK - success
 *  any other code - failure
 */
cc_causes_t
gsmsdp_negotiate_extmap (cc_sdp_t *cc_sdp_p,
                          fsmdef_media_t *media,
                          boolean offer)
{
    boolean audio_level = FALSE;
    u16 audio_level_id = 0xFFFF;
    int level = media->level;
    int i;
    const char* uri;

    /*
     * Remove any previously negotiated extmap attributes from the
     * local SDP
     */
    sdp_result_e result = SDP_SUCCESS;
    while (result == SDP_SUCCESS) {
        result = sdp_delete_attr (cc_sdp_p->src_sdp, level, 0,
                                  SDP_ATTR_EXTMAP, 1);
    }

    i = 1;
    do {
        uri = sdp_attr_get_extmap_uri(cc_sdp_p->dest_sdp, level, i);

        if (uri != NULL && strcmp(uri, SDP_EXTMAP_AUDIO_LEVEL) == 0) {
          audio_level = TRUE;
          audio_level_id = sdp_attr_get_extmap_id(cc_sdp_p->dest_sdp, level, i);
        }
        i++;
    } while (uri != NULL);

    media->audio_level = audio_level;
    media->audio_level_id = audio_level_id;

    /*
     * Now, in our local SDP, set extmap types that both we and the
     * remote party support
     */
    if (media->audio_level) {
        gsmsdp_set_extmap_attribute (level, cc_sdp_p->src_sdp, audio_level_id, SDP_EXTMAP_AUDIO_LEVEL);
    }

    return CC_CAUSE_OK;
}

/*
 * gsmsdp_negotiate_media_lines
 *
 * Description:
 *
 * Walk down the media lines provided in the remote sdp. Compare each
 * media line to the corresponding media line in the local sdp. If
 * the media line does not exist in the local sdp, add it. If the media
 * line exists in the local sdp but is different from the remote sdp,
 * change the local sdp to match the remote sdp. If the media line
 * is an AUDIO format, negotiate the codec and update the local sdp
 * as needed.
 *
 * Parameters:
 *
 * fcb_p - Pointer to the FCB containing thhe DCB whose media lines are being negotiated
 * sdp_p - Pointer to the local and remote SDP
 * initial_offer - Boolean indicating if the remote SDP came in the first OFFER of this session
 * offer - Boolean indicating if the remote SDP came in an OFFER.
 * notify_stream_added - Boolean indicating the UI should be notified of streams added
 * create_answer - indicates whether the provided offer is supposed to generate
 *                 an answer (versus simply setting the remote description)
 *
 * In practice, the initial_offer, offer, and create_answer flags only make
 * sense in a limited number of configurations:
 *
 *   Phase                        | i_o   | offer | c_a
 *  ------------------------------+-------+-------+-------
 *   SetRemote (initial offer)    | true  | true  | false
 *   SetRemote (reneg. offer)     | false | true  | false
 *   SetRemote (answer)           | false | false | false
 *   CreateAnswer (initial)       | true  | true  | true
 *   CreateAnswer (renegotitaion) | false | true  | true
 *
 * TODO(adam@nostrum.com): These flags make the code very hard to read at
 *   the calling site.  They should be replaced by an enumeration that
 *   contains only those five valid combinations described above.
 */
cc_causes_t
gsmsdp_negotiate_media_lines (fsm_fcb_t *fcb_p, cc_sdp_t *sdp_p, boolean initial_offer,
                              boolean offer, boolean notify_stream_added, boolean create_answer)
{
    static const char fname[] = "gsmsdp_negotiate_media_lines";
    cc_causes_t     cause = CC_CAUSE_OK;
    uint16_t        num_m_lines = 0;
    uint16_t        num_local_m_lines = 0;
    uint16_t        i = 0;
    sdp_media_e     media_type;
    fsmdef_dcb_t   *dcb_p = fcb_p->dcb;
    uint16_t        port;
    boolean         update_local_ret_value = TRUE;
    sdp_transport_e transport;
    uint16_t        crypto_inst;
    boolean         media_found = FALSE;
    cpr_ip_addr_t   remote_addr;
    boolean         new_media;
    sdp_direction_e video_avail = SDP_DIRECTION_INACTIVE;
    boolean        unsupported_line;
    fsmdef_media_t *media;
    uint8_t         cap_index;
    sdp_direction_e remote_direction;
    boolean         result;
    int             sdpmode = 0;
    char           *session_pwd;
    cc_action_data_t  data;
    int             j=0;
    int             rtcpmux = 0;
    tinybool        rtcp_mux = FALSE;
    sdp_result_e    sdp_res;
    boolean         created_media_stream = FALSE;
    int             lsm_rc;
    int             sctp_port;
    u32             datachannel_streams;

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    num_m_lines = sdp_get_num_media_lines(sdp_p->dest_sdp);
    if (num_m_lines == 0) {
        GSM_DEBUG(DEB_L_C_F_PREFIX"no media lines found.",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
        return CC_CAUSE_NO_MEDIA;
    }

    /*
     * Validate the anat values
     */
    if (!gsmsdp_validate_anat(sdp_p)) {
        /* Failed anat validation */
        GSM_DEBUG(DEB_L_C_F_PREFIX"failed anat validation",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
        return (CC_CAUSE_NO_MEDIA);
    }

    /*
     * Process each media line in the remote SDP
     */
    for (i = 1; i <= num_m_lines; i++) {
        unsupported_line = FALSE; /* assume line will be supported */
        new_media        = FALSE;
        media            = NULL;
        media_type = sdp_get_media_type(sdp_p->dest_sdp, i);

        /*
         * Only perform these checks when called from createanswer
         * because at this point we are concerned as to which m= lines
         * have been created in the answer.
         */
        if (create_answer) {

            /* Since the incoming SDP might not be in the same order as
               our media, we find them by type rather than location
               for this check. Note that we're not checking for the
               value of any _particular_ m= section; we're just checking
               whether (at least) one of the specified type exists.  */
            media = gsmsdp_find_media_by_media_type(dcb_p, media_type);

            if (media_type == SDP_MEDIA_AUDIO && !media) {
                /* continue if answer will not add this m= line */
                continue;
            }

            if (media_type == SDP_MEDIA_VIDEO && !media) {
                continue;
            }

            if (media_type == SDP_MEDIA_APPLICATION && !media) {
                continue;
            }
        }

        port = (uint16_t) sdp_get_media_portnum(sdp_p->dest_sdp, i);
        GSM_DEBUG(DEB_L_C_F_PREFIX"Port is %d at %d %d",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname),
                  port, i, initial_offer);

        switch (media_type) {
        case SDP_MEDIA_AUDIO:
        case SDP_MEDIA_VIDEO:
        case SDP_MEDIA_APPLICATION:
            /*
             * Get remote address before other negotiations process in case
             * the address 0.0.0.0 (old style hold) to be used
             * for direction negotiation.
             */
            if (!gsmsdp_get_remote_media_address(dcb_p, sdp_p, i,
                                             &remote_addr)) {
                /* failed to get the remote address */
                GSM_DEBUG(DEB_L_C_F_PREFIX"unable to get remote addr at %d",
                          DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), i);
                unsupported_line = TRUE;
                break;
            }

            /*
             * Find the corresponding media entry in the dcb to see
             * this has been negiotiated previously (from the
             * last offer/answer session).
             */
            if(!create_answer)
              media = gsmsdp_find_media_by_level(dcb_p, i);

            if (media == NULL) {
                /* No previous media, negotiate adding new media line. */
                media = gsmsdp_negotiate_add_media_line(dcb_p, media_type, i,
                                                        port, offer);
                if (media == NULL) {
                    /* new one can not be added */
                    unsupported_line = TRUE;
                    break;
                }
                /*
                 * This media is a newly added, it is by itself an
                 * initial offer of this line.
                 */
                new_media = TRUE;
                GSM_DEBUG(DEB_L_C_F_PREFIX"new media entry at %d",
                          DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), i);
            } else if (media->type == media_type) {
                /*
                 * Use the remote port to determine whether the
                 * media line is to be removed from the SDP.
                 */
                if (gsmsdp_negotiate_remove_media_line(dcb_p, media, port,
                                                       offer)) {
                    /* the media line is to be removed from the SDP */
                    unsupported_line = TRUE;
                    GSM_DEBUG(DEB_L_C_F_PREFIX"media at %d is removed",
                              DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), i);
                   break;
                }
            } else {
                /* The media at the same level but not the expected type */
                GSM_ERR_MSG(GSM_L_C_F_PREFIX"mismatch media type at %d",
                            dcb_p->line, dcb_p->call_id, fname, i);
                unsupported_line = TRUE;
                break;
            }

            /* Do not negotiate if media is set to inactive */
            if (SDP_DIRECTION_INACTIVE == media->direction) {
                break;
            }

            /* Reset multicast flag and port */
            media->is_multicast = FALSE;
            media->multicast_port = 0;

            /* Update remote address */
            media->previous_sdp.dest_addr = media->dest_addr;
            media->dest_addr = remote_addr;

            /*
             * Associate the new media (for adding new media line) to
             * the capability table.
             */
            if (media->cap_index == CC_MAX_MEDIA_CAP) {
                if (!gsmsdp_assign_cap_entry_to_incoming_media(dcb_p, sdp_p,
                                                               media)) {
                    unsupported_line = TRUE;
                    GSM_DEBUG(DEB_L_C_F_PREFIX"unable to assign capability entry at %d",
                              DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), i);
                    // Check if we need to update the UI that video has been offered
                    if ( offer && media_type == SDP_MEDIA_VIDEO &&
                          ( ( g_media_table.cap[CC_VIDEO_1].support_direction !=
                                   SDP_DIRECTION_INACTIVE) )  ) {
                        // passed basic checks, now on to more expensive checks...
                        remote_direction = gsmsdp_get_remote_sdp_direction(dcb_p,
                                                                           media->level,
                                                                           &media->dest_addr);
                        cap_index        = gsmdsp_find_best_match_media_cap_index(dcb_p,
                                                                                  sdp_p,
                                                                                  media,
                                                                                  MEDIA_TABLE_GLOBAL);

                        GSM_DEBUG(DEB_L_C_F_PREFIX"remote_direction: %d global match %sfound",
                            DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname),
                            remote_direction, (cap_index != CC_MAX_MEDIA_CAP) ? "" : "not ");
                        if ( cap_index != CC_MAX_MEDIA_CAP &&
                               remote_direction != SDP_DIRECTION_INACTIVE ) {
                           // this is an offer and platform can support video
                           GSM_DEBUG(DEB_L_C_F_PREFIX"\n\n\n\nUpdate video Offered Called %d",
                                    DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), remote_direction);
                           lsm_update_video_offered(dcb_p->line, dcb_p->call_id, remote_direction);
                        }
                    }
                    break;
                }
            }

            /*
             * Negotiate address type and take only address type
             * that can be accepted.
             */
            if (!gsmsdp_negotiate_addr_type(dcb_p, media)) {
                unsupported_line = TRUE;
                break;
            }

            /*
             * Negotiate RTP/SRTP. The result is the media transport
             * which could be RTP/SRTP or fail.
             */
            transport = gsmsdp_negotiate_media_transport(dcb_p, sdp_p,
                                                         offer, media,
                                                         &crypto_inst, i);
            if (transport == SDP_TRANSPORT_INVALID) {
                /* unable to negotiate transport */
                unsupported_line = TRUE;
                GSM_DEBUG(DEB_L_C_F_PREFIX"transport mismatch at %d",
                          DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), i);
                break;
            }

            /* Don't need to negotiate a codec for an m= applicaton line */
            if (SDP_MEDIA_APPLICATION != media_type) {

                /*
                 * Negotiate to a single codec
                 */
                if (gsmsdp_negotiate_codec(dcb_p, sdp_p, media, offer, initial_offer, i) ==
                    RTP_NONE) {
                    /* unable to negotiate codec */
                    unsupported_line = TRUE;
                    /* Failed codec negotiation */
                    cause = CC_CAUSE_PAYLOAD_MISMATCH;
                    GSM_DEBUG(DEB_L_C_F_PREFIX"codec mismatch at %d",
                              DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), i);
                    break;
                }
            } else {
                gsmsdp_negotiate_datachannel_attribs(dcb_p, sdp_p, i, media, offer);
            }

            /*
             * Both media transport (RTP/SRTP) and codec are
             * now negotiated to common ones, update transport
             * parameters to be used for SRTP, if there is any.
             */
            gsmsdp_update_negotiated_transport(dcb_p, sdp_p, media,
                                               crypto_inst, transport, i);
            GSM_DEBUG(DEB_F_PREFIX"local transport after updating negotiated: %d",DEB_F_PREFIX_ARGS(GSM, fname), sdp_get_media_transport(dcb_p->sdp->src_sdp, 1));
            /*
             * Add to or update media line to the local SDP as needed.
             */
            if (gsmsdp_is_multicast_address(media->dest_addr)) {
                /*
                 * Multicast, if the address is multicast
                 * then change the local sdp and do the necessary
                 * call to set up reception of multicast packets
                 */
                GSM_DEBUG(DEB_L_C_F_PREFIX"Got multicast offer",
                         DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
                media->is_multicast = TRUE;
                media->multicast_port = port;
                update_local_ret_value =
                     gsmsdp_update_local_sdp_for_multicast(dcb_p, port,
                                                           media, offer,
                                                           new_media);
            } else {
                update_local_ret_value = gsmsdp_update_local_sdp(dcb_p,
                                                                 offer,
                                                                 new_media,
                                                                 media);
            }
            GSM_DEBUG(DEB_F_PREFIX"local transport after updateing local SDP: %d",DEB_F_PREFIX_ARGS(GSM, fname), sdp_get_media_transport(dcb_p->sdp->src_sdp, 1));

            /*
             * Successful codec negotiated cache direction for  ui video update
             */
            if (media_type == SDP_MEDIA_VIDEO ) {
                video_avail = media->direction;
            }

            if (update_local_ret_value == TRUE) {
                media->previous_sdp.dest_port = media->dest_port;
                media->dest_port = port;
                if (media_type == SDP_MEDIA_AUDIO || sdpmode) {
                    /* at least found one workable audio media line */
                    media_found = TRUE;
                }
            } else {
                /*
                 * Rejecting multicast because direction is not RECVONLY
                 */
                unsupported_line = TRUE;
                update_local_ret_value = TRUE;
            }
            /* Negotiate rtcp feedback mechanisms */
            if (media && media_type == SDP_MEDIA_VIDEO) {
                gsmsdp_negotiate_rtcp_fb (dcb_p->sdp, media, offer);
            }
            /* Negotiate redundancy mechanisms */
            if (media && media_type == SDP_MEDIA_AUDIO) {
                gsmsdp_negotiate_extmap (dcb_p->sdp, media, offer);
            }

            /*
             * Negotiate rtcp-mux
             */
            if(SDP_MEDIA_APPLICATION != media_type) {
              sdp_res = sdp_attr_get_rtcp_mux_attribute(sdp_p->dest_sdp, i,
                                                        0, SDP_ATTR_RTCP_MUX,
                                                        1, &rtcp_mux);
              if (SDP_SUCCESS == sdp_res) {
                media->rtcp_mux = TRUE;
              }
            }

            /*
              Negotiate datachannel attributes.
              We are using our port from config and reflecting the
              number of streams from the other side.
             */
            if (media_type == SDP_MEDIA_APPLICATION) {
              config_get_value(CFGID_SCTP_PORT, &sctp_port, sizeof(sctp_port));
              media->local_datachannel_port = sctp_port;

              sdp_res = sdp_attr_get_sctpmap_streams(sdp_p->dest_sdp,
                media->level, 0, 1, &datachannel_streams);

              /* If no streams value from the other side we will use our default
               */
              if (sdp_res == SDP_SUCCESS) {
                  media->datachannel_streams = datachannel_streams;
              }

              gsmsdp_set_sctp_attributes(sdp_p->src_sdp, i, media);
            }

            if (!unsupported_line) {

              if (sdpmode) {
                  sdp_setup_type_e remote_setup_type;
                  int j;

                  sdp_res = sdp_attr_get_setup_attribute(
                      sdp_p->dest_sdp, i, 0, 1, &remote_setup_type);

                  /*
                   * Although a=setup is required for DTLS per RFC 5763,
                   * there are several implementations (including older
                   * versions of Firefox) that don't signal direction.
                   * To work with these cases, we assume that an omitted
                   * direction in SDP means "PASSIVE" in an offer, and
                   * "ACTIVE" in an answer.
                   */
                  if (sdp_res != SDP_SUCCESS) {
                      remote_setup_type =
                          offer ? SDP_SETUP_PASSIVE : SDP_SETUP_ACTIVE;
                  }

                  /* The DTLS role will be set based on the media->setup
                     value when the TransportFlow is created */
                  switch (remote_setup_type) {
                    case SDP_SETUP_ACTIVE:
                        media->setup = SDP_SETUP_PASSIVE;
                        break;
                    case SDP_SETUP_PASSIVE:
                        media->setup = SDP_SETUP_ACTIVE;
                        break;
                    case SDP_SETUP_ACTPASS:
                        /*
                         * This should only happen in an offer. If the
                         * remote side is ACTPASS, we choose to be ACTIVE;
                         * this allows us to start setting up the DTLS
                         * association immediately, saving 1/2 RTT in
                         * association establishment.
                         */
                        media->setup = SDP_SETUP_ACTIVE;
                        break;
                    case SDP_SETUP_HOLDCONN:
                        media->setup = SDP_SETUP_HOLDCONN;
                        media->direction = SDP_DIRECTION_INACTIVE;
                        break;
                    default:
                        /*
                         * If we don't recognize the remote endpoint's setup
                         * attribute, we fall back to being active if they
                         * sent an offer, and passive if they sent an answer.
                         */
                        media->setup =
                            offer ? SDP_SETUP_ACTIVE : SDP_SETUP_PASSIVE;
                  }

                  if (create_answer) {
                      gsmsdp_set_setup_attribute(media->level,
                                                 dcb_p->sdp->src_sdp,
                                                 media->setup);
                  }

                  /* Set ICE */
                  for (j=0; j<media->candidate_ct; j++) {
                    gsmsdp_set_ice_attribute (SDP_ATTR_ICE_CANDIDATE, media->level,
                                              sdp_p->src_sdp, media->candidatesp[j]);
                  }

                  /* Set RTCPMux if we have it turned on in our config
                     and the other side requests it */
                  config_get_value(CFGID_RTCPMUX, &rtcpmux, sizeof(rtcpmux));
                  if (rtcpmux && media->rtcp_mux) {
                    gsmsdp_set_rtcp_mux_attribute (SDP_ATTR_RTCP_MUX, media->level,
                                                   sdp_p->src_sdp, TRUE);
                  }

                  if (notify_stream_added) {
                      /*
                       * Add track to remote streams in dcb
                       */
                      if (SDP_MEDIA_APPLICATION != media_type &&
                          /* Do not expect to receive media if we're sendonly! */
                          (media->direction == SDP_DIRECTION_SENDRECV ||
                           media->direction == SDP_DIRECTION_RECVONLY)) {
                          int pc_stream_id = -1;

                          /* This is a hack to keep all the media in a single
                             stream.
                             TODO(ekr@rtfm.com): revisit when we have media
                             assigned to streams in the SDP */
                          if (!created_media_stream){
                              lsm_rc = lsm_add_remote_stream (dcb_p->line,
                                                              dcb_p->call_id,
                                                              media,
                                                              &pc_stream_id);
                              if (lsm_rc) {
                                return (CC_CAUSE_NO_MEDIA);
                              } else {
                                MOZ_ASSERT(pc_stream_id == 0);
                                /* Use index 0 because we only have one stream */
                                result = gsmsdp_add_remote_stream(0,
                                                                  pc_stream_id,
                                                                  dcb_p);
                                MOZ_ASSERT(result);  /* TODO(ekr@rtfm.com)
                                                        add real error checking,
                                                        but this "can't fail" */
                                created_media_stream = TRUE;
                              }
                          }

                          if (created_media_stream) {
                                /* Now add the track to the single media stream.
                                   use index 0 because we only have one stream */
                                result = gsmsdp_add_remote_track(0, i, dcb_p, media);
                                MOZ_ASSERT(result);  /* TODO(ekr@rtfm.com) add real
                                                       error checking, but this
                                                       "can't fail" */
                          }
                      }
                  }
              }
            }

            break;

        default:
            /* Not a support media type stream */
            unsupported_line = TRUE;
            break;
        }

        if (unsupported_line) {
            /* add this line to unsupported line */
            gsmsdp_add_unsupported_stream_to_local_sdp(sdp_p, i);
            gsmsdp_set_mid_attr(sdp_p->src_sdp, i);
            /* Remove the media if one to be removed */
            if (media != NULL) {
                /* remove this media off the list */
                gsmsdp_remove_media(dcb_p, media);
            }
        }
        if (!gsmsdp_validate_mid(sdp_p, i)) {
             /* Failed mid validation */
            cause = CC_CAUSE_NO_MEDIA;
            GSM_DEBUG(DEB_L_C_F_PREFIX"failed mid validation at %d",
                      DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), i);
        }
    }

    /*
     * Must have at least one media found and at least one audio
     * line.
     */
    if (!media_found) {
        if (cause != CC_CAUSE_PAYLOAD_MISMATCH) {
            cause = CC_CAUSE_NO_MEDIA;
        }
    } else {
        if (cause == CC_CAUSE_PAYLOAD_MISMATCH) {
            /*
             * some media lines have codec mismatch but there are some
             * that works, do not return error.
             */
            cause = CC_CAUSE_OK;
        }

        /*
         * If we are processing an offer sdp, need to set the
         * start time and stop time based on the remote SDP
         */
        gsmsdp_update_local_time_stamp(dcb_p, offer, initial_offer);

        /*
         * workable media line was found. Need to make sure we don't
         * advertise more than workable media lines. Loop through
         * remaining media lines in local SDP and set port to zero.
         */
        num_local_m_lines = sdp_get_num_media_lines(sdp_p->src_sdp);
        if (num_local_m_lines > num_m_lines) {
            for (i = num_m_lines + 1; i <= num_local_m_lines; i++) {
                (void) sdp_set_media_portnum(sdp_p->src_sdp, i, 0, 0);
            }
        }

        /*
         * Update UI for Remote Stream Added
         */
        if (sdpmode) {

            /* Fail negotiation if DTLS is not in SDP */
            cause = gsmsdp_configure_dtls_data_attributes(fcb_p);
            if (cause != CC_CAUSE_OK) {
                GSM_DEBUG("gsmsdp_negotiate_media_lines- DTLS negotiation failed");
                return cause;
            }

            /* ToDO(emannion)
             * Fail negotiation if ICE is not negotiated.
             */

            /*
             * Bubble the stream added event up to the PC UI
             */
            if (notify_stream_added) {
                for (j=0; j < CC_MAX_STREAMS; j++ ) {
                    if (dcb_p->remote_media_stream_tbl->streams[j].
                        num_tracks &&
                        (!dcb_p->remote_media_stream_tbl->streams[j].
                         num_tracks_notified)) {
                        /* Note that we only notify when the number of tracks
                           changes from 0 -> !0 (i.e. on creation).
                           TODO(adam@nostrum.com): Figure out how to notify
                           when streams gain tracks */
                        ui_on_remote_stream_added(evOnRemoteStreamAdd,
                            fcb_p->state, dcb_p->line, dcb_p->call_id,
                            dcb_p->caller_id.call_instance_id,
                            dcb_p->remote_media_stream_tbl->streams[j]);

                        dcb_p->remote_media_stream_tbl->streams[j].num_tracks_notified =
                            dcb_p->remote_media_stream_tbl->streams[j].num_tracks;
                    }
                }
            }
        }
    }
    /*
     * We have negotiated the line, clear flag that we have set
     * that we are waiting for an answer SDP in ack.
     */
    dcb_p->remote_sdp_in_ack = FALSE;

    /*
     * check to see if UI needs to be updated for video
     */
    GSM_DEBUG(DEB_L_C_F_PREFIX"Update video Avail Called %d",
               DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname),video_avail);

    // update direction but preserve the cast attrib
    dcb_p->cur_video_avail &= CC_ATTRIB_CAST;
    dcb_p->cur_video_avail |= (uint8_t)video_avail;

    lsm_update_video_avail(dcb_p->line, dcb_p->call_id, dcb_p->cur_video_avail);

    return cause;
}

/*
 * This function returns boolean parameters indicating what media types
 * exist in the offered SDP.
 */
cc_causes_t
gsmsdp_get_offered_media_types (fsm_fcb_t *fcb_p, cc_sdp_t *sdp_p, boolean *has_audio,
                                boolean *has_video, boolean *has_data)
{
    cc_causes_t     cause = CC_CAUSE_OK;
    uint16_t        num_m_lines = 0;
    uint16_t        i = 0;
    sdp_media_e     media_type;
    fsmdef_dcb_t   *dcb_p = fcb_p->dcb;
    boolean         result;

    num_m_lines = sdp_get_num_media_lines(sdp_p->dest_sdp);
    if (num_m_lines == 0) {
        GSM_DEBUG(DEB_L_C_F_PREFIX"no media lines found.",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, __FUNCTION__));
        return CC_CAUSE_NO_MEDIA;
    }

    *has_audio = FALSE;
    *has_video = FALSE;
    *has_data = FALSE;

    /*
     * Process each media line in the remote SDP
     */
    for (i = 1; i <= num_m_lines; i++) {
        media_type = sdp_get_media_type(sdp_p->dest_sdp, i);

        if(SDP_MEDIA_AUDIO == media_type)
            *has_audio = TRUE;
        else if(SDP_MEDIA_VIDEO == media_type)
            *has_video = TRUE;
        else if(SDP_MEDIA_APPLICATION == media_type)
            *has_data = TRUE;
    }

    return cause;
}

/*
 * gsmsdp_init_local_sdp
 *
 * Description:
 *
 * This function initializes the local sdp for generation of an offer sdp or
 * an answer sdp. The following sdp values are initialized.
 *
 * v= line
 * o= line <username><session id><version><network type><address type><address>
 * s= line
 * t= line <start time><stop time>
 *
 * Parameters:
 *
 * peerconnection - handle to peerconnection object
 * sdp_pp     - Pointer to the local sdp
 *
 * returns    cc_causes_t
 *            CC_CAUSE_OK - indicates success
 *            Any other code - indicates failure
 */
static cc_causes_t
gsmsdp_init_local_sdp (const char *peerconnection, cc_sdp_t **sdp_pp)
{
    char            addr_str[MAX_IPADDR_STR_LEN];
    cpr_ip_addr_t   ipaddr;
    unsigned long   session_id = 0;
    char            session_version_str[GSMSDP_VERSION_STR_LEN];
    void           *local_sdp_p = NULL;
    cc_sdp_t       *sdp_p = NULL;
    int             nat_enable = 0;
    char           *p_addr_str;
    cpr_ip_mode_e   ip_mode;
    char           *strtok_state;

    if (!peerconnection) {
        return CC_CAUSE_NO_PEERCONNECTION;
    }
    if (!sdp_pp) {
        return CC_CAUSE_NULL_POINTER;
    }

    ip_mode = platform_get_ip_address_mode();
    /*
     * Get device address. We will need this later.
     */
    config_get_value(CFGID_NAT_ENABLE, &nat_enable, sizeof(nat_enable));
    if (nat_enable == 0) {
        if ((ip_mode == CPR_IP_MODE_DUAL) || (ip_mode == CPR_IP_MODE_IPV6)) {
            sip_config_get_net_ipv6_device_ipaddr(&ipaddr);
        } else if (ip_mode == CPR_IP_MODE_IPV4) {
            sip_config_get_net_device_ipaddr(&ipaddr);
        }
    } else {
        sip_config_get_nat_ipaddr(&ipaddr);
    }


	ipaddr2dotted(addr_str, &ipaddr);

    p_addr_str = PL_strtok_r(addr_str, "[ ]", &strtok_state);

    /*
     * Create the local sdp struct
     */
    if (*sdp_pp == NULL) {
        sipsdp_src_dest_create(peerconnection,
            CCSIP_SRC_SDP_BIT, sdp_pp);
    } else {
        sdp_p = *sdp_pp;
        if (sdp_p->src_sdp != NULL) {
            sipsdp_src_dest_free(CCSIP_SRC_SDP_BIT, sdp_pp);
        }
        sipsdp_src_dest_create(peerconnection,
            CCSIP_SRC_SDP_BIT, sdp_pp);
    }
    sdp_p = *sdp_pp;

    if ( sdp_p == NULL )
       return CC_CAUSE_NO_SDP;

    local_sdp_p = sdp_p->src_sdp;

    /*
     * v= line
     */
    (void) sdp_set_version(local_sdp_p, SIPSDP_VERSION);

    /*
     * o= line <username><session id><version><network type>
     * <address type><address>
     */
    (void) sdp_set_owner_username(local_sdp_p, SIPSDP_ORIGIN_USERNAME);

    session_id = abs(cpr_rand() % 28457);
    snprintf(session_version_str, sizeof(session_version_str), "%d",
             (int) session_id);
    (void) sdp_set_owner_sessionid(local_sdp_p, session_version_str);

    snprintf(session_version_str, sizeof(session_version_str), "%d", 0);
    (void) sdp_set_owner_version(local_sdp_p, session_version_str);

    (void) sdp_set_owner_network_type(local_sdp_p, SDP_NT_INTERNET);

    if ((ip_mode == CPR_IP_MODE_DUAL) || (ip_mode == CPR_IP_MODE_IPV6)) {
        (void) sdp_set_owner_address_type(local_sdp_p, SDP_AT_IP6);
    } else if (ip_mode == CPR_IP_MODE_IPV4) {
       (void) sdp_set_owner_address_type(local_sdp_p, SDP_AT_IP4);
    }
    (void) sdp_set_owner_address(local_sdp_p, p_addr_str);

    /*
     * s= line
     */
    (void) sdp_set_session_name(local_sdp_p, SIPSDP_SESSION_NAME);

    /*
     * t= line <start time><stop time>
     * We init these to zero. If we are building an answer sdp, these will
     * be reset from the offer sdp.
     */
    (void) sdp_set_time_start(local_sdp_p, "0");
    (void) sdp_set_time_stop(local_sdp_p, "0");

    return CC_CAUSE_OK;
}

/**
 * The function sets the capabilities from media capability to the
 * media structure.
 *
 * @param[in]media     - pointer to the fsmdef_media_t to be set with
 *                       capabilites from the media_cap.
 * @param[in]media_cap - media capability to be used with this new media
 *                       line.
 *
 * @return           None.
 *
 * @pre              (media not_eq NULL)
 * @pre              (media_cap not_eq NULL)
 */
static void
gsmsdp_set_media_capability (fsmdef_media_t *media,
                             const cc_media_cap_t *media_cap)
{
    /* set default direction */
    media->direction = media_cap->support_direction;
    media->support_direction = media_cap->support_direction;
    if (media_cap->support_security) {
        /* support security */
        FSM_SET_FLAGS(media->flags, FSM_MEDIA_F_SUPPORT_SECURITY);
    }
}

/**
 * The function adds a media line into the local SDP.
 *
 * @param[in]dcb_p     - pointer to the fsmdef_dcb_t
 * @param[in]media_cap - media capability to be used with this new media
 *                       line.
 * @param[in]cap_index - media capability entry index to associate with
 *                       the media line.
 * @param[in]level     - media line order in the SDP so called level.
 * @param[in]addr_type - cpr_ip_type for address of the media line to add.
 *
 * @return           Pointer to the fsmdef_media_t if successfully
 *                   add a new line otherwise return NULL.
 *
 * @pre              (dcb_p not_eq NULL)
 * @pre              (media_cap not_eq NULL)
 */
static fsmdef_media_t *
gsmsdp_add_media_line (fsmdef_dcb_t *dcb_p, const cc_media_cap_t *media_cap,
                       uint8_t cap_index, uint16_t level,
                       cpr_ip_type addr_type, boolean offer)
{
    static const char fname[] = "gsmsdp_add_media_line";
    cc_action_data_t  data;
    fsmdef_media_t   *media = NULL;
    int               i=0;
    int               rtcpmux = 0;
    int               sctp_port = 0;

    switch (media_cap->type) {
    case SDP_MEDIA_AUDIO:
    case SDP_MEDIA_VIDEO:
    case SDP_MEDIA_APPLICATION:
        media = gsmsdp_get_new_media(dcb_p, media_cap->type, level);
        if (media == NULL) {
            /* should not happen */
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"no media entry available",
                        dcb_p->line, dcb_p->call_id, fname);
            return (NULL);
        }

        /* set capabilities */
        gsmsdp_set_media_capability(media, media_cap);

        /* associate this media line to the capability entry */
        media->cap_index = cap_index; /* keep the media cap entry index */

        /* override the direction for special feature */
        gsmsdp_feature_overide_direction(dcb_p, media);
        if (media->support_direction == SDP_DIRECTION_INACTIVE) {
            GSM_DEBUG(DEB_L_C_F_PREFIX"feature overrides direction to inactive"
                      " no media added\n",
                      DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));

            /*
             * For an answer, SDP_DIRECTION_INACTIVE will now add an m=
             * line but it is disabled.
             */
            if (!offer) {
                media->src_port = 0;
            } else {
              gsmsdp_remove_media(dcb_p, media);
              return (NULL);
            }
        }

        if (media->support_direction != SDP_DIRECTION_INACTIVE) {
          /*
           * Get the local RTP port. The src port will be set in the dcb
           * within the call to cc_call_action(CC_ACTION_OPEN_RCV)
           */
          data.open_rcv.is_multicast = FALSE;
          data.open_rcv.listen_ip = ip_addr_invalid;
          data.open_rcv.port = 0;
          data.open_rcv.keep = FALSE;
          /*
           * Indicate type of media (audio/video etc) becase some for
           * supporting video over vieo, the port is obtained from other
           * entity.
           */
          data.open_rcv.media_type = media->type;
          data.open_rcv.media_refid = media->refid;
          if (cc_call_action(dcb_p->call_id, dcb_p->line,
                             CC_ACTION_OPEN_RCV,
                             &data) != CC_RC_SUCCESS) {
              GSM_ERR_MSG(GSM_L_C_F_PREFIX"allocate rx port failed",
                          dcb_p->line, dcb_p->call_id, fname);
              gsmsdp_remove_media(dcb_p, media);
              return (NULL);
          }

          /* allocate port successful, save the port */

          media->src_port = data.open_rcv.port;

          if(media_cap->type == SDP_MEDIA_APPLICATION) {
            config_get_value(CFGID_SCTP_PORT, &sctp_port, sizeof(sctp_port));
            media->local_datachannel_port = sctp_port;
          }

          /*
           * Setup the local source address.
           */
          if (addr_type == CPR_IP_ADDR_IPV6) {
              gsmsdp_get_local_source_v6_address(media);
          } else if (addr_type == CPR_IP_ADDR_IPV4) {
              gsmsdp_get_local_source_v4_address(media);
          } else {
              GSM_ERR_MSG(GSM_L_C_F_PREFIX"invalid IP address mode",
                          dcb_p->line, dcb_p->call_id, fname);
              gsmsdp_remove_media(dcb_p, media);
              return (NULL);
          }

        }

        /*
         * Initialize the media transport for RTP or SRTP (or do not thing
         * and leave to the gsmsdp_update_local_sdp_media to set default)
         */
        gsmsdp_init_sdp_media_transport(dcb_p, dcb_p->sdp->src_sdp, media);


        gsmsdp_update_local_sdp_media(dcb_p, dcb_p->sdp, TRUE, media,
                                          media->transport);

        if (media->support_direction != SDP_DIRECTION_INACTIVE) {

          gsmsdp_set_local_sdp_direction(dcb_p, media, media->direction);

          /* Add supported rtcp-fb types */
          if (media_cap->type == SDP_MEDIA_VIDEO) {
              gsmsdp_add_rtcp_fb (level, dcb_p->sdp->src_sdp, RTP_NONE, /* RTP_NONE == all */
                  sdp_rtcp_fb_nack_to_bitmap(SDP_RTCP_FB_NACK_BASIC) |
                  sdp_rtcp_fb_nack_to_bitmap(SDP_RTCP_FB_NACK_PLI) |
                  sdp_rtcp_fb_ccm_to_bitmap(SDP_RTCP_FB_CCM_FIR));
          }
          /* Add supported audio level rtp extension */
          if (media_cap->type == SDP_MEDIA_AUDIO) {
              gsmsdp_set_extmap_attribute(level, dcb_p->sdp->src_sdp, 1,
                  SDP_EXTMAP_AUDIO_LEVEL);
          }

          /* Add a=setup attribute */
          gsmsdp_set_setup_attribute(level, dcb_p->sdp->src_sdp, media->setup);
          /*
           * wait until here to set ICE candidates as SDP is now initialized
           */
          for (i=0; i<media->candidate_ct; i++) {
            gsmsdp_set_ice_attribute (SDP_ATTR_ICE_CANDIDATE, level, dcb_p->sdp->src_sdp, media->candidatesp[i]);
          }

          config_get_value(CFGID_RTCPMUX, &rtcpmux, sizeof(rtcpmux));
          if (SDP_MEDIA_APPLICATION != media_cap->type && rtcpmux) {
            gsmsdp_set_rtcp_mux_attribute (SDP_ATTR_RTCP_MUX, level, dcb_p->sdp->src_sdp, TRUE);
          }


          /*
           * Since we are initiating an initial offer and opening a
           * receive port, store initial media settings.
           */
          media->previous_sdp.avt_payload_type = media->avt_payload_type;
          media->previous_sdp.direction = media->direction;
          media->previous_sdp.packetization_period = media->packetization_period;
          gsmsdp_copy_payloads_to_previous_sdp(media);
          break;
        }

    default:
        /* Unsupported media type, not added */
        GSM_DEBUG(DEB_L_C_F_PREFIX"media type %d is not supported",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), media_cap->type);
        break;
    }
    return (media);
}

/*
 * gsmsdp_create_local_sdp
 *
 * Description:
 *
 * Parameters:
 *
 * dcb_p - Pointer to the DCB whose local SDP is to be updated.
 * force_streams_enabled - temporarily generate SDP even when no
 *                         streams are added
 *
 * returns    cc_causes_t
 *            CC_CAUSE_OK - indicates success
 *            Any other code- indicates failure
 */
cc_causes_t
gsmsdp_create_local_sdp (fsmdef_dcb_t *dcb_p, boolean force_streams_enabled,
                         boolean audio, boolean video, boolean data, boolean offer)
{
    static const char fname[] = "gsmsdp_create_local_sdp";
    uint16_t        level;
    const cc_media_cap_table_t *media_cap_tbl;
    const cc_media_cap_t       *media_cap;
    cpr_ip_mode_e   ip_mode;
    uint8_t         cap_index;
    fsmdef_media_t  *media;
    boolean         has_audio;
    int             sdpmode = 0;
    boolean         media_enabled;
    cc_causes_t     cause;

    cause = gsmsdp_init_local_sdp(dcb_p->peerconnection, &(dcb_p->sdp));
    if ( cause != CC_CAUSE_OK) {
      return cause;
    }

    config_get_value(CFGID_SDPMODE, &sdpmode, sizeof(sdpmode));

    dcb_p->src_sdp_version = 0;

    media_cap_tbl = dcb_p->media_cap_tbl;

    if (media_cap_tbl == NULL) {
        /* should not happen */
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"no media capbility available",
                    dcb_p->line, dcb_p->call_id, fname);
        return (CC_CAUSE_NO_MEDIA_CAPABILITY);
    }

    media_cap = &media_cap_tbl->cap[0];
    level = 0;
    for (cap_index = 0; cap_index < CC_MAX_MEDIA_CAP-1; cap_index++) {

        /* Build local m lines based on m lines that were in the offered SDP */
        media_enabled = TRUE;
        if (FALSE == audio && SDP_MEDIA_AUDIO == media_cap->type) {
            media_enabled = FALSE;
        } else if (FALSE == video && SDP_MEDIA_VIDEO == media_cap->type) {
            media_enabled = FALSE;
        } else if (FALSE == data && SDP_MEDIA_APPLICATION == media_cap->type) {
            media_enabled = FALSE;
        }

        /*
         * Add each enabled media line to the SDP
         */
        if (media_enabled && ( media_cap->enabled || force_streams_enabled)) {
            level = level + 1;  /* next level */

            /* Only audio and video use two ICE components */
            if (media_cap->type != SDP_MEDIA_AUDIO &&
                media_cap->type != SDP_MEDIA_VIDEO) {
                vcmDisableRtcpComponent(dcb_p->peerconnection, level);
            }

            ip_mode = platform_get_ip_address_mode();
            if (ip_mode >= CPR_IP_MODE_IPV6) {
                if (gsmsdp_add_media_line(dcb_p, media_cap, cap_index,
                                          level, CPR_IP_ADDR_IPV6, offer)
                    == NULL) {
                    /* fail to add a media line, go back one level */
                    level = level - 1;
                }

                if (ip_mode == CPR_IP_MODE_DUAL) {
                    level = level + 1;  /* next level */
                    if (gsmsdp_add_media_line(dcb_p, media_cap, cap_index,
                                              level, CPR_IP_ADDR_IPV4, offer) ==
                        NULL) {
                        /* fail to add a media line, go back one level */
                        level = level - 1;
                    }
                }
            } else {
                if (gsmsdp_add_media_line(dcb_p, media_cap, cap_index, level,
                                          CPR_IP_ADDR_IPV4, offer) == NULL) {
                    /* fail to add a media line, go back one level */
                    level = level - 1;
                }
            }
        }
        /* next capability */
        media_cap++;
    }

    if (level == 0) {
        /*
         * Did not find media line for the SDP and we do not
         * support SDP without any media line.
         */
        GSM_ERR_MSG(GSM_L_C_F_PREFIX"no media line for SDP",
                    dcb_p->line, dcb_p->call_id, fname);
        return (CC_CAUSE_NO_M_LINE);
    }

    /*
     *
     * This is a suitable place to add ice ufrag and pwd to the SDP
     */

    if (dcb_p->ice_ufrag)
        gsmsdp_set_ice_attribute (SDP_ATTR_ICE_UFRAG, SDP_SESSION_LEVEL, dcb_p->sdp->src_sdp, dcb_p->ice_ufrag);
    if (dcb_p->ice_pwd)
        gsmsdp_set_ice_attribute (SDP_ATTR_ICE_PWD, SDP_SESSION_LEVEL, dcb_p->sdp->src_sdp, dcb_p->ice_pwd);

    if(strlen(dcb_p->digest_alg)  > 0)
        gsmsdp_set_dtls_fingerprint_attribute (SDP_ATTR_DTLS_FINGERPRINT, SDP_SESSION_LEVEL,
            dcb_p->sdp->src_sdp, dcb_p->digest_alg, dcb_p->digest);

    if (!sdpmode) {

       /*
        * Ensure that there is at least one audio line.
        */
        has_audio = FALSE;
        GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
            if (media->type == SDP_MEDIA_AUDIO) {
                has_audio = TRUE; /* found one audio line, done */
                break;
            }
        }
        if (!has_audio) {
            /* No audio, do not allow */
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"no audio media line for SDP",
                    dcb_p->line, dcb_p->call_id, fname);
            return (CC_CAUSE_NO_AUDIO);
        }
    }

    return CC_CAUSE_OK;
}

/**
 * The function creates a SDP that contains phone's current
 * capability for an option.
 *
 * @param[in/out]sdp_pp     - pointer to a pointer to cc_sdp_t to return
 *                            the created SDP.
 * @return                  none.
 * @pre              (sdp_pp not_eq NULL)
 */
void
gsmsdp_create_options_sdp (cc_sdp_t ** sdp_pp)
{
    cc_sdp_t *sdp_p;

    /* This empty string represents to associated peerconnection object */
    if (gsmsdp_init_local_sdp("", sdp_pp) != CC_CAUSE_OK) {
        return;
    }

    sdp_p = *sdp_pp;

    /*
     * Insert media line at level 1.
     */
    if (sdp_insert_media_line(sdp_p->src_sdp, 1) != SDP_SUCCESS) {
        // Error
        return;
    }

    (void) sdp_set_media_type(sdp_p->src_sdp, 1, SDP_MEDIA_AUDIO);
    (void) sdp_set_media_portnum(sdp_p->src_sdp, 1, 0, 0);
    gsmsdp_set_media_transport_for_option(sdp_p->src_sdp, 1);

    /*
     * Add all supported media formats to the local sdp.
     */
    gsmsdp_add_default_audio_formats_to_local_sdp(NULL, sdp_p, NULL);

    /* Add Video m line if video caps are enabled */
    if ( g_media_table.cap[CC_VIDEO_1].enabled == TRUE ) {
        if (sdp_insert_media_line(sdp_p->src_sdp, 2) != SDP_SUCCESS) {
            // Error
            return;
        }

        (void) sdp_set_media_type(sdp_p->src_sdp, 2, SDP_MEDIA_VIDEO);
        (void) sdp_set_media_portnum(sdp_p->src_sdp, 2, 0, 0);
        gsmsdp_set_media_transport_for_option(sdp_p->src_sdp, 2);

        gsmsdp_add_default_video_formats_to_local_sdp(NULL, sdp_p, NULL);
    }
}

/**
 * The function checks and removes media capability for the media
 * lines that is to be removed.
 *
 * @param[in]dcb_p   - Pointer to DCB
 *
 * @return           TRUE  - if there is a media line removed.
 *                   FALSE - if there is no media line to remove.
 *
 * @pre              (dcb_p not_eq NULL)
 */
static boolean
gsmsdp_check_remove_local_sdp_media (fsmdef_dcb_t *dcb_p)
{
    static const char fname[] = "gsmsdp_check_remove_local_sdp_media";
    fsmdef_media_t             *media, *media_to_remove;
    const cc_media_cap_t       *media_cap;
    boolean                    removed = FALSE;

    media = GSMSDP_FIRST_MEDIA_ENTRY(dcb_p);
    while (media) {
        media_cap = gsmsdp_get_media_cap_entry_by_index(media->cap_index,dcb_p);
        if (media_cap != NULL) {
            /* found the corresponding capability of the media line */
            if (!media_cap->enabled) {
                GSM_DEBUG(DEB_L_C_F_PREFIX"remove media at level %d",
                          DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), media->level);
                /* set the media line to unused */
                gsmsdp_add_unsupported_stream_to_local_sdp(dcb_p->sdp,
                                                           media->level);
                /*
                 * remember the media to remove and get the next media to
                 * work on before removing this media off the linked list.
                 */
                media_to_remove = media;
                media = GSMSDP_NEXT_MEDIA_ENTRY(media);

                /* remove the media from the list */
                gsmsdp_remove_media(dcb_p, media_to_remove);
                removed = TRUE;
                continue;
            }
        }
        media = GSMSDP_NEXT_MEDIA_ENTRY(media);
    }
    return (removed);
}

/**
 * The function checks and adds media capability for the media
 * lines that is to be added.
 *
 * @param[in]dcb_p   - Pointer to DCB
 * @param[in]hold    - TRUE indicates the newly media line
 *                     should have direction that indicates hold.
 *
 * @return           TRUE  - if there is a media line added.
 *                   FALSE - if there is no media line to added.
 *
 * @pre              (dcb_p not_eq NULL)
 */
static boolean
gsmsdp_check_add_local_sdp_media (fsmdef_dcb_t *dcb_p, boolean hold)
{
    static const char fname[] = "gsmsdp_check_add_local_sdp_media";
    fsmdef_media_t             *media;
    const cc_media_cap_t       *media_cap;
    uint8_t                    cap_index;
    uint16_t                   num_m_lines, level_to_use;
    void                       *src_sdp;
    boolean                    need_mix = FALSE;
    boolean                    added = FALSE;
    cpr_ip_mode_e              ip_mode;
    cpr_ip_type                ip_addr_type[2]; /* for 2 IP address types */
    uint16_t                   i, num_ip_addrs;

    if (fsmcnf_get_ccb_by_call_id(dcb_p->call_id) != NULL) {
        /*
         * This call is part of a local conference. The mixing
         * support will be needed for additional media line.
         * If platform does not have capability to support mixing
         * of a particular media type for the local conference, either
         * leg in the conference will not see addition media line
         * added.
         */
        need_mix = TRUE;
    }

    /*
     * Find new media entries to be added.
     */
    src_sdp = dcb_p->sdp ? dcb_p->sdp->src_sdp : NULL;
    for (cap_index = 0; cap_index < CC_MAX_MEDIA_CAP; cap_index++) {
        media_cap = gsmsdp_get_media_cap_entry_by_index(cap_index, dcb_p);
        if (media_cap == NULL) {
            GSM_ERR_MSG(GSM_L_C_F_PREFIX"no media capbility available",
                        dcb_p->line, dcb_p->call_id, fname);
            continue;
        }
        if (!media_cap->enabled) {
            /* this entry is disabled, skip it */
            continue;
        }
        media = gsmsdp_find_media_by_cap_index(dcb_p, cap_index);
        if (media != NULL) {
            /* this media entry exists, skip it */
            continue;
        }

        /*
         * This is a new entry the capability table to be added.
         */
        if (CC_IS_AUDIO(cap_index) && need_mix) {
            if (!gsmsdp_platform_addition_mix(dcb_p, media_cap->type)) {
                /* platform can not support additional mixing of this type */
                GSM_DEBUG(DEB_L_C_F_PREFIX"no support addition mixing for %d "
                          "media type\n",
                          DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname),
						  media_cap->type);
                continue;
            }
        }

        /*
         * It depends on current address mode, if the mode is dual mode then
         * we need to add 2 media lines.
         */
        ip_mode = platform_get_ip_address_mode();
        switch (ip_mode) {
        case CPR_IP_MODE_DUAL:
            /* add both addresses, IPV6 line is first as it is prefered */
            num_ip_addrs = 2;
            ip_addr_type[0] = CPR_IP_ADDR_IPV6;
            ip_addr_type[1] = CPR_IP_ADDR_IPV4;
            break;
        case CPR_IP_MODE_IPV6:
            /* add IPV6 address only */
            num_ip_addrs = 1;
            ip_addr_type[0] = CPR_IP_ADDR_IPV6;
            break;
        default:
            /* add IPV4 address only */
            num_ip_addrs = 1;
            ip_addr_type[0] = CPR_IP_ADDR_IPV4;
            break;
        }
        /* add media line or lines */
        for (i = 0; i < num_ip_addrs; i++) {
            /*
             * This is a new stream to add, find an unused media line
             * in the SDP. Find the unused media line that has the same
             * media type as the one to be added. The RFC-3264 allows reuse
             * any unused slot in the SDP body but it was recomended to use
             * the same type to increase the chance of interoperability by
             * using the unuse slot that has the same media type.
             */
            level_to_use = gsmsdp_find_unused_media_line_with_type(src_sdp,
                               media_cap->type);
            if (level_to_use == 0) {
                /* no empty slot is found, add a new line to the SDP */
                num_m_lines  = sdp_get_num_media_lines(src_sdp);
                level_to_use = num_m_lines + 1;
            }
            GSM_DEBUG(DEB_L_C_F_PREFIX"add media at level %d",
                      DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), level_to_use);

            /* add a new media */
            media = gsmsdp_add_media_line(dcb_p, media_cap, cap_index,
                                          level_to_use, ip_addr_type[i], FALSE);
            if (media != NULL) {
                /* successfully add a new media line */
                if (hold) {
                    /* this new media needs to be sent out with hold */
                    gsmsdp_set_local_hold_sdp(dcb_p, media);
                }
                added = TRUE;
            } else {
                GSM_ERR_MSG(GSM_L_C_F_PREFIX"Unable to add a new media",
                            dcb_p->line, dcb_p->call_id, fname);
            }
        }
    }
    return (added);
}

/**
 * The function checks support direction changes and updates the support
 * direction of media lines.
 *
 * @param[in]dcb_p         - Pointer to DCB
 * @param[in]no_sdp_update - TRUE indicates do not update SDP.
 *
 * @return           TRUE  - if there is a media line support direction
 *                           changes.
 *                   FALSE - if there is no media line that has
 *                           support direction change.
 *
 * @pre              (dcb_p not_eq NULL)
 */
static boolean
gsmsdp_check_direction_change_local_sdp_media (fsmdef_dcb_t *dcb_p,
                                               boolean no_sdp_update)
{
    static const char fname[] = "gsmsdp_check_direction_change_local_sdp_media";
    fsmdef_media_t             *media;
    const cc_media_cap_t       *media_cap;
    boolean                    direction_change = FALSE;
    sdp_direction_e            save_supported_direction;

    media = GSMSDP_FIRST_MEDIA_ENTRY(dcb_p);
    while (media) {
        media_cap = gsmsdp_get_media_cap_entry_by_index(media->cap_index, dcb_p);
        if (media_cap != NULL) {
            if (media->support_direction !=
                media_cap->support_direction) {
                /*
                 * There is a possibility that supported direction has
                 * been overrided due to some feature. Check to see
                 * the supported direction remains the same after override
                 * take place. If it is different then there is a direction
                 * change.
                 */
                save_supported_direction = media->support_direction;
                media->support_direction = media_cap->support_direction;
                gsmsdp_feature_overide_direction(dcb_p, media);
                if (media->support_direction == save_supported_direction) {
                    /* nothing change after override */
                } else {
                    /* there is no override, this is a change */
                    direction_change = TRUE;
                }
                if (direction_change) {
                    /* Support direction changed */
                    GSM_DEBUG(DEB_L_C_F_PREFIX"change support direction at level %d"
                              " from %d to %d\n",
							  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname),
							  media->level, media->support_direction,
                              media_cap->support_direction);
                    if (no_sdp_update) {
                        /*
                         * The caller does not want to update SDP.
                         */
                        media->direction = media_cap->support_direction;
                    } else {
                        /*
                         * Need to update direction in the SDP.
                         * The direction in the media structure will
                         * be set by the gsmsdp_set_local_sdp_direction.
                         */
                        gsmsdp_set_local_sdp_direction(dcb_p, media,
                                               media->support_direction);
                    }
                }
            }
        }
        media = GSMSDP_NEXT_MEDIA_ENTRY(media);
    }
    return (direction_change);
}

/**
 * The function resets media specified takes the hold state into account
 *
 * @param[in]dcb_p   - Pointer to DCB
 * @param[in]media   - Media to be updated
 * @param[in]hold    - Set media line will be set to hold.
 */
static void gsmsdp_reset_media(fsmdef_dcb_t *dcb_p, fsmdef_media_t *media, boolean hold){
    gsmsdp_reset_local_sdp_media(dcb_p, media, hold);
    if (hold) {
        gsmsdp_set_local_hold_sdp(dcb_p, media);
    } else {
        gsmsdp_set_local_resume_sdp(dcb_p, media);
    }
}

/**
 *
 * This functions checks if the media IP Address has changed
 *
 * Convert the configMedia IP String to Ip address format,
 * check if it is valid and then compare to current media source
 * address, if the address is valid and the media source and
 * config media ip differ, then
 *
 *  1) Stop the media to close the socket
 *  2) Set flag to true, to initiate re-invite
 *  3) Update the media src addr
 *  4) Update the c= line to reflect the new IP address source
 *
 * @param[in]dcb_p   - Pointer to DCB
 * @return         TRUE  - if IP changed
 *                 FALSE - if no no IP changed
 *
 * @pre              (dcb_p not_eq NULL)
 */
boolean
gsmsdp_media_ip_changed (fsmdef_dcb_t *dcb_p)
{
    static const char     fname[] = "gsmsdp_media_ip_changed";
    boolean               ip_changed = FALSE;
    cpr_ip_addr_t         addr ;
    char                  curr_media_ip[MAX_IPADDR_STR_LEN];
    char                  addr_str[MAX_IPADDR_STR_LEN];
    fsmdef_media_t        *media;

    /*
     * Check if media IP has changed
     */
    init_empty_str(curr_media_ip);
    config_get_value(CFGID_MEDIA_IP_ADDR, curr_media_ip,
                        MAX_IPADDR_STR_LEN);
    if (!is_empty_str(curr_media_ip)) {
        str2ip(curr_media_ip, &addr);
        util_ntohl(&addr, &addr);
        GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
            if ((util_check_if_ip_valid(&media->src_addr) == TRUE) &&
                (util_check_if_ip_valid(&addr) == TRUE) &&
                (util_compare_ip(&media->src_addr, &addr) == FALSE)) {
                ipaddr2dotted(curr_media_ip, &media->src_addr); // for logging

                (void)cc_call_action(dcb_p->call_id, dcb_p->line,
                          CC_ACTION_STOP_MEDIA,
                          NULL);
                ip_changed = TRUE;
                media->src_addr = addr;
                if (dcb_p->sdp != NULL) {
                    gsmsdp_set_connection_address(dcb_p->sdp->src_sdp,
                            media->level,
                            dcb_p->ice_default_candidate_addr);
                }
                ipaddr2dotted(addr_str, &media->src_addr);  // for logging
                GSM_ERR_MSG("%s MEDIA IP_CHANGED: after Update IP %s"\
                            " before %s" ,fname, addr_str, curr_media_ip );
            }
        }
    }

    return (ip_changed);
}

/**
 * this function will check whether the media ip addres in local sdp is same as to
 * media IP provided by application. If IP differs, then re-INVITE request is posted.
 */
boolean is_gsmsdp_media_ip_updated_to_latest( fsmdef_dcb_t * dcb ) {
    cpr_ip_addr_t         media_ip_in_host_order ;
    char                  curr_media_ip[MAX_IPADDR_STR_LEN];
    fsmdef_media_t        *media;

    init_empty_str(curr_media_ip);
    config_get_value(CFGID_MEDIA_IP_ADDR, curr_media_ip, MAX_IPADDR_STR_LEN);
    if (is_empty_str(curr_media_ip) == FALSE) {
        str2ip(curr_media_ip, &media_ip_in_host_order);
        util_ntohl(&media_ip_in_host_order, &media_ip_in_host_order);

        GSMSDP_FOR_ALL_MEDIA(media, dcb) {
            if (util_check_if_ip_valid(&media->src_addr) == TRUE) {
                if (util_compare_ip(&media->src_addr, &media_ip_in_host_order) == FALSE) {
                    return FALSE;
                }
            }
        }
    }
    return TRUE;
}


/**
 *
 * The function checks for media capability changes and updates the
 * local SDP for the changes. The function also provides a couple of options
 * 1)  reset the unchange media lines to initialize the codec list,
 * crypto etc. and 2) an option to update media directions for all
 * media lines for hold. 3)
 *
 * @param[in]dcb_p   - Pointer to DCB
 * @param[in]reset   - Reset the unchanged media lines to include all
 *                     codecs etc. again.
 * @param[in]hold    - Set media line will be set to hold.
 *
 * @return         TRUE  - if media changes occur.
 *                 FALSE - if no media change occur.
 *
 * @pre              (dcb_p not_eq NULL)
 */
boolean
gsmsdp_update_local_sdp_media_capability (fsmdef_dcb_t *dcb_p, boolean reset,
                                          boolean hold)
{
    static const char     fname[] = "gsmsdp_update_local_sdp_media_capability";
    fsmdef_media_t             *media;
    boolean                    change_found = FALSE;
    boolean                    check_for_change = FALSE;

    change_found = gsmsdp_media_ip_changed(dcb_p);

    /*
     * check to see if media capability table has changed, by checking
     * the ID.
     */
    if ((g_media_table.id != dcb_p->media_cap_tbl->id) || reset) {

            /*
             * capabilty table ID different or we are doing a reset for
             * the full offer again, need to check for various changes.
         * Update capabilities to match platform caps
         */
            check_for_change = TRUE;
        }

    /*
     * Find any capability changes. The changes allowed are:
     * 1) media entry is disabled (to be removed).
     * 2) supported direction change.
     * 3) new media entry is enabled (to be added) keep it the last one.
     *
     * Find a media to be removed first so that if there is another
     * media line to add then there might be an unused a media line in the
     * SDP to use.
     */
    if (check_for_change && gsmsdp_check_remove_local_sdp_media(dcb_p)) {
        /* there were some media lines removed */
        change_found = TRUE;
    }

    /*
     * Find media lines that may have direction changes
     */
    if ( check_for_change &&
         gsmsdp_check_direction_change_local_sdp_media(dcb_p, reset)) {
        /* there were media lines that directions changes */
        change_found = TRUE;
    }

    /*
     * Reset all the existing media lines to have full codec etc.
     * again if the caller requested.
     */
    if (reset) {
        GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
            gsmsdp_reset_media(dcb_p, media, hold);
        }
    }

    /*
     * Find new media entries to be added.
     */
    if ( check_for_change && gsmsdp_check_add_local_sdp_media(dcb_p, hold)) {
        change_found = TRUE;
    }

    if (change_found) {
        GSM_DEBUG(DEB_L_C_F_PREFIX"media capability change found",
                  DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname));
    }


    /* report back any changes made */
    return (change_found);
}

/*
 * gsmsdp_reset_local_sdp_media
 *
 * Description:
 *
 * This function initializes the media portion of the local sdp. The following
 * lines are initialized.
 *
 * m= line <media><port><transport><fmt list>
 * a= session level line <media direction>
 *
 * Parameters:
 *
 * dcb_p - Pointer to DCB whose local SDP media is to be initialized
 * media - Pointer to fsmdef_media_t for the media entry of the SDP.
 *         If the value is NULL, it indicates all medie entries i the
 *         dcb is reset.
 * hold  - Boolean indicating if SDP is being initialized for sending hold
 *
 */
void
gsmsdp_reset_local_sdp_media (fsmdef_dcb_t *dcb_p, fsmdef_media_t *media,
                              boolean hold)
{
    fsmdef_media_t *start_media, *end_media;

    if (media == NULL) {
        /* NULL value of the given media indicates for all media */
        start_media = GSMSDP_FIRST_MEDIA_ENTRY(dcb_p);
        end_media   = NULL; /* NULL means till the end of the list */
    } else {
        /* given media, uses the provided media */
        start_media = media;
        end_media   = media;
    }

    GSMSDP_FOR_MEDIA_LIST(media, start_media, end_media, dcb_p) {
        if (!GSMSDP_MEDIA_ENABLED(media)) {
            continue;
        }

        /*
         * Reset media transport in preparation for hold or resume.
         * It is possible that transport media may
         * change from the current media transport (for SRTP re-offer
         * to a different end point).
         */
        gsmsdp_reset_sdp_media_transport(dcb_p, dcb_p->sdp ? dcb_p->sdp->src_sdp : NULL,
                                             media, hold);

        gsmsdp_update_local_sdp_media(dcb_p, dcb_p->sdp, TRUE, media,
                                          media->transport);


        /*
         * a= line
         * If hold is being signaled, do not alter the media direction. This
         * will be taken care of by the state machine handler function.
         */
        if (!hold) {
            /*
             * We are not locally held, set direction to the supported
             * direction.
             */
            gsmsdp_set_local_sdp_direction(dcb_p, media,
                                           media->support_direction);
        }
    }
}

/*
 * gsmsdp_set_local_hold_sdp
 *
 * Description:
 *
 * Manipulates the local SDP of the specified DCB to indicate hold
 * to the far end.
 *
 * Parameters:
 *
 * dcb_p - Pointer to the DCB whose SDP is to be manipulated.
 * media - Pointer to the fsmdef_media_t for the current media entry.
 *         If the value is NULL, it indicates all medie entries i the
 *         dcb is reset.
 */
void
gsmsdp_set_local_hold_sdp (fsmdef_dcb_t *dcb_p, fsmdef_media_t *media)
{
    int             old_style_hold = 0;
    fsmdef_media_t *start_media, *end_media;

    if (media == NULL) {
        /* NULL value of the given media indicates for all media */
        start_media = GSMSDP_FIRST_MEDIA_ENTRY(dcb_p);
        end_media   = NULL; /* NULL means till the end of the list */
    } else {
        /* given media, uses the provided media */
        start_media = media;
        end_media   = media;
    }

    GSMSDP_FOR_MEDIA_LIST(media, start_media, end_media, dcb_p) {
        if (!GSMSDP_MEDIA_ENABLED(media)) {
            continue;
        }
        /*
         * Check if configuration indicates that the old style hold should be
         * signaled per RFC 2543. That is, c=0.0.0.0. Although
         * 2543 does not speak to the SDP direction attribute, we set
         * the direction to INACTIVE to be consistent with the connection
         * address setting.
         */
        config_get_value(CFGID_2543_HOLD, &old_style_hold,
                         sizeof(old_style_hold));
        if (old_style_hold) {
            gsmsdp_set_2543_hold_sdp(dcb_p, media->level);
            gsmsdp_set_local_sdp_direction(dcb_p, media,
                                           SDP_DIRECTION_INACTIVE);
        } else {
            /*
             * RFC3264 states that hold is signaled by setting the media
             * direction attribute to SENDONLY if in SENDRECV mode.
             * INACTIVE if RECVONLY mode (mutual hold).
             */
            if (media->direction == SDP_DIRECTION_SENDRECV ||
                media->direction == SDP_DIRECTION_SENDONLY) {
                gsmsdp_set_local_sdp_direction(dcb_p, media,
                                               SDP_DIRECTION_SENDONLY);
            } else {
                gsmsdp_set_local_sdp_direction(dcb_p, media,
                                               SDP_DIRECTION_INACTIVE);
            }
        }
    }
}

/*
 * gsmsdp_set_local_resume_sdp
 *
 * Description:
 *
 * Manipulates the local SDP of the specified DCB to indicate hold
 * to the far end.
 *
 * Parameters:
 *
 * dcb_p - Pointer to the DCB whose SDP is to be manipulated.
 * media - Pointer to the fsmdef_media_t for the current media entry.
 *
 */
void
gsmsdp_set_local_resume_sdp (fsmdef_dcb_t *dcb_p, fsmdef_media_t *media)
{
    fsmdef_media_t *start_media, *end_media;

    if (media == NULL) {
        /* NULL value of the given media indicates for all media */
        start_media = GSMSDP_FIRST_MEDIA_ENTRY(dcb_p);
        end_media   = NULL; /* NULL means till the end of the list */
    } else {
        /* given media, uses the provided media */
        start_media = media;
        end_media   = media;
    }

    GSMSDP_FOR_MEDIA_LIST(media, start_media, end_media, dcb_p) {
        if (!GSMSDP_MEDIA_ENABLED(media)) {
            continue;
        }
        /*
         * We are not locally held, set direction to the supported
         * direction.
         */
        gsmsdp_set_local_sdp_direction(dcb_p, media, media->support_direction);
    }
}

/*
 * gsmsdp_encode_sdp
 *
 * Description:
 * The function encodes SDP from the internal SDP representation
 * to the SDP body to be sent out.
 *
 * Parameters:
 * sdp_p    - pointer to the internal SDP info. block.
 * msg_body - pointer to the msg body info. block.
 *
 * Returns:
 * cc_causes_t to indicate failure or success.
 */
cc_causes_t
gsmsdp_encode_sdp (cc_sdp_t *sdp_p, cc_msgbody_info_t *msg_body)
{
    char           *sdp_body;
    cc_msgbody_t   *part;
    uint32_t        body_length;

    if (!msg_body || !sdp_p) {
        return CC_CAUSE_NULL_POINTER;
    }

    /* Support single SDP encoding for now */
    sdp_body = sipsdp_write_to_buf(sdp_p->src_sdp, &body_length);

    if (sdp_body == NULL) {
        return CC_CAUSE_SDP_ENCODE_FAILED;
    } else if (body_length == 0) {
        cpr_free(sdp_body);
        return CC_CAUSE_SDP_ENCODE_FAILED;
    }

    /* Clear off the bodies info */
    cc_initialize_msg_body_parts_info(msg_body);

    /* Set up for one SDP entry */
    msg_body->num_parts = 1;
    msg_body->content_type = cc_content_type_SDP;
    part = &msg_body->parts[0];
    part->body = sdp_body;
    part->body_length = body_length;
    part->content_type = cc_content_type_SDP;
    part->content_disposition.required_handling = FALSE;
    part->content_disposition.disposition = cc_disposition_session;
    part->content_id = NULL;
    return CC_CAUSE_OK;
}

/*
 * gsmsdp_encode_sdp_and_update_version
 *
 * Description:
 * The function encodes SDP from the internal SDP representation
 * to the SDP body to be sent out.  It also post-increments the owner
 * version number to prepare for the next SDP to be sent out.
 *
 * Parameters:
 * sdp_p    - pointer to DCB whose local SDP is to be encoded.
 * msg_body - pointer to the msg body info. block.
 *
 * Returns:
 * cc_causes_t to indicate failure or success.
 */
cc_causes_t
gsmsdp_encode_sdp_and_update_version (fsmdef_dcb_t *dcb_p, cc_msgbody_info_t *msg_body)
{
    char version_str[GSMSDP_VERSION_STR_LEN];
    cc_causes_t cause;

    snprintf(version_str, sizeof(version_str), "%d", dcb_p->src_sdp_version);

    if ( dcb_p->sdp == NULL || dcb_p->sdp->src_sdp == NULL ) {
        cause = gsmsdp_init_local_sdp(dcb_p->peerconnection, &(dcb_p->sdp));
        if ( cause != CC_CAUSE_OK) {
            return cause;
        }
    }
    (void) sdp_set_owner_version(dcb_p->sdp->src_sdp, version_str);

    if (gsmsdp_encode_sdp(dcb_p->sdp, msg_body) != CC_CAUSE_OK) {
        return CC_CAUSE_SDP_ENCODE_FAILED;
    }

    dcb_p->src_sdp_version++;
    return CC_CAUSE_OK;
}

/*
 * gsmsdp_get_sdp
 *
 * Description:
 * The function searches the SDP from the the all of msg. body parts.
 * All body parts having the content type SDP will be stored at the
 * given destination arrays. The number of SDP body are returned to
 * indicate the number of SDP bodies found. The function searches
 * the msg body in backward to order to form SDP in the destination array
 * to be from the highest to the lowest preferences.
 *
 * Parameters:
 * msg_body   - pointer to the incoming message body or cc_msgbody_info_t.
 * part_array - pointer to pointer of cc_msgbody_t to store the
 *              sorted order of the incoming SDP.
 * max_part   - the maximum number of SDP can be written to the
 *              part_array or the part_array size.
 * Returns:
 * The number of SDP parts found.
 */
static uint32_t
gsmsdp_get_sdp_body (cc_msgbody_info_t *msg_body,
                     cc_msgbody_t **part_array,
                     uint32_t max_parts)
{
    uint32_t      i, count;
    cc_msgbody_t *part;

    if ((msg_body == NULL) || (msg_body->num_parts == 0)) {
        /* No msg. body or no body parts in the msg. */
        return (0);
    }
    /*
     * Extract backward. The SDP are sent from the lowest
     * preference to the highest preference. The highest
     * preference will be extracted first into the given array
     * so that when we negotiate, the SDP we will attempt to
     * negotiate from the remote's highest to the lowest
     * preferences.
     */
    count = 0;
    part = &msg_body->parts[msg_body->num_parts - 1];
    for (i = 0; (i < msg_body->num_parts) && (i < max_parts); i++) {
        if (part->content_type == cc_content_type_SDP) {
            /* Found an SDP, keep the pointer to the part */
            *part_array = part; /* save pointer to SDP entry */
            part_array++;       /* next entry                */
            count++;
        }
        /* next one backward */
        part--;
    }
    /* return the number of SDP bodies found */
    return (count);
}

/*
 * gsmsdp_realloc_dest_sdp
 *
 * Description:
 * The function re-allocates the internal SDP info. block for the
 * remote or destination SDP. If there the SDP info. or the
 * SDP block does not exist, the new one is allocated. If SDP block
 * exists, the current one is released and is replaced by a new one.
 *
 * Parameters:
 * dcb_p - pointer to fsmdef_dcb_t.
 *
 * Returns:
 * cc_causes_t to indicate failure or success.
 */
static cc_causes_t
gsmsdp_realloc_dest_sdp (fsmdef_dcb_t *dcb_p)
{
    /* There are SDPs to process, prepare for parsing the SDP */
    if (dcb_p->sdp == NULL) {
        /* Create internal SDP information block with dest sdp block */
        sipsdp_src_dest_create(dcb_p->peerconnection,
            CCSIP_DEST_SDP_BIT, &dcb_p->sdp);
    } else {
        /*
         * SDP info. block exists, remove the previously received
         * remote or destination SDP and create a new one for
         * the new SDP.
         */
        if (dcb_p->sdp->dest_sdp) {
            sipsdp_src_dest_free(CCSIP_DEST_SDP_BIT, &dcb_p->sdp);
        }
        sipsdp_src_dest_create(dcb_p->peerconnection,
            CCSIP_DEST_SDP_BIT, &dcb_p->sdp);
    }

    /* No SDP info block and parsed control block are available */
    if ((dcb_p->sdp == NULL) || (dcb_p->sdp->dest_sdp == NULL)) {
        /* Unable to create internal SDP structure to parse SDP. */
        return CC_CAUSE_SDP_CREATE_FAILED;
    }
    return CC_CAUSE_OK;
}

/*
 * gsmsdp_negotiate_answer_sdp
 *
 * Description:
 *
 * Interface function used to negotiate an ANSWER SDP.
 *
 * Parameters:
 *
 * fcb_p - Pointer to the FCB containing the DCB whose local SDP is being negotiated.
 * msg_body - Pointer to the cc_msgbody_info_t that contain the remote
 *
 */
cc_causes_t
gsmsdp_negotiate_answer_sdp (fsm_fcb_t *fcb_p, cc_msgbody_info_t *msg_body)
{
    static const char fname[] = "gsmsdp_negotiate_answer_sdp";
    fsmdef_dcb_t *dcb_p = fcb_p->dcb;
    cc_msgbody_t *sdp_bodies[CC_MAX_BODY_PARTS];
    uint32_t      i, num_sdp_bodies;
    cc_causes_t   status;
    char         *sdp_body;

    /* Get just the SDP bodies */
    num_sdp_bodies = gsmsdp_get_sdp_body(msg_body, &sdp_bodies[0],
                                         CC_MAX_BODY_PARTS);
    GSM_DEBUG(DEB_F_PREFIX"",DEB_F_PREFIX_ARGS(GSM, fname));
    if (num_sdp_bodies == 0) {
        /*
         * Clear the call - we don't have any remote SDP info!
         */
        return CC_CAUSE_NO_SDP;
    }

    /* There are SDPs to process, prepare for parsing the SDP */
    status = gsmsdp_realloc_dest_sdp(dcb_p);
    if (status != CC_CAUSE_OK) {
        /* Unable to create internal SDP structure to parse SDP. */
        return status;
    }

    /*
     * Parse the SDP into internal structure,
     * now just parse one
     */
    status = CC_CAUSE_SDP_PARSE_FAILED;
    for (i = 0; (i < num_sdp_bodies); i++) {
        if ((sdp_bodies[i]->body != NULL) && (sdp_bodies[i]->body_length > 0)) {
            /* Found a body */
            sdp_body = sdp_bodies[i]->body;
            if (sdp_parse(dcb_p->sdp->dest_sdp, &sdp_body,
                          (uint16_t)sdp_bodies[i]->body_length)
                    == SDP_SUCCESS) {
                status = CC_CAUSE_OK;
                break;
            }
        }
    }
    if (status != CC_CAUSE_OK) {
        /* Error parsing SDP */
        return status;
    }

    gsmsdp_set_remote_sdp(dcb_p, dcb_p->sdp);

    status = gsmsdp_negotiate_media_lines(fcb_p, dcb_p->sdp, FALSE, FALSE, TRUE, TRUE);
    GSM_DEBUG(DEB_F_PREFIX"returns with %d",DEB_F_PREFIX_ARGS(GSM, fname), status);
    return (status);
}




/*
 * gsmsdp_negotiate_offer_sdp
 *
 * Description:
 *
 * Interface function used to negotiate an OFFER SDP.
 *
 * Parameters:
 *
 * fcb_p - Pointer to the FCB containing the DCB whose local SDP is being negotiated.
 * msg_body - Pointer to remote SDP body infostructure.
 * init - Boolean indicating if the local SDP should be initialized as if this is the
 *        first local SDP of this session.
 *
 */
cc_causes_t
gsmsdp_negotiate_offer_sdp (fsm_fcb_t *fcb_p,
  cc_msgbody_info_t *msg_body, boolean init)
{
    cc_causes_t status;
    fsmdef_dcb_t *dcb_p = fcb_p->dcb;

    status = gsmsdp_process_offer_sdp(fcb_p, msg_body, init);
    if (status != CC_CAUSE_OK)
       return status;

    /*
     * If a new error code has been added to sdp processing please make sure
     * the sip side is aware of it
     */
    status = gsmsdp_negotiate_media_lines(fcb_p, dcb_p->sdp, init, TRUE, FALSE, FALSE);
    return (status);
}


/*
 * gsmsdp_process_offer_sdp
 *
 * Description:
 *
 * Interface function used to process an OFFER SDP.
 * Does not negotiate.
 *
 * Parameters:
 *
 * fcb_p - Pointer to the FCB containing the DCB whose local SDP is being negotiated.
 * msg_body - Pointer to remote SDP body infostructure.
 * init - Boolean indicating if the local SDP should be initialized as if this is the
 *        first local SDP of this session.
 *
 */
cc_causes_t
gsmsdp_process_offer_sdp (fsm_fcb_t *fcb_p,
  cc_msgbody_info_t *msg_body, boolean init)
{
    static const char fname[] = "gsmsdp_process_offer_sdp";
    fsmdef_dcb_t *dcb_p = fcb_p->dcb;
    cc_causes_t   status;
    cc_msgbody_t *sdp_bodies[CC_MAX_BODY_PARTS];
    uint32_t      i, num_sdp_bodies;
    char         *sdp_body;

    /* Get just the SDP bodies */
    num_sdp_bodies = gsmsdp_get_sdp_body(msg_body, &sdp_bodies[0],
                                         CC_MAX_BODY_PARTS);
    GSM_DEBUG(DEB_L_C_F_PREFIX"Init is %d",
        DEB_L_C_F_PREFIX_ARGS(GSM, dcb_p->line, dcb_p->call_id, fname), init);
    if (num_sdp_bodies == 0) {
        /*
         * No remote SDP. So we will offer in our response and receive far end
         * answer in the ack. Only need to create local sdp if this is first offer
         * of a session. Otherwise, we will send what we have.
         */
        if (init) {
            status = gsmsdp_create_local_sdp(dcb_p, FALSE, TRUE,
                                             TRUE, TRUE, TRUE);
            if ( status != CC_CAUSE_OK) {
                return status;
            }
        } else {
            /*
             * Reset all media entries that we have to offer all capabilities
             */
           (void)gsmsdp_update_local_sdp_media_capability(dcb_p, TRUE, FALSE);
        }
        dcb_p->remote_sdp_in_ack = TRUE;
        return CC_CAUSE_OK;
    }

    /* There are SDPs to process, prepare for parsing the SDP */
    status = gsmsdp_realloc_dest_sdp(dcb_p);
    if (status != CC_CAUSE_OK) {
        /* Unable to create internal SDP structure to parse SDP. */
        return status;
    }

    /*
     * Parse the SDP into internal structure,
     * now just parse one
     */
    status = CC_CAUSE_SDP_PARSE_FAILED;
    for (i = 0; (i < num_sdp_bodies); i++) {
        if ((sdp_bodies[i]->body != NULL) && (sdp_bodies[i]->body_length > 0)) {
            /* Found a body */
            sdp_body = sdp_bodies[i]->body;
            if (sdp_parse(dcb_p->sdp->dest_sdp, &sdp_body,
                          (uint16_t)sdp_bodies[i]->body_length)
                    == SDP_SUCCESS) {
                status = CC_CAUSE_OK;
                break;
            }
        }
    }
    if (status != CC_CAUSE_OK) {
        /* Error parsing SDP */
        return status;
    }

    if (init) {
        (void)gsmsdp_init_local_sdp(dcb_p->peerconnection, &(dcb_p->sdp));
        /* Note that there should not a previous version here as well */
    }

    gsmsdp_set_remote_sdp(dcb_p, dcb_p->sdp);

    return (status);
}


/*
 * gsmsdp_check_peer_ice_attributes_exist
 *
 * Read ICE parameters from the SDP and return failure
 * if they are not complete.
 *
 * fcb_p - pointer to the fcb
 *
 */
cc_causes_t
gsmsdp_check_ice_attributes_exist(fsm_fcb_t *fcb_p) {
    fsmdef_dcb_t    *dcb_p = fcb_p->dcb;
    sdp_result_e     sdp_res;
    char            *ufrag;
    char            *pwd;
    fsmdef_media_t  *media;
    boolean          has_session_ufrag = FALSE;
    boolean          has_session_pwd = FALSE;

    /* Check for valid ICE parameters */
    sdp_res = sdp_attr_get_ice_attribute(dcb_p->sdp->dest_sdp,
        SDP_SESSION_LEVEL, 0, SDP_ATTR_ICE_UFRAG, 1, &ufrag);
    if (sdp_res == SDP_SUCCESS && ufrag) {
        has_session_ufrag = TRUE;
    }

    sdp_res = sdp_attr_get_ice_attribute(dcb_p->sdp->dest_sdp,
        SDP_SESSION_LEVEL, 0, SDP_ATTR_ICE_PWD, 1, &pwd);
    if (sdp_res == SDP_SUCCESS && pwd) {
        has_session_pwd = TRUE;
    }

    if (has_session_ufrag && has_session_pwd) {
        /* Both exist at session level, success */
        return CC_CAUSE_OK;
    }

    /* Incomplete ICE params at session level, check all media levels */
    GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
        if (!GSMSDP_MEDIA_ENABLED(media)) {
            continue;
        }

        if (!has_session_ufrag) {
            sdp_res = sdp_attr_get_ice_attribute(dcb_p->sdp->dest_sdp,
                media->level, 0, SDP_ATTR_ICE_UFRAG, 1, &ufrag);

            if (sdp_res != SDP_SUCCESS || !ufrag) {
                GSM_ERR_MSG(GSM_L_C_F_PREFIX"missing ICE ufrag parameter.",
                            dcb_p->line, dcb_p->call_id, __FUNCTION__);
                return CC_CAUSE_MISSING_ICE_ATTRIBUTES;
            }
        }

        if (!has_session_pwd) {
            sdp_res = sdp_attr_get_ice_attribute(dcb_p->sdp->dest_sdp,
                media->level, 0, SDP_ATTR_ICE_PWD, 1, &pwd);

            if (sdp_res != SDP_SUCCESS || !pwd) {
                GSM_ERR_MSG(GSM_L_C_F_PREFIX"missing ICE pwd parameter.",
                            dcb_p->line, dcb_p->call_id, __FUNCTION__);
                return CC_CAUSE_MISSING_ICE_ATTRIBUTES;
            }
        }
    }

    return CC_CAUSE_OK;
}

/*
 * gsmsdp_install_peer_ice_attributes
 *
 * Read ICE parameters from the SDP and set them into
 * the ice engine. Check SESSION_LEVEL first then each media line.
 *
 * fcb_p - pointer to the fcb
 *
 */
cc_causes_t
gsmsdp_install_peer_ice_attributes(fsm_fcb_t *fcb_p)
{
    char            *ufrag;
    char            *pwd;
    char            **candidates;
    int             candidate_ct;
    sdp_result_e    sdp_res;
    short           vcm_res;
    fsmdef_dcb_t    *dcb_p = fcb_p->dcb;
    cc_sdp_t        *sdp_p = dcb_p->sdp;
    fsmdef_media_t  *media;
    int             level;
    cc_causes_t     result;

    /* Tolerate missing ufrag/pwd here at the session level
       because it might be at the media level */
    sdp_res = sdp_attr_get_ice_attribute(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0,
      SDP_ATTR_ICE_UFRAG, 1, &ufrag);
    if (sdp_res != SDP_SUCCESS)
      ufrag = NULL;

    sdp_res = sdp_attr_get_ice_attribute(sdp_p->dest_sdp, SDP_SESSION_LEVEL, 0,
      SDP_ATTR_ICE_PWD, 1, &pwd);
    if (sdp_res != SDP_SUCCESS)
      pwd = NULL;

    /* ice-lite is a session level attribute only, RFC 5245 15.3 */
    dcb_p->peer_ice_lite = sdp_attr_is_present(sdp_p->dest_sdp,
      SDP_ATTR_ICE_LITE, SDP_SESSION_LEVEL, 0);

    if ((ufrag && pwd) || dcb_p->peer_ice_lite) {
        vcm_res = vcmSetIceSessionParams(dcb_p->peerconnection, ufrag, pwd,
                                         dcb_p->peer_ice_lite);
        if (vcm_res)
            return (CC_CAUSE_SETTING_ICE_SESSION_PARAMETERS_FAILED);
    }

    /* Now process all the media lines */
    GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
      if (!GSMSDP_MEDIA_ENABLED(media))
        continue;

      /* If we are muxing, disable the second
         component of the ICE stream */
      if (media->rtcp_mux) {
        vcm_res = vcmDisableRtcpComponent(dcb_p->peerconnection,
          media->level);

        if (vcm_res) {
          return (CC_CAUSE_SETTING_ICE_SESSION_PARAMETERS_FAILED);
        }
      }

      sdp_res = sdp_attr_get_ice_attribute(sdp_p->dest_sdp, media->level, 0,
        SDP_ATTR_ICE_UFRAG, 1, &ufrag);
      if (sdp_res != SDP_SUCCESS)
        ufrag = NULL;

      sdp_res = sdp_attr_get_ice_attribute(sdp_p->dest_sdp, media->level, 0,
        SDP_ATTR_ICE_PWD, 1, &pwd);
      if (sdp_res != SDP_SUCCESS)
        pwd = NULL;

      candidate_ct = 0;
      candidates = NULL;
      result = gsmsdp_get_ice_attributes (SDP_ATTR_ICE_CANDIDATE, media->level, sdp_p->dest_sdp,
                                          &candidates, &candidate_ct);
      if(result != CC_CAUSE_OK)
        return (result);

      /* Set ICE parameters into ICE engine */

      vcm_res = vcmSetIceMediaParams(dcb_p->peerconnection, media->level, ufrag, pwd,
                                    candidates, candidate_ct);

      /* Clean up */
      if(candidates) {
        int i;

        for (i=0; i<candidate_ct; i++) {
          if (candidates[i])
            cpr_free(candidates[i]);
        }
        cpr_free(candidates);
      }

      if (vcm_res)
        return (CC_CAUSE_SETTING_ICE_SESSION_PARAMETERS_FAILED);

    }

    return CC_CAUSE_OK;
}

/*
 * gsmsdp_configure_dtls_data_attributes
 *
 * Read DTLS algorithm and digest from the SDP.
 * If data is found at session level then that is used for each media stream
 * else each media stream is set with its corresponding DTLS data from the remote SDP
 *
 * fcb_p - pointer to the fcb
 *
 */
cc_causes_t
gsmsdp_configure_dtls_data_attributes(fsm_fcb_t *fcb_p)
{
    char            *fingerprint = NULL;
    char            *session_fingerprint = NULL;
    sdp_result_e    sdp_res;
    sdp_result_e    sdp_session_res;
    short           vcm_res;
    fsmdef_dcb_t    *dcb_p = fcb_p->dcb;
    cc_sdp_t        *sdp_p = dcb_p->sdp;
    fsmdef_media_t  *media;
    int             level = SDP_SESSION_LEVEL;
    short           result;
    char           *token;
    char            line_to_split[FSMDEF_MAX_DIGEST_ALG_LEN + FSMDEF_MAX_DIGEST_LEN + 2];
    char           *delim = " ";
    char            digest_alg[FSMDEF_MAX_DIGEST_ALG_LEN];
    char            digest[FSMDEF_MAX_DIGEST_LEN];
    char           *strtok_state;
    cc_causes_t     cause = CC_CAUSE_OK;

    /* First check for session level algorithm and key */
    sdp_session_res = sdp_attr_get_dtls_fingerprint_attribute (sdp_p->dest_sdp, SDP_SESSION_LEVEL,
                                      0, SDP_ATTR_DTLS_FINGERPRINT, 1, &session_fingerprint);

    /* Now process all the media lines */
    GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
        if (!GSMSDP_MEDIA_ENABLED(media))
            continue;

        /* check for media level algorithm and key */
        sdp_res = sdp_attr_get_dtls_fingerprint_attribute (sdp_p->dest_sdp, media->level,
                                    0, SDP_ATTR_DTLS_FINGERPRINT, 1, &fingerprint);

        if (SDP_SUCCESS == sdp_res ) {
            if (strlen(fingerprint) >= sizeof(line_to_split))
                return CC_CAUSE_DTLS_FINGERPRINT_TOO_LONG;
            sstrncpy(line_to_split, fingerprint, sizeof(line_to_split));
        } else if (SDP_SUCCESS == sdp_session_res) {
            if (strlen(session_fingerprint) >= sizeof(line_to_split))
                return CC_CAUSE_DTLS_FINGERPRINT_TOO_LONG;
            sstrncpy(line_to_split, session_fingerprint, sizeof(line_to_split));
        } else {
            cause = CC_CAUSE_NO_DTLS_FINGERPRINT;
            continue;
        }

        if (SDP_SUCCESS == sdp_res || SDP_SUCCESS == sdp_session_res) {
            if(!(token = PL_strtok_r(line_to_split, delim, &strtok_state)))
                return CC_CAUSE_DTLS_FINGERPRINT_PARSE_ERROR;

            if (strlen(token) >= sizeof(digest_alg))
                return CC_CAUSE_DTLS_DIGEST_ALGORITHM_TOO_LONG;

            sstrncpy(digest_alg, token, sizeof(digest_alg));
            if(!(token = PL_strtok_r(NULL, delim, &strtok_state)))
                return CC_CAUSE_DTLS_FINGERPRINT_PARSE_ERROR;

            if (strlen(token) >= sizeof(digest))
                return CC_CAUSE_DTLS_DIGEST_TOO_LONG;

            sstrncpy(digest, token, sizeof(digest));

            if (strlen(digest_alg) >= sizeof(media->negotiated_crypto.algorithm))
                return CC_CAUSE_DTLS_DIGEST_ALGORITHM_TOO_LONG;

            sstrncpy(media->negotiated_crypto.algorithm, digest_alg, sizeof(media->negotiated_crypto.algorithm));
            if (strlen(media->negotiated_crypto.algorithm) == 0) {
                return CC_CAUSE_DTLS_DIGEST_ALGORITHM_EMPTY;
            }

            if (strlen(digest) >= sizeof(media->negotiated_crypto.digest))
                return CC_CAUSE_DTLS_DIGEST_TOO_LONG;

            sstrncpy(media->negotiated_crypto.digest, digest, sizeof(media->negotiated_crypto.digest));
            if (strlen(media->negotiated_crypto.digest) == 0) {
                return CC_CAUSE_DTLS_DIGEST_EMPTY;
            }

            /* Here we have DTLS data */
            cause = CC_CAUSE_OK;

        } else {
            GSM_DEBUG(DEB_F_PREFIX"DTLS attribute error",
                                   DEB_F_PREFIX_ARGS(GSM, __FUNCTION__));
            return CC_CAUSE_DTLS_ATTRIBUTE_ERROR;
        }
    }

    return cause;
}

/*
 * gsmsdp_free
 *
 * Description:
 * The function frees SDP resources that were allocated during the
 * course of the call.
 *
 * Parameters:
 * dcb_p    - pointer to fsmdef_dcb_t.
 */
void
gsmsdp_free (fsmdef_dcb_t *dcb_p)
{
    if ((dcb_p != NULL) && (dcb_p->sdp != NULL)) {
        sipsdp_free(&dcb_p->sdp);
        dcb_p->sdp = NULL;
    }
}

/*
 * gsmsdp_sdp_differs_from_previous_sdp
 *
 * Description:
 *
 * Interface function used to compare newly received SDP to previously
 * received SDP. Returns FALSE if attributes of interest are the same.
 * Otherwise, returns TRUE.
 *
 * Parameters:
 *
 * rcv_only - If TRUE, check for receive port perspective.
 * media    - Pointer to the fsmdef_media_t for the current media entry.
 */
boolean
gsmsdp_sdp_differs_from_previous_sdp (boolean rcv_only, fsmdef_media_t *media)
{
    static const char fname[] = "gsmsdp_sdp_differs_from_previous_sdp";
    char    prev_addr_str[MAX_IPADDR_STR_LEN];
    char    dest_addr_str[MAX_IPADDR_STR_LEN];
    int     i;

    /* Consider attributes of interest for both directions */

    if ((0 == media->num_payloads) || (0 == media->previous_sdp.num_payloads) ||
        (media->num_payloads != media->previous_sdp.num_payloads)){
        GSM_DEBUG(DEB_F_PREFIX"previous # payloads: %d new # payloads: %d",
                  DEB_F_PREFIX_ARGS(GSM, fname),
                  media->previous_sdp.num_payloads, media->num_payloads);
    }

    if (media->previous_sdp.avt_payload_type != media->avt_payload_type){
        GSM_DEBUG(DEB_F_PREFIX"previous avt PT: %d new avt PT: %d",
                  DEB_F_PREFIX_ARGS(GSM, fname),
                  media->previous_sdp.avt_payload_type,
                  media->avt_payload_type);
        return TRUE;
    }

    for (i = 0; i < media->num_payloads; i++) {
      if ((media->previous_sdp.payloads[i].remote_rtp_pt !=
           media->payloads[i].remote_rtp_pt) ||
          (media->previous_sdp.payloads[i].codec_type !=
           media->payloads[i].codec_type)){
          GSM_DEBUG(DEB_F_PREFIX"previous dynamic payload (PT) #%d: "
                    "%d; new dynamic payload: %d\n",
                    DEB_F_PREFIX_ARGS(GSM, fname), i,
                    media->previous_sdp.payloads[i].remote_rtp_pt,
                    media->payloads[i].remote_rtp_pt);
          GSM_DEBUG(DEB_F_PREFIX"previous codec #%d: %d; new codec: %d",
                    DEB_F_PREFIX_ARGS(GSM, fname), i,
                    media->previous_sdp.payloads[i].codec_type,
                    media->payloads[i].codec_type);
          return TRUE;
      }
    }

    /*
     * Consider attributes of interest for transmit directions.
     * If previous dest port is 0 then this is the first time
     * we received sdp for comparison. We treat this situation
     * as if addr and port did not change.
     */
    if ( (media->previous_sdp.dest_port != 0) && (rcv_only == FALSE)) {
        if ((util_compare_ip(&(media->previous_sdp.dest_addr),
                            &(media->dest_addr)) == FALSE) ||
            (media->previous_sdp.dest_port != media->dest_port)) {
        prev_addr_str[0] = '\0'; /* ensure valid string if convesion fails */
        dest_addr_str[0] = '\0';
        ipaddr2dotted(prev_addr_str, &media->previous_sdp.dest_addr);
        ipaddr2dotted(dest_addr_str, &media->dest_addr);
        GSM_DEBUG(DEB_F_PREFIX"previous address: %s new address: %s",
                  DEB_F_PREFIX_ARGS(GSM, fname), prev_addr_str, dest_addr_str);
        GSM_DEBUG(DEB_F_PREFIX"previous port: %d new port: %d",
                  DEB_F_PREFIX_ARGS(GSM, fname), media->previous_sdp.dest_port, media->dest_port);
            return TRUE;
        } else if ( media->tias_bw != media->previous_sdp.tias_bw) {
            GSM_DEBUG(DEB_F_PREFIX"previous bw: %d new bw: %d",
                  DEB_F_PREFIX_ARGS(GSM, fname), media->previous_sdp.tias_bw, media->tias_bw);
            return TRUE;
        } else if ( media->profile_level != media->previous_sdp.profile_level) {
            GSM_DEBUG(DEB_F_PREFIX"previous prof_level: %X new prof_level: %X",
                  DEB_F_PREFIX_ARGS(GSM, fname), media->previous_sdp.profile_level, media->profile_level);
            return TRUE;
        }
    }


    /* Check crypto parameters if we are doing SRTP */
    if (gsmsdp_crypto_params_change(rcv_only, media)) {
        return TRUE;
    }
    return FALSE;
}


/*
 * gsmsdp_add_remote_stream
 *
 * Description:
 *
 * Add a media stream with no tracks to the dcb for the
 * current session.
 *
 * Parameters:
 *
 * idx   - Stream index
 * pc_stream_id - stream id from vcm layer, will be set as stream id
 * dcb_p - Pointer to the DCB whose SDP is to be manipulated.
 *
 * returns TRUE for success and FALSE for failure
 */
static boolean gsmsdp_add_remote_stream(uint16_t idx, int pc_stream_id, fsmdef_dcb_t *dcb_p) {
  PR_ASSERT(idx < CC_MAX_STREAMS);
  if (idx >= CC_MAX_STREAMS)
    return FALSE;

  PR_ASSERT(!dcb_p->remote_media_stream_tbl->streams[idx].created);
  if (dcb_p->remote_media_stream_tbl->streams[idx].created)
    return FALSE;

  dcb_p->remote_media_stream_tbl->streams[idx].media_stream_id = pc_stream_id;
  dcb_p->remote_media_stream_tbl->streams[idx].created = TRUE;

  return TRUE;
}


/*
 * gsmsdp_add_remote_track
 *
 * Description:
 *
 * Add a track to a media stream
 *
 * Parameters:
 *
 * idx   - Stream index
 * track - the track id
 * dcb_p - Pointer to the DCB whose SDP is to be manipulated.
 * media - the media object to add.
 *
 * returns TRUE for success and FALSE for failure
 */
static boolean gsmsdp_add_remote_track(uint16_t idx, uint16_t track,
                                       fsmdef_dcb_t *dcb_p,
                                       fsmdef_media_t *media) {
  cc_media_remote_track_table_t *stream;
  int vcm_ret;

  PR_ASSERT(idx < CC_MAX_STREAMS);
  if (idx >= CC_MAX_STREAMS)
    return FALSE;

  stream = &dcb_p->remote_media_stream_tbl->streams[idx];

  PR_ASSERT(stream->created);
  if (!stream->created)
    return FALSE;

  PR_ASSERT(stream->num_tracks < (CC_MAX_TRACKS - 1));
  if (stream->num_tracks > (CC_MAX_TRACKS - 1))
    return FALSE;

  stream->track[stream->num_tracks].media_stream_track_id = track;
  stream->track[stream->num_tracks].video =
      (media->type == SDP_MEDIA_VIDEO) ? TRUE : FALSE;

  ++stream->num_tracks;

  if (media->type == SDP_MEDIA_VIDEO) {
    vcm_ret = vcmAddRemoteStreamHint(dcb_p->peerconnection, idx, TRUE);
  } else if (media->type == SDP_MEDIA_AUDIO) {
    vcm_ret = vcmAddRemoteStreamHint(dcb_p->peerconnection, idx, FALSE);
  } else {
    // No other track types should be valid here
    MOZ_ASSERT(FALSE);
    // Not setting a hint for this track type will simply cause the
    // onaddstream callback not to wait for the track to be ready.
    vcm_ret = 0;
  }

  if (vcm_ret) {
      CSFLogError(logTag, "%s: vcmAddRemoteStreamHint returned error: %d",
          __FUNCTION__, vcm_ret);
      return FALSE;
  }

  return TRUE;
}

cc_causes_t
gsmsdp_find_level_from_mid(fsmdef_dcb_t * dcb_p, const char * mid, uint16_t *level) {

    fsmdef_media_t  *media;
    u32              mid_id;
    char             buf[5];

    GSMSDP_FOR_ALL_MEDIA(media, dcb_p) {
        if (!GSMSDP_MEDIA_ENABLED(media))
            continue;

        mid_id = sdp_attr_get_simple_u32(dcb_p->sdp->dest_sdp, SDP_ATTR_MID, media->level, 0, 1);
        snprintf(buf, sizeof(buf), "%u", mid_id);
        if (strcmp(mid, buf) == 0) {
        	*level = media->level;
        	return CC_CAUSE_OK;
        }
    }
    return CC_CAUSE_VALUE_NOT_FOUND;
}

/**
 * The function performs cleaning candidate list of a given call. It walks
 * through the list and deallocates each candidate entry.
 *
 * @param[in]dcb   - pointer to fsmdef_def_t for the dcb whose
 *                   media list to be cleaned.
 *
 * @return  none
 *
 * @pre     (dcb not_eq NULL)
 */
void gsmsdp_clean_candidate_list (fsmdef_dcb_t *dcb_p)
{
    fsmdef_candidate_t *candidate = NULL;

    while (TRUE) {
        /* unlink head and free the media */
        candidate = (fsmdef_candidate_t *)sll_lite_unlink_head(&dcb_p->candidate_list);
        if (candidate) {
            strlib_free(candidate->candidate);
            free(candidate);
        } else {
            break;
        }
    }
}

