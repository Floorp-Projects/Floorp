/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Robin J. Maxwell 11-22-96
 * Fredrik Roubert <roubert@google.com> 2010-07-23
 * Matt Austern <austern@google.com> 2010-07-23
 */

#ifndef _PRSTRMS_H
#define _PRSTRMS_H

#include <cstddef>
#include <istream>
#include <ostream>
#include <streambuf>

#include "prio.h"

#ifdef _MSC_VER
// http://support.microsoft.com/kb/q168958/
class PR_IMPLEMENT(std::_Mutex);
class PR_IMPLEMENT(std::ios_base);
#endif


class PR_IMPLEMENT(PRfilebuf): public std::streambuf
{
public:
    PRfilebuf();
    PRfilebuf(PRFileDesc *fd);
    PRfilebuf(PRFileDesc *fd, char_type *ptr, std::streamsize len);
    virtual ~PRfilebuf();

    bool is_open() const {
        return _fd != NULL;
    }

    PRfilebuf *open(
        const char *name,
        std::ios_base::openmode flags,
        PRIntn mode);
    PRfilebuf *attach(PRFileDesc *fd);
    PRfilebuf *close();

protected:
    virtual std::streambuf *setbuf(char_type *ptr, std::streamsize len);
    virtual pos_type seekoff(
        off_type offset,
        std::ios_base::seekdir dir,
        std::ios_base::openmode flags);
    virtual pos_type seekpos(
        pos_type pos,
        std::ios_base::openmode flags) {
        return seekoff(pos, std::ios_base::beg, flags);
    }
    virtual int sync();
    virtual int_type underflow();
    virtual int_type overflow(int_type c = traits_type::eof());

    // TODO: Override pbackfail(), showmanyc(), uflow(), xsgetn(), and xsputn().

private:
    bool allocate();
    void setb(char_type *buf_base, char_type *buf_end, bool user_buf);

    PRFileDesc *_fd;
    bool _opened;
    bool _allocated;
    bool _unbuffered;
    bool _user_buf;
    char_type *_buf_base;
    char_type *_buf_end;
};


class PR_IMPLEMENT(PRifstream): public std::istream
{
public:
    PRifstream();
    PRifstream(PRFileDesc *fd);
    PRifstream(PRFileDesc *fd, char_type *ptr, std::streamsize len);
    PRifstream(const char *name, openmode flags = in, PRIntn mode = 0);
    virtual ~PRifstream();

    PRfilebuf *rdbuf() const {
        return &_filebuf;
    }
    bool is_open() const {
        return _filebuf.is_open();
    }

    void open(const char *name, openmode flags = in, PRIntn mode = 0);
    void attach(PRFileDesc *fd);
    void close();

private:
    mutable PRfilebuf _filebuf;
};


class PR_IMPLEMENT(PRofstream): public std::ostream
{
public:
    PRofstream();
    PRofstream(PRFileDesc *fd);
    PRofstream(PRFileDesc *fd, char_type *ptr, std::streamsize len);
    PRofstream(const char *name, openmode flags = out, PRIntn mode = 0);
    virtual ~PRofstream();

    PRfilebuf *rdbuf() const {
        return &_filebuf;
    }
    bool is_open() const {
        return _filebuf.is_open();
    }

    void open(const char *name, openmode flags = out, PRIntn mode = 0);
    void attach(PRFileDesc *fd);
    void close();

private:
    mutable PRfilebuf _filebuf;
};


class PR_IMPLEMENT(PRfstream): public std::iostream
{
public:
    PRfstream();
    PRfstream(PRFileDesc *fd);
    PRfstream(PRFileDesc *fd, char_type *ptr, std::streamsize len);
    PRfstream(const char *name, openmode flags = in | out, PRIntn mode = 0);
    virtual ~PRfstream();

    PRfilebuf *rdbuf() const {
        return &_filebuf;
    }
    bool is_open() const {
        return _filebuf.is_open();
    }

    void open(const char *name, openmode flags = in | out, PRIntn mode = 0);
    void attach(PRFileDesc *fd);
    void close();

private:
    mutable PRfilebuf _filebuf;
};


#endif /* _PRSTRMS_H */
