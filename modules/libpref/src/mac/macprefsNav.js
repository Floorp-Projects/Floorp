/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// Mac specific preference defaults
// TBD: Move Mac-specific Mime fields here?

platform.mac = true;

mime_type("mime.image_gif", "image/gif", "gif", 2, "JPEGView", "JVWR", "GIFf");
mime_type("mime.image_jpeg", "image/jpeg", "jpg,jpeg,jpe", 2, "JPEGView", "JVWR", "JPEG");
mime_type("mime.image_pict", "image/pict", "pic,pict", 1, "SimpleText", "ttxt", "PICT");
mime_type("mime.image_tiff", "image/tiff", "tif,tiff", 1, "JPEGView", "JVWR", "TIFF");
mime_type("mime.image_x_xbitmap", "image/x-xbitmap", "xbm", 2, "Unknown", "ttxt", "????");

mime_type("mime.audio_basic", "audio/basic", "au,snd", 1, "Sound Machine", "SNDM", "ULAW");
mime_type("mime.audio_aiff", "audio/aiff", "aiff,aif", 1, "Sound Machine", "SNDM", "AIFF");
mime_type("mime.audio_x_wav", "audio/x-wav", "wav", 1, "Sound Machine", "SNDM", "WAVE");

mime_type("mime.video_quicktime", "video/quicktime", "qt,mov", 1, "Simple Player", "TVOD", "MooV");
mime_type("mime.video_mpeg", "video/mpeg", "mpg,mpeg,mpe", 1, "Sparkle", "mMPG", "MPEG");
mime_type("mime.video_x_msvideo", "video/x-msvideo", "avi", 1, "AVI to QT Utility", "AVIC", "????");
mime_type("mime.video_x_qtc", "video/x-qtc", "qtc", 1, "Conferencing Helper Application", "mtwh", ".qtc");

mime_type("mime.application_mac_binhex40", "application/mac-binhex40", "hqx", 1, "Stuffit Expander", "SITx", "TEXT");
mime_type("mime.application_x_stuffit", "application/x-stuffit", "sit", 1, "Stuffit Expander", "SITx", "SITD");
mime_type("mime.application_x_macbinary", "application/x-macbinary", "bin", 1, "Stuffit Expander", "SITx", "TEXT");
mime_type("mime.application_x_conference", "application/x-conference", "nsc", 1, "Netscape Conference", "Ncq¹", "TEXT");

mime_type("mime.application_zip", "application/zip", "z,zip", 1, "ZipIt", "ZIP ", "ZIP ");
mime_type("mime.application_gzip", "application/gzip", "gz", 1, "MacGzip", "Gzip", "Gzip");
mime_type("mime.application_msword", "application/msword", "doc", 0, "MS Word", "MSWD", "W6BN");
mime_type("mime.application_x_excel", "application/x-excel", "xls", 0, "MS Excel", "XCEL", "XLS5");

mime_type("mime.application_octet_stream", "application/octet-stream", "exe", 3, "", "", "");
mime_type("mime.application_postscript", "application/postscript", "ai,eps,ps", 3, "SimpleText", "ttxt", "TEXT");
mime_type("mime.application_rtf", "application/rtf", "rtf", 0, "MS Word", "MSWD", "TEXT");
mime_type("mime.application_x_compressed", "application/x-compressed", ".Z", 1, "MacCompress", "LZIV", "ZIVM");
mime_type("mime.application_x_tar", "application/x-tar", "tar", 1, "tar", "TAR ", "TARF");

mime_type("mime.Netscape_Source", "Netscape/Source", "", 2, "SimpleText", "ttxt", "TEXT");
pref("mime.Netscape_Source.description", "View Source");
mime_type("mime.Netscape_Telnet", "Netscape/Telnet", "", 1, "NCSA Telnet", "NCSA", "CONF");
mime_type("mime.Netscape_tn3270", "Netscape/tn3270", "", 1, "tn3270", "GFTM", "GFTS");

mime_type("mime.image_x_cmu_raster", "image/x-cmu-raster", "ras", 3, "Unknown", "ttxt", "????");
mime_type("mime.image_x_portable_anymap", "image/x-portable-anymap", "pnm", 3, "Unknown", "ttxt", "????");
mime_type("mime.image_x_portable_bitmap", "image/x-portable-bitmap", "pbm", 3, "Unknown", "ttxt", "????");
mime_type("mime.image_x_portable_graymap", "image/x-portable-graymap", "pgm", 3, "Unknown", "ttxt", "????");
mime_type("mime.image_x_rgb", "image/x-rgb", "rgb", 3, "Unknown", "ttxt", "????");
mime_type("mime.image_x_xpixmap", "image/x-xpixmap", "xpm", 2, "Unknown", "ttxt", "????");

// Add mime type for SimpleText read only files
mime_type("mime.SimpleText_ReadOnly", "text/plain", "", 2, "SimpleText", "ttxt", "ttro");
