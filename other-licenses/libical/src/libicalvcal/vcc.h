/***************************************************************************
(C) Copyright 1996 Apple Computer, Inc., AT&T Corp., International             
Business Machines Corporation and Siemens Rolm Communications Inc.             
                                                                               
For purposes of this license notice, the term Licensors shall mean,            
collectively, Apple Computer, Inc., AT&T Corp., International                  
Business Machines Corporation and Siemens Rolm Communications Inc.             
The term Licensor shall mean any of the Licensors.                             
                                                                               
Subject to acceptance of the following conditions, permission is hereby        
granted by Licensors without the need for written agreement and without        
license or royalty fees, to use, copy, modify and distribute this              
software for any purpose.                                                      
                                                                               
The above copyright notice and the following four paragraphs must be           
reproduced in all copies of this software and any software including           
this software.                                                                 
                                                                               
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS AND NO LICENSOR SHALL HAVE       
ANY OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS OR       
MODIFICATIONS.                                                                 
                                                                               
IN NO EVENT SHALL ANY LICENSOR BE LIABLE TO ANY PARTY FOR DIRECT,              
INDIRECT, SPECIAL OR CONSEQUENTIAL DAMAGES OR LOST PROFITS ARISING OUT         
OF THE USE OF THIS SOFTWARE EVEN IF ADVISED OF THE POSSIBILITY OF SUCH         
DAMAGE.                                                                        
                                                                               
EACH LICENSOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, EXPRESS OR IMPLIED,       
INCLUDING BUT NOT LIMITED TO ANY WARRANTY OF NONINFRINGEMENT OR THE            
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR             
PURPOSE.                                                                       

The software is provided with RESTRICTED RIGHTS.  Use, duplication, or         
disclosure by the government are subject to restrictions set forth in          
DFARS 252.227-7013 or 48 CFR 52.227-19, as applicable.                         

***************************************************************************/

#ifndef __VCC_H__
#define __VCC_H__ 1

#include "vobject.h"


#if defined(__CPLUSPLUS__) || defined(__cplusplus)
extern "C" {
#endif

typedef void (*MimeErrorHandler)(char *);

extern DLLEXPORT(void) registerMimeErrorHandler(MimeErrorHandler);

extern DLLEXPORT(VObject*) Parse_MIME(const char *input, unsigned long len);
extern DLLEXPORT(VObject*) Parse_MIME_FromFileName(char* fname);


/* NOTE regarding Parse_MIME_FromFile
The function above, Parse_MIME_FromFile, comes in two flavors,
neither of which is exported from the DLL. Each version takes
a CFile or FILE* as a parameter, neither of which can be
passed across a DLL interface (at least that is my experience).
If you are linking this code into your build directly then
you may find them a more convenient API that the other flavors
that take a file name. If you use them with the DLL LIB you
will get a link error.
*/


#if INCLUDEMFC
extern VObject* Parse_MIME_FromFile(CFile *file);
#else
extern VObject* Parse_MIME_FromFile(FILE *file);
#endif

#if defined(__CPLUSPLUS__) || defined(__cplusplus)
}
#endif

#endif /* __VCC_H__ */

