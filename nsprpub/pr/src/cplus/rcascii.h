/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
** Class definitions to format ASCII data.
*/

#if defined(_RCASCII_H)
#else
#define _RCASCII_H

/*
** RCFormatStuff
**  This class maintains no state - that is the responsibility of
**  the class' client. For each call to Sx_printf(), the StuffFunction
**  will be called for each embedded "%" in the 'fmt' string and once
**  for each interveaning literal.
*/
class PR_IMPLEMENT(RCFormatStuff)
{
public:
    RCFormatStuff();
    virtual ~RCFormatStuff();

    /*
    ** Process the arbitrary argument list using as indicated by
    ** the 'fmt' string. Each segment of the processing the stuff
    ** function is called with the relavent translation.
    */
    virtual PRInt32 Sx_printf(void *state, const char *fmt, ...);

    /*
    ** The 'state' argument is not processed by the runtime. It
    ** is merely passed from the Sx_printf() call. It is intended
    ** to be used by the client to figure out what to do with the
    ** new string.
    **
    ** The new string ('stuff') is ASCII translation driven by the
    ** Sx_printf()'s 'fmt' string. It is not guaranteed to be a null
    ** terminated string.
    **
    ** The return value is the number of bytes copied from the 'stuff'
    ** string. It is accumulated for each of the calls to the stuff
    ** function and returned from the original caller of Sx_printf().
    */
    virtual PRSize StuffFunction(
        void *state, const char *stuff, PRSize stufflen) = 0;
};  /* RCFormatStuff */


/*
** RCFormatBuffer
**  The caller is supplying the buffer, the runtime is doing all
**  the conversion. The object contains no state, so is reusable
**  and reentrant.
*/
class PR_IMPLEMENT(RCFormatBuffer): public RCFormatStuff
{
public:
    RCFormatBuffer();
    virtual ~RCFormatBuffer();

    /*
    ** Format the trailing arguments as indicated by the 'fmt'
    ** string. Put the result in 'buffer'. Return the number
    ** of bytes moved into 'buffer'. 'buffer' will always be
    ** a properly terminated string even if the convresion fails.
    */
    virtual PRSize Sn_printf(
        char *buffer, PRSize length, const char *fmt, ...);

    virtual char *Sm_append(char *buffer, const char *fmt, ...);

private:
    /*
    ** This class overrides the stuff function, does not preserve
    ** its virtual-ness and no longer allows the clients to call
    ** it in the clear. In other words, it is now the implementation
    ** for this class.
    */
    PRSize StuffFunction(void*, const char*, PRSize);
        
};  /* RCFormatBuffer */

/*
** RCFormat
**  The runtime is supplying the buffer. The object has state - the
**  buffer. Each operation must run to completion before the object
**  can be reused. When it is, the buffer is reset (whatever that
**  means). The result of a conversion is available via the extractor.
**  After extracted, the memory still belongs to the class - if the
**  caller wants to retain or modify, it must first be copied.
*/
class PR_IMPLEMENT(RCFormat): pubic RCFormatBuffer
{
public:
    RCFormat();
    virtual ~RCFormat();

    /*
    ** Translate the trailing arguments according to the 'fmt'
    ** string and store the results in the object.
    */
    virtual PRSize Sm_printf(const char *fmt, ...);

    /*
    ** Extract the latest translation.
    ** The object does not surrender the memory occupied by
    ** the string. If the caller wishes to modify the data,
    ** it must first be copied.
    */
    const char*();

private:
    char *buffer;

    RCFormat(const RCFormat&);
    RCFormat& operator=(const RCFormat&);
}; /* RCFormat */

/*
** RCPrint
**  The output is formatted and then written to an associated file
**  descriptor. The client can provide a suitable file descriptor
**  or can indicate that the output should be directed to one of
**  the well-known "console" devices.
*/
class PR_IMPLEMENT(RCPrint): public RCFormat
{
    virtual ~RCPrint();
    RCPrint(RCIO* output);
    RCPrint(RCFileIO::SpecialFile output);

    virtual PRSize Printf(const char *fmt, ...);
private:
    RCPrint();
};  /* RCPrint */

#endif /* defined(_RCASCII_H) */

/* RCAscii.h */
