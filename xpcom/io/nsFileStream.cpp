/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 
//	First checked in on 98/12/08 by John R. McMullen.
//  Since nsFileStream.h is entirely templates, common code (such as open())
//  which does not actually depend on the charT, can be placed here.

#include "nsFileStream.h"
#include "nsFileSpec.h"
#include "nsIFileSpec.h"
#include "nsIStringStream.h"

#include <string.h>
#include <stdio.h>


//========================================================================================
//          nsInputStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsInputStream::~nsInputStream()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
char nsInputStream::get()
//----------------------------------------------------------------------------------------
{
	char c;
    if (read(&c, sizeof(c)) == sizeof(c))
        return c;
    return 0;
}

//----------------------------------------------------------------------------------------
PRInt32 nsInputStream::read(void* s, PRInt32 n)
//----------------------------------------------------------------------------------------
{
  if (!mInputStream)
      return 0;
  PRInt32 result = 0;
  mInputStream->Read((char*)s, n, (PRUint32*)&result);
  if (result == 0)
      set_at_eof(PR_TRUE);
  return result;
} // nsInputStream::read

//----------------------------------------------------------------------------------------
static void TidyEndOfLine(char*& cp)
// Assumes that cp is pointing at \n or \r.  Nulls out the character, checks for
// a second terminator (of the opposite persuasion), and returns cp pointing past the
// entire eol construct (one or two characters).
//----------------------------------------------------------------------------------------
{
    char ch = *cp;
    *cp++ = '\0'; // terminate at the newline, then skip past it
    if ((ch == '\n' && *cp == '\r') || (ch == '\r' && *cp == '\n'))
        cp++; // possibly a pair.
}

//----------------------------------------------------------------------------------------
nsInputStream& nsInputStream::operator >> (char& c)
//----------------------------------------------------------------------------------------
{
	c = get();
	return *this;
}

//========================================================================================
//          nsOutputStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsOutputStream::~nsOutputStream()
//----------------------------------------------------------------------------------------
{
}

//----------------------------------------------------------------------------------------
void nsOutputStream::put(char c)
//----------------------------------------------------------------------------------------
{
    write(&c, sizeof(c));
}

//----------------------------------------------------------------------------------------
PRInt32 nsOutputStream::write(const void* s, PRInt32 n)
//----------------------------------------------------------------------------------------
{
  if (!mOutputStream)
      return 0;
  PRInt32 result = 0;
  mOutputStream->Write((char*)s, n, (PRUint32*)&result);
  return result;
} // nsOutputStream::write

//----------------------------------------------------------------------------------------
nsresult nsOutputStream::flush()
//----------------------------------------------------------------------------------------
{
  return NS_OK;
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (char c)
//----------------------------------------------------------------------------------------
{
	put(c);
	return *this;
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (const char* s)
//----------------------------------------------------------------------------------------
{
	if (s)
		write(s, strlen(s));
	return *this;
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (short val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%hd", val);
	return (*this << buf);
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (unsigned short val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%hu", val);
	return (*this << buf);
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (long val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%ld", val);
	return (*this << buf);
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (unsigned long val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%lu", val);
	return (*this << buf);
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (int val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%d", val);
	return (*this << buf);
}

//----------------------------------------------------------------------------------------
nsOutputStream& nsOutputStream::operator << (unsigned int val)
//----------------------------------------------------------------------------------------
{
	char buf[30];
	sprintf(buf, "%u", val);
	return (*this << buf);
}

//========================================================================================
//          nsRandomAccessInputStream
//========================================================================================

//----------------------------------------------------------------------------------------
PRBool nsRandomAccessInputStream::readline(char* s, PRInt32 n)
// This will truncate if the buffer is too small.  Result will always be null-terminated.
//----------------------------------------------------------------------------------------
{
    PRBool bufferLargeEnough = PR_TRUE; // result
    if (!s || !n)
        return PR_TRUE;

    PRIntn position = tell();
    if (position < 0)
        return PR_FALSE;
    PRInt32 bytesRead = read(s, n - 1);
    if (failed())
        return PR_FALSE;
    s[bytesRead] = '\0'; // always terminate at the end of the buffer
    char* tp = strpbrk(s, "\n\r");
    if (tp)
    {
        TidyEndOfLine(tp);
        bytesRead = (tp - s);
    }
    else if (!eof() && n-1 == bytesRead)
        bufferLargeEnough = PR_FALSE;
    position += bytesRead;
    seek(position);
    return bufferLargeEnough;
} // nsRandomAccessInputStream::readline

//========================================================================================
//          nsInputStringStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsInputStringStream::nsInputStringStream(const char* stringToRead)
//----------------------------------------------------------------------------------------
{
	nsISupports* stream;
	if (NS_FAILED(NS_NewCharInputStream(&stream, stringToRead)))
		return;
	mInputStream = do_QueryInterface(stream);
	mStore = do_QueryInterface(stream);
	NS_RELEASE(stream);
}

//----------------------------------------------------------------------------------------
nsInputStringStream::nsInputStringStream(const nsString& stringToRead)
//----------------------------------------------------------------------------------------
{
	nsISupports* stream;
	if (NS_FAILED(NS_NewStringInputStream(&stream, stringToRead)))
		return;
	mInputStream = do_QueryInterface(stream);
	mStore = do_QueryInterface(stream);
	NS_RELEASE(stream);
}

//========================================================================================
//          nsOutputStringStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsOutputStringStream::nsOutputStringStream(char*& stringToChange)
//----------------------------------------------------------------------------------------
{
	nsISupports* stream;
	if (NS_FAILED(NS_NewCharOutputStream(&stream, &stringToChange)))
		return;
	mOutputStream = do_QueryInterface(stream);
	mStore = do_QueryInterface(stream);
	NS_RELEASE(stream);
}

//----------------------------------------------------------------------------------------
nsOutputStringStream::nsOutputStringStream(nsString& stringToChange)
//----------------------------------------------------------------------------------------
{
	nsISupports* stream;
	if (NS_FAILED(NS_NewStringOutputStream(&stream, stringToChange)))
		return;
	mOutputStream = do_QueryInterface(stream);
	mStore = do_QueryInterface(stream);
	NS_RELEASE(stream);
}

//========================================================================================
//          nsInputFileStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsInputFileStream::nsInputFileStream(
	const nsFileSpec& inFile,
	int nsprMode,
	PRIntn accessMode)
//----------------------------------------------------------------------------------------
{
	nsISupports* stream;
	if (NS_FAILED(NS_NewIOFileStream(&stream, inFile, nsprMode, accessMode)))
        return;
	AssignFrom(stream);
	NS_RELEASE(stream);
} // nsInputFileStream::nsInputFileStream

//----------------------------------------------------------------------------------------
nsInputFileStream::nsInputFileStream(nsIFileSpec* inSpec)
//----------------------------------------------------------------------------------------
{
	nsIInputStream* stream;
	if (NS_FAILED(inSpec->GetInputStream(&stream)))
        return;
	AssignFrom(stream);
	NS_RELEASE(stream);
} // nsInputFileStream::nsInputFileStream

//----------------------------------------------------------------------------------------
nsInputFileStream::~nsInputFileStream()
//----------------------------------------------------------------------------------------
{
//    if (is_open())
//      close();
}

//----------------------------------------------------------------------------------------
void nsInputFileStream::AssignFrom(nsISupports* stream)
//----------------------------------------------------------------------------------------
{
	mFile = do_QueryInterface(stream);
	mInputStream = do_QueryInterface(stream);
	mStore = do_QueryInterface(stream);
	mFileInputStream = do_QueryInterface(stream);
}

//========================================================================================
//          nsOutputFileStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsOutputFileStream::nsOutputFileStream(nsIFileSpec* inSpec)
//----------------------------------------------------------------------------------------
{
	if (!inSpec)
		return;
	nsIOutputStream* stream;
	if (NS_FAILED(inSpec->GetOutputStream(&stream)))
	  return;
	AssignFrom(stream);
	NS_RELEASE(stream);
}

//----------------------------------------------------------------------------------------
nsOutputFileStream::~nsOutputFileStream()
//----------------------------------------------------------------------------------------
{
//    if (is_open())
//      close();
}
//----------------------------------------------------------------------------------------
void nsOutputFileStream::AssignFrom(nsISupports* stream)
//----------------------------------------------------------------------------------------
{
    mFile = do_QueryInterface(stream);
    mOutputStream = do_QueryInterface(stream);
    mStore = do_QueryInterface(stream);
    mFileOutputStream = do_QueryInterface(stream);
}

//----------------------------------------------------------------------------------------
nsresult nsOutputFileStream::flush()
//----------------------------------------------------------------------------------------
{
	if (mFileOutputStream)
		mFileOutputStream->Flush();
    return error();
}

//----------------------------------------------------------------------------------------
void nsOutputFileStream::abort()
//----------------------------------------------------------------------------------------
{
	mResult = NS_FILE_FAILURE;
	close();
}

//========================================================================================
//          nsSaveViaTempStream
//========================================================================================

//----------------------------------------------------------------------------------------
nsSaveViaTempStream::nsSaveViaTempStream(const nsFileSpec& inFileToSave)
//----------------------------------------------------------------------------------------
	: mFileToSave(inFileToSave)
	, mTempFileSpec(new nsFileSpec(inFileToSave))
{
	// Make sure the temp file's unique (in particular, different from the target file)
	mTempFileSpec->MakeUnique();
	open(*mTempFileSpec, (PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE), 0666);
} // nsSaveViaTempStream::nsSaveViaTempStream

//----------------------------------------------------------------------------------------
void nsSaveViaTempStream::close()
//----------------------------------------------------------------------------------------
{
	if (!mTempFileSpec)
		return;
	nsresult currentResult = mResult;
	Inherited::close();
	mResult = currentResult;
	if (error())
	{
		mTempFileSpec->Delete(PR_FALSE);
	}
	else
	{
		nsFileSpec thirdSpec(mFileToSave);
		thirdSpec.MakeUnique();
		nsSimpleCharString originalName(mFileToSave.GetLeafName());
		((nsFileSpec&)mFileToSave).Rename(nsSimpleCharString(thirdSpec.GetLeafName()));
		if (NS_SUCCEEDED(mTempFileSpec->Rename(originalName)) && mTempFileSpec->Valid())
			mFileToSave.Delete(PR_FALSE);
	}
	delete mTempFileSpec;
} // nsSaveViaTempStream::~nsSaveViaTempStream

//----------------------------------------------------------------------------------------
nsSaveViaTempStream::~nsSaveViaTempStream()
//----------------------------------------------------------------------------------------
{
	delete mTempFileSpec;
} // nsSaveViaTempStream::~nsSaveViaTempStream

//========================================================================================
//        Manipulators
//========================================================================================

//----------------------------------------------------------------------------------------
nsOutputStream& nsEndl(nsOutputStream& os)
//----------------------------------------------------------------------------------------
{
#if defined(XP_WIN) || defined(XP_OS2)
    os.write("\r\n", 2);
#elif defined (XP_MAC)
    os.put('\r');
#else
    os.put('\n');
#endif
    //os.flush();
    return os;
} // nsEndl
