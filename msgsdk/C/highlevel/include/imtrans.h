/*
 *	 The contents of this file are subject to the Netscape Public
 *	 License Version 1.1 (the "License"); you may not use this file
 *	 except in compliance with the License. You may obtain a copy of
 *	 the License at http://www.mozilla.org/NPL/
 *	
 *	 Software distributed under the License is distributed on an "AS
 *	 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 *	 implied. See the License for the specific language governing
 *	 rights and limitations under the License.
 *	
 *	 The Original Code is the Netscape Messaging Access SDK Version 3.5 code, 
 *	released on or about June 15, 1998.  *	
 *	 The Initial Developer of the Original Code is Netscape Communications 
 *	 Corporation. Portions created by Netscape are
 *	 Copyright (C) 1998 Netscape Communications Corporation. All
 *	 Rights Reserved.
 *	
 *	 Contributor(s): ______________________________________. 
*/

/*
 * Copyright (c) 1997 and 1998 Netscape Communications Corporation
 * (http://home.netscape.com/misc/trademarks.html)
 */
 
#ifndef IMTRANS_H 
#define IMTRANS_H 

#ifdef __cplusplus 
extern "C" { 
#endif 

/* ERRORS */ 
#define IMTRANS_OK                      0 
#define IMTRANS_ERR_IO_READ             -1 
#define IMTRANS_ERR_IO_WRITE            -2 
#define IMTRANS_ERR_NO_DATA             -3 
#define IMTRANS_ERR_INVALID_MSG       	-4 
#define IMTRANS_ERR_INVALID_HOST      	-5 
  

/* CONTENT PRIMARY TYPES */ 
#define CONTENT_TYPE_TEXT               1 
#define CONTENT_TYPE_AUDIO              2 
#define CONTENT_TYPE_VIDEO              3 
#define CONTENT_TYPE_IMAGE              4 
#define CONTENT_TYPE_APPLICATION     	5 

/* MIME ENCODING TYPES */ 
#define MIME_ENCODING_BASE64            1 
#define MIME_ENCODING_QP                2 
#define MIME_ENCODING_NONE              3 

/* CONTENT DISPOSITON TYPES */ 
#define CONT_DISP_INLINE                1 
#define CONT_DISP_ATTACH                2 

typedef struct IMTrans_Attachment 
{ 
        int contentPrimaryType; 
        char * content_subType; 
        char * contentTypeParams;     /* Can be NULL */ 
        int  contentDisposition;      /*  If 0 content-disposition attribute will not be set */ 
        int  mimeEncoding;            /*  If 0 will assume a default based on content-type */ 
        char *  pData;                /* Actual document data in buffer. Must be NULL if pFileData is not */ 
        FILE *  pFileData;            /* Actual document data in File. Must be NULL if pData is not.  */ 
        struct IMTrans_Attachment * next; 

} IMTrans_Attachment_t; 

typedef struct IMTrans_MsgHeader 
{ 
        char * name; 
        char * value; 
        struct IMTrans_MsgHeader * next;   /* Must be NULL terminated */ 

} IMTrans_MsgHeader_t; 
  

/*************************************************************************************** 
 * Connects to the SMTP transport at the specified host and submits the message. 
 * NOTE: The MIME Message itself can be created using the netscape MIME API or 
 * by any other means. 
 * 
 * Parameters: 
 *    in_host               Name of the host to connect to. 
 *    in_sender             Sender of the message. 
 *    in_recipients         Email addresses of the recipients to send the message to. 
 *    in_pMessage           The actual message to send in MIME format. 
 *    out_rejectedRecips    Recipients to whom the message could not be submitted. 
 * 
 * Returns:      IMTRANS_OK on success. 
 *               Other errors as listed above are returned for error conditions. 
 * 
 ***************************************************************************************/ 
int IMTrans_sendMessage (char *     in_host, 
                         char *     in_sender, 
                         char **    in_recipients, 
                         char *     in_pMessage, 
                         char **    out_rejectedRecips); 

/*************************************************************************************** 
 * Builds a MIME message with the specified parameters. Connects to the SMTP transport 
 * at the specified host and submits the message. If > 1 attachments are specified, they 
 * will be sent as MIME multipart/mixed type message. If > 1 attachments are specified 
 * the corresponding parameters (e.g. contentPrimaryTypes, encodings etc.) should be 
 * specified in the same order as the attachments. 
 * NOTE: This method facilitates mailing documents by mail-enabling an otherwise 
 * mail-ignorant application. Any other uses are better served by the sendMessage() 
 * method in association with the netscape MIME API or by other Messaging APIs provide 
 * by Netscape for more sophisticated needs. 
 * 
 * Parameters: 
 *    in_host              Name of the host to connect to. 
 *    in_sender            Sender of the message. 
 *    in_recipients        Email addresses of the recipients to send the message to. 
 *    in_subject           Subject of the message. Can be NULL. 
 *    in_msgHeaders        Additional RFC-822 Message headers. 
 *    in_pAttachments      A list of Attachments to be sent. 
 *    out_rejectedRecips   Recipients to whom the message could not be submitted. 
 * 
 * Returns:      IMTRANS_OK on success. 
 *               Other errors as listed above are returned for error conditions. 
 * 
 ***************************************************************************************/ 
int IMTrans_sendDocuments (char *     in_host, 
                           char *     in_sender, 
                           char **   in_recipients, 
                           char *     in_subject, 
                           IMTrans_MsgHeader_t * in_msgHeaders, 
                           IMTrans_Attachment_t *  in_pAttachments, 
                           char **    out_rejectedRecips); 
  
  

#ifdef __cplusplus 
} 
#endif 
  
  
#endif /* IMTRANS_H */ 
