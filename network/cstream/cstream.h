/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef CSTREAM_H
#define CSTREAM_H

XP_BEGIN_PROTOS

/* builds an outgoing stream and returns a stream class structure
 * containing a stream function table
 */
extern NET_StreamClass * NET_StreamBuilder (
        FO_Present_Types  format_out,
        URL_Struct *      anchor,
        MWContext *       window_id);

/* stream functions
 */
typedef unsigned int
(*MKStreamWriteReadyFunc) (NET_StreamClass *stream);

#define MAX_WRITE_READY (((unsigned) (~0) << 1) >> 1)   /* must be <= than MAXINT */

typedef int
(*MKStreamWriteFunc) (NET_StreamClass *stream, const char *str, int32 len);

typedef void
(*MKStreamCompleteFunc) (NET_StreamClass *stream);

typedef void
(*MKStreamAbortFunc) (NET_StreamClass *stream, int status);

/*
 *  NewMKStream
 *  Utility to create a new initialized NET_StreamClass object
 */
extern NET_StreamClass * NET_NewStream(char *,
                                       MKStreamWriteFunc,
                                       MKStreamCompleteFunc,
                                       MKStreamAbortFunc,
                                       MKStreamWriteReadyFunc,
                                       void *,
                                       MWContext *);
/* streamclass function
 */
struct _NET_StreamClass {

    char      * name;          /* Just for diagnostics */

    MWContext * window_id;     /* used for progress messages, etc. */

    void      * data_object;   /* a pointer to whatever
                                * structure you wish to have
                                * passed to the routines below
                                * during writes, etc...
                                *
                                * this data object should hold
                                * the document, document
                                * structure or a pointer to the
                                * document.
                                */

    MKStreamWriteReadyFunc  is_write_ready;   /* checks to see if the stream is ready
   						* for writing.  Returns 0 if not ready
   						* or the number of bytes that it can
   						* accept for write
   						*/

    MKStreamWriteFunc       put_block;        /* writes a block of data to the stream */
    MKStreamCompleteFunc    complete;         /* normal end */
    MKStreamAbortFunc       abort;            /* abnormal end */

    Bool                    is_multipart;    /* is the stream part of a multipart sequence */
};



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
extern void
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

XP_END_PROTOS

#endif  /* CSTREAM.h */
