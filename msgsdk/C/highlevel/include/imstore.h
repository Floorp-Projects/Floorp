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
 
#ifndef IMSTORE_H 
#define IMSTORE_H 

#ifdef __cplusplus 
extern "C" { 
#endif 

/* ERRORS */ 
#define IMSTORE_OK                        0 
#define IMSTORE_ERR_IO_READ              -1 
#define IMSTORE_ERR_IO_WRITE             -2 
#define IMSTORE_ERR_NO_DATA              -3 
#define IMSTORE_ERR_INVALID_MSG          -4 
#define IMSTORE_ERR_INVALID_FOLDER       -5 
#define IMSTORE_ERR_INVALID_HOST         -6 
#define IMSTORE_ERR_INVALID_PROTOCOL  	 -7 
#define IMSTORE_ERR_CANT_DELETE          -8 
#define IMSTORE_ERR_CANT_COPY            -9 
#define IMSTORE_ERR_CANT_MOVE            -10 
  

typedef struct IMMessageSummary 
{ 
        char * foldername; 
        char * sender; 
        char * subject; 
        char * date; 
        char * msgid; 
        long   size; 
} IMMessageSummary_t; 

typedef struct IMSearchCriteria 
{ 
     char *     fieldName; /* The special string "TextBody" implies all text parts in the Message */ 
     char *     searchString;         /* The string to search for */ 
     int           andOr;             /* 0 = OR; 1 = AND */ 
     struct IMSearchCriteria * next; 

} IMSearchCriteria_t; 

typedef struct IMStore IMStore_t;     /* Opaque handle to the store */ 

/*************************************************************************************** 
 * Connects to the message store at the specified host, on the default port based on the 
 * specified protocol. 
 * 
 * Parameters: 
 *     in_host            Name of the host to connect to. 
 *     in_protocol        Protocol to be used to connect to the message store. 
 *                        Valid values for protocol are "POP3" and "IMAP4". 
 *     out_ppStore        Opaque handle to store, to be passed to the subsequent calls to the store.
 * 
 * Returns:      IMSTORE_OK on success and a valid handle to the store is returned in ppStore. 
 *               Other errors as listed above are returned for error conditions. 
 * See Also:     IMStore_disconnect(). 
 * 
 ***************************************************************************************/ 
int IMStore_connect (char *       in_host, 
                     char *       in_protocol, 
                     IMStore_t ** out_ppStore); 

/*************************************************************************************** 
 * Logs into message store as the specified user. 
 * 
 * Parameters: 
 *    in_pStore          Opaque handle to store, obtained from IMStore_connect(). 
 *    in_user            Username to login with. 
 *    in_pass            Password to login with. 
 * 
 * Returns:          IMSTORE_OK on success. 
 *                   Other errors as listed above are returned for error conditions. 
 * See Also:         IMStore_logout(). 
 * 
 ***************************************************************************************/ 
int IMStore_login (IMStore_t *      in_pStore, 
                   char *           in_user, 
                   char *           in_pass); 
  
/*************************************************************************************** 
 *  Lists the subs folders one level below the specified root folder. 
 * 
 * Parameters: 
 *    in_pStore           Opaque handle to store, obtained from IMStore_connect(). 
 *    in_root             Name of the root folder. Can't be NULL. 
 *    out_ppSubFolders    Message summary structures. 
 * 
 * Returns:          IMSTORE_OK on success. 
 *                   Other errors as listed above are returned for error conditions. 
 * 
 ***************************************************************************************/ 
int IMStore_listSubFolders (IMStore_t *     in_pStore, 
                            char *          in_root, 
                            char **         out_ppSubFolders); 
  
  

/*************************************************************************************** 
 * Lists the message summaries for the messages in the specified folder. 
 * 
 * Parameters: 
 *    in_pStore               Opaque handle to store, obtained from IMStore_connect(). 
 *    in_folder               Name of the folder. NULL implies INBOX. 
 *    out_ppSummary           Message summary structures. 
 * 
 * Returns:          IMSTORE_OK on success. 
 *                   Other errors as listed above are returned for error conditions. 
 * 
 ***************************************************************************************/ 
int IMStore_listMessages (IMStore_t *     	in_pStore, 
                          char *              	in_folder, 
                          IMMessageSummary_t ** out_ppSummary); 

/*************************************************************************************** 
 * Deletes the specified message in the specified folder. 
 * 
 * Parameters: 
 *    in_pStore             Opaque handle to store, obtained from IMStore_connect(). 
 *    in_folder             Name of the folder. NULL implies INBOX. 
 *    in_msgnum             Sequence number of the Message to delete. 
 * 
 * Returns:      IMSTORE_OK on success. 
 *               Other errors as listed above are returned for error conditions. 
 * 
 ***************************************************************************************/ 
int IMStore_delete (IMStore_t *     in_pStore, 
                    char *          in_folder, 
                    int             in_msgnum); 

/*************************************************************************************** 
 * Copies the specified message from the specified folder, to the specified folder. 
 * 
 * Parameters: 
 *    in_pStore            Opaque handle to store, obtained from IMStore_connect(). 
 *    in_source_folder     Name of the source folder. NULL implies INBOX. 
 *    in_dest_folder       Name of the destination folder. NULL implies INBOX. 
 *    in_msgnum            Sequence number of the Message to copy. 
 * 
 * Returns:      IMSTORE_OK on success. 
 *                   Other errors as listed above are returned for error conditions. 
 * 
 ***************************************************************************************/ 
int IMStore_copy (IMStore_t *     in_pStore, 
                  char *          in_source_folder, 
                  char *          in_dest_folder, 
                  int             in_msgnum); 

/*************************************************************************************** 
 * Moves the specified message from the specified folder, to the specified folder. 
 * 
 * Parameters: 
 *    in_pStore            Opaque handle to store, obtained from IMStore_connect(). 
 *    in_source_folder     Name of the source folder. NULL implies INBOX. 
 *    in_dest_folder       Name of the destination folder. NULL implies INBOX. 
 *    in_msgnum            Sequence number of the Message to move. 
 * 
 * Returns:      IMSTORE_OK on success. 
 *               Other errors as listed above are returned for error conditions. 
 * 
 ***************************************************************************************/ 
int IMStore_move (IMStore_t *     in_pStore, 
                  char *          in_source_folder, 
                  char *          in_dest_folder, 
                  int             in_msgnum); 

/*************************************************************************************** 
 * Retrieves the specified message from the specified folder. 
 * 
 * Parameters: 
 *    in_pStore         Opaque handle to store, obtained from IMStore_connect(). 
 *    in_folder         Name of the folder. NULL implies INBOX. 
 *    in_msgnum         Sequence number of the Message. 
 *    in_ppMsg          pointer the message buffer in MIME format. 
 * 
 * Returns:      IMSTORE_OK on success. 
 *               Other errors as listed above are returned for error conditions. 
 * 
 ***************************************************************************************/ 
int IMStore_retrieve (IMStore_t *     in_pStore, 
                      char *          in_folder, 
                      int             in_msgnum, 
                      char **         out_ppMsg); 

/*************************************************************************************** 
 * Fetches the message as specified by the imapURL parameter. This kind of fetch is useful 
 * for applications that obtain the URL from other sources (like from a webpage). 
 * NOTE: For fetch, it is not necessary to connect / login to the message store first. 
 * The URL must conform to the format specified in RFC 2192 and must represent a 
 * unique message.  An example URL is: 
 * imap://foo.com/public.messages;UIDVALIDITY=123456/;UID=789 
 * 
 * Parameters: 
 *    in_imapURL           imapURL for a unique message. 
 *    out_ppMsg            pointer the message buffer in MIME format. 
 * 
 * Returns:      IMSTORE_OK on success. 
 *               Other errors as listed above are returned for error conditions. 
 * 
 ***************************************************************************************/ 
int IMStore_fetch (char *         in_imapURL, 
                   char **        out_ppMsg); 
  

/*************************************************************************************** 
 * Searches the specified folder for the messages matching the specified criteria. 
 * 
 * Parameters: 
 *    in_pStore            Opaque handle to store, obtained from IMStore_connect(). 
 *    in_folder            Name of the folder. NULL implies INBOX. 
 *    in_pSearchCriteria   A list of IMSearchCriteria_t structures. 
 *    out_pMsgnums         Message sequence numbers of all matched messages. 
 * 
 * Returns:      IMSTORE_OK on success. 
 *               Other errors as listed above are returned for error conditions. 
 * 
 ***************************************************************************************/ 
int IMStore_search (IMStore_t *            in_pStore, 
                    char *                 in_folder, 
                    IMSearchCriteria_t *   in_pSearchCriteria, 
                    int **                 out_ppMsgnums); 

/*************************************************************************************** 
 * Logs out from the message store. Another login can be attempted or can disconnect from 
 * the store. 
 * 
 * Parameters: 
 *    in_pStore        Opaque handle to store, obtained earlier from IMStore_connect(). 
 * 
 * Returns:      IMSTORE_OK on success. 
 *               Other errors as listed above are returned for error conditions. 
 * See Also:     IMStore_login(). 
 * 
 ***************************************************************************************/ 
int IMStore_logout (IMStore_t *  in_pStore); 
  

/*************************************************************************************** 
 * Disconnects from the message store. 
 * 
 * Parameters: 
 *   in_pStore   Opaque handle to store, obtained earlier from IMStore_connect(). 
 * 
 * Returns:      IMSTORE_OK on success. 
 *               Other errors as listed above are returned for error conditions. 
 * See Also:     IMStore_connect(). 
 * 
 ***************************************************************************************/ 
int IMStore_disconnect (IMStore_t *  in_pStore); 
  

#ifdef __cplusplus 
} 
#endif 
  
  
#endif /* IMSTORE_H */ 
