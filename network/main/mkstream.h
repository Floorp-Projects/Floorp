/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#ifndef MKSTREAM_H
#define MKSTREAM_H

/* prints out all converter mime types during successive calls
 * call with first equal true to reset to the beginning
 * and with first equal false to print out the next
 * converter in the list.  Returns zero when all converters
 * are printed out.
 */
extern char *
XP_ListNextPresentationType(PRBool first);

/* prints out all encoding mime types during successive calls
 * call with first equal true to reset to the beginning
 * and with first equal false to print out the next
 * converter in the list.  Returns zero when all converters
 * are printed out.
 */
extern char *
XP_ListNextEncodingType(PRBool first);

/* register a mime type and a command to be executed
 */
extern PUBLIC void
NET_RegisterExternalViewerCommand(char * format_in, 
								  char * system_command, 
								  unsigned int stream_block_size);

/* removes all external viewer commands
 */
extern void NET_ClearExternalViewerConverters(void);

MODULE_PRIVATE void
NET_RegisterExternalConverterCommand(char * format_in,
                                     FO_Present_Types format_out,
                                     char * system_command,
                                     char * new_format);

#ifdef DEBUG 
extern void NET_DisplayStreamInfoAsHTML(ActiveEntry *cur_entry);
#endif /* DEBUG */

void
NET_RegisterAllEncodingConverters (char *format_in,
								   FO_Present_Types format_out);

/* register an encoding converter that is used for everything,
 * no exeptions
 * this is necessary for chunked encoding
 */
void
NET_RegisterUniversalEncodingConverter(char *encoding_in,
                              void          * data_obj,
                              NET_Converter * converter_func);

void
NET_DumpDecoders();

#endif  /* MKSTREAM.h */
