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

/*
* Example dynamic parsing program
*/ 

#ifndef TESTAPP_H
#define TESTAPP_H

#ifdef __cplusplus
extern "C" {
#endif


#define BUFFER_SIZE2		256


void dynamicParseEntireFile (char *szFilename);

void mimeDataSink2_header (mimeDataSinkPtr_t pSink,void *pCallbackObject, char *name, char *value);
void mimeDataSink2_addHeader (mimeDataSinkPtr_t pSink,void *pCallbackObject, char *name, char *value);
void mimeDataSink2_contentType (mimeDataSinkPtr_t pSink, void *pCallbackObject, int nContentType);
void mimeDataSink2_contentSubType (mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentSubType);
void mimeDataSink2_contentTypeParams (mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentTypeParams);
void mimeDataSink2_contentID (mimeDataSinkPtr_t pSink, void *pCallbackObject, char *contentID);
void mimeDataSink2_contentMD5 (mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentMD5);
void mimeDataSink2_contentDisposition (mimeDataSinkPtr_t pSink, void *pCallbackObject, int nContentDisposition);
void mimeDataSink2_contentDispParams (mimeDataSinkPtr_t pSink, void *pCallbackObject, char * contentDispParams);
void mimeDataSink2_contentDescription (mimeDataSinkPtr_t pSink, void *pCallbackObject, char *contentDescription);
void mimeDataSink2_contentEncoding (mimeDataSinkPtr_t pSink, void *pCallbackObject, int nContentEncoding);
void *mimeDataSink2_startMessageLocalStorage (mimeDataSinkPtr_t pSink, mime_message_t *m);
void *mimeDataSink2_startMessage (mimeDataSinkPtr_t pSink);
void mimeDataSink2_endMessage (mimeDataSinkPtr_t pSink, void *pCallbackObject);
void *mimeDataSink2_startBasicPartLocalStorage (mimeDataSinkPtr_t pSink, mime_basicPart_t *m);
void *mimeDataSink2_startBasicPart (mimeDataSinkPtr_t pSink);
void mimeDataSink2_bodyData (mimeDataSinkPtr_t pSink, void *pCallbackObject, char bodyData[], int len);
void mimeDataSink2_endBasicPart (mimeDataSinkPtr_t pSink, void *pCallbackObject);
void *mimeDataSink2_startMultiPartLocalStorage (mimeDataSinkPtr_t pSink, mime_multiPart_t *m);
void *mimeDataSink2_startMultiPart (mimeDataSinkPtr_t pSink);
void mimeDataSink2_boundary (mimeDataSinkPtr_t pSink, void *pCallbackObject, char * boundary);
void mimeDataSink2_endMultiPart (mimeDataSinkPtr_t pSink, void *pCallbackObject);
void *mimeDataSink2_startMessagePartLocalStorage (mimeDataSinkPtr_t pSink, mime_messagePart_t *m);
void *mimeDataSink2_startMessagePart (mimeDataSinkPtr_t pSink);
void mimeDataSink2_endMessagePart (mimeDataSinkPtr_t pSink, void *pCallbackObject);


#ifdef __cplusplus
}
#endif


#endif /* TESTAPP_H */
