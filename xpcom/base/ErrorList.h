// IWYU pragma: private, include "nsError.h"
/* Helper file for nsError.h, via preprocessor magic */
  /* Standard "it worked" return value */
  ERROR(NS_OK,  0),

  /* ======================================================================= */
  /* Core errors, not part of any modules */
  /* ======================================================================= */
  ERROR(NS_ERROR_BASE,                          0xC1F30000),
  /* Returned when an instance is not initialized */
  ERROR(NS_ERROR_NOT_INITIALIZED,               NS_ERROR_BASE + 1),
  /* Returned when an instance is already initialized */
  ERROR(NS_ERROR_ALREADY_INITIALIZED,           NS_ERROR_BASE + 2),
  /* Returned by a not implemented function */
  ERROR(NS_ERROR_NOT_IMPLEMENTED,               0x80004001),
  /* Returned when a given interface is not supported. */
  ERROR(NS_NOINTERFACE,                         0x80004002),
  ERROR(NS_ERROR_NO_INTERFACE,                  NS_NOINTERFACE),
  /* Returned when a function aborts */
  ERROR(NS_ERROR_ABORT,                         0x80004004),
  /* Returned when a function fails */
  ERROR(NS_ERROR_FAILURE,                       0x80004005),
  /* Returned when an unexpected error occurs */
  ERROR(NS_ERROR_UNEXPECTED,                    0x8000ffff),
  /* Returned when a memory allocation fails */
  ERROR(NS_ERROR_OUT_OF_MEMORY,                 0x8007000e),
  /* Returned when an illegal value is passed */
  ERROR(NS_ERROR_ILLEGAL_VALUE,                 0x80070057),
  ERROR(NS_ERROR_INVALID_ARG,                   NS_ERROR_ILLEGAL_VALUE),
  ERROR(NS_ERROR_INVALID_POINTER,               NS_ERROR_INVALID_ARG),
  ERROR(NS_ERROR_NULL_POINTER,                  NS_ERROR_INVALID_ARG),
  /* Returned when a class doesn't allow aggregation */
  ERROR(NS_ERROR_NO_AGGREGATION,                0x80040110),
  /* Returned when an operation can't complete due to an unavailable resource */
  ERROR(NS_ERROR_NOT_AVAILABLE,                 0x80040111),
  /* Returned when a class is not registered */
  ERROR(NS_ERROR_FACTORY_NOT_REGISTERED,        0x80040154),
  /* Returned when a class cannot be registered, but may be tried again later */
  ERROR(NS_ERROR_FACTORY_REGISTER_AGAIN,        0x80040155),
  /* Returned when a dynamically loaded factory couldn't be found */
  ERROR(NS_ERROR_FACTORY_NOT_LOADED,            0x800401f8),
  /* Returned when a factory doesn't support signatures */
  ERROR(NS_ERROR_FACTORY_NO_SIGNATURE_SUPPORT,  NS_ERROR_BASE + 0x101),
  /* Returned when a factory already is registered */
  ERROR(NS_ERROR_FACTORY_EXISTS,                NS_ERROR_BASE + 0x100),


  /* ======================================================================= */
  /* 1: NS_ERROR_MODULE_XPCOM */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_XPCOM
  /* Result codes used by nsIVariant */
  ERROR(NS_ERROR_CANNOT_CONVERT_DATA,       FAILURE(1)),
  ERROR(NS_ERROR_OBJECT_IS_IMMUTABLE,       FAILURE(2)),
  ERROR(NS_ERROR_LOSS_OF_SIGNIFICANT_DATA,  FAILURE(3)),
  /* Result code used by nsIThreadManager */
  ERROR(NS_ERROR_NOT_SAME_THREAD,           FAILURE(4)),
  /* Various operations are not permitted during XPCOM shutdown and will fail
   * with this exception. */
  ERROR(NS_ERROR_ILLEGAL_DURING_SHUTDOWN,   FAILURE(30)),
  ERROR(NS_ERROR_SERVICE_NOT_AVAILABLE,     FAILURE(22)),

  ERROR(NS_SUCCESS_LOSS_OF_INSIGNIFICANT_DATA,  SUCCESS(1)),
  /* Used by nsCycleCollectionParticipant */
  ERROR(NS_SUCCESS_INTERRUPTED_TRAVERSE,        SUCCESS(2)),
  /* DEPRECATED */
  ERROR(NS_ERROR_SERVICE_NOT_FOUND,             SUCCESS(22)),
  /* DEPRECATED */
  ERROR(NS_ERROR_SERVICE_IN_USE,                SUCCESS(23)),
#undef MODULE


  /* ======================================================================= */
  /* 2: NS_ERROR_MODULE_BASE */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_BASE
  /* I/O Errors */

  /*  Stream closed */
  ERROR(NS_BASE_STREAM_CLOSED,          FAILURE(2)),
  /*  Error from the operating system */
  ERROR(NS_BASE_STREAM_OSERROR,         FAILURE(3)),
  /*  Illegal arguments */
  ERROR(NS_BASE_STREAM_ILLEGAL_ARGS,    FAILURE(4)),
  /*  For unichar streams */
  ERROR(NS_BASE_STREAM_NO_CONVERTER,    FAILURE(5)),
  /*  For unichar streams */
  ERROR(NS_BASE_STREAM_BAD_CONVERSION,  FAILURE(6)),
  ERROR(NS_BASE_STREAM_WOULD_BLOCK,     FAILURE(7)),
#undef MODULE


  /* ======================================================================= */
  /* 3: NS_ERROR_MODULE_GFX */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_GFX
  /* no printer available (e.g. cannot find _any_ printer) */
  ERROR(NS_ERROR_GFX_PRINTER_NO_PRINTER_AVAILABLE,        FAILURE(1)),
  /* _specified_ (by name) printer not found */
  ERROR(NS_ERROR_GFX_PRINTER_NAME_NOT_FOUND,              FAILURE(2)),
  /* print-to-file: could not open output file */
  ERROR(NS_ERROR_GFX_PRINTER_COULD_NOT_OPEN_FILE,         FAILURE(3)),
  /* print: starting document */
  ERROR(NS_ERROR_GFX_PRINTER_STARTDOC,                    FAILURE(4)),
  /* print: ending document */
  ERROR(NS_ERROR_GFX_PRINTER_ENDDOC,                      FAILURE(5)),
  /* print: starting page */
  ERROR(NS_ERROR_GFX_PRINTER_STARTPAGE,                   FAILURE(6)),
  /* The document is still being loaded */
  ERROR(NS_ERROR_GFX_PRINTER_DOC_IS_BUSY,                 FAILURE(7)),

  /* Font cmap is strangely structured - avoid this font! */
  ERROR(NS_ERROR_GFX_CMAP_MALFORMED,                      FAILURE(51)),
#undef MODULE


  /* ======================================================================= */
  /* 6: NS_ERROR_MODULE_NETWORK */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_NETWORK
  /* General async request error codes:
   *
   * These error codes are commonly passed through callback methods to indicate
   * the status of some requested async request.
   *
   * For example, see nsIRequestObserver::onStopRequest.
   */

  /* The async request completed successfully. */
  ERROR(NS_BINDING_SUCCEEDED,  NS_OK),

  /* The async request failed for some unknown reason.  */
  ERROR(NS_BINDING_FAILED,      FAILURE(1)),
  /* The async request failed because it was aborted by some user action. */
  ERROR(NS_BINDING_ABORTED,     FAILURE(2)),
  /* The async request has been "redirected" to a different async request.
   * (e.g., an HTTP redirect occurred).
   *
   * This error code is used with load groups to notify the load group observer
   * when a request in the load group is redirected to another request. */
  ERROR(NS_BINDING_REDIRECTED,  FAILURE(3)),
  /* The async request has been "retargeted" to a different "handler."
   *
   * This error code is used with load groups to notify the load group observer
   * when a request in the load group is removed from the load group and added
   * to a different load group. */
  ERROR(NS_BINDING_RETARGETED,  FAILURE(4)),

  /* Miscellaneous error codes: These errors are not typically passed via
   * onStopRequest. */

  /* The URI is malformed. */
  ERROR(NS_ERROR_MALFORMED_URI,                       FAILURE(10)),
  /* The requested action could not be completed while the object is busy.
   * Implementations of nsIChannel::asyncOpen will commonly return this error
   * if the channel has already been opened (and has not yet been closed). */
  ERROR(NS_ERROR_IN_PROGRESS,                         FAILURE(15)),
  /* Returned from nsIChannel::asyncOpen to indicate that OnDataAvailable will
   * not be called because there is no content available.  This is used by
   * helper app style protocols (e.g., mailto).  XXX perhaps this should be a
   * success code. */
  ERROR(NS_ERROR_NO_CONTENT,                          FAILURE(17)),
  /* The URI scheme corresponds to an unknown protocol handler. */
  ERROR(NS_ERROR_UNKNOWN_PROTOCOL,                    FAILURE(18)),
  /* The content encoding of the source document was incorrect, for example
   * returning a plain HTML document advertised as Content-Encoding: gzip */
  ERROR(NS_ERROR_INVALID_CONTENT_ENCODING,            FAILURE(27)),
  /* A transport level corruption was found in the source document. for example
   * a document with a calculated checksum that does not match the Content-MD5
   * http header. */
  ERROR(NS_ERROR_CORRUPTED_CONTENT,                   FAILURE(29)),
  /* While parsing for the first component of a header field using syntax as in
   * Content-Disposition or Content-Type, the first component was found to be
   * empty, such as in: Content-Disposition: ; filename=foo */
  ERROR(NS_ERROR_FIRST_HEADER_FIELD_COMPONENT_EMPTY,  FAILURE(34)),
  /* Returned from nsIChannel::asyncOpen when trying to open the channel again
   * (reopening is not supported). */
  ERROR(NS_ERROR_ALREADY_OPENED,                      FAILURE(73)),

  /* Connectivity error codes: */

  /* The connection is already established.  XXX unused - consider removing. */
  ERROR(NS_ERROR_ALREADY_CONNECTED,         FAILURE(11)),
  /* The connection does not exist.  XXX unused - consider removing. */
  ERROR(NS_ERROR_NOT_CONNECTED,             FAILURE(12)),
  /* The connection attempt failed, for example, because no server was
   * listening at specified host:port. */
  ERROR(NS_ERROR_CONNECTION_REFUSED,        FAILURE(13)),
  /* The connection was lost due to a timeout error.  */
  ERROR(NS_ERROR_NET_TIMEOUT,               FAILURE(14)),
  /* The requested action could not be completed while the networking library
   * is in the offline state. */
  ERROR(NS_ERROR_OFFLINE,                   FAILURE(16)),
  /* The requested action was prohibited because it would have caused the
   * networking library to establish a connection to an unsafe or otherwise
   * banned port. */
  ERROR(NS_ERROR_PORT_ACCESS_NOT_ALLOWED,   FAILURE(19)),
  /* The connection was established, but no data was ever received. */
  ERROR(NS_ERROR_NET_RESET,                 FAILURE(20)),
  /* The connection was established, but the data transfer was interrupted. */
  ERROR(NS_ERROR_NET_INTERRUPT,             FAILURE(71)),
  /* The connection attempt to a proxy failed. */
  ERROR(NS_ERROR_PROXY_CONNECTION_REFUSED,  FAILURE(72)),
  /* A transfer was only partially done when it completed. */
  ERROR(NS_ERROR_NET_PARTIAL_TRANSFER,      FAILURE(76)),

  /* XXX really need to better rationalize these error codes.  are consumers of
   * necko really expected to know how to discern the meaning of these?? */
  /* This request is not resumable, but it was tried to resume it, or to
   * request resume-specific data. */
  ERROR(NS_ERROR_NOT_RESUMABLE,        FAILURE(25)),
  /* The request failed as a result of a detected redirection loop.  */
  ERROR(NS_ERROR_REDIRECT_LOOP,        FAILURE(31)),
  /* It was attempted to resume the request, but the entity has changed in the
   * meantime. */
  ERROR(NS_ERROR_ENTITY_CHANGED,       FAILURE(32)),
  /* The request failed because the content type returned by the server was not
   * a type expected by the channel (for nested channels such as the JAR
   * channel). */
  ERROR(NS_ERROR_UNSAFE_CONTENT_TYPE,  FAILURE(74)),
  /* The request failed because the user tried to access to a remote XUL
   * document from a website that is not in its white-list. */
  ERROR(NS_ERROR_REMOTE_XUL,           FAILURE(75)),


  /* FTP specific error codes: */

  ERROR(NS_ERROR_FTP_LOGIN,  FAILURE(21)),
  ERROR(NS_ERROR_FTP_CWD,    FAILURE(22)),
  ERROR(NS_ERROR_FTP_PASV,   FAILURE(23)),
  ERROR(NS_ERROR_FTP_PWD,    FAILURE(24)),
  ERROR(NS_ERROR_FTP_LIST,   FAILURE(28)),

  /* DNS specific error codes: */

  /* The lookup of a hostname failed.  This generally refers to the hostname
   * from the URL being loaded. */
  ERROR(NS_ERROR_UNKNOWN_HOST,           FAILURE(30)),
  /* A low or medium priority DNS lookup failed because the pending queue was
   * already full. High priorty (the default) always makes room */
  ERROR(NS_ERROR_DNS_LOOKUP_QUEUE_FULL,  FAILURE(33)),
  /* The lookup of a proxy hostname failed.  If a channel is configured to
   * speak to a proxy server, then it will generate this error if the proxy
   * hostname cannot be resolved. */
  ERROR(NS_ERROR_UNKNOWN_PROXY_HOST,     FAILURE(42)),


  /* Socket specific error codes: */

  /* The specified socket type does not exist. */
  ERROR(NS_ERROR_UNKNOWN_SOCKET_TYPE,   FAILURE(51)),
  /* The specified socket type could not be created. */
  ERROR(NS_ERROR_SOCKET_CREATE_FAILED,  FAILURE(52)),
  /* The operating system doesn't support the given type of address. */
  ERROR(NS_ERROR_SOCKET_ADDRESS_NOT_SUPPORTED, FAILURE(53)),
  /* The address to which we tried to bind the socket was busy. */
  ERROR(NS_ERROR_SOCKET_ADDRESS_IN_USE, FAILURE(54)),

  /* Cache specific error codes: */
  ERROR(NS_ERROR_CACHE_KEY_NOT_FOUND,        FAILURE(61)),
  ERROR(NS_ERROR_CACHE_DATA_IS_STREAM,       FAILURE(62)),
  ERROR(NS_ERROR_CACHE_DATA_IS_NOT_STREAM,   FAILURE(63)),
  ERROR(NS_ERROR_CACHE_WAIT_FOR_VALIDATION,  FAILURE(64)),
  ERROR(NS_ERROR_CACHE_ENTRY_DOOMED,         FAILURE(65)),
  ERROR(NS_ERROR_CACHE_READ_ACCESS_DENIED,   FAILURE(66)),
  ERROR(NS_ERROR_CACHE_WRITE_ACCESS_DENIED,  FAILURE(67)),
  ERROR(NS_ERROR_CACHE_IN_USE,               FAILURE(68)),
  /* Error passed through onStopRequest if the document could not be fetched
   * from the cache. */
  ERROR(NS_ERROR_DOCUMENT_NOT_CACHED,        FAILURE(70)),

  /* Effective TLD Service specific error codes: */

  /* The requested number of domain levels exceeds those present in the host
   * string. */
  ERROR(NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS,  FAILURE(80)),
  /* The host string is an IP address. */
  ERROR(NS_ERROR_HOST_IS_IP_ADDRESS,          FAILURE(81)),


  /* StreamLoader specific result codes: */

  /* Result code returned by nsIStreamLoaderObserver to indicate that the
   * observer is taking over responsibility for the data buffer, and the loader
   * should NOT free it. */
  ERROR(NS_SUCCESS_ADOPTED_DATA,  SUCCESS(90)),

  /* FTP */
  ERROR(NS_NET_STATUS_BEGIN_FTP_TRANSACTION,  SUCCESS(27)),
  ERROR(NS_NET_STATUS_END_FTP_TRANSACTION,    SUCCESS(28)),

  /* This success code may be returned by nsIAuthModule::getNextToken to
   * indicate that the authentication is finished and thus there's no need
   * to call getNextToken again. */
  ERROR(NS_SUCCESS_AUTH_FINISHED,  SUCCESS(40)),

  /* These are really not "results", they're statuses, used by nsITransport and
   * friends.  This is abuse of nsresult, but we'll put up with it for now. */
  /* nsITransport */
  ERROR(NS_NET_STATUS_READING,  FAILURE(8)),
  ERROR(NS_NET_STATUS_WRITING,  FAILURE(9)),

  /* nsISocketTransport */
  ERROR(NS_NET_STATUS_RESOLVING_HOST,  FAILURE(3)),
  ERROR(NS_NET_STATUS_RESOLVED_HOST,   FAILURE(11)),
  ERROR(NS_NET_STATUS_CONNECTING_TO,   FAILURE(7)),
  ERROR(NS_NET_STATUS_CONNECTED_TO,    FAILURE(4)),
  ERROR(NS_NET_STATUS_SENDING_TO,      FAILURE(5)),
  ERROR(NS_NET_STATUS_WAITING_FOR,     FAILURE(10)),
  ERROR(NS_NET_STATUS_RECEIVING_FROM,  FAILURE(6)),
#undef MODULE


  /* ======================================================================= */
  /* 7: NS_ERROR_MODULE_PLUGINS */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_PLUGINS
  ERROR(NS_ERROR_PLUGINS_PLUGINSNOTCHANGED,        FAILURE(1000)),
  ERROR(NS_ERROR_PLUGIN_DISABLED,                  FAILURE(1001)),
  ERROR(NS_ERROR_PLUGIN_BLOCKLISTED,               FAILURE(1002)),
  ERROR(NS_ERROR_PLUGIN_TIME_RANGE_NOT_SUPPORTED,  FAILURE(1003)),
  ERROR(NS_ERROR_PLUGIN_CLICKTOPLAY,               FAILURE(1004)),
#undef MODULE


  /* ======================================================================= */
  /* 8: NS_ERROR_MODULE_LAYOUT */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_LAYOUT
  /* Return code for nsITableLayout */
  ERROR(NS_TABLELAYOUT_CELL_NOT_FOUND,  SUCCESS(0)),
  /* Return code for nsFrame::GetNextPrevLineFromeBlockFrame */
  ERROR(NS_POSITION_BEFORE_TABLE,       SUCCESS(3)),
  /** Return codes for nsPresState::GetProperty() */
  /* Returned if the property exists */
  ERROR(NS_STATE_PROPERTY_EXISTS,       NS_OK),
  /* Returned if the property does not exist */
  ERROR(NS_STATE_PROPERTY_NOT_THERE,    SUCCESS(5)),
#undef MODULE


  /* ======================================================================= */
  /* 9: NS_ERROR_MODULE_HTMLPARSER */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_HTMLPARSER
  ERROR(NS_ERROR_HTMLPARSER_CONTINUE,  NS_OK),

  ERROR(NS_ERROR_HTMLPARSER_EOF,                        FAILURE(1000)),
  ERROR(NS_ERROR_HTMLPARSER_UNKNOWN,                    FAILURE(1001)),
  ERROR(NS_ERROR_HTMLPARSER_CANTPROPAGATE,              FAILURE(1002)),
  ERROR(NS_ERROR_HTMLPARSER_CONTEXTMISMATCH,            FAILURE(1003)),
  ERROR(NS_ERROR_HTMLPARSER_BADFILENAME,                FAILURE(1004)),
  ERROR(NS_ERROR_HTMLPARSER_BADURL,                     FAILURE(1005)),
  ERROR(NS_ERROR_HTMLPARSER_INVALIDPARSERCONTEXT,       FAILURE(1006)),
  ERROR(NS_ERROR_HTMLPARSER_INTERRUPTED,                FAILURE(1007)),
  ERROR(NS_ERROR_HTMLPARSER_BLOCK,                      FAILURE(1008)),
  ERROR(NS_ERROR_HTMLPARSER_BADTOKENIZER,               FAILURE(1009)),
  ERROR(NS_ERROR_HTMLPARSER_BADATTRIBUTE,               FAILURE(1010)),
  ERROR(NS_ERROR_HTMLPARSER_UNRESOLVEDDTD,              FAILURE(1011)),
  ERROR(NS_ERROR_HTMLPARSER_MISPLACEDTABLECONTENT,      FAILURE(1012)),
  ERROR(NS_ERROR_HTMLPARSER_BADDTD,                     FAILURE(1013)),
  ERROR(NS_ERROR_HTMLPARSER_BADCONTEXT,                 FAILURE(1014)),
  ERROR(NS_ERROR_HTMLPARSER_STOPPARSING,                FAILURE(1015)),
  ERROR(NS_ERROR_HTMLPARSER_UNTERMINATEDSTRINGLITERAL,  FAILURE(1016)),
  ERROR(NS_ERROR_HTMLPARSER_HIERARCHYTOODEEP,           FAILURE(1017)),
  ERROR(NS_ERROR_HTMLPARSER_FAKE_ENDTAG,                FAILURE(1018)),
  ERROR(NS_ERROR_HTMLPARSER_INVALID_COMMENT,            FAILURE(1019)),

  ERROR(NS_HTMLTOKENS_NOT_AN_ENTITY,       SUCCESS(2000)),
  ERROR(NS_HTMLPARSER_VALID_META_CHARSET,  SUCCESS(3000)),
#undef MODULE


  /* ======================================================================= */
  /* 10: NS_ERROR_MODULE_RDF */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_RDF
  /* Returned from nsIRDFDataSource::Assert() and Unassert() if the assertion
   * (or unassertion was accepted by the datasource */
  ERROR(NS_RDF_ASSERTION_ACCEPTED,  NS_OK),
  /* Returned from nsIRDFCursor::Advance() if the cursor has no more
   * elements to enumerate */
  ERROR(NS_RDF_CURSOR_EMPTY,        SUCCESS(1)),
  /* Returned from nsIRDFDataSource::GetSource() and GetTarget() if the
   * source/target has no value */
  ERROR(NS_RDF_NO_VALUE,            SUCCESS(2)),
  /* Returned from nsIRDFDataSource::Assert() and Unassert() if the assertion
   * (or unassertion) was rejected by the datasource; i.e., the datasource was
   * not willing to record the statement. */
  ERROR(NS_RDF_ASSERTION_REJECTED,  SUCCESS(3)),
  /* Return this from rdfITripleVisitor to stop cycling */
  ERROR(NS_RDF_STOP_VISIT,          SUCCESS(4)),
#undef MODULE


  /* ======================================================================= */
  /* 11: NS_ERROR_MODULE_UCONV */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_UCONV
  ERROR(NS_ERROR_UCONV_NOCONV,       FAILURE(1)),
  ERROR(NS_ERROR_UDEC_ILLEGALINPUT,  FAILURE(14)),

  ERROR(NS_SUCCESS_USING_FALLBACK_LOCALE,  SUCCESS(2)),
  ERROR(NS_OK_UDEC_EXACTLENGTH,            SUCCESS(11)),
  ERROR(NS_OK_UDEC_MOREINPUT,              SUCCESS(12)),
  ERROR(NS_OK_UDEC_MOREOUTPUT,             SUCCESS(13)),
  ERROR(NS_OK_UDEC_NOBOMFOUND,             SUCCESS(14)),
  ERROR(NS_OK_UENC_EXACTLENGTH,            SUCCESS(33)),
  ERROR(NS_OK_UENC_MOREOUTPUT,             SUCCESS(34)),
  ERROR(NS_ERROR_UENC_NOMAPPING,           SUCCESS(35)),
  ERROR(NS_OK_UENC_MOREINPUT,              SUCCESS(36)),

  /* BEGIN DEPRECATED */
  ERROR(NS_EXACT_LENGTH,         NS_OK_UDEC_EXACTLENGTH),
  ERROR(NS_PARTIAL_MORE_INPUT,   NS_OK_UDEC_MOREINPUT),
  ERROR(NS_PARTIAL_MORE_OUTPUT,  NS_OK_UDEC_MOREOUTPUT),
  ERROR(NS_ERROR_ILLEGAL_INPUT,  NS_ERROR_UDEC_ILLEGALINPUT),
  /* END DEPRECATED */
#undef MODULE


  /* ======================================================================= */
  /* 13: NS_ERROR_MODULE_FILES */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_FILES
  ERROR(NS_ERROR_FILE_UNRECOGNIZED_PATH,      FAILURE(1)),
  ERROR(NS_ERROR_FILE_UNRESOLVABLE_SYMLINK,   FAILURE(2)),
  ERROR(NS_ERROR_FILE_EXECUTION_FAILED,       FAILURE(3)),
  ERROR(NS_ERROR_FILE_UNKNOWN_TYPE,           FAILURE(4)),
  ERROR(NS_ERROR_FILE_DESTINATION_NOT_DIR,    FAILURE(5)),
  ERROR(NS_ERROR_FILE_TARGET_DOES_NOT_EXIST,  FAILURE(6)),
  ERROR(NS_ERROR_FILE_COPY_OR_MOVE_FAILED,    FAILURE(7)),
  ERROR(NS_ERROR_FILE_ALREADY_EXISTS,         FAILURE(8)),
  ERROR(NS_ERROR_FILE_INVALID_PATH,           FAILURE(9)),
  ERROR(NS_ERROR_FILE_DISK_FULL,              FAILURE(10)),
  ERROR(NS_ERROR_FILE_CORRUPTED,              FAILURE(11)),
  ERROR(NS_ERROR_FILE_NOT_DIRECTORY,          FAILURE(12)),
  ERROR(NS_ERROR_FILE_IS_DIRECTORY,           FAILURE(13)),
  ERROR(NS_ERROR_FILE_IS_LOCKED,              FAILURE(14)),
  ERROR(NS_ERROR_FILE_TOO_BIG,                FAILURE(15)),
  ERROR(NS_ERROR_FILE_NO_DEVICE_SPACE,        FAILURE(16)),
  ERROR(NS_ERROR_FILE_NAME_TOO_LONG,          FAILURE(17)),
  ERROR(NS_ERROR_FILE_NOT_FOUND,              FAILURE(18)),
  ERROR(NS_ERROR_FILE_READ_ONLY,              FAILURE(19)),
  ERROR(NS_ERROR_FILE_DIR_NOT_EMPTY,          FAILURE(20)),
  ERROR(NS_ERROR_FILE_ACCESS_DENIED,          FAILURE(21)),

  ERROR(NS_SUCCESS_FILE_DIRECTORY_EMPTY,  SUCCESS(1)),
  /* Result codes used by nsIDirectoryServiceProvider2 */
  ERROR(NS_SUCCESS_AGGREGATE_RESULT,      SUCCESS(2)),
#undef MODULE


  /* ======================================================================= */
  /* 14: NS_ERROR_MODULE_DOM */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_DOM
  /* XXX If you add a new DOM error code, also add an error string to
   * dom/base/domerr.msg */

  /* Standard DOM error codes: http://dvcs.w3.org/hg/domcore/raw-file/tip/Overview.html */
  ERROR(NS_ERROR_DOM_INDEX_SIZE_ERR,               FAILURE(1)),
  ERROR(NS_ERROR_DOM_HIERARCHY_REQUEST_ERR,        FAILURE(3)),
  ERROR(NS_ERROR_DOM_WRONG_DOCUMENT_ERR,           FAILURE(4)),
  ERROR(NS_ERROR_DOM_INVALID_CHARACTER_ERR,        FAILURE(5)),
  ERROR(NS_ERROR_DOM_NO_MODIFICATION_ALLOWED_ERR,  FAILURE(7)),
  ERROR(NS_ERROR_DOM_NOT_FOUND_ERR,                FAILURE(8)),
  ERROR(NS_ERROR_DOM_NOT_SUPPORTED_ERR,            FAILURE(9)),
  ERROR(NS_ERROR_DOM_INUSE_ATTRIBUTE_ERR,          FAILURE(10)),
  ERROR(NS_ERROR_DOM_INVALID_STATE_ERR,            FAILURE(11)),
  ERROR(NS_ERROR_DOM_SYNTAX_ERR,                   FAILURE(12)),
  ERROR(NS_ERROR_DOM_INVALID_MODIFICATION_ERR,     FAILURE(13)),
  ERROR(NS_ERROR_DOM_NAMESPACE_ERR,                FAILURE(14)),
  ERROR(NS_ERROR_DOM_INVALID_ACCESS_ERR,           FAILURE(15)),
  ERROR(NS_ERROR_DOM_TYPE_MISMATCH_ERR,            FAILURE(17)),
  ERROR(NS_ERROR_DOM_SECURITY_ERR,                 FAILURE(18)),
  ERROR(NS_ERROR_DOM_NETWORK_ERR,                  FAILURE(19)),
  ERROR(NS_ERROR_DOM_ABORT_ERR,                    FAILURE(20)),
  ERROR(NS_ERROR_DOM_URL_MISMATCH_ERR,             FAILURE(21)),
  ERROR(NS_ERROR_DOM_QUOTA_EXCEEDED_ERR,           FAILURE(22)),
  ERROR(NS_ERROR_DOM_TIMEOUT_ERR,                  FAILURE(23)),
  ERROR(NS_ERROR_DOM_INVALID_NODE_TYPE_ERR,        FAILURE(24)),
  ERROR(NS_ERROR_DOM_DATA_CLONE_ERR,               FAILURE(25)),
  /* XXX Should be JavaScript native errors */
  ERROR(NS_ERROR_TYPE_ERR,                         FAILURE(26)),
  ERROR(NS_ERROR_RANGE_ERR,                        FAILURE(27)),
  /* StringEncoding API errors from http://wiki.whatwg.org/wiki/StringEncoding */
  ERROR(NS_ERROR_DOM_ENCODING_NOT_SUPPORTED_ERR,   FAILURE(28)),
  ERROR(NS_ERROR_DOM_ENCODING_NOT_UTF_ERR,         FAILURE(29)),
  ERROR(NS_ERROR_DOM_ENCODING_DECODE_ERR,          FAILURE(30)),
  ERROR(NS_ERROR_DOM_INVALID_POINTER_ERR,          FAILURE(31)),
  /* WebCrypto API errors from http://www.w3.org/TR/WebCryptoAPI/ */
  ERROR(NS_ERROR_DOM_UNKNOWN_ERR,                  FAILURE(32)),
  ERROR(NS_ERROR_DOM_DATA_ERR,                     FAILURE(33)),
  ERROR(NS_ERROR_DOM_OPERATION_ERR,                FAILURE(34)),
  /* DOM error codes defined by us */
  ERROR(NS_ERROR_DOM_SECMAN_ERR,                   FAILURE(1001)),
  ERROR(NS_ERROR_DOM_WRONG_TYPE_ERR,               FAILURE(1002)),
  ERROR(NS_ERROR_DOM_NOT_OBJECT_ERR,               FAILURE(1003)),
  ERROR(NS_ERROR_DOM_NOT_XPC_OBJECT_ERR,           FAILURE(1004)),
  ERROR(NS_ERROR_DOM_NOT_NUMBER_ERR,               FAILURE(1005)),
  ERROR(NS_ERROR_DOM_NOT_BOOLEAN_ERR,              FAILURE(1006)),
  ERROR(NS_ERROR_DOM_NOT_FUNCTION_ERR,             FAILURE(1007)),
  ERROR(NS_ERROR_DOM_TOO_FEW_PARAMETERS_ERR,       FAILURE(1008)),
  ERROR(NS_ERROR_DOM_BAD_DOCUMENT_DOMAIN,          FAILURE(1009)),
  ERROR(NS_ERROR_DOM_PROP_ACCESS_DENIED,           FAILURE(1010)),
  ERROR(NS_ERROR_DOM_XPCONNECT_ACCESS_DENIED,      FAILURE(1011)),
  ERROR(NS_ERROR_DOM_BAD_URI,                      FAILURE(1012)),
  ERROR(NS_ERROR_DOM_RETVAL_UNDEFINED,             FAILURE(1013)),
  ERROR(NS_ERROR_DOM_QUOTA_REACHED,                FAILURE(1014)),
  ERROR(NS_ERROR_DOM_JS_EXCEPTION,                 FAILURE(1015)),

  /* May be used to indicate when e.g. setting a property value didn't
   * actually change the value, like for obj.foo = "bar"; obj.foo = "bar";
   * the second assignment throws NS_SUCCESS_DOM_NO_OPERATION.
   */
  ERROR(NS_SUCCESS_DOM_NO_OPERATION,               SUCCESS(1)),
#undef MODULE


  /* ======================================================================= */
  /* 15: NS_ERROR_MODULE_IMGLIB */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_IMGLIB
  ERROR(NS_IMAGELIB_SUCCESS_LOAD_FINISHED,  SUCCESS(0)),
  ERROR(NS_IMAGELIB_CHANGING_OWNER,         SUCCESS(1)),

  ERROR(NS_IMAGELIB_ERROR_FAILURE,       FAILURE(5)),
  ERROR(NS_IMAGELIB_ERROR_NO_DECODER,    FAILURE(6)),
  ERROR(NS_IMAGELIB_ERROR_NOT_FINISHED,  FAILURE(7)),
  ERROR(NS_IMAGELIB_ERROR_NO_ENCODER,    FAILURE(9)),
#undef MODULE


  /* ======================================================================= */
  /* 17: NS_ERROR_MODULE_EDITOR */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_EDITOR
  ERROR(NS_ERROR_EDITOR_NO_SELECTION,  FAILURE(1)),
  ERROR(NS_ERROR_EDITOR_NO_TEXTNODE,   FAILURE(2)),
  ERROR(NS_FOUND_TARGET,               FAILURE(3)),

  ERROR(NS_EDITOR_ELEMENT_NOT_FOUND,   SUCCESS(1)),
#undef MODULE


  /* ======================================================================= */
  /* 18: NS_ERROR_MODULE_XPCONNECT */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_XPCONNECT
  ERROR(NS_ERROR_XPC_NOT_ENOUGH_ARGS,                  FAILURE(1)),
  ERROR(NS_ERROR_XPC_NEED_OUT_OBJECT,                  FAILURE(2)),
  ERROR(NS_ERROR_XPC_CANT_SET_OUT_VAL,                 FAILURE(3)),
  ERROR(NS_ERROR_XPC_NATIVE_RETURNED_FAILURE,          FAILURE(4)),
  ERROR(NS_ERROR_XPC_CANT_GET_INTERFACE_INFO,          FAILURE(5)),
  ERROR(NS_ERROR_XPC_CANT_GET_PARAM_IFACE_INFO,        FAILURE(6)),
  ERROR(NS_ERROR_XPC_CANT_GET_METHOD_INFO,             FAILURE(7)),
  ERROR(NS_ERROR_XPC_UNEXPECTED,                       FAILURE(8)),
  ERROR(NS_ERROR_XPC_BAD_CONVERT_JS,                   FAILURE(9)),
  ERROR(NS_ERROR_XPC_BAD_CONVERT_NATIVE,               FAILURE(10)),
  ERROR(NS_ERROR_XPC_BAD_CONVERT_JS_NULL_REF,          FAILURE(11)),
  ERROR(NS_ERROR_XPC_BAD_OP_ON_WN_PROTO,               FAILURE(12)),
  ERROR(NS_ERROR_XPC_CANT_CONVERT_WN_TO_FUN,           FAILURE(13)),
  ERROR(NS_ERROR_XPC_CANT_DEFINE_PROP_ON_WN,           FAILURE(14)),
  ERROR(NS_ERROR_XPC_CANT_WATCH_WN_STATIC,             FAILURE(15)),
  ERROR(NS_ERROR_XPC_CANT_EXPORT_WN_STATIC,            FAILURE(16)),
  ERROR(NS_ERROR_XPC_SCRIPTABLE_CALL_FAILED,           FAILURE(17)),
  ERROR(NS_ERROR_XPC_SCRIPTABLE_CTOR_FAILED,           FAILURE(18)),
  ERROR(NS_ERROR_XPC_CANT_CALL_WO_SCRIPTABLE,          FAILURE(19)),
  ERROR(NS_ERROR_XPC_CANT_CTOR_WO_SCRIPTABLE,          FAILURE(20)),
  ERROR(NS_ERROR_XPC_CI_RETURNED_FAILURE,              FAILURE(21)),
  ERROR(NS_ERROR_XPC_GS_RETURNED_FAILURE,              FAILURE(22)),
  ERROR(NS_ERROR_XPC_BAD_CID,                          FAILURE(23)),
  ERROR(NS_ERROR_XPC_BAD_IID,                          FAILURE(24)),
  ERROR(NS_ERROR_XPC_CANT_CREATE_WN,                   FAILURE(25)),
  ERROR(NS_ERROR_XPC_JS_THREW_EXCEPTION,               FAILURE(26)),
  ERROR(NS_ERROR_XPC_JS_THREW_NATIVE_OBJECT,           FAILURE(27)),
  ERROR(NS_ERROR_XPC_JS_THREW_JS_OBJECT,               FAILURE(28)),
  ERROR(NS_ERROR_XPC_JS_THREW_NULL,                    FAILURE(29)),
  ERROR(NS_ERROR_XPC_JS_THREW_STRING,                  FAILURE(30)),
  ERROR(NS_ERROR_XPC_JS_THREW_NUMBER,                  FAILURE(31)),
  ERROR(NS_ERROR_XPC_JAVASCRIPT_ERROR,                 FAILURE(32)),
  ERROR(NS_ERROR_XPC_JAVASCRIPT_ERROR_WITH_DETAILS,    FAILURE(33)),
  ERROR(NS_ERROR_XPC_CANT_CONVERT_PRIMITIVE_TO_ARRAY,  FAILURE(34)),
  ERROR(NS_ERROR_XPC_CANT_CONVERT_OBJECT_TO_ARRAY,     FAILURE(35)),
  ERROR(NS_ERROR_XPC_NOT_ENOUGH_ELEMENTS_IN_ARRAY,     FAILURE(36)),
  ERROR(NS_ERROR_XPC_CANT_GET_ARRAY_INFO,              FAILURE(37)),
  ERROR(NS_ERROR_XPC_NOT_ENOUGH_CHARS_IN_STRING,       FAILURE(38)),
  ERROR(NS_ERROR_XPC_SECURITY_MANAGER_VETO,            FAILURE(39)),
  ERROR(NS_ERROR_XPC_INTERFACE_NOT_SCRIPTABLE,         FAILURE(40)),
  ERROR(NS_ERROR_XPC_INTERFACE_NOT_FROM_NSISUPPORTS,   FAILURE(41)),
  ERROR(NS_ERROR_XPC_CANT_GET_JSOBJECT_OF_DOM_OBJECT,  FAILURE(42)),
  ERROR(NS_ERROR_XPC_CANT_SET_READ_ONLY_CONSTANT,      FAILURE(43)),
  ERROR(NS_ERROR_XPC_CANT_SET_READ_ONLY_ATTRIBUTE,     FAILURE(44)),
  ERROR(NS_ERROR_XPC_CANT_SET_READ_ONLY_METHOD,        FAILURE(45)),
  ERROR(NS_ERROR_XPC_CANT_ADD_PROP_TO_WRAPPED_NATIVE,  FAILURE(46)),
  ERROR(NS_ERROR_XPC_CALL_TO_SCRIPTABLE_FAILED,        FAILURE(47)),
  ERROR(NS_ERROR_XPC_JSOBJECT_HAS_NO_FUNCTION_NAMED,   FAILURE(48)),
  ERROR(NS_ERROR_XPC_BAD_ID_STRING,                    FAILURE(49)),
  ERROR(NS_ERROR_XPC_BAD_INITIALIZER_NAME,             FAILURE(50)),
  ERROR(NS_ERROR_XPC_HAS_BEEN_SHUTDOWN,                FAILURE(51)),
  ERROR(NS_ERROR_XPC_CANT_MODIFY_PROP_ON_WN,           FAILURE(52)),
  ERROR(NS_ERROR_XPC_BAD_CONVERT_JS_ZERO_ISNOT_NULL,   FAILURE(53)),
  ERROR(NS_ERROR_XPC_CANT_PASS_CPOW_TO_NATIVE,         FAILURE(54)),
  /* any new errors here should have an associated entry added in xpc.msg */

  ERROR(NS_SUCCESS_I_DID_SOMETHING,      SUCCESS(1)),
#undef MODULE


  /* ======================================================================= */
  /* 19: NS_ERROR_MODULE_PROFILE */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_PROFILE
  ERROR(NS_ERROR_LAUNCHED_CHILD_PROCESS,  FAILURE(200)),
#undef MODULE


  /* ======================================================================= */
  /* 21: NS_ERROR_MODULE_SECURITY */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_SECURITY
  /* Error code for CSP */
  ERROR(NS_ERROR_CSP_FRAME_ANCESTOR_VIOLATION,     FAILURE(99)),

  /* CMS specific nsresult error codes.  Note: the numbers used here correspond
   * to the values in nsICMSMessageErrors.idl. */
  ERROR(NS_ERROR_CMS_VERIFY_NOT_SIGNED,            FAILURE(1024)),
  ERROR(NS_ERROR_CMS_VERIFY_NO_CONTENT_INFO,       FAILURE(1025)),
  ERROR(NS_ERROR_CMS_VERIFY_BAD_DIGEST,            FAILURE(1026)),
  ERROR(NS_ERROR_CMS_VERIFY_NOCERT,                FAILURE(1028)),
  ERROR(NS_ERROR_CMS_VERIFY_UNTRUSTED,             FAILURE(1029)),
  ERROR(NS_ERROR_CMS_VERIFY_ERROR_UNVERIFIED,      FAILURE(1031)),
  ERROR(NS_ERROR_CMS_VERIFY_ERROR_PROCESSING,      FAILURE(1032)),
  ERROR(NS_ERROR_CMS_VERIFY_BAD_SIGNATURE,         FAILURE(1033)),
  ERROR(NS_ERROR_CMS_VERIFY_DIGEST_MISMATCH,       FAILURE(1034)),
  ERROR(NS_ERROR_CMS_VERIFY_UNKNOWN_ALGO,          FAILURE(1035)),
  ERROR(NS_ERROR_CMS_VERIFY_UNSUPPORTED_ALGO,      FAILURE(1036)),
  ERROR(NS_ERROR_CMS_VERIFY_MALFORMED_SIGNATURE,   FAILURE(1037)),
  ERROR(NS_ERROR_CMS_VERIFY_HEADER_MISMATCH,       FAILURE(1038)),
  ERROR(NS_ERROR_CMS_VERIFY_NOT_YET_ATTEMPTED,     FAILURE(1039)),
  ERROR(NS_ERROR_CMS_VERIFY_CERT_WITHOUT_ADDRESS,  FAILURE(1040)),
  ERROR(NS_ERROR_CMS_ENCRYPT_NO_BULK_ALG,          FAILURE(1056)),
  ERROR(NS_ERROR_CMS_ENCRYPT_INCOMPLETE,           FAILURE(1057)),
#undef MODULE


  /* ======================================================================= */
  /* 22: NS_ERROR_MODULE_DOM_XPATH */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_DOM_XPATH
  /* DOM error codes from http://www.w3.org/TR/DOM-Level-3-XPath/ */
  ERROR(NS_ERROR_DOM_INVALID_EXPRESSION_ERR,  FAILURE(51)),
  ERROR(NS_ERROR_DOM_TYPE_ERR,                FAILURE(52)),
#undef MODULE


  /* ======================================================================= */
  /* 24: NS_ERROR_MODULE_URILOADER */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_URILOADER
  ERROR(NS_ERROR_WONT_HANDLE_CONTENT,   FAILURE(1)),
  /* The load has been cancelled because it was found on a malware or phishing
   * blacklist. */
  ERROR(NS_ERROR_MALWARE_URI,           FAILURE(30)),
  ERROR(NS_ERROR_PHISHING_URI,          FAILURE(31)),
  ERROR(NS_ERROR_TRACKING_URI,          FAILURE(34)),
  /* Used when "Save Link As..." doesn't see the headers quickly enough to
   * choose a filename.  See nsContextMenu.js. */
  ERROR(NS_ERROR_SAVE_LINK_AS_TIMEOUT,  FAILURE(32)),
  /* Used when the data from a channel has already been parsed and cached so it
   * doesn't need to be reparsed from the original source. */
  ERROR(NS_ERROR_PARSED_DATA_CACHED,    FAILURE(33)),

  /* This success code indicates that a refresh header was found and
   * successfully setup.  */
  ERROR(NS_REFRESHURI_HEADER_FOUND,  SUCCESS(2)),
#undef MODULE


  /* ======================================================================= */
  /* 25: NS_ERROR_MODULE_CONTENT */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_CONTENT
  /* Error codes for image loading */
  ERROR(NS_ERROR_IMAGE_SRC_CHANGED,             FAILURE(4)),
  ERROR(NS_ERROR_IMAGE_BLOCKED,                 FAILURE(5)),
  /* Error codes for content policy blocking */
  ERROR(NS_ERROR_CONTENT_BLOCKED,               FAILURE(6)),
  ERROR(NS_ERROR_CONTENT_BLOCKED_SHOW_ALT,      FAILURE(7)),
  /* Success variations of content policy blocking */
  ERROR(NS_PROPTABLE_PROP_NOT_THERE,            FAILURE(10)),
  /* Error code for XBL */
  ERROR(NS_ERROR_XBL_BLOCKED,                   FAILURE(15)),
  /* Error code for when the content process crashed */
  ERROR(NS_ERROR_CONTENT_CRASHED,               FAILURE(16)),

  /* XXX this is not really used */
  ERROR(NS_HTML_STYLE_PROPERTY_NOT_THERE,   SUCCESS(2)),
  ERROR(NS_CONTENT_BLOCKED,                 SUCCESS(8)),
  ERROR(NS_CONTENT_BLOCKED_SHOW_ALT,        SUCCESS(9)),
  ERROR(NS_PROPTABLE_PROP_OVERWRITTEN,      SUCCESS(11)),
  /* Error codes for FindBroadcaster in XULDocument.cpp */
  ERROR(NS_FINDBROADCASTER_NOT_FOUND,       SUCCESS(12)),
  ERROR(NS_FINDBROADCASTER_FOUND,           SUCCESS(13)),
  ERROR(NS_FINDBROADCASTER_AWAIT_OVERLAYS,  SUCCESS(14)),
#undef MODULE


  /* ======================================================================= */
  /* 27: NS_ERROR_MODULE_XSLT */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_XSLT
  ERROR(NS_ERROR_XPATH_INVALID_ARG,  NS_ERROR_INVALID_ARG),

  ERROR(NS_ERROR_XSLT_PARSE_FAILURE,                  FAILURE(1)),
  ERROR(NS_ERROR_XPATH_PARSE_FAILURE,                 FAILURE(2)),
  ERROR(NS_ERROR_XSLT_ALREADY_SET,                    FAILURE(3)),
  ERROR(NS_ERROR_XSLT_EXECUTION_FAILURE,              FAILURE(4)),
  ERROR(NS_ERROR_XPATH_UNKNOWN_FUNCTION,              FAILURE(5)),
  ERROR(NS_ERROR_XSLT_BAD_RECURSION,                  FAILURE(6)),
  ERROR(NS_ERROR_XSLT_BAD_VALUE,                      FAILURE(7)),
  ERROR(NS_ERROR_XSLT_NODESET_EXPECTED,               FAILURE(8)),
  ERROR(NS_ERROR_XSLT_ABORTED,                        FAILURE(9)),
  ERROR(NS_ERROR_XSLT_NETWORK_ERROR,                  FAILURE(10)),
  ERROR(NS_ERROR_XSLT_WRONG_MIME_TYPE,                FAILURE(11)),
  ERROR(NS_ERROR_XSLT_LOAD_RECURSION,                 FAILURE(12)),
  ERROR(NS_ERROR_XPATH_BAD_ARGUMENT_COUNT,            FAILURE(13)),
  ERROR(NS_ERROR_XPATH_BAD_EXTENSION_FUNCTION,        FAILURE(14)),
  ERROR(NS_ERROR_XPATH_PAREN_EXPECTED,                FAILURE(15)),
  ERROR(NS_ERROR_XPATH_INVALID_AXIS,                  FAILURE(16)),
  ERROR(NS_ERROR_XPATH_NO_NODE_TYPE_TEST,             FAILURE(17)),
  ERROR(NS_ERROR_XPATH_BRACKET_EXPECTED,              FAILURE(18)),
  ERROR(NS_ERROR_XPATH_INVALID_VAR_NAME,              FAILURE(19)),
  ERROR(NS_ERROR_XPATH_UNEXPECTED_END,                FAILURE(20)),
  ERROR(NS_ERROR_XPATH_OPERATOR_EXPECTED,             FAILURE(21)),
  ERROR(NS_ERROR_XPATH_UNCLOSED_LITERAL,              FAILURE(22)),
  ERROR(NS_ERROR_XPATH_BAD_COLON,                     FAILURE(23)),
  ERROR(NS_ERROR_XPATH_BAD_BANG,                      FAILURE(24)),
  ERROR(NS_ERROR_XPATH_ILLEGAL_CHAR,                  FAILURE(25)),
  ERROR(NS_ERROR_XPATH_BINARY_EXPECTED,               FAILURE(26)),
  ERROR(NS_ERROR_XSLT_LOAD_BLOCKED_ERROR,             FAILURE(27)),
  ERROR(NS_ERROR_XPATH_INVALID_EXPRESSION_EVALUATED,  FAILURE(28)),
  ERROR(NS_ERROR_XPATH_UNBALANCED_CURLY_BRACE,        FAILURE(29)),
  ERROR(NS_ERROR_XSLT_BAD_NODE_NAME,                  FAILURE(30)),
  ERROR(NS_ERROR_XSLT_VAR_ALREADY_SET,                FAILURE(31)),

  ERROR(NS_XSLT_GET_NEW_HANDLER,  SUCCESS(1)),
#undef MODULE


  /* ======================================================================= */
  /* 29: NS_ERROR_MODULE_SVG */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_SVG
  /* SVG DOM error codes from http://www.w3.org/TR/SVG11/svgdom.html */
  ERROR(NS_ERROR_DOM_SVG_WRONG_TYPE_ERR,         FAILURE(0)),
  /* Yes, the spec says "INVERTABLE", not "INVERTIBLE" */
  ERROR(NS_ERROR_DOM_SVG_MATRIX_NOT_INVERTABLE,  FAILURE(2)),
#undef MODULE


  /* ======================================================================= */
  /* 30: NS_ERROR_MODULE_STORAGE */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_STORAGE
  /* To add additional errors to Storage, please append entries to the bottom
   * of the list in the following format:
   *   NS_ERROR_STORAGE_YOUR_ERR,  FAILURE(n)
   * where n is the next unique positive integer.  You must also add an entry
   * to js/xpconnect/src/xpc.msg under the code block beginning with the
   * comment 'storage related codes (from mozStorage.h)', in the following
   * format: 'XPC_MSG_DEF(NS_ERROR_STORAGE_YOUR_ERR, "brief description of your
   * error")' */
  ERROR(NS_ERROR_STORAGE_BUSY,        FAILURE(1)),
  ERROR(NS_ERROR_STORAGE_IOERR,       FAILURE(2)),
  ERROR(NS_ERROR_STORAGE_CONSTRAINT,  FAILURE(3)),
#undef MODULE


  /* ======================================================================= */
  /* 32: NS_ERROR_MODULE_DOM_FILE */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_DOM_FILE
  ERROR(NS_ERROR_DOM_FILE_NOT_FOUND_ERR,     FAILURE(0)),
  ERROR(NS_ERROR_DOM_FILE_NOT_READABLE_ERR,  FAILURE(1)),
  ERROR(NS_ERROR_DOM_FILE_ABORT_ERR,         FAILURE(2)),
#undef MODULE


  /* ======================================================================= */
  /* 33: NS_ERROR_MODULE_DOM_INDEXEDDB */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_DOM_INDEXEDDB
  /* IndexedDB error codes http://dvcs.w3.org/hg/IndexedDB/raw-file/tip/Overview.html */
  ERROR(NS_ERROR_DOM_INDEXEDDB_UNKNOWN_ERR,               FAILURE(1)),
  ERROR(NS_ERROR_DOM_INDEXEDDB_NOT_FOUND_ERR,             FAILURE(3)),
  ERROR(NS_ERROR_DOM_INDEXEDDB_CONSTRAINT_ERR,            FAILURE(4)),
  ERROR(NS_ERROR_DOM_INDEXEDDB_DATA_ERR,                  FAILURE(5)),
  ERROR(NS_ERROR_DOM_INDEXEDDB_NOT_ALLOWED_ERR,           FAILURE(6)),
  ERROR(NS_ERROR_DOM_INDEXEDDB_TRANSACTION_INACTIVE_ERR,  FAILURE(7)),
  ERROR(NS_ERROR_DOM_INDEXEDDB_ABORT_ERR,                 FAILURE(8)),
  ERROR(NS_ERROR_DOM_INDEXEDDB_READ_ONLY_ERR,             FAILURE(9)),
  ERROR(NS_ERROR_DOM_INDEXEDDB_TIMEOUT_ERR,               FAILURE(10)),
  ERROR(NS_ERROR_DOM_INDEXEDDB_QUOTA_ERR,                 FAILURE(11)),
  ERROR(NS_ERROR_DOM_INDEXEDDB_VERSION_ERR,               FAILURE(12)),
  ERROR(NS_ERROR_DOM_INDEXEDDB_RECOVERABLE_ERR,           FAILURE(1001)),
#undef MODULE


  /* ======================================================================= */
  /* 34: NS_ERROR_MODULE_DOM_FILEHANDLE */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_DOM_FILEHANDLE
  ERROR(NS_ERROR_DOM_FILEHANDLE_UNKNOWN_ERR,              FAILURE(1)),
  ERROR(NS_ERROR_DOM_FILEHANDLE_NOT_ALLOWED_ERR,          FAILURE(2)),
  ERROR(NS_ERROR_DOM_FILEHANDLE_INACTIVE_ERR,             FAILURE(3)),
  ERROR(NS_ERROR_DOM_FILEHANDLE_ABORT_ERR,                FAILURE(4)),
  ERROR(NS_ERROR_DOM_FILEHANDLE_READ_ONLY_ERR,            FAILURE(5)),
  ERROR(NS_ERROR_DOM_FILEHANDLE_QUOTA_ERR,                FAILURE(6)),
#undef MODULE

  /* ======================================================================= */
  /* 35: NS_ERROR_MODULE_SIGNED_JAR */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_SIGNED_JAR
  ERROR(NS_ERROR_SIGNED_JAR_NOT_SIGNED,                   FAILURE(1)),
  ERROR(NS_ERROR_SIGNED_JAR_MODIFIED_ENTRY,               FAILURE(2)),
  ERROR(NS_ERROR_SIGNED_JAR_UNSIGNED_ENTRY,               FAILURE(3)),
  ERROR(NS_ERROR_SIGNED_JAR_ENTRY_MISSING,                FAILURE(4)),
  ERROR(NS_ERROR_SIGNED_JAR_WRONG_SIGNATURE,              FAILURE(5)),
  ERROR(NS_ERROR_SIGNED_JAR_ENTRY_TOO_LARGE,              FAILURE(6)),
  ERROR(NS_ERROR_SIGNED_JAR_ENTRY_INVALID,                FAILURE(7)),
  ERROR(NS_ERROR_SIGNED_JAR_MANIFEST_INVALID,             FAILURE(8)),
#undef MODULE

  /* ======================================================================= */
  /* 36: NS_ERROR_MODULE_DOM_FILESYSTEM */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_DOM_FILESYSTEM
  ERROR(NS_ERROR_DOM_FILESYSTEM_INVALID_PATH_ERR,          FAILURE(1)),
  ERROR(NS_ERROR_DOM_FILESYSTEM_INVALID_MODIFICATION_ERR,  FAILURE(2)),
  ERROR(NS_ERROR_DOM_FILESYSTEM_NO_MODIFICATION_ALLOWED_ERR, FAILURE(3)),
  ERROR(NS_ERROR_DOM_FILESYSTEM_PATH_EXISTS_ERR,           FAILURE(4)),
  ERROR(NS_ERROR_DOM_FILESYSTEM_TYPE_MISMATCH_ERR,         FAILURE(5)),
  ERROR(NS_ERROR_DOM_FILESYSTEM_UNKNOWN_ERR,               FAILURE(6)),
#undef MODULE

  /* ======================================================================= */
  /* 51: NS_ERROR_MODULE_GENERAL */
  /* ======================================================================= */
#define MODULE NS_ERROR_MODULE_GENERAL
  /* Error code used internally by the incremental downloader to cancel the
   * network channel when the download is already complete. */
  ERROR(NS_ERROR_DOWNLOAD_COMPLETE,      FAILURE(1)),
  /* Error code used internally by the incremental downloader to cancel the
   * network channel when the response to a range request is 200 instead of
   * 206. */
  ERROR(NS_ERROR_DOWNLOAD_NOT_PARTIAL,   FAILURE(2)),
  ERROR(NS_ERROR_UNORM_MOREOUTPUT,       FAILURE(33)),

  ERROR(NS_ERROR_DOCSHELL_REQUEST_REJECTED,  FAILURE(1001)),
  /* This is needed for displaying an error message when navigation is
   * attempted on a document when printing The value arbitrary as long as it
   * doesn't conflict with any of the other values in the errors in
   * DisplayLoadError */
  ERROR(NS_ERROR_DOCUMENT_IS_PRINTMODE,  FAILURE(2001)),

  ERROR(NS_SUCCESS_DONT_FIXUP,           SUCCESS(1)),
  /* This success code may be returned by nsIAppStartup::Run to indicate that
   * the application should be restarted.  This condition corresponds to the
   * case in which nsIAppStartup::Quit was called with the eRestart flag. */
  ERROR(NS_SUCCESS_RESTART_APP,          SUCCESS(1)),
  ERROR(NS_SUCCESS_RESTART_METRO_APP,    SUCCESS(2)),
  ERROR(NS_SUCCESS_UNORM_NOTFOUND,  SUCCESS(17)),


  /* a11y */
  /* raised when current pivot's position is needed but it's not in the tree */
  ERROR(NS_ERROR_NOT_IN_TREE,  FAILURE(38)),

  /* see nsTextEquivUtils */
  ERROR(NS_OK_NO_NAME_CLAUSE_HANDLED,  SUCCESS(34))
#undef MODULE
