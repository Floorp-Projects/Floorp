/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
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

// The originals are in: nsprpub/lib/pstreams/
// currently not being built into nspr.. these files will go away.

#ifndef __PRSTRM
#define __PRSTRM

#include "prtypes.h"
#include "prio.h"
#include <iostream.h>

#ifdef XP_MAC

class PRofstream : public ofstream
{
public:
	inline void attach(PRFileDesc *) {};
};

#else


#if defined(__GNUC__)
#define _PRSTR_BP _strbuf
#define _PRSTR_DELBUF(x)    /* as nothing */
#define _PRSTR_DELBUF_C(c, x)  /* as nothing */
#elif defined(WIN32)
#define _PRSTR_BP bp
#define _PRSTR_DELBUF(x)	delbuf(x)
#define _PRSTR_DELBUF_C(c, x)	c::_PRSTR_DELBUF(x)
#elif defined(VMS)
#undef  _PRSTR_BP
#define _PRSTR_DELBUF(x) /* as nothing */
#define _PRSTR_DELBUF_C(c, x)   /* as nothing */
#elif defined(OSF1)
#define _PRSTR_BP m_psb
#define _PRSTR_DELBUF(x) /* as nothing */
#define _PRSTR_DELBUF_C(c, x)	/* as nothing */
#elif defined(__QNX__)
#undef  _PRSTR_BP
#define _PRSTR_DELBUF(x) /* as nothing */
#define _PRSTR_DELBUF_C(c, x)   /* as nothing */
#else
#define _PRSTR_BP bp
// Unix compilers do not believe in encapsulation
// At least on Solaris this is also ignored
#define _PRSTR_DELBUF(x)	delbuf = x
#define _PRSTR_DELBUF_C(c, x)	c::_PRSTR_DELBUF(x)
#endif

#ifndef XP_MAC
class PR_IMPLEMENT(PRfilebuf): public streambuf
#else
__declspec(export) class PRfilebuf : public streambuf
#endif
{
public:
    PRfilebuf();
    PRfilebuf(PRFileDesc *fd);
    PRfilebuf(PRFileDesc *fd, char * buffptr, int bufflen);
    ~PRfilebuf();
    virtual	int	overflow(int=EOF);
    virtual	int	underflow();
    virtual	streambuf *setbuf(char *buff, int bufflen);
#ifndef XP_MAC
    virtual	streampos seekoff(streamoff, ios::seek_dir, int);
#else
    virtual	streampos seekoff(streamoff, ios_base::seekdir, int);
#endif
    virtual int sync();
    PRfilebuf *open(const char *name, int mode, int flags);
   	PRfilebuf *attach(PRFileDesc *fd);
    PRfilebuf *close();
   	int	is_open() const {return (_fd != 0);}
    PRFileDesc *fd(){return _fd;}

private:
    PRFileDesc * _fd;
    PRBool _opened;
	PRBool _allocated;
};


#ifndef XP_MAC
class PR_IMPLEMENT(PRofstream) : public ostream {
#else
__declspec(export) class PRofstream : public ostream {
#endif
public:
	PRofstream();
	PRofstream(const char *, int mode=ios::out, int flags = 0);
	PRofstream(PRFileDesc *);
	PRofstream(PRFileDesc *, char *, int);
	~PRofstream();

	streambuf * setbuf(char *, int);
	PRfilebuf* rdbuf() { return (PRfilebuf*) ios::rdbuf(); }

	void attach(PRFileDesc *);
	PRFileDesc *fd() {return rdbuf()->fd();}

	int is_open(){return rdbuf()->is_open();}
	void open(const char *, int =ios::out, int = 0);
	void close();
};

#endif /* XP_MAC */
#endif /* __PRSTRM */
