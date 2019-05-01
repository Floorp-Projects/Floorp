/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsSecurityHeaderParser_h
#define nsSecurityHeaderParser_h

#include "mozilla/LinkedList.h"
#include "nsCOMPtr.h"
#include "nsString.h"

// Utility class for handing back parsed directives and (optional) values
class nsSecurityHeaderDirective
    : public mozilla::LinkedListElement<nsSecurityHeaderDirective> {
 public:
  nsCString mName;
  nsCString mValue;
};

// This class parses security-related HTTP headers like
// Strict-Transport-Security. The Augmented Backus-Naur Form syntax for this
// header is reproduced below, for reference:
//
//   Strict-Transport-Security = "Strict-Transport-Security" ":"
//                               [ directive ]  *( ";" [ directive ] )
//
//   directive                 = directive-name [ "=" directive-value ]
//   directive-name            = token
//   directive-value           = token | quoted-string
//
//   where:
//
//   token          = <token, defined in [RFC2616], Section 2.2>
//   quoted-string  = <quoted-string, defined in [RFC2616], Section 2.2>/
//
// For further reference, see [RFC6797], Section 6.1

class nsSecurityHeaderParser {
 public:
  // The input to this class must be null-terminated, and must have a lifetime
  // greater than or equal to the lifetime of the created
  // nsSecurityHeaderParser.
  explicit nsSecurityHeaderParser(const nsCString& aHeader);
  ~nsSecurityHeaderParser();

  // Only call Parse once.
  nsresult Parse();
  // The caller does not take ownership of the memory returned here.
  mozilla::LinkedList<nsSecurityHeaderDirective>* GetDirectives();

 private:
  bool Accept(char aChr);
  bool Accept(bool (*aClassifier)(signed char));
  void Expect(char aChr);
  void Advance();
  void Header();          // header = [ directive ] *( ";" [ directive ] )
  void Directive();       // directive = directive-name [ "=" directive-value ]
  void DirectiveName();   // directive-name = token
  void DirectiveValue();  // directive-value = token | quoted-string
  void Token();           // token = 1*<any CHAR except CTLs or separators>
  void QuotedString();    // quoted-string = (<"> *( qdtext | quoted-pair ) <">)
  void QuotedText();      // qdtext = <any TEXT except <"> and "\">
  void QuotedPair();      // quoted-pair = "\" CHAR

  // LWS = [CRLF] 1*( SP | HT )
  void LWSMultiple();  // Handles *( LWS )
  void LWSCRLF();      // Handles the [CRLF] part of LWS
  void LWS();          // Handles the 1*( SP | HT ) part of LWS

  mozilla::LinkedList<nsSecurityHeaderDirective> mDirectives;
  const char* mCursor;
  nsSecurityHeaderDirective* mDirective;

  nsCString mOutput;
  bool mError;
};

#endif  // nsSecurityHeaderParser_h
