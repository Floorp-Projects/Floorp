/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This server simulates the behavior of /account/login on the Firefox Accounts
 * auth server in the case where the user is trying to sign in with an email
 * with the wrong capitalization.
 *
 * https://github.com/mozilla/fxa-auth-server/blob/master/docs/api.md#post-v1accountlogin
 *
 * The expected behavior is that on the first attempt, with the wrong email,
 * the server will respond with a 400 and the canonical email capitalization
 * that the client should use.  The client then has one chance to sign in with
 * this different capitalization.
 *
 * In this test, the user with the account id "Greta.Garbo@gmail.COM" initially
 * tries to sign in as "greta.garbo@gmail.com".
 *
 * On success, the client is responsible for updating its sign-in user state
 * and recording the proper email capitalization.
 */

const CC = Components.Constructor;
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream",
                             "setInputStream");

const goodEmail = "Greta.Garbo@gmail.COM";
const badEmail = "greta.garbo@gmail.com";

function handleRequest(request, response) {
  let body = new BinaryInputStream(request.bodyInputStream);
  let bytes = [];
  let available;
  while ((available = body.available()) > 0) {
    Array.prototype.push.apply(bytes, body.readByteArray(available));
  }

  let data = JSON.parse(String.fromCharCode.apply(null, bytes));
  let message;

  switch (data.email) {
    case badEmail:
      // Almost - try again with fixed email case
      message = {
        code: 400,
        errno: 120,
        error: "Incorrect email case",
        email: goodEmail,
      };
      response.setStatusLine(request.httpVersion, 400, "Almost");
      break;

    case goodEmail:
      // Successful login.
      message = {
        uid: "your-uid",
        sessionToken: "your-sessionToken",
        keyFetchToken: "your-keyFetchToken",
        verified: true,
        authAt: 1392144866,
      };
      response.setStatusLine(request.httpVersion, 200, "Yay");
      break;

    default:
      // Anything else happening in this test is a failure.
      message = {
        code: 400,
        errno: 999,
        error: "What happened!?",
      };
      response.setStatusLine(request.httpVersion, 400, "Ouch");
      break;
  }

  messageStr = JSON.stringify(message);
  response.bodyOutputStream.write(messageStr, messageStr.length);
}

