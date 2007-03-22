/*
    JMURLConnection.h
 */

#pragma once

#ifndef	__JMURLConnection__
#define	__JMURLConnection__

#ifdef __cplusplus
extern "C" {
#endif

#if PRAGMA_IMPORT_SUPPORTED
#pragma import on
#endif

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif

typedef void* JMURLConnectionRef;
typedef void* JMURLInputStreamRef;
typedef void* JMURLOutputStreamRef;

enum JMURLConnectionOptions
{
	eDefault			= 0,
	eNoCaching			= (1 << 2),
	eNoUserInteraction	= (1 << 3),
	eNoRedirection		= (1 << 4)
};

typedef enum JMURLConnectionOptions JMURLConnectionOptions;

typedef CALLBACK_API_C(OSStatus, JMURLOpenConnectionProcPtr)(
	/* in URL = */ JMTextRef url,
	/* in RequestMethod = */ JMTextRef requestMethod,
	/* in ConnectionOptions = */ JMURLConnectionOptions options,
	/* in AppletViewer = */ JMAppletViewerRef appletViewer,
	/* out URLConnectionRef = */ JMURLConnectionRef* urlConnectionRef
	);

typedef CALLBACK_API_C(OSStatus, JMURLCloseConnectionProcPtr)(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef
	);

typedef CALLBACK_API_C(Boolean, JMURLUsingProxyProcPtr)(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef
	);

typedef CALLBACK_API_C(OSStatus, JMURLGetCookieProcPtr)(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* out CookieValue = */ JMTextRef* cookie
	);

typedef CALLBACK_API_C(OSStatus, JMURLSetCookieProcPtr)(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* in CookieValue = */ JMTextRef cookie
	);

typedef CALLBACK_API_C(OSStatus, JMURLSetRequestPropertiesProcPtr)(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* in NumberOfProperties = */ int numberOfProperties,
	/* in PropertyNames = */ JMTextRef* keys,
	/* in Values = */ JMTextRef* value
	);

typedef CALLBACK_API_C(OSStatus, JMURLGetResponsePropertiesCountProcPtr)(
	/* in URLConnectionRef = */ JMURLInputStreamRef iStreamRef,
	/* out numberOfProperties = */ int* numberOfProperties
	);

/*
 * JManager allocates memory to hold the specified number of (properties)
 * JMTextRefs. The proc is expected to create new JMTextRefs that should
 * be shoved in the array passed. The JManager takes care of disposing
 * these JMTextRefs.
 */
typedef CALLBACK_API_C(OSStatus, JMURLGetResponsePropertiesProcPtr)(
	/* in URLConnectionRef = */ JMURLInputStreamRef iStreamRef,
	/* in numberOfProperties = */ int numberOfProperties,
	/* out PropertyNames = */ JMTextRef* keys,
	/* out Values = */ JMTextRef* values
	);

typedef CALLBACK_API_C(OSStatus, JMURLOpenInputStreamProcPtr)(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* out URLStreamRef = */ JMURLInputStreamRef* urlInputStreamRef
	);

typedef CALLBACK_API_C(OSStatus, JMURLOpenOutputStreamProcPtr)(
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* out URLOutputStreamRef = */ JMURLOutputStreamRef* urlOutputStreamRef
	);

typedef CALLBACK_API_C(OSStatus, JMURLCloseInputStreamProcPtr)(
	/* in URLInputStreamRef = */ JMURLInputStreamRef urlInputStreamRef
	);

typedef CALLBACK_API_C(OSStatus, JMURLCloseOutputStreamProcPtr)(
	/* in URLOutputStreamRef = */ JMURLOutputStreamRef urlOutputStreamRef
	);

typedef CALLBACK_API_C(OSStatus, JMURLReadProcPtr)(
	/* in URLConnectionRef = */ JMURLInputStreamRef iStreamRef,
	/* out Buffer = */ void* buffer,
	/* in BufferSize = */ UInt32 bufferSize,
	/* out BytesRead = */ SInt32* bytesRead
	);

typedef CALLBACK_API_C(OSStatus, JMURLWriteProcPtr)(
	/* in URLConnectionRef = */ JMURLOutputStreamRef oStreamRef,
	/* in Buffer = */ void* buffer,
	/* in BytesToWrite = */ SInt32 bytesToWrite
	);

struct JMURLCallbacks {
	UInt32 									fVersion;	/* should be set to kJMVersion */
	JMURLOpenConnectionProcPtr 				fOpenConnection;
	JMURLCloseConnectionProcPtr 			fCloseConnection;
	JMURLUsingProxyProcPtr					fUsingProxy;
	JMURLGetCookieProcPtr					fGetCookie;
	JMURLSetCookieProcPtr					fSetCookie;
	JMURLSetRequestPropertiesProcPtr 		fSetRequestProperties;
	JMURLGetResponsePropertiesCountProcPtr	fGetResponsePropertiesCount;
	JMURLGetResponsePropertiesProcPtr		fGetResponseProperties;
	JMURLOpenInputStreamProcPtr				fOpenInputStream;
	JMURLOpenOutputStreamProcPtr			fOpenOutputStream;
	JMURLCloseInputStreamProcPtr			fCloseInputStream;
	JMURLCloseOutputStreamProcPtr			fCloseOutputStream;
	JMURLReadProcPtr 						fRead;
	JMURLWriteProcPtr 						fWrite;
};

typedef struct JMURLCallbacks		JMURLCallbacks;

EXTERN_API_C(OSStatus)
JMURLSetCallbacks(
	JMSessionRef session,
	const char* protocol,
	JMURLCallbacks* cb
	);

EXTERN_API_C(OSStatus)
JMURLOpenConnection(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URL = */ JMTextRef url,
	/* in RequestMethod = */ JMTextRef requestMethod,
	/* in ConnectionOptions = */ JMURLConnectionOptions options,
	/* in AppletViewer = */ JMAppletViewerRef appletViewer,
	/* out URLConnectionRef = */ JMURLConnectionRef* urlConnectionRef
	);

EXTERN_API_C(OSStatus)
JMURLCloseConnection(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef
	);

EXTERN_API_C(Boolean)
JMURLUsingProxy(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef
	);

EXTERN_API_C(OSStatus)
JMURLGetCookie(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* out CookieValue = */ JMTextRef* cookie
	);

EXTERN_API_C(OSStatus)
JMURLSetCookie(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* in CookieValue = */ JMTextRef cookie
	);

EXTERN_API_C(OSStatus)
JMURLSetRequestProperties(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* in NumberOfProperties = */ int numberOfProperties,
	/* in PropertyNames = */ JMTextRef* keys,
	/* in Values = */ JMTextRef* value
	);

EXTERN_API_C(OSStatus)
JMURLGetResponsePropertiesCount(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in InputStreamRef = */ JMURLInputStreamRef iStreamRef,
	/* out numberOfProperties = */ int* numberOfProperties
	);

EXTERN_API_C(OSStatus)
JMURLGetResponseProperties(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in InputStreamRef = */ JMURLInputStreamRef iStreamRef,
	/* in numberOfProperties = */ int numberOfProperties,
	/* out PropertyNames = */ JMTextRef* keys,
	/* out Values = */ JMTextRef* values
	);

EXTERN_API_C(OSStatus)
JMURLOpenInputStream(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* out URLInputStreamRef = */ JMURLInputStreamRef* urlInputStreamRef
	);

EXTERN_API_C(OSStatus)
JMURLOpenOutputStream(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URLConnectionRef = */ JMURLConnectionRef urlConnectionRef,
	/* out URLOutputStreamRef = */ JMURLOutputStreamRef* urlOutputStreamRef
	);

EXTERN_API_C(OSStatus)
JMURLCloseInputStream(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URLInputStreamRef = */ JMURLInputStreamRef urlInputStreamRef
	);

EXTERN_API_C(OSStatus)
JMURLCloseOutputStream(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URLOutputStreamRef = */ JMURLOutputStreamRef urlOutputStreamRef
	);

EXTERN_API_C(OSStatus)
JMURLRead(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URLInputStreamRef = */ JMURLInputStreamRef urlInputStreamRef,
	/* out Buffer = */ void* buffer,
	/* in BufferSize = */ UInt32 bufferSize,
	/* out BytesRead = */ SInt32* bytesRead
	);

EXTERN_API_C(OSStatus)
JMURLWrite(
	/* in JMSessionRef = */ JMSessionRef session,
	/* in URLOutputStreamRef = */ JMURLOutputStreamRef urlOutputStreamRef,
	/* in Buffer = */ void* buffer,
	/* in BytesToWrite = */ SInt32 bytesToWrite
	);

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

#if PRAGMA_IMPORT_SUPPORTED
#pragma import off
#endif

#ifdef __cplusplus
}
#endif

#endif	/* __JMURLConnection__ */

