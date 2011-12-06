//* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is Url Classifier code
 *
 * The Initial Developer of the Original Code is
 * the Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Dave Camp <dcamp@mozilla.com>
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef ProtocolParser_h__
#define ProtocolParser_h__

#include "HashStore.h"
#include "nsICryptoHMAC.h"

namespace mozilla {
namespace safebrowsing {

/**
 * Some helpers for parsing the safe
 */
class ProtocolParser {
public:
  struct ForwardedUpdate {
    nsCString table;
    nsCString url;
    nsCString mac;
  };

  ProtocolParser(PRUint32 aHashKey);
  ~ProtocolParser();

  nsresult Status() const { return mUpdateStatus; }

  nsresult Init(nsICryptoHash* aHasher);

  nsresult InitHMAC(const nsACString& aClientKey,
                    const nsACString& aServerMAC);
  nsresult FinishHMAC();

  void SetCurrentTable(const nsACString& aTable);

  nsresult Begin();
  nsresult AppendStream(const nsACString& aData);

  // Forget the table updates that were created by this pass.  It
  // becomes the caller's responsibility to free them.  This is shitty.
  TableUpdate *GetTableUpdate(const nsACString& aTable);
  void ForgetTableUpdates() { mTableUpdates.Clear(); }
  nsTArray<TableUpdate*> &GetTableUpdates() { return mTableUpdates; }

  // Update information.
  const nsTArray<ForwardedUpdate> &Forwards() const { return mForwards; }
  int32 UpdateWait() { return mUpdateWait; }
  bool ResetRequested() { return mResetRequested; }
  bool RekeyRequested() { return mRekeyRequested; }

private:
  nsresult ProcessControl(bool* aDone);
  nsresult ProcessMAC(const nsCString& aLine);
  nsresult ProcessExpirations(const nsCString& aLine);
  nsresult ProcessChunkControl(const nsCString& aLine);
  nsresult ProcessForward(const nsCString& aLine);
  nsresult AddForward(const nsACString& aUrl, const nsACString& aMac);
  nsresult ProcessChunk(bool* done);
  nsresult ProcessPlaintextChunk(const nsACString& aChunk);
  nsresult ProcessShaChunk(const nsACString& aChunk);
  nsresult ProcessHostAdd(const Prefix& aDomain, PRUint8 aNumEntries,
                          const nsACString& aChunk, PRUint32* aStart);
  nsresult ProcessHostSub(const Prefix& aDomain, PRUint8 aNumEntries,
                          const nsACString& aChunk, PRUint32* aStart);
  nsresult ProcessHostAddComplete(PRUint8 aNumEntries, const nsACString& aChunk,
                                  PRUint32 *aStart);
  nsresult ProcessHostSubComplete(PRUint8 numEntries, const nsACString& aChunk,
                                  PRUint32* start);
  bool NextLine(nsACString& aLine);

  void CleanupUpdates();

  enum ParserState {
    PROTOCOL_STATE_CONTROL,
    PROTOCOL_STATE_CHUNK
  };
  ParserState mState;

  enum ChunkType {
    CHUNK_ADD,
    CHUNK_SUB
  };

  struct ChunkState {
    ChunkType type;
    uint32 num;
    uint32 hashSize;
    uint32 length;
    void Clear() { num = 0; hashSize = 0; length = 0; }
  };
  ChunkState mChunkState;

  PRUint32 mHashKey;
  nsCOMPtr<nsICryptoHash> mCryptoHash;

  nsresult mUpdateStatus;
  nsCString mPending;

  nsCOMPtr<nsICryptoHMAC> mHMAC;
  nsCString mServerMAC;

  uint32 mUpdateWait;
  bool mResetRequested;
  bool mRekeyRequested;

  nsTArray<ForwardedUpdate> mForwards;
  nsTArray<TableUpdate*> mTableUpdates;
  TableUpdate *mTableUpdate;
};

}
}

#endif
