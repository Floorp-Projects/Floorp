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
* util.h
* prasad
* carsonl, oct 10,97
*/ 

#ifndef UTIL_H
#define UTIL_H

#ifdef __cplusplus
extern "C" {
#endif


#define null					NULL
#define TRUE					1
#define FALSE					0
#define LOG_FILE				"error.log"
#define BASE64_NOFIT -1
#define BASE64_SOMELEFT -2



/* log structure */
typedef struct errorLog
{
	FILE *m_pFile;				/* file pointer */
	char m_achFilename[128];		/* log filename */
	BOOLEAN m_bActive;			/* TRUE if you want output to log file */

} errorLog_t;


typedef struct buf_instream_tracker
{
	char * pDataBuf;			/* data buffer */
	long total_size;			/* totoal buffer size */
	long bytes_read;			/* number of bytes read */
	char * pNext;					

} buf_instream_tracker_t;


typedef struct  file_instream_tracker
{
     FILE * fp;
     char * file_name;
     long bytes_read;

} file_instream_tracker_t;

typedef struct  file_outstream_tracker
{
     FILE * fp;
     char * file_name;
     long bytes_written;

} file_outstream_tracker_t;



/* string utilities */
int bStringEquals( char *s1, char *s2 );		/* equals */
char *szTrim( char *s );				/* all trim */
char *szRTrim( char *s );				/* right trim */
char *szLTrim( char *s );				/* left trim */
char *szStringClone( char *s );				/* clone */
BOOLEAN bIsBasicPart( int nType );			/* test if type is a basicpart */


/* base64 utilities */
int decodeBase64  (char *szInput, char *szOutput, 
		   int nInputBufferSize, int nMaxBufferSize, 
		   int *pOut_byte, int *pOut_bits);

int decodeBase64Vector  (int nStart, int nEnd, Vector *v, 
			char **szDecodedBuffer, int nRawMessageSize, 
			int *pOut_byte, int *pOut_bits );

/* quoted printable utilities */
int decodeQP (char *szInput, char *szOutput, int nMaxBufferSize);
int decodeQPVector (int nStart, int nEnd, Vector *v, 
		    char **szDecodedBuffer, int nRawMessageSize, char *szLeftOverBytes, 
		    int *nNoOfLeftOverBytes);

/* misc */
char *decodeHeader (char *szInputString);
int nConvertHexToDec (int nFirstByte, int nSecondByte);
char * generateBoundary();
int append_str (char * dest, char * src);

/* If s1 equals s2, returns TRUE, FALSE otherwise. */
BOOLEAN equalsIgnoreCase (char * s1, char * s2);

char * getFileExtn (char * fileName);
char * getFileShortName (char * fileName);

/* inputstream utilities */
long get_inputstream_size (struct nsmail_inputstream * pTheStream);
int buf_instream_create (char * pDataBuf, long data_size, 
			 struct nsmail_inputstream ** ppRetInputStream);
int buf_instream_read (void * rock, char *buf, int size);
void buf_instream_rewind (void * rock);
void buf_instream_close (void * rock);

int file_instream_create (char * fileName, struct nsmail_inputstream ** ppRetInputStream);
void file_instream_close (void * rock);
void file_instream_rewind (void * rock);
int file_instream_read (void * rock, char *buf, int size);

int file_outstream_create (char * fileName, nsmail_outputstream_t ** ppRetOutputStream);
void file_outstream_close (void * rock);


/* log utilities */
errorLog_t *errorLog_new( char *szFilename );	/* constructor */
void errorLog_free( errorLog_t *pLog );		/* destructor, calls closeLog() automatically */
void initErrorLog( char *szFilename );		/* init */
void closeErrorLog();				/* close log */
void errorLog (char *szOwner, int nError);	/* report error to log */
void errorLog2 (errorLog_t *pLog, char *szOwner, int nError);	/* report error to log */
void errorLogMsg( char *szMsg );		/* display custom error message */
void errorLogMsg2 (errorLog_t *pLog, char *szMsg);	/* display custom error message */
void errorLogOn();				/* turn log on */
void errorLogOff();				/* turn log off */

#ifdef __cplusplus
}
#endif


#endif /* UTIL_H */
