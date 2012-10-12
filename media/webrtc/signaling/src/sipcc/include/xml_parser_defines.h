/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef XML_PARSER_DEFINES_H
#define XML_PARSER_DEFINES_H

#include "sll_lite.h"
#include "cc_constants.h"


/**
 * In general, when a parser constructs a xml string,
 * it should translate the enum to cooresponding string
 * value that is defined in the accompanied xsd files.
 */
/**
 * Define the state values
 */
typedef enum {
    XML_STATE_PARTIAL = 0, //Encode as "partial"
    XML_STATE_FULL         //"full"
} xml_state_t;

/**
 * Define the call orientation
 */
typedef enum {
    XML_CALL_ORIENTATION_UNSPECIFIED = 0,
    XML_CALL_ORIENTATION_TO,
    XML_CALL_ORIENTATION_FROM
} xml_call_orientation_t;

/**
 * Define the call lock status
 */
typedef enum {
    XML_CALLLOCK_UNLOCKED = 0,
    XML_CALLLOCK_LOCKED,
    XML_CALLLOCK_REMOTE_LOCKED
} xml_calllock_t;

/**
 * Define the direction values
 */
typedef enum {
    XML_DIRECTION_INITIATOR = 0,
    XML_DIRECTION_RECIPIENT
} xml_direction_t;

/**
 * Define the event values
 */
typedef enum {
    XML_EVENT_CANCELLED = 0,
    XML_EVENT_REJECTED,
    XML_EVENT_REPLACED,
    XML_EVENT_LOCAL_BYE,
    XML_EVENT_REMOTE_BYE,
    XML_EVENT_ERROR,
    XML_EVENT_TIMEOUT
} xml_event_t;

/**
 * Define the yes or no values
 */
typedef enum {
    XML_NO = 0,
    XML_YES,
    XML_NONEAPPLICABLE //"na"
} xml_yes_no_t;

/**
 * Define the on or off value
 */
typedef enum {
    XML_OFF = 0,
    XML_ON
} xml_on_off_t;

/**
 * Define the true or false values
 */
typedef enum {
    XML_FALSE = 0,
    XML_TRUE
} xml_true_false_t;

/**
 * Define the line key events
 */
typedef enum {
    XML_LINE_KEY_EVENT_LINE = 0,
    XML_LINE_KEY_EVENT_SPEEDDIAL
} xml_line_key_event_t;

/**
 * Define the persist types
 */
typedef enum {
    XML_PERSIST_TYPE_ONE_SHOT = 0,
    XML_PERSIST_TYPE_PERSIST,
    XML_PERSIST_TYPE_SINGLE_NOTIFY
} xml_persist_type_t;

/**
 * Define the soft key invoke type
 */
typedef enum {
    XML_SKEY_INVOKE_EXPLICIT = 0,
    XML_SKEY_NVOKE_IMPLICIT
} xml_skey_invoke_t;

/**
 * Define the soft key event data
 */
typedef enum {
    XML_SKEY_EVENT_UNDEFINED = 0,
    XML_SKEY_EVENT_REDIAL,
    XML_SKEY_EVENT_NEWCALL,
    XML_SKEY_EVENT_HOLD,
    XML_SKEY_EVENT_TRANSFER,
    XML_SKEY_EVENT_CFWDALL, //5
    XML_SKEY_EVENT_CFWDBUSY,
    XML_SKEY_EVENT_CFWDNOANSWER,
    XML_SKEY_EVENT_BACKSPACE,
    XML_SKEY_EVENT_ENDCALL,
    XML_SKEY_EVENT_RESUME, //10
    XML_SKEY_EVENT_ANSWER,
    XML_SKEY_EVENT_INFO,
    XML_SKEY_EVENT_CONFERENCE,
    XML_SKEY_EVENT_JION, //15
    XML_SKEY_EVENT_REMVOVE_LAST_CONF_PARTICIPANT,
    XML_SKEY_EVENT_DIRECT_XFER,
    XML_SKEY_EVENT_SELECT, //25
    XML_SKEY_EVENT_TRANSFER_TO_VOICE_MAIL,
    XML_SKEY_EVENT_SAC,
    XML_SKEY_EVENT_UNSELECT, //35
    XML_SKEY_EVENT_CANCEL,
    XML_SKEY_EVENT_COPNFERENCE_DETAILS,//40
    XML_SKEY_EVENT_TRASFMG = 65,
    XML_SKEY_EVENT_INTRCPT,
    XML_SKEY_EVENT_SETWTCH,
    XML_SKEY_EVENT_TRNSFVM,
    XML_SKEY_EVENT_TRNSFAS
} xml_skey_event_code_t;

/**
 * Define the map for station sequence mapping
 */
typedef enum {
    XML_STATION_SEQ_FIRST = 0,
    XML_STATION_SEQ_MORE,
    XML_STATION_SEQ_LAST
} xml_stataionseq_t;

/**
 * Define the hold reasons
 */
typedef enum {
    XML_HOLD_REASON_NONE = 0,
    XML_HOLD_REASON_TRANSFER,
    XML_HOLD_REASON_CONFERENCE,
    XML_HOLD_REASON_INTERNAL
} xml_hold_reason_t;

/**
 * Define the lamp status
 */
typedef enum {
    XML_LAMP_STATE_OFF = 0,
    XML_LAMP_STATE_ON,
    XML_LAMP_STATE_BLINK,
    XML_LAMP_STATE_FLASH
} xml_lamp_state_t;

/**
 * Define the lamp type
 */
typedef enum {
    XML_LAMP_TYPE_LINE = 1,
    XML_LAMP_TYPE_VOICE_MAIL
} xml_lamp_type_t;

/**
 * Define the image down load method
 */
typedef enum {
    XML_IMAGE_DOWNLOAD_METHOD_TFTP = 1,
    XML_IMAGE_DOWNLAOD_METHOD_HTTP,
    XML_IMAGE_DOWNLOAD_METHOD_PPID
} xml_image_dl_method_t;

/**
 * Define the image download failure reason
 */
typedef enum {
    XML_IMAGE_DOWNLOAD_FAILURE_REASON_DISKFULL = 1,
    XML_IMAGE_DOWNLOAD_FAILURE_REASON_IMAGE_NOT_AVAILABLE,
    XML_IMAGE_DOWNLOAD_FAILURE_REASON_ACCESS_VIOLATION
} xml_image_dl_failure_reason_t;

typedef signed long         xml_signed32;
typedef unsigned long       xml_unsigned32;
typedef unsigned short      xml_unsigned16;
typedef unsigned char       xml_unsigned8;

// start of copy from ccsip_eventbodies.h
typedef struct State {
	xml_signed32		event;
	xml_signed32		code;
	xml_signed32		state;
} State;

typedef struct Replaces {
	char			call_id[128];
	char			local_tag[64];
	char			remote_tag[64];
} Replaces;

typedef struct RefferedBy {
	char			display_name[64];
	char			uri[64];
} RefferedBy;

typedef struct RouteSet {
	char			hop[5][16];
} RouteSet;

typedef struct Identity {
	char			display_name[64];
	char			uri[64];
} Identity;

typedef struct Param {
	char			pname[32];
	char			pval[32];
} Param;

typedef struct Target {
	Param	param[4];
	char			uri[64];
} Target;

typedef struct SessionDescription {
	char			type[32];
} SessionDescription;

typedef struct Participant {
	Identity	identity;
	Target	target;
	SessionDescription	session_description;
	xml_unsigned16		cseq;
} Participant;

typedef struct primCall {
	char			call_id[128];
	char			local_tag[64];
	char			remote_tag[64];
	xml_signed32		h_reason;
} primCall;

typedef struct callFeature {
    char            cfwdall_set[128];
    char            cfwdall_clear[128];
} callFeature;

typedef struct Stream {
	char			reverse[16];
} Stream;

typedef struct Regex {
	char			regexData[32];
	char			tag[32];
	char			pre[32];
} Regex;

typedef struct Pattern {
	xml_signed32		flush;
	Regex	regex;
	xml_signed32		persist;
	xml_unsigned32		interdigittimer;
	xml_unsigned32		criticaldigittimer;
	xml_unsigned32		extradigittimer;
	xml_unsigned16		longhold;
	xml_unsigned8		longrepeat;
	xml_unsigned8		nopartial;
	char			enterkey[8];
} Pattern;

typedef struct KPMLRequest {
	Stream	stream;
	Pattern	pattern;
	char			version[16];
} KPMLRequest;

typedef struct KPMLResponse {
	char			version[16];
	char			code[16];
	char			text[16];
	xml_unsigned8		suppressed;
	char			forced_flush[16];
	char			digits[16];
	char			tag[16];
} KPMLResponse;

typedef struct dialogID {
	char			callid[128];
	char			localtag[64];
	char			remotetag[64];
} dialogID;

typedef struct consultDialogID {
	char			callid[128];
	char			localtag[64];
	char			remotetag[64];
} consultDialogID;

typedef struct joindialogID {
	char			callid[128];
	char			localtag[64];
	char			remotetag[64];
} joindialogID;

typedef struct reg_contact_t {
	char			Register[16];
	char			Unregister[16];
	xml_unsigned32		line;
	xml_unsigned32		low;
	xml_unsigned32		high;
	xml_signed32		all;
} reg_contact_t;


typedef struct remotecc {
	char			status[16];
} remotecc;

typedef struct combine {
	xml_unsigned16		max_bodies;
	remotecc	remotecc;
	char			service_control[16];
} combine;

typedef struct dialog {
	char			usage[64];
	char			unot[16];
	char			sub[16];
} dialog;

typedef struct presence {
	char			usage[64];
	char			unot[16];
	char			sub[16];
} presence;

typedef struct voice_msg_t {
	xml_signed32		newCount;
	xml_signed32		oldCount;
} voice_msg_t;

typedef struct voice_msg_hp_t {
	xml_signed32		newCount;
	xml_signed32		oldCount;
} voice_msg_hp_t;

typedef struct fax_msg_t {
	xml_signed32		newCount;
	xml_signed32		oldCount;
} fax_msg_t;

typedef struct fax_msg_hp_t {
	xml_signed32		newCount;
	xml_signed32		oldCount;
} fax_msg_hp_t;

typedef struct emwi_t {
	voice_msg_t	voice_msg;
	voice_msg_hp_t	voice_msg_hp;
	fax_msg_t	fax_msg;
	fax_msg_hp_t	fax_msg_hp;
} emwi_t;

typedef struct cfwdallupdate {
	char			fwdAddress[256];
} cfwdallupdate;

typedef struct Contact_t {
	xml_unsigned32		line;
	xml_unsigned32		high;
	xml_unsigned32		low;
	xml_signed32		all;
	xml_signed32		mwi;
	emwi_t	emwi;
	cfwdallupdate	cfwdallupdate;
} Contact_t;

typedef struct dialog_t {
	char			usage[64];
	char			unot[12];
	char			sub[12];
} dialog_t;

typedef struct presence_t {
	char			usage[64];
	char			unot[12];
	char			sub[12];
} presence_t;

typedef struct options_ans_t {
	combine	combine;
	dialog_t	dialog;
	presence_t	presence;
} options_ans_t;

typedef struct PersonStatusStruct {
	char			basic[32];
} PersonStatusStruct;

typedef struct ActivitiesStruct {
	char			alerting[12];
	char			onThePhone[12];
	char			busy[12];
	char			away[12];
	char			meeting[12];
} ActivitiesStruct;

typedef struct PersonStruct {
	char			id[256];
	PersonStatusStruct	personStatus;
	ActivitiesStruct	activities;
} PersonStruct;

typedef struct StatusStruct {
	char			basic[32];
	ActivitiesStruct	activities;
} StatusStruct;

typedef struct TupleStruct {
	char			id[256];
	StatusStruct	status;
	char			contact[1][256];
	char			note[1][1024];
} TupleStruct;

typedef struct PresenceRPIDStruct {
	char			entity[256];
	PersonStruct	person;
	TupleStruct	tuple[1];
	char			note[5][1024];
} PresenceRPIDStruct;

typedef struct sipProfile {
	xml_unsigned16		kpml_val;
} sipProfile;

typedef struct ConfigApp_req_data_t {
	sipProfile	sip_profile;
} ConfigApp_req_data_t;

typedef struct to_encoder_t {
	xml_unsigned32		picture_fast_update;
} to_encoder_t;

typedef struct vc_primivite_t {
	to_encoder_t	to_encoder;
	char			stream_id[128];
} vc_primivite_t;

typedef struct Media_Control_t {
	vc_primivite_t	vc_primitive;
	char			general_error[128];
} Media_Control_t;

// end of copy from ccsip_eventbodies.h

typedef struct Presence_ext_t_ {
    PresenceRPIDStruct presence_body;
/*
 * Some of the tags' mere presence in the rpid document has a meaning. These tags
 * may not contain any value between starting and ending tag. So we need a way to
 * indicate the presence of a tag. We will use the following boolean memeber fields.
 */
    boolean onThePhone;
    boolean busy;
    boolean away;
    boolean meeting;
    boolean alerting;
} Presence_ext_t;


typedef enum {
    EVENT_DATA_INVALID = 0,
    EVENT_DATA_KPML_REQUEST,
    EVENT_DATA_KPML_RESPONSE,
    EVENT_DATA_REMOTECC_REQUEST,
    EVENT_DATA_PRESENCE,
    EVENT_DATA_DIALOG,
    EVENT_DATA_RAW,
    EVENT_DATA_CONFIGAPP_REQUEST,
    EVENT_DATA_MEDIA_INFO
} ccsip_event_data_type_e;

typedef struct {
    char    *data;
    uint32_t length;
} raw_data_t;

typedef struct {
    Media_Control_t media_control;
    uint32_t   picture_fast_update;
} media_control_ext_t;

#define TAG_LENGTH  16
typedef struct {
    char current_method[TAG_LENGTH];
    char hookstate[TAG_LENGTH];
    char presence[TAG_LENGTH];
} Options_ind_t;



typedef struct rcc_response_t {
       xml_unsigned16          code;
       char                    reason[128];
       xml_unsigned32          applicationid;
       xml_unsigned32          transactionid;
       xml_signed32            stationsequence;
       xml_unsigned16          displaypriority;
       xml_unsigned16          appinstance;
       xml_unsigned16          linenumber;
       xml_unsigned32          routingid;
       xml_unsigned32          confid;
       char                    callID[128];
       options_ans_t   options_ind;
} rcc_response_t;

typedef enum {
    RCC_NULL_REQ = 0,
    RCC_INITCALL_REQ = 1,
    RCC_MONITORCALL_REQ,
    RCC_DIALCALL_REQ,
    RCC_DIALDTMF_REQ,
    RCC_ANSCALL_REQ,
    RCC_DISCCALL_REQ,
    RCC_XFERSETUP_REQ,
    RCC_XFERCOMPLETE_REQ,
    RCC_CONFSETUP_REQ,
    RCC_CONFCOMPLETE_REQ,
    RCC_HOLD_REQ,
    RCC_HOLDRETRIEVE_REQ,
    RCC_DATAPASSTHROUGH_REQ,
    RCC_CFWDALL_REQ,
    RCC_LINEKEY_EVT,
    RCC_STATUS_UPDATE_REQ,
    RCC_SET_IDLE_STATUS_PROMPT_REQ,
    RCC_PLAY_TONE_REQ,
    RCC_STOP_TONE_REQ,
    RCC_CALL_SELECT_REQ,
    RCC_SOFTKEY_EVT,
    RCC_LINE_RINGER_SET_REQ,
    RCC_HOLD_REVERSION_REQ,
    RCC_LAMP_CONTROL_REQ,
    RCC_LINEKEY_UPDATE,
    RCC_BULKREGISTER_REQ,
    RCC_OPTIONS_IND,
    RCC_BULK_UPDATE,
    RCC_CALL_JOIN_REQ,
    RCC_NOTIFY_REQ,
    RCC_MONITOR_UPDATE_REQ,
    RCC_MAX_REQ
} rcc_request_type_t;

typedef struct rcc_softkey_event_msg_t {
	xml_signed32		softkeyevent;
	dialogID	dialogid;
	xml_unsigned16		linenumber;
	xml_unsigned16		participantnum;
	dialogID	consultdialogid;
	xml_unsigned8		state;
	dialogID	joindialogid;
	//eventData	eventdata;
	char			userdata[32];
	xml_unsigned16		soktkeyid;
	xml_unsigned16		applicationid;
} rcc_softkey_event_msg_t;


typedef struct RCC_req_data {
	rcc_softkey_event_msg_t	rcc_softkey_event_msg;
} RCC_req_data;

typedef struct rcc_int_t_ {
    RCC_req_data rcc_int;
    // User added fields
    xml_unsigned8 iterations;
    cc_lineid_t line;
    cc_callid_t gsm_id;
    cc_callid_t consult_gsm_id;
    cc_callid_t join_gsm_id;
    rcc_request_type_t rcc_request_type;
} RCC_data;


// Data for event generation
typedef struct ccsip_event_data_t_ {
    struct ccsip_event_data_t_ *next;
    ccsip_event_data_type_e type;
    union {
        KPMLResponse   kpml_response;
        KPMLRequest    kpml_request;
        RCC_data       remotecc_data;
        rcc_response_t remotecc_data_response;
        Options_ind_t  options_ind;
        Presence_ext_t presence_rpid;
        raw_data_t     raw_data; // used for cmxml and other body types
        ConfigApp_req_data_t configapp_data;
        media_control_ext_t  media_control_data;
    } u;
} ccsip_event_data_t;


/**
 * Request to allocate memory for external xml parser
 * @param [in] size ofrequested memory
 * @return pointer to memory allocated.
 */
void *ccAllocXML(cc_size_t size);

/**
 * Free xml memory
 * @param [in] mem - memory to free
 * @return void
 */
void ccFreeXML(void *mem);
#endif
