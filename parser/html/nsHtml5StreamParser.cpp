/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=2 et tw=79: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Henri Sivonen <hsivonen@iki.fi>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHtml5StreamParser.h"
#include "nsICharsetConverterManager.h"
#include "nsServiceManagerUtils.h"
#include "nsEncoderDecoderUtils.h"
#include "nsContentUtils.h"
#include "nsICharsetDetector.h"
#include "nsHtml5Tokenizer.h"
#include "nsIHttpChannel.h"
#include "nsHtml5Parser.h"
#include "nsHtml5TreeBuilder.h"
#include "nsHtml5AtomTable.h"

static NS_DEFINE_CID(kCharsetAliasCID, NS_CHARSETALIAS_CID);

NS_IMPL_CYCLE_COLLECTING_ADDREF(nsHtml5StreamParser)
NS_IMPL_CYCLE_COLLECTING_RELEASE(nsHtml5StreamParser)

NS_INTERFACE_TABLE_HEAD(nsHtml5StreamParser)
  NS_INTERFACE_TABLE2(nsHtml5StreamParser, 
                      nsIStreamListener, 
                      nsICharsetDetectionObserver)
  NS_INTERFACE_TABLE_TO_MAP_SEGUE_CYCLE_COLLECTION(nsHtml5StreamParser)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTION_3(nsHtml5StreamParser, mObserver, mRequest, mOwner)

nsHtml5StreamParser::nsHtml5StreamParser(nsHtml5TreeOpExecutor* aExecutor,
                                         nsHtml5Parser* aOwner)
  : mFirstBuffer(new nsHtml5UTF16Buffer(NS_HTML5_STREAM_PARSER_READ_BUFFER_SIZE))
  , mLastBuffer(mFirstBuffer)
  , mExecutor(aExecutor)
  , mTreeBuilder(new nsHtml5TreeBuilder(mExecutor->GetStage()))
  , mTokenizer(new nsHtml5Tokenizer(mTreeBuilder))
  , mTokenizerMutex("nsHtml5StreamParser mTokenizerMutex")
  , mOwner(aOwner)
  , mTerminatedMutex("nsHtml5StreamParser mTerminatedMutex")
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mAtomTable.Init(); // we aren't checking for OOM anyway...
  mTokenizer->setInterner(&mAtomTable);
  mTokenizer->setEncodingDeclarationHandler(this);
  // There's a zeroing operator new for everything else
}

nsHtml5StreamParser::~nsHtml5StreamParser()
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mTokenizer->end();
  mRequest = nsnull;
  mObserver = nsnull;
  mUnicodeDecoder = nsnull;
  mSniffingBuffer = nsnull;
  mMetaScanner = nsnull;
  while (mFirstBuffer) {
     nsHtml5UTF16Buffer* old = mFirstBuffer;
     mFirstBuffer = mFirstBuffer->next;
     delete old;
  }
  mExecutor = nsnull;
  mTreeBuilder = nsnull;
  mTokenizer = nsnull;
  mOwner = nsnull;
}

nsresult
nsHtml5StreamParser::GetChannel(nsIChannel** aChannel)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  return mRequest ? CallQueryInterface(mRequest, aChannel) :
                    NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsHtml5StreamParser::Notify(const char* aCharset, nsDetectionConfident aConf)
{
  if (aConf == eBestAnswer || aConf == eSureAnswer) {
    mCharset.Assign(aCharset);
    mCharsetSource = kCharsetFromAutoDetection;
    mTreeBuilder->SetDocumentCharset(mCharset);
  }
  return NS_OK;
}

nsresult
nsHtml5StreamParser::SetupDecodingAndWriteSniffingBufferAndCurrentSegment(const PRUint8* aFromSegment, // can be null
                                                                          PRUint32 aCount,
                                                                          PRUint32* aWriteCount)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsICharsetConverterManager> convManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = convManager->GetUnicodeDecoder(mCharset.get(), getter_AddRefs(mUnicodeDecoder));
  if (rv == NS_ERROR_UCONV_NOCONV) {
    mCharset.Assign("windows-1252"); // lower case is the raw form
    mCharsetSource = kCharsetFromWeakDocTypeDefault;
    rv = convManager->GetUnicodeDecoderRaw(mCharset.get(), getter_AddRefs(mUnicodeDecoder));
    mTreeBuilder->SetDocumentCharset(mCharset);
  }
  NS_ENSURE_SUCCESS(rv, rv);
  mUnicodeDecoder->SetInputErrorBehavior(nsIUnicodeDecoder::kOnError_Recover);
  return WriteSniffingBufferAndCurrentSegment(aFromSegment, aCount, aWriteCount);
}

nsresult
nsHtml5StreamParser::WriteSniffingBufferAndCurrentSegment(const PRUint8* aFromSegment, // can be null
                                                          PRUint32 aCount,
                                                          PRUint32* aWriteCount)
{
  nsresult rv = NS_OK;
  if (mSniffingBuffer) {
    PRUint32 writeCount;
    rv = WriteStreamBytes(mSniffingBuffer, mSniffingLength, &writeCount);
    NS_ENSURE_SUCCESS(rv, rv);
    mSniffingBuffer = nsnull;
  }
  mMetaScanner = nsnull;
  if (aFromSegment) {
    rv = WriteStreamBytes(aFromSegment, aCount, aWriteCount);
  }
  return rv;
}

nsresult
nsHtml5StreamParser::SetupDecodingFromBom(const char* aCharsetName, const char* aDecoderCharsetName)
{
  nsresult rv = NS_OK;
  nsCOMPtr<nsICharsetConverterManager> convManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = convManager->GetUnicodeDecoderRaw(aDecoderCharsetName, getter_AddRefs(mUnicodeDecoder));
  NS_ENSURE_SUCCESS(rv, rv);
  mCharset.Assign(aCharsetName);
  mCharsetSource = kCharsetFromByteOrderMark;
  mTreeBuilder->SetDocumentCharset(mCharset);
  mSniffingBuffer = nsnull;
  mMetaScanner = nsnull;
  mBomState = BOM_SNIFFING_OVER;
  return rv;
}

nsresult
nsHtml5StreamParser::FinalizeSniffing(const PRUint8* aFromSegment, // can be null
                                      PRUint32 aCount,
                                      PRUint32* aWriteCount,
                                      PRUint32 aCountToSniffingLimit)
{
  // meta scan failed.
  if (mCharsetSource >= kCharsetFromHintPrevDoc) {
    return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(aFromSegment, aCount, aWriteCount);
  }
  // maybe try chardet now; instantiation copied from nsDOMFile
  const nsAdoptingString& detectorName = nsContentUtils::GetLocalizedStringPref("intl.charset.detector");
  if (!detectorName.IsEmpty()) {
    nsCAutoString detectorContractID;
    detectorContractID.AssignLiteral(NS_CHARSET_DETECTOR_CONTRACTID_BASE);
    AppendUTF16toUTF8(detectorName, detectorContractID);
    nsCOMPtr<nsICharsetDetector> detector = do_CreateInstance(detectorContractID.get());
    if (detector) {
      nsresult rv = detector->Init(this);
      NS_ENSURE_SUCCESS(rv, rv);
      PRBool dontFeed = PR_FALSE;
      if (mSniffingBuffer) {
        rv = detector->DoIt((const char*)mSniffingBuffer.get(), mSniffingLength, &dontFeed);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      if (!dontFeed && aFromSegment) {
        rv = detector->DoIt((const char*)aFromSegment, aCountToSniffingLimit, &dontFeed);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      rv = detector->Done();
      NS_ENSURE_SUCCESS(rv, rv);
      // fall thru; callback may have changed charset
    } else {
      NS_ERROR("Could not instantiate charset detector.");
    }
  }
  if (mCharsetSource == kCharsetUninitialized) {
    // Hopefully this case is never needed, but dealing with it anyway
    mCharset.Assign("windows-1252");
    mCharsetSource = kCharsetFromWeakDocTypeDefault;
    mTreeBuilder->SetDocumentCharset(mCharset);
  }
  return SetupDecodingAndWriteSniffingBufferAndCurrentSegment(aFromSegment, aCount, aWriteCount);
}

nsresult
nsHtml5StreamParser::SniffStreamBytes(const PRUint8* aFromSegment,
                                      PRUint32 aCount,
                                      PRUint32* aWriteCount)
{
  nsresult rv = NS_OK;
  PRUint32 writeCount;
  for (PRUint32 i = 0; i < aCount; i++) {
    switch (mBomState) {
      case BOM_SNIFFING_NOT_STARTED:
        NS_ASSERTION(i == 0, "Bad BOM sniffing state.");
        switch (*aFromSegment) {
          case 0xEF:
            mBomState = SEEN_UTF_8_FIRST_BYTE;
            break;
          case 0xFF:
            mBomState = SEEN_UTF_16_LE_FIRST_BYTE;
            break;
          case 0xFE:
            mBomState = SEEN_UTF_16_BE_FIRST_BYTE;
            break;
          default:
            mBomState = BOM_SNIFFING_OVER;
            break;
        }
        break;
      case SEEN_UTF_16_LE_FIRST_BYTE:
        if (aFromSegment[i] == 0xFE) {
          rv = SetupDecodingFromBom("UTF-16", "UTF-16LE"); // upper case is the raw form
          NS_ENSURE_SUCCESS(rv, rv);
          PRUint32 count = aCount - (i + 1);
          rv = WriteStreamBytes(aFromSegment + (i + 1), count, &writeCount);
          NS_ENSURE_SUCCESS(rv, rv);
          *aWriteCount = writeCount + (i + 1);
          return rv;
        }
        mBomState = BOM_SNIFFING_OVER;
        break;
      case SEEN_UTF_16_BE_FIRST_BYTE:
        if (aFromSegment[i] == 0xFF) {
          rv = SetupDecodingFromBom("UTF-16", "UTF-16BE"); // upper case is the raw form
          NS_ENSURE_SUCCESS(rv, rv);
          PRUint32 count = aCount - (i + 1);
          rv = WriteStreamBytes(aFromSegment + (i + 1), count, &writeCount);
          NS_ENSURE_SUCCESS(rv, rv);
          *aWriteCount = writeCount + (i + 1);
          return rv;
        }
        mBomState = BOM_SNIFFING_OVER;
        break;
      case SEEN_UTF_8_FIRST_BYTE:
        if (aFromSegment[i] == 0xBB) {
          mBomState = SEEN_UTF_8_SECOND_BYTE;
        } else {
          mBomState = BOM_SNIFFING_OVER;
        }
        break;
      case SEEN_UTF_8_SECOND_BYTE:
        if (aFromSegment[i] == 0xBF) {
          rv = SetupDecodingFromBom("UTF-8", "UTF-8"); // upper case is the raw form
          NS_ENSURE_SUCCESS(rv, rv);
          PRUint32 count = aCount - (i + 1);
          rv = WriteStreamBytes(aFromSegment + (i + 1), count, &writeCount);
          NS_ENSURE_SUCCESS(rv, rv);
          *aWriteCount = writeCount + (i + 1);
          return rv;
        }
        mBomState = BOM_SNIFFING_OVER;
        break;
      default:
        goto bom_loop_end;
    }
  }
  // if we get here, there either was no BOM or the BOM sniffing isn't complete yet
  bom_loop_end:
  
  if (!mMetaScanner) {
    mMetaScanner = new nsHtml5MetaScanner();
  }
  
  if (mSniffingLength + aCount >= NS_HTML5_STREAM_PARSER_SNIFFING_BUFFER_SIZE) {
    // this is the last buffer
    PRUint32 countToSniffingLimit = NS_HTML5_STREAM_PARSER_SNIFFING_BUFFER_SIZE - mSniffingLength;
    nsHtml5ByteReadable readable(aFromSegment, aFromSegment + countToSniffingLimit);
    mMetaScanner->sniff(&readable, getter_AddRefs(mUnicodeDecoder), mCharset);
    if (mUnicodeDecoder) {
      mUnicodeDecoder->SetInputErrorBehavior(nsIUnicodeDecoder::kOnError_Recover);
      // meta scan successful
      mCharsetSource = kCharsetFromMetaPrescan;
      mTreeBuilder->SetDocumentCharset(mCharset);
      mMetaScanner = nsnull;
      return WriteSniffingBufferAndCurrentSegment(aFromSegment, aCount, aWriteCount);
    }
    return FinalizeSniffing(aFromSegment, aCount, aWriteCount, countToSniffingLimit);
  }

  // not the last buffer
  nsHtml5ByteReadable readable(aFromSegment, aFromSegment + aCount);
  mMetaScanner->sniff(&readable, getter_AddRefs(mUnicodeDecoder), mCharset);
  if (mUnicodeDecoder) {
    // meta scan successful
    mCharsetSource = kCharsetFromMetaPrescan;
    mTreeBuilder->SetDocumentCharset(mCharset);
    mMetaScanner = nsnull;
    return WriteSniffingBufferAndCurrentSegment(aFromSegment, aCount, aWriteCount);
  }
  if (!mSniffingBuffer) {
    mSniffingBuffer = new PRUint8[NS_HTML5_STREAM_PARSER_SNIFFING_BUFFER_SIZE];
  }
  memcpy(mSniffingBuffer + mSniffingLength, aFromSegment, aCount);
  mSniffingLength += aCount;
  *aWriteCount = aCount;
  return NS_OK;
}

nsresult
nsHtml5StreamParser::WriteStreamBytes(const PRUint8* aFromSegment,
                                      PRUint32 aCount,
                                      PRUint32* aWriteCount)
{
  // mLastBuffer always points to a buffer of the size NS_HTML5_STREAM_PARSER_READ_BUFFER_SIZE.
  if (mLastBuffer->getEnd() == NS_HTML5_STREAM_PARSER_READ_BUFFER_SIZE) {
    mLastBuffer = (mLastBuffer->next = new nsHtml5UTF16Buffer(NS_HTML5_STREAM_PARSER_READ_BUFFER_SIZE));
  }
  PRUint32 totalByteCount = 0;
  for (;;) {
    PRInt32 end = mLastBuffer->getEnd();
    PRInt32 byteCount = aCount - totalByteCount;
    PRInt32 utf16Count = NS_HTML5_STREAM_PARSER_READ_BUFFER_SIZE - end;

    NS_ASSERTION(utf16Count, "Trying to convert into a buffer with no free space!");

    nsresult convResult = mUnicodeDecoder->Convert((const char*)aFromSegment, &byteCount, mLastBuffer->getBuffer() + end, &utf16Count);

    end += utf16Count;
    mLastBuffer->setEnd(end);
    totalByteCount += byteCount;
    aFromSegment += byteCount;

    NS_ASSERTION(mLastBuffer->getEnd() <= NS_HTML5_STREAM_PARSER_READ_BUFFER_SIZE, "The Unicode decoder wrote too much data.");

    if (NS_FAILED(convResult)) {
      if (totalByteCount < aCount) { // mimicking nsScanner even though this seems wrong
        ++totalByteCount;
        ++aFromSegment;
      }
      mLastBuffer->getBuffer()[end] = 0xFFFD;
      ++end;
      mLastBuffer->setEnd(end);
      if (end == NS_HTML5_STREAM_PARSER_READ_BUFFER_SIZE) {
          mLastBuffer = (mLastBuffer->next = new nsHtml5UTF16Buffer(NS_HTML5_STREAM_PARSER_READ_BUFFER_SIZE));
      }
      mUnicodeDecoder->Reset();
      if (totalByteCount == aCount) {
        *aWriteCount = totalByteCount;
        return NS_OK;
      }
    } else if (convResult == NS_PARTIAL_MORE_OUTPUT) {
      mLastBuffer = (mLastBuffer->next = new nsHtml5UTF16Buffer(NS_HTML5_STREAM_PARSER_READ_BUFFER_SIZE));
      NS_ASSERTION(totalByteCount < aCount, "The Unicode decoder has consumed too many bytes.");
    } else {
      NS_ASSERTION(totalByteCount == aCount, "The Unicode decoder consumed the wrong number of bytes.");
      *aWriteCount = totalByteCount;
      return NS_OK;
    }
  }
}

// nsIRequestObserver methods:
nsresult
nsHtml5StreamParser::OnStartRequest(nsIRequest* aRequest, nsISupports* aContext)
{
  NS_PRECONDITION(STREAM_NOT_STARTED == mStreamState,
                  "Got OnStartRequest when the stream had already started.");
  NS_PRECONDITION(mExecutor->GetLifeCycle() == NOT_STARTED, 
                  "Got OnStartRequest at the wrong stage in the executor life cycle.");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  if (mObserver) {
    mObserver->OnStartRequest(aRequest, aContext);
  }
  mRequest = aRequest;

  mStreamState = STREAM_BEING_READ;

  PRBool scriptingEnabled = mExecutor->IsScriptEnabled();
  mOwner->StartTokenizer(scriptingEnabled);
  mTreeBuilder->setScriptingEnabled(scriptingEnabled);
  mTokenizer->start();
  mExecutor->Start();
  mExecutor->StartReadingFromStage();
  /*
   * If you move the following line, be very careful not to cause 
   * WillBuildModel to be called before the document has had its 
   * script global object set.
   */
  mExecutor->WillBuildModel(eDTDMode_unknown);
  
  nsresult rv = NS_OK;

  mReparseForbidden = PR_FALSE;
  nsCOMPtr<nsIHttpChannel> httpChannel(do_QueryInterface(mRequest, &rv));
  if (NS_SUCCEEDED(rv)) {
    nsCAutoString method;
    httpChannel->GetRequestMethod(method);
    // XXX does Necko have a way to renavigate POST, etc. without hitting
    // the network?
    if (!method.EqualsLiteral("GET")) {
      // This is the old Gecko behavior but the HTML5 spec disagrees.
      // Don't reparse on POST.
      mReparseForbidden = PR_TRUE;
    }
  }
  
  if (mCharsetSource < kCharsetFromChannel) {
    // we aren't ready to commit to an encoding yet
    // leave converter uninstantiated for now
    return NS_OK;
  }
  
  nsCOMPtr<nsICharsetConverterManager> convManager = do_GetService(NS_CHARSETCONVERTERMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = convManager->GetUnicodeDecoder(mCharset.get(), getter_AddRefs(mUnicodeDecoder));
  NS_ENSURE_SUCCESS(rv, rv);
  mUnicodeDecoder->SetInputErrorBehavior(nsIUnicodeDecoder::kOnError_Recover);
  return NS_OK;
}

nsresult
nsHtml5StreamParser::OnStopRequest(nsIRequest* aRequest,
                             nsISupports* aContext,
                             nsresult status)
{
  NS_PRECONDITION(STREAM_BEING_READ == mStreamState,
                  "Stream ended without being open.");
  NS_ASSERTION(mRequest == aRequest, "Got Stop on wrong stream.");
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  nsresult rv = NS_OK;
  if (!mUnicodeDecoder) {
    PRUint32 writeCount;
    rv = FinalizeSniffing(nsnull, 0, &writeCount, 0);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  mStreamState = STREAM_ENDED;

  if (mObserver) {
    mObserver->OnStopRequest(aRequest, aContext, status);
  }
  // TODO: proxy this to parser thread
  if (!mWaitingForScripts) {
    ParseUntilScript();
  }  
  return NS_OK;
}

// nsIStreamListener method:
/*
 * This function is invoked as a result of a call to a stream's
 * ReadSegments() method. It is called for each contiguous buffer
 * of data in the underlying stream or pipe. Using ReadSegments
 * allows us to avoid copying data to read out of the stream.
 */
NS_METHOD
nsHtml5StreamParser::ParserWriteFunc(nsIInputStream* aInStream,
                void* aHtml5StreamParser,
                const char* aFromSegment,
                PRUint32 aToOffset,
                PRUint32 aCount,
                PRUint32* aWriteCount)
{
  nsHtml5StreamParser* streamParser = static_cast<nsHtml5StreamParser*> (aHtml5StreamParser);
  if (streamParser->HasDecoder()) {
    return streamParser->WriteStreamBytes((const PRUint8*)aFromSegment, aCount, aWriteCount);
  } else {
    return streamParser->SniffStreamBytes((const PRUint8*)aFromSegment, aCount, aWriteCount);
  }
}

nsresult
nsHtml5StreamParser::OnDataAvailable(nsIRequest* aRequest,
                               nsISupports* aContext,
                               nsIInputStream* aInStream,
                               PRUint32 aSourceOffset,
                               PRUint32 aLength)
{
  NS_PRECONDITION(STREAM_BEING_READ == mStreamState,
                  "OnDataAvailable called when stream not open.");
  NS_ASSERTION(mRequest == aRequest, "Got data on wrong stream.");
  PRUint32 totalRead;
  nsresult rv = aInStream->ReadSegments(nsHtml5StreamParser::ParserWriteFunc, 
                                        static_cast<void*> (this), 
                                        aLength, 
                                        &totalRead);
  NS_ASSERTION(totalRead == aLength, "ReadSegments read the wrong number of bytes.");
  if (!mWaitingForScripts) {
    ParseUntilScript();
  }
  return rv;
}

void
nsHtml5StreamParser::internalEncodingDeclaration(nsString* aEncoding)
{
  if (mCharsetSource >= kCharsetFromMetaTag) { // this threshold corresponds to "confident" in the HTML5 spec
    return;
  }
  nsresult rv = NS_OK;
  nsCOMPtr<nsICharsetAlias> calias(do_GetService(kCharsetAliasCID, &rv));
  if (NS_FAILED(rv)) {
    return;
  }
  nsCAutoString newEncoding;
  CopyUTF16toUTF8(*aEncoding, newEncoding);
  PRBool eq;
  rv = calias->Equals(newEncoding, mCharset, &eq);
  if (NS_FAILED(rv)) {
    return;
  }
  if (eq) {
    mCharsetSource = kCharsetFromMetaTag; // become confident
    return;
  }
  
  // XXX check HTML5 non-IANA aliases here

  // The encodings are different. We want to reparse.
  if (mReparseForbidden) {
    return; // not reparsing after all
  }
  
  // we still want to reparse
  mTreeBuilder->NeedsCharsetSwitchTo(newEncoding);
  mTreeBuilder->Flush();
  // the tree op executor will cause the stream parser to terminate
  // if the charset switch request is accepted
}

void
nsHtml5StreamParser::ParseUntilScript()
{
  if (IsTerminated()) {
    return;
  }

  // TODO: Relax this mutex so that the parser doesn't speculate to
  // completion when it's already known that the speculation will fail.
  mozilla::MutexAutoLock autoLock(mTokenizerMutex);

  for (;;) {
    if (!mFirstBuffer->hasMore()) {
      if (mFirstBuffer == mLastBuffer) {
        switch (mStreamState) {
          case STREAM_BEING_READ:
            // never release the last buffer. instead just zero its indeces for refill
            mFirstBuffer->setStart(0);
            mFirstBuffer->setEnd(0);
            mTreeBuilder->Flush();
            return; // no more data for now but expecting more
          case STREAM_ENDED:
            Terminate(); // TODO Don't terminate if this is a speculation
            mTokenizer->eof();
            mTreeBuilder->StreamEnded();
            mTreeBuilder->Flush();
            TellExecutorToFlush();
            return; // no more data and not expecting more
          default:
            NS_NOTREACHED("It should be impossible to reach this.");
            return;
        }
      } else {
        nsHtml5UTF16Buffer* oldBuf = mFirstBuffer;
        mFirstBuffer = mFirstBuffer->next;
        delete oldBuf;
        continue;
      }
    }

    // now we have a non-empty buffer
    mFirstBuffer->adjust(mLastWasCR);
    mLastWasCR = PR_FALSE;
    if (mFirstBuffer->hasMore()) {
      mLastWasCR = mTokenizer->tokenizeBuffer(mFirstBuffer);
      // At this point, internalEncodingDeclaration() may have called 
      // Terminate, but that never happens together with script.
      // Can't assert that here, though, because it's possible that the main
      // thread has called Terminate() while this thread was parsing.
      if (IsTerminated()) {
        return;
      }
      if (mTreeBuilder->HasScript()) {
        mTreeBuilder->AddSnapshotToScript(mTreeBuilder->newSnapshot());
        mTreeBuilder->Flush();
        TellExecutorToFlush();
        // XXX start speculation
        mWaitingForScripts = PR_TRUE;
        return; // ContinueAfterScripts() will re-enable this parser
      }
      mTreeBuilder->MaybeFlush();
    }
    continue;
  }
}

void
nsHtml5StreamParser::ContinueAfterScripts(nsHtml5Tokenizer* aTokenizer, 
                                          nsHtml5TreeBuilder* aTreeBuilder,
                                          PRBool aLastWasCR)
{
  NS_ASSERTION(NS_IsMainThread(), "Wrong thread!");
  mExecutor->StartReadingFromStage();
  // TODO:
  // test if the state of the argument tokenizer and tree builder match
  // the earliest speculation.
  // If not, rendez-vous at barrier, zaps all speculations, rewind the stream 
  // and copy over the state.
  // If yes:
  // If there are multiple speculations or the stream parser has terminated.
  // load the tree op queue from the earliest speculation into the tree op 
  // executor and discard the stream data for that speculation. Return.
  // Otherwise, rendez-vous at barrier, load the tree op queue from the 
  // speculation into the tree op executor, set the tree op executor to read 
  // from the stage, set the stream parser tree builder to write to stage,
  // discard the stream data for the speculation.
  
  {
    mozilla::MutexAutoLock autoLock(mTokenizerMutex); 

    // Approximation: Copy state over for now unconditionally.
    mLastWasCR = aLastWasCR;
    mTokenizer->loadState(aTokenizer);
    mTreeBuilder->loadState(aTreeBuilder, &mAtomTable);
    
    mWaitingForScripts = PR_FALSE;
  }
  // TODO: proxy the tail of this method to the parser thread
  ParseUntilScript();
}

class nsHtml5StreamParserExecutorFlushEvent : public nsRunnable
{
public:
  nsRefPtr<nsHtml5StreamParser> mStreamParser;
  nsHtml5StreamParserExecutorFlushEvent(nsHtml5StreamParser* aStreamParser)
    : mStreamParser(aStreamParser)
  {}
  NS_IMETHODIMP Run()
  {
    mStreamParser->DoExecFlush();
    return NS_OK;
  }
};

void
nsHtml5StreamParser::DoExecFlush()
{
  mExecutor->Flush();
}

void
nsHtml5StreamParser::TellExecutorToFlush()
{
  // TODO: Make this cross-thread
  nsCOMPtr<nsIRunnable> event = new nsHtml5StreamParserExecutorFlushEvent(this);
  if (NS_FAILED(NS_DispatchToMainThread(event))) {
    NS_WARNING("failed to dispatch executor flush event");
  }  
}
