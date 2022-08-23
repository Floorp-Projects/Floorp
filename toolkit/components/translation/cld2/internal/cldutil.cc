// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//
// Author: dsites@google.com (Dick Sites)
// Updated 2014.01 for dual table lookup
//

#include "cldutil.h"
#include <string>

#include "cld2tablesummary.h"
#include "integral_types.h"
#include "port.h"
#include "utf8statetable.h"

namespace CLD2 {

// Caller supplies the right tables in scoringcontext

// Runtime routines for hashing, looking up, and scoring
// unigrams (CJK), bigrams (CJK), quadgrams, and octagrams.
// Unigrams and bigrams are for CJK languages only, including simplified/
// traditional Chinese, Japanese, Korean, Vietnamese Han characters, and
// Zhuang Han characters. Surrounding spaces are not considered.
// Quadgrams and octagrams for for non-CJK and include two bits indicating
// preceding and trailing spaces (word boundaries).


static const int kMinCJKUTF8CharBytes = 3;

static const int kMinGramCount = 3;
static const int kMaxGramCount = 16;

static const int UTFmax = 4;        // Max number of bytes in a UTF-8 character

  // 1 to skip ASCII space, vowels AEIOU aeiou and UTF-8 continuation bytes 80-BF
  static const uint8 kSkipSpaceVowelContinue[256] = {
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,1,0,0,0,1,0,0, 0,1,0,0,0,0,0,1, 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0,
    0,1,0,0,0,1,0,0, 0,1,0,0,0,0,0,1, 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0,

    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  };

  // 1 to skip ASCII space, and UTF-8 continuation bytes 80-BF
  static const uint8 kSkipSpaceContinue[256] = {
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,

    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  };


  // Always advances one UTF-8 character
  static const uint8 kAdvanceOneChar[256] = {
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,

    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2, 2,2,2,2,2,2,2,2,
    3,3,3,3,3,3,3,3, 3,3,3,3,3,3,3,3, 4,4,4,4,4,4,4,4, 4,4,4,4,4,4,4,4,
  };

  // Advances *only* on space (or illegal byte)
  static const uint8 kAdvanceOneCharSpace[256] = {
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,

    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
  };


// Routines to access a hash table of <key:wordhash, value:probs> pairs
// Buckets have 4-byte wordhash for sizes < 32K buckets, but only
// 2-byte wordhash for sizes >= 32K buckets, with other wordhash bits used as
// bucket subscript.
// Probs is a packed: three languages plus a subscript for probability table
// Buckets have all the keys together, then all the values.Key array never
// crosses a cache-line boundary, so no-match case takes exactly one cache miss.
// Match case may sometimes take an additional cache miss on value access.
//
// Other possibilites include 5 or 10 6-byte entries plus pad to make 32 or 64
// byte buckets with single cache miss.
// Or 2-byte key and 6-byte value, allowing 5 languages instead  of three.
//------------------------------------------------------------------------------

//----------------------------------------------------------------------------//
// Hashing groups of 1/2/4/8 letters, perhaps with spaces or underscores      //
//----------------------------------------------------------------------------//

//----------------------------------------------------------------------------//
// Scoring single groups of letters                                           //
//----------------------------------------------------------------------------//

// BIGRAM, QUADGRAM, OCTAGRAM score one => tote
// Input: 4-byte entry of 3 language numbers and one probability subscript, plus
//  an accumulator tote. (language 0 means unused entry)
// Output: running sums in tote updated
void ProcessProbV2Tote(uint32 probs, Tote* tote) {
  uint8 prob123 = (probs >> 0) & 0xff;
  const uint8* prob123_entry = LgProb2TblEntry(prob123);

  uint8 top1 = (probs >> 8) & 0xff;
  if (top1 > 0) {tote->Add(top1, LgProb3(prob123_entry, 0));}
  uint8 top2 = (probs >> 16) & 0xff;
  if (top2 > 0) {tote->Add(top2, LgProb3(prob123_entry, 1));}
  uint8 top3 = (probs >> 24) & 0xff;
  if (top3 > 0) {tote->Add(top3, LgProb3(prob123_entry, 2));}
}

// Return score for a particular per-script language, or zero
int GetLangScore(uint32 probs, uint8 pslang) {
  uint8 prob123 = (probs >> 0) & 0xff;
  const uint8* prob123_entry = LgProb2TblEntry(prob123);
  int retval = 0;
  uint8 top1 = (probs >> 8) & 0xff;
  if (top1 == pslang) {retval += LgProb3(prob123_entry, 0);}
  uint8 top2 = (probs >> 16) & 0xff;
  if (top2 == pslang) {retval += LgProb3(prob123_entry, 1);}
  uint8 top3 = (probs >> 24) & 0xff;
  if (top3 == pslang) {retval += LgProb3(prob123_entry, 2);}
  return retval;
}

//----------------------------------------------------------------------------//
// Routines to accumulate probabilities                                       //
//----------------------------------------------------------------------------//


// BIGRAM, using hash table, always advancing by 1 char
// Caller supplies table, such as &kCjkBiTable_obj or &kGibberishTable_obj
// Score all bigrams in isrc, using languages that have bigrams (CJK)
// Return number of bigrams that hit in the hash table
int DoBigramScoreV3(const CLD2TableSummary* bigram_obj,
                         const char* isrc, int srclen, Tote* chunk_tote) {
  int hit_count = 0;
  const char* src = isrc;

  // Hashtable-based CJK bigram lookup
  const uint8* usrc = reinterpret_cast<const uint8*>(src);
  const uint8* usrclimit1 = usrc + srclen - UTFmax;

  while (usrc < usrclimit1) {
    int len = kAdvanceOneChar[usrc[0]];
    int len2 = kAdvanceOneChar[usrc[len]] + len;

    if ((kMinCJKUTF8CharBytes * 2) <= len2) {      // Two CJK chars possible
      // Lookup and score this bigram
      // Always ignore pre/post spaces
      uint32 bihash = BiHashV2(reinterpret_cast<const char*>(usrc), len2);
      uint32 probs = QuadHashV3Lookup4(bigram_obj, bihash);
      // Now go indirect on the subscript
      probs = bigram_obj->kCLDTableInd[probs &
        ~bigram_obj->kCLDTableKeyMask];

      // Process the bigram
      if (probs != 0) {
        ProcessProbV2Tote(probs, chunk_tote);
        ++hit_count;
      }
    }
    usrc += len;  // Advance by one char
  }

  return hit_count;
}


// Score up to 64KB of a single script span in one pass
// Make a dummy entry off the end to calc length of last span
// Return offset of first unused input byte
int GetUniHits(const char* text,
                     int letter_offset, int letter_limit,
                     ScoringContext* scoringcontext,
                     ScoringHitBuffer* hitbuffer) {
  const char* isrc = &text[letter_offset];
  const char* src = isrc;
  // Limit is end, which has extra 20 20 20 00 past len
  const char* srclimit = &text[letter_limit];

  // Local copies
  const UTF8PropObj* unigram_obj =
    scoringcontext->scoringtables->unigram_obj;
  int next_base = hitbuffer->next_base;
  int next_base_limit = hitbuffer->maxscoringhits;

  // Visit all unigrams
  if (src[0] == ' ') {++src;}   // skip any initial space
  while (src < srclimit) {
    const uint8* usrc = reinterpret_cast<const uint8*>(src);
    int len = kAdvanceOneChar[usrc[0]];
    src += len;
    // Look up property of one UTF-8 character and advance over it.
    // Updates usrc and len (bad interface design), hence increment above
    int propval = UTF8GenericPropertyBigOneByte(unigram_obj, &usrc, &len);
    if (propval > 0) {
      // Save indirect subscript for later scoring; 1 or 2 langprobs
      int indirect_subscr = propval;
      hitbuffer->base[next_base].offset = src - text;     // Offset in text
      hitbuffer->base[next_base].indirect = indirect_subscr;
      ++next_base;
    }

    if (next_base >= next_base_limit) {break;}
  }

  hitbuffer->next_base = next_base;

  // Make a dummy entry off the end to calc length of last span
  int dummy_offset = src - text;
  hitbuffer->base[hitbuffer->next_base].offset = dummy_offset;
  hitbuffer->base[hitbuffer->next_base].indirect = 0;

  return src - text;
}

// Score up to 64KB of a single script span, doing both delta-bi and
// distinct bis in one pass
void GetBiHits(const char* text,
                     int letter_offset, int letter_limit,
                     ScoringContext* scoringcontext,
                     ScoringHitBuffer* hitbuffer) {
  const char* isrc = &text[letter_offset];
  const char* src = isrc;
  // Limit is end
  const char* srclimit1 = &text[letter_limit];

  // Local copies
  const CLD2TableSummary* deltabi_obj =
    scoringcontext->scoringtables->deltabi_obj;
  const CLD2TableSummary* distinctbi_obj =
    scoringcontext->scoringtables->distinctbi_obj;
  int next_delta = hitbuffer->next_delta;
  int next_delta_limit = hitbuffer->maxscoringhits;
  int next_distinct = hitbuffer->next_distinct;
  // We can do 2 inserts per loop, so -1
  int next_distinct_limit = hitbuffer->maxscoringhits - 1;

  while (src < srclimit1) {
    const uint8* usrc = reinterpret_cast<const uint8*>(src);
    int len = kAdvanceOneChar[usrc[0]];
    int len2 = kAdvanceOneChar[usrc[len]] + len;

    if ((kMinCJKUTF8CharBytes * 2) <= len2) {      // Two CJK chars possible
      // Lookup and this bigram and save <offset, indirect>
      uint32 bihash = BiHashV2(src, len2);
      uint32 probs = QuadHashV3Lookup4(deltabi_obj, bihash);
      // Now go indirect on the subscript
      if (probs != 0) {
        // Save indirect subscript for later scoring; 1 langprob
        int indirect_subscr = probs & ~deltabi_obj->kCLDTableKeyMask;
        hitbuffer->delta[next_delta].offset = src - text;
        hitbuffer->delta[next_delta].indirect = indirect_subscr;
        ++next_delta;
      }
      // Lookup this distinct bigram and save <offset, indirect>
      probs = QuadHashV3Lookup4(distinctbi_obj, bihash);
      if (probs != 0) {
        int indirect_subscr = probs & ~distinctbi_obj->kCLDTableKeyMask;
        hitbuffer->distinct[next_distinct].offset = src - text;
        hitbuffer->distinct[next_distinct].indirect = indirect_subscr;
        ++next_distinct;
      }
    }
    src += len;  // Advance by one char (not two)

    // Almost always srclimit hit first
    if (next_delta >= next_delta_limit) {break;}
    if (next_distinct >= next_distinct_limit) {break;}
  }

  hitbuffer->next_delta = next_delta;
  hitbuffer->next_distinct = next_distinct;

  // Make a dummy entry off the end to calc length of last span
  int dummy_offset = src - text;
  hitbuffer->delta[hitbuffer->next_delta].offset = dummy_offset;
  hitbuffer->delta[hitbuffer->next_delta].indirect = 0;
  hitbuffer->distinct[hitbuffer->next_distinct].offset = dummy_offset;
  hitbuffer->distinct[hitbuffer->next_distinct].indirect = 0;
}

// Score up to 64KB of a single script span in one pass
// Make a dummy entry off the end to calc length of last span
// Return offset of first unused input byte
int GetQuadHits(const char* text,
                     int letter_offset, int letter_limit,
                     ScoringContext* scoringcontext,
                     ScoringHitBuffer* hitbuffer) {
  const char* isrc = &text[letter_offset];
  const char* src = isrc;
  // Limit is end, which has extra 20 20 20 00 past len
  const char* srclimit = &text[letter_limit];

  // Local copies
  const CLD2TableSummary* quadgram_obj =
    scoringcontext->scoringtables->quadgram_obj;
  const CLD2TableSummary* quadgram_obj2 =
    scoringcontext->scoringtables->quadgram_obj2;
  int next_base = hitbuffer->next_base;
  int next_base_limit = hitbuffer->maxscoringhits;

  // Run a little cache of last quad hits to catch overly-repetitive "text"
  // We don't care if we miss a couple repetitions at scriptspan boundaries
  int next_prior_quadhash = 0;
  uint32 prior_quadhash[2] = {0, 0};

  // Visit all quadgrams
  if (src[0] == ' ') {++src;}   // skip any initial space
  while (src < srclimit) {
    // Find one quadgram
    const char* src_end = src;
    src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
    src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
    const char* src_mid = src_end;
    src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
    src_end += kAdvanceOneCharButSpace[(uint8)src_end[0]];
    int len = src_end - src;
    // Hash the quadgram
    uint32 quadhash = QuadHashV2(src, len);

    // Filter out recent repeats
    if ((quadhash != prior_quadhash[0]) && (quadhash != prior_quadhash[1])) {
      // Look up this quadgram and save <offset, indirect>
      uint32 indirect_flag = 0;   // For dual tables
      const CLD2TableSummary* hit_obj = quadgram_obj;
      uint32 probs = QuadHashV3Lookup4(quadgram_obj, quadhash);
      if ((probs == 0) && (quadgram_obj2->kCLDTableSize != 0)) {
        // Try lookup in dual table if not found in first one
        // Note: we need to know later which of two indirect tables to use.
        indirect_flag = 0x80000000u;
        hit_obj = quadgram_obj2;
        probs = QuadHashV3Lookup4(quadgram_obj2, quadhash);
      }
      if (probs != 0) {
        // Round-robin two entries of actual hits
        prior_quadhash[next_prior_quadhash] = quadhash;
        next_prior_quadhash = (next_prior_quadhash + 1) & 1;

        // Save indirect subscript for later scoring; 1 or 2 langprobs
        int indirect_subscr = probs & ~hit_obj->kCLDTableKeyMask;
        hitbuffer->base[next_base].offset = src - text;     // Offset in text
        // Flip the high bit for table2
        hitbuffer->base[next_base].indirect = indirect_subscr | indirect_flag;
        ++next_base;
      }
    }

    // Advance: all the way past word if at end-of-word, else 2 chars
    if (src_end[0] == ' ') {
      src = src_end;
    } else {
      src = src_mid;
    }

    // Skip over space at end of word, or ASCII vowel in middle of word
    // Use kAdvanceOneCharSpace instead to get rid of vowel hack
    if (src < srclimit) {
      src += kAdvanceOneCharSpaceVowel[(uint8)src[0]];
    } else {
      // Advancing by 4/8/16 can overshoot, but we are about to exit anyway
      src = srclimit;
    }

    if (next_base >= next_base_limit) {break;}
  }

  hitbuffer->next_base = next_base;

  // Make a dummy entry off the end to calc length of last span
  int dummy_offset = src - text;
  hitbuffer->base[hitbuffer->next_base].offset = dummy_offset;
  hitbuffer->base[hitbuffer->next_base].indirect = 0;

  return src - text;
}

// inputs:
//  const tables
//  const char* isrc, int srclen (in sscriptbuffer)
// intermediates:
//  vector of octa <offset, probs>   (which need indirect table to decode)
//  vector of distinct <offset, probs>   (which need indirect table to decode)

// Score up to 64KB of a single script span, doing both delta-octa and
// distinct words in one pass
void GetOctaHits(const char* text,
                     int letter_offset, int letter_limit,
                     ScoringContext* scoringcontext,
                     ScoringHitBuffer* hitbuffer) {
  const char* isrc = &text[letter_offset];
  const char* src = isrc;
  // Limit is end+1, to include extra space char (0x20) off the end
  const char* srclimit = &text[letter_limit + 1];

  // Local copies
  const CLD2TableSummary* deltaocta_obj =
    scoringcontext->scoringtables->deltaocta_obj;
  int next_delta = hitbuffer->next_delta;
  int next_delta_limit = hitbuffer->maxscoringhits;

  const CLD2TableSummary* distinctocta_obj =
    scoringcontext->scoringtables->distinctocta_obj;
  int next_distinct = hitbuffer->next_distinct;
  // We can do 2 inserts per loop, so -1
  int next_distinct_limit = hitbuffer->maxscoringhits - 1;

  // Run a little cache of last octa hits to catch overly-repetitive "text"
  // We don't care if we miss a couple repetitions at scriptspan boundaries
  int next_prior_octahash = 0;
  uint64 prior_octahash[2] = {0, 0};

  // Score all words truncated to 8 characters
  int charcount = 0;
  // Skip any initial space
  if (src[0] == ' ') {++src;}

  // Begin the first word
  const char* prior_word_start = src;
  const char* word_start = src;
  const char* word_end = word_start;
  while (src < srclimit) {
    // Terminate previous word or continue current word
    if (src[0] == ' ') {
      int len = word_end - word_start;
      // Hash the word
      uint64 wordhash40 = OctaHash40(word_start, len);
      uint32 probs;

      // Filter out recent repeats. Unlike quads, we update even if no hit,
      // so we can get hits on same word if separated by non-hit words
      if ((wordhash40 != prior_octahash[0]) &&
          (wordhash40 != prior_octahash[1])) {
        // Round-robin two entries of words
        prior_octahash[next_prior_octahash] = wordhash40;
        next_prior_octahash = 1 - next_prior_octahash;    // Alternates 0,1,0,1

        // (1) Lookup distinct word PAIR. For a pair, we want an asymmetrical
        // function of the two word hashs. For words A B C, B-A and C-B are good
        // enough and fast. We use the same table as distinct single words
        // Do not look up a pair of identical words -- all pairs hash to zero
        // Both 1- and 2-word distinct lookups are in distinctocta_obj now
        // Do this first, because it has the lowest offset
        uint64 tmp_prior_hash = prior_octahash[next_prior_octahash];
        if ((tmp_prior_hash != 0) && (tmp_prior_hash != wordhash40)) {
          uint64 pair_hash = PairHash(tmp_prior_hash, wordhash40);
          probs = OctaHashV3Lookup4(distinctocta_obj, pair_hash);
          if (probs != 0) {
            int indirect_subscr = probs & ~distinctocta_obj->kCLDTableKeyMask;
            hitbuffer->distinct[next_distinct].offset = prior_word_start - text;
            hitbuffer->distinct[next_distinct].indirect = indirect_subscr;
            ++next_distinct;
          }
        }

        // (2) Lookup this distinct word and save <offset, indirect>
        probs = OctaHashV3Lookup4(distinctocta_obj, wordhash40);
        if (probs != 0) {
          int indirect_subscr = probs & ~distinctocta_obj->kCLDTableKeyMask;
          hitbuffer->distinct[next_distinct].offset = word_start - text;
          hitbuffer->distinct[next_distinct].indirect = indirect_subscr;
          ++next_distinct;
        }

        // (3) Lookup this word and save <offset, indirect>
        probs = OctaHashV3Lookup4(deltaocta_obj, wordhash40);
        if (probs != 0) {
          // Save indirect subscript for later scoring; 1 langprob
          int indirect_subscr = probs & ~deltaocta_obj->kCLDTableKeyMask;
          hitbuffer->delta[next_delta].offset = word_start - text;
          hitbuffer->delta[next_delta].indirect = indirect_subscr;
          ++next_delta;
        }
      }

      // Begin the next word
      charcount = 0;
      prior_word_start = word_start;
      word_start = src + 1;   // Over the space
      word_end = word_start;
    } else {
      ++charcount;
    }

    // Advance to next char
    src += UTF8OneCharLen(src);
    if (charcount <= 8) {
      word_end = src;
    }
    // Almost always srclimit hit first
    if (next_delta >= next_delta_limit) {break;}
    if (next_distinct >= next_distinct_limit) {break;}
  }

  hitbuffer->next_delta = next_delta;
  hitbuffer->next_distinct = next_distinct;

  // Make a dummy entry off the end to calc length of last span
  int dummy_offset = src - text;
  hitbuffer->delta[hitbuffer->next_delta].offset = dummy_offset;
  hitbuffer->delta[hitbuffer->next_delta].indirect = 0;
  hitbuffer->distinct[hitbuffer->next_distinct].offset = dummy_offset;
  hitbuffer->distinct[hitbuffer->next_distinct].indirect = 0;
}


//----------------------------------------------------------------------------//
// Reliability calculations, for single language and between languages        //
//----------------------------------------------------------------------------//

// Return reliablity of result 0..100 for top two scores
// delta==0 is 0% reliable, delta==fully_reliable_thresh is 100% reliable
// (on a scale where +1 is a factor of  2 ** 1.6 = 3.02)
// Threshold is uni/quadgram increment count, bounded above and below.
//
// Requiring a factor of 3 improvement (e.g. +1 log base 3)
// for each scored quadgram is too stringent, so I've backed this off to a
// factor of 2 (e.g. +5/8 log base 3).
//
// I also somewhat lowered the Min/MaxGramCount limits above
//
// Added: if fewer than 8 quads/unis, max reliability is 12*n percent
//
int ReliabilityDelta(int value1, int value2, int gramcount) {
  int max_reliability_percent = 100;
  if (gramcount < 8) {
    max_reliability_percent = 12 * gramcount;
  }
  int fully_reliable_thresh = (gramcount * 5) >> 3;     // see note above
  if (fully_reliable_thresh < kMinGramCount) {          // Fully = 3..16
    fully_reliable_thresh = kMinGramCount;
  } else if (fully_reliable_thresh > kMaxGramCount) {
    fully_reliable_thresh = kMaxGramCount;
  }

  int delta = value1 - value2;
  if (delta >= fully_reliable_thresh) {return max_reliability_percent;}
  if (delta <= 0) {return 0;}
  return minint(max_reliability_percent,
                     (100 * delta) / fully_reliable_thresh);
}

// Return reliablity of result 0..100 for top score vs. expected mainsteam score
// Values are score per 1024 bytes of input
// ratio = max(top/mainstream, mainstream/top)
// ratio > 4.0 is 0% reliable, <= 2.0 is 100% reliable
// Change: short-text word scoring can give unusually good results.
//  Let top exceed mainstream by 4x at 50% reliable
//
// dsites April 2010: These could be tightened up. It would be
// reasonable with newer data and round-robin table allocation to start ramping
// down at mean * 1.5 and mean/1.5, while letting mean*2 and mean/2 pass,
// but just barely.
//
// dsites March 2013: Tightened up a bit.
static const double kRatio100 = 1.5;
static const double kRatio0 = 4.0;
int ReliabilityExpected(int actual_score_1kb, int expected_score_1kb) {
  if (expected_score_1kb == 0) {return 100;}    // No reliability data available yet
  if (actual_score_1kb == 0) {return 0;}        // zero score = unreliable
  double ratio;
  if (expected_score_1kb > actual_score_1kb) {
    ratio = (1.0 * expected_score_1kb) / actual_score_1kb;
  } else {
    ratio = (1.0 * actual_score_1kb) / expected_score_1kb;
  }
  // Ratio 1.0 .. 1.5 scores 100%
  // Ratio 2.0 scores 80%
  // Linear decline, to ratio 4.0 scores 0%
  if (ratio <= kRatio100) {return 100;}
  if (ratio > kRatio0) {return 0;}

  int percent_good = 100.0 * (kRatio0 - ratio) / (kRatio0 - kRatio100);
  return percent_good;
}

// Create a langprob packed value from its parts.
// qprob is quantized [0..12]
// We use Latn script to represent any RTypeMany language
uint32 MakeLangProb(Language lang, int qprob) {
  uint32 pslang = PerScriptNumber(ULScript_Latin, lang);
  uint32 retval = (pslang << 8) | kLgProbV2TblBackmap[qprob];
  return retval;
}

}       // End namespace CLD2





