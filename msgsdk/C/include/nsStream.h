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


/* input output stream utilities
 * nsStream.h
 * @author prasad@netscape.com
 * @version 1.0
 */
 
#ifndef NSIOSTREAM_H
#define NSIOSTREAM_H
 
#include "nsmail.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Creates an input-stream from a file that can be passed to all messaging 
 * SDK API calls that require a parameter of type nsmail_inputstream_t.
 * Implements the stream and the associated internal functions for read(),
 * rewind() and close().
 * 
 * Params:
 * =======
 *
 * in_fileName:           Full path name of the file to create the input-stream on. 
 *			  The file must exist.
 * out_ppRetInputStream:  The created input-stream is returned here.
 * RETURN Value:	  0 on success < 0 on failure. See nsmail.h for possible err codes.
 *
 *  USAGE:
 *  ======
 *  nsmail_inputstream_t * pInputStream;
 *
 *  ret_value = file_inputStream_create (<file-name>, &pInputStream);
 *
 *  ret_value = mime_basicPart_setDataStream (<other-params>, pInputStream);
 *
 *  When done close and free the stream.
 *  ----------------------------------
 *  pInputStream->close (pInputStream->rock);
 *  nsStream_free (pInputStream); pInputStream = NULL;
 *
 */
int file_inputStream_create (char * in_fileName, 
			     nsmail_inputstream_t ** out_ppRetInputStream);



/*
 * Creates an input-stream from a buffer that can be passed to all messaging 
 * SDK API calls that require a parameter of type nsmail_inputstream_t.
 * Implements the stream and the associated internal functions for read(),
 * rewind() and close().
 * 
 * Params:
 * =======
 *
 * in_pDataBuf:           Data buf to base the input-stream on. Should not be NULL.
 *                        A reference to the buffer is stored in the stream. Hence
 *                        The buffer should not be freed until the stream is closed.
 * in_data_size		  Size of data in in_pDataBuf.
 * out_ppRetInputStream:  The created input-stream is returned here.
 * RETURN Value:	  0 on success < 0 on failure. See nsmail.h for possible err codes.
 *
 *
 *  USAGE:
 *  ======
 *  
 *  Same as file_inputStream above.
 *
 */
int buf_inputStream_create (char * in_pDataBuf, 
			    long in_data_size,
			    nsmail_inputstream_t ** out_ppRetInputStream);


 
/*
 * Creates an output-stream from a file that can be passed to all messaging 
 * SDK API calls that require a parameter of type nsmail_outputstream_t.
 * 
 * Params:
 * =======
 *
 * in_fileName:           Full path name of the file to create the output-stream on. 
 *			  Creates the file if needed. Blows away any previous data in the file.
 * out_ppRetInputStream:  The created output-stream is returned here.
 * RETURN Value:	  0 on success < 0 on failure. See nsmail.h for possible err codes.
 * 
 *  USAGE:
 *  ======
 *  nsmail_outputstream_t * pOutputStream;
 *
 *  ret_value = file_outputStream_create (<file-name>, &pOutputStream);
 *
 *  ret_value = mime_message_putByteStream (<other-params>, pOutputStream);
 *
 *  When done close and free the stream.
 *  ----------------------------------
 *  pOutputStream->close (pOutputStream->rock);   
 *  nsStream_free (pOutputStream); pOutputStream = NULL;
 * 
 *  NOTE: The file under the stream needs to be removed separately.
 */
int file_outputStream_create (char * in_fileName, 
			      nsmail_outputstream_t ** out_ppRetOutputStream);


/* free the memory allocated for the (input/output) stream */
void nsStream_free (void * pMem);

#ifdef __cplusplus
}
#endif
 
 
#endif /* NSIOSTREAM_H */
