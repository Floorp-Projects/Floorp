/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

/*
 * Robin J. Maxwell 11-22-96
 */

#ifndef _PRSTRMS_H
#define _PRSTRMS_H

#include "prtypes.h"
#include "prio.h"

#ifdef _MSC_VER
#pragma warning( disable : 4275)
#endif
#include <iostream.h>

#if defined (PRFSTREAMS_BROKEN)

// fix it sometime

#define	 PRfilebuf	streambuf
#define  PRifstream	ifstream
#define	 PRofstream	ofstream
#define	 PRfstream	fstream

#else

class PR_IMPLEMENT(PRfilebuf): public streambuf
{
public:
    PRfilebuf();
    PRfilebuf(PRFileDesc *fd);
    PRfilebuf(PRFileDesc *fd, char * buffptr, int bufflen);
    ~PRfilebuf();
    virtual	int	overflow(int=EOF);
    virtual	int	underflow();
    virtual	streambuf *setbuf(char *buff, int bufflen);
    virtual	streampos seekoff(streamoff, ios::seek_dir, int);
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

class PR_IMPLEMENT(PRifstream): public istream {
public:
	PRifstream();
	PRifstream(const char *, int mode=ios::in, int flags = 0);
	PRifstream(PRFileDesc *);
	PRifstream(PRFileDesc *, char *, int);
	~PRifstream();

	streambuf * setbuf(char *, int);
	PRfilebuf* rdbuf(){return (PRfilebuf*) ios::rdbuf(); }

	void attach(PRFileDesc *fd);
	PRFileDesc *fd() {return rdbuf()->fd();}

	int is_open(){return rdbuf()->is_open();}
	void open(const char *, int mode=ios::in, int flags= 0);
	void close();
};

class PR_IMPLEMENT(PRofstream) : public ostream {
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
	
class PR_IMPLEMENT(PRfstream) : public iostream {
public:
	PRfstream();
	PRfstream(const char *name, int mode, int flags= 0);
	PRfstream(PRFileDesc *fd);
	PRfstream(PRFileDesc *fd, char *buff, int bufflen);
	~PRfstream();

	streambuf * setbuf(char *, int);
	PRfilebuf* rdbuf(){ return (PRfilebuf*) ostream::rdbuf(); }

	void attach(PRFileDesc *);
	PRFileDesc *fd() { return rdbuf()->fd(); }

	int is_open() { return rdbuf()->is_open(); }
	void open(const char *, int, int = 0);
	void close();
};

#endif

#endif /* _PRSTRMS_H */
