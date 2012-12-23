/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
/*
 * This interface allows any module to access the encoder/decoder 
 * routines for RFC822 headers. This will allow any mail/news module
 * to call on these routines.
 */
#ifndef nsMimeTypes_h_
#define nsMimeTypes_h_

/* Defines for various MIME content-types and encodings.
   Whenever you type in a content-type, you should use one of these defines
   instead, to help catch typos, and make central management of them easier.
 */

#define AUDIO_WILDCARD                      "audio/*"
#define IMAGE_WILDCARD                      "image/*"

#define APPLICATION_APPLEFILE               "application/applefile"
#define APPLICATION_BINHEX                  "application/mac-binhex40"
#define APPLICATION_MACBINARY               "application/x-macbinary"
#define APPLICATION_COMPRESS                "application/x-compress"
#define APPLICATION_COMPRESS2               "application/compress"
#define APPLICATION_FORTEZZA_CKL            "application/x-fortezza-ckl"
#define APPLICATION_FORTEZZA_KRL            "application/x-fortezza-krl"
#define APPLICATION_GZIP                    "application/x-gzip"
#define APPLICATION_GZIP2                   "application/gzip"
#define APPLICATION_GZIP3                   "application/x-gunzip"
#define APPLICATION_ZIP                     "application/zip"
#define APPLICATION_HTTP_INDEX_FORMAT       "application/http-index-format"
#define APPLICATION_ECMASCRIPT              "application/ecmascript"
#define APPLICATION_JAVASCRIPT              "application/javascript"
#define APPLICATION_XJAVASCRIPT             "application/x-javascript"
#define APPLICATION_JSON                    "application/json"
#define APPLICATION_NETSCAPE_REVOCATION     "application/x-netscape-revocation"
#define APPLICATION_NS_PROXY_AUTOCONFIG     "application/x-ns-proxy-autoconfig"
#define APPLICATION_NS_JAVASCRIPT_AUTOCONFIG        "application/x-javascript-config"
#define APPLICATION_OCTET_STREAM            "application/octet-stream"
#define APPLICATION_PGP                     "application/pgp"
#define APPLICATION_PGP2                    "application/x-pgp-message"
#define APPLICATION_POSTSCRIPT              "application/postscript"
#define APPLICATION_PDF                     "application/pdf"
#define APPLICATION_PRE_ENCRYPTED           "application/pre-encrypted"
#define APPLICATION_RDF                     "application/rdf+xml"
#define APPLICATION_UUENCODE                "application/x-uuencode"
#define APPLICATION_UUENCODE2               "application/x-uue"
#define APPLICATION_UUENCODE3               "application/uuencode"
#define APPLICATION_UUENCODE4               "application/uue"
#define APPLICATION_X509_CA_CERT            "application/x-x509-ca-cert"
#define APPLICATION_X509_SERVER_CERT        "application/x-x509-server-cert"
#define APPLICATION_X509_EMAIL_CERT         "application/x-x509-email-cert"
#define APPLICATION_X509_USER_CERT          "application/x-x509-user-cert"
#define APPLICATION_X509_CRL                "application/x-pkcs7-crl"
#define APPLICATION_XPKCS7_MIME             "application/x-pkcs7-mime"
#define APPLICATION_PKCS7_MIME              "application/pkcs7-mime"
#define APPLICATION_XPKCS7_SIGNATURE        "application/x-pkcs7-signature"
#define APPLICATION_PKCS7_SIGNATURE         "application/pkcs7-signature"
#define APPLICATION_WWW_FORM_URLENCODED     "application/x-www-form-urlencoded"
#define APPLICATION_OLEOBJECT               "application/oleobject"
#define APPLICATION_OLEOBJECT2              "application/x-oleobject"
#define APPLICATION_JAVAARCHIVE             "application/java-archive"
#define APPLICATION_MARIMBA                 "application/marimba"
#define APPLICATION_XMARIMBA                "application/x-marimba"
#define APPLICATION_XPINSTALL               "application/x-xpinstall"
#define APPLICATION_XML                     "application/xml"
#define APPLICATION_XHTML_XML               "application/xhtml+xml"
#define APPLICATION_XSLT_XML                "application/xslt+xml"
#define APPLICATION_MATHML_XML              "application/mathml+xml"
#define APPLICATION_RDF_XML                 "application/rdf+xml"

#define AUDIO_BASIC                         "audio/basic"
#define AUDIO_OGG                           "audio/ogg"
#define AUDIO_WAV                           "audio/x-wav"
#define AUDIO_WEBM                          "audio/webm"
#define AUDIO_MP3                           "audio/mpeg"
#define AUDIO_MP4                           "audio/mp4"

#define BINARY_OCTET_STREAM                 "binary/octet-stream"

#define IMAGE_GIF                           "image/gif"
#define IMAGE_JPG                           "image/jpeg"
#define IMAGE_PJPG                          "image/pjpeg"
#define IMAGE_PNG                           "image/png"
#define IMAGE_PPM                           "image/x-portable-pixmap"
#define IMAGE_XBM                           "image/x-xbitmap"
#define IMAGE_XBM2                          "image/x-xbm"
#define IMAGE_XBM3                          "image/xbm"
#define IMAGE_ART                           "image/x-jg"
#define IMAGE_TIFF                          "image/tiff"
#define IMAGE_BMP                           "image/bmp"
#define IMAGE_ICO                           "image/x-icon"
#define IMAGE_MNG                           "video/x-mng"
#define IMAGE_JNG                           "image/x-jng"
#define IMAGE_SVG_XML                       "image/svg+xml"

#define MESSAGE_EXTERNAL_BODY               "message/external-body"
#define MESSAGE_NEWS                        "message/news"
#define MESSAGE_RFC822                      "message/rfc822"

#define MULTIPART_ALTERNATIVE               "multipart/alternative"
#define MULTIPART_APPLEDOUBLE               "multipart/appledouble"
#define MULTIPART_DIGEST                    "multipart/digest"
#define MULTIPART_HEADER_SET                "multipart/header-set"
#define MULTIPART_MIXED                     "multipart/mixed"
#define MULTIPART_PARALLEL                  "multipart/parallel"
#define MULTIPART_SIGNED                    "multipart/signed"
#define MULTIPART_RELATED                   "multipart/related"
#define MULTIPART_MIXED_REPLACE             "multipart/x-mixed-replace"
#define MULTIPART_BYTERANGES                "multipart/byteranges"

#define SUN_ATTACHMENT                      "x-sun-attachment"

#define TEXT_ENRICHED                       "text/enriched"
#define TEXT_CALENDAR                       "text/calendar"
#define TEXT_HTML                           "text/html"
#define TEXT_MDL                            "text/mdl"
#define TEXT_PLAIN                          "text/plain"
#define TEXT_RICHTEXT                       "text/richtext"
#define TEXT_VCARD                          "text/x-vcard"
#define TEXT_CSS                            "text/css"
#define TEXT_JSSS                           "text/jsss"
#define TEXT_XML                            "text/xml"
#define TEXT_RDF                            "text/rdf"
#define TEXT_XUL                            "application/vnd.mozilla.xul+xml"
#define TEXT_ECMASCRIPT                     "text/ecmascript"
#define TEXT_JAVASCRIPT                     "text/javascript"
#define TEXT_XSL                            "text/xsl"
#define TEXT_EVENT_STREAM                   "text/event-stream"
#define TEXT_CACHE_MANIFEST                 "text/cache-manifest"

#define VIDEO_MPEG                          "video/mpeg"
#define VIDEO_MP4                           "video/mp4"
#define VIDEO_RAW                           "video/x-raw-yuv"
#define VIDEO_OGG                           "video/ogg"
#define VIDEO_WEBM                          "video/webm"
#define VIDEO_3GPP                          "video/3gpp"
#define APPLICATION_OGG                     "application/ogg"
#ifdef MOZ_DASH
#define APPLICATION_DASH                    "application/dash+xml"
#endif

/* x-uuencode-apple-single. QuickMail made me do this. */
#define UUENCODE_APPLE_SINGLE               "x-uuencode-apple-single"

/* The standard MIME message-content-encoding values:
 */
#define ENCODING_7BIT                       "7bit"
#define ENCODING_8BIT                       "8bit"
#define ENCODING_BINARY                     "binary"
#define ENCODING_BASE64                     "base64"
#define ENCODING_QUOTED_PRINTABLE           "quoted-printable"

/* Some nonstandard encodings.  Note that the names are TOTALLY RANDOM,
   and code that looks for these in network-provided data must look for
   all the possibilities.
 */
#define ENCODING_COMPRESS                   "x-compress"
#define ENCODING_COMPRESS2                  "compress"
#define ENCODING_ZLIB                       "x-zlib"
#define ENCODING_ZLIB2                      "zlib"
#define ENCODING_GZIP                       "x-gzip"
#define ENCODING_GZIP2                      "gzip"
#define ENCODING_DEFLATE                    "x-deflate"
#define ENCODING_DEFLATE2                   "deflate"
#define ENCODING_UUENCODE                   "x-uuencode"
#define ENCODING_UUENCODE2                  "x-uue"
#define ENCODING_UUENCODE3                  "uuencode"
#define ENCODING_UUENCODE4                  "uue"
#define ENCODING_YENCODE                    "x-yencode"

/* Some names of parameters that various MIME headers include.
 */
#define PARAM_PROTOCOL                      "protocol"
#define PARAM_MICALG                        "micalg"
#define PARAM_MICALG_MD2                    "rsa-md2"
#define PARAM_MICALG_MD5                    "rsa-md5"
#define PARAM_MICALG_MD5_2                  "md5"
#define PARAM_MICALG_SHA1                   "sha1"
#define PARAM_MICALG_SHA1_2                 "sha-1"
#define PARAM_MICALG_SHA1_3                 "rsa-sha1"
#define PARAM_MICALG_SHA1_4                 "rsa-sha-1"
#define PARAM_MICALG_SHA1_5                 "rsa-sha"
#define PARAM_MICALG_SHA256                 "sha-256"
#define PARAM_MICALG_SHA256_2               "sha256"
#define PARAM_MICALG_SHA256_3               "2.16.840.1.101.3.4.2.1"
#define PARAM_MICALG_SHA384                 "sha-384"
#define PARAM_MICALG_SHA384_2               "sha384"
#define PARAM_MICALG_SHA384_3               "2.16.840.1.101.3.4.2.2"
#define PARAM_MICALG_SHA512                 "sha-512"
#define PARAM_MICALG_SHA512_2               "sha512"
#define PARAM_MICALG_SHA512_3               "2.16.840.1.101.3.4.2.3"
#define PARAM_X_MAC_CREATOR                 "x-mac-creator"
#define PARAM_X_MAC_TYPE                    "x-mac-type"
#define PARAM_FORMAT                        "format"

#define UNKNOWN_CONTENT_TYPE                "application/x-unknown-content-type"
#define APPLICATION_GUESS_FROM_EXT          "application/x-vnd.mozilla.guess-from-ext"
#define VIEWSOURCE_CONTENT_TYPE             "application/x-view-source"

#define APPLICATION_DIRECTORY				        "application/directory" /* text/x-vcard is synonym */
#define APPLICATION_CACHED_XUL              "mozilla.application/cached-xul"

#endif /* nsMimeTypes_h_ */
