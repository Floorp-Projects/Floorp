GeckoView Login Storage API
===========================

Eugen Sawin <esawin@mozilla.com>

December 20th, 2019

Motivation
----------

The current GV Autofill API provides all the essential callbacks and meta
information for the implementation of autofill/login app support. It also
manages the fallback to the Android ``AutofillManager``, which delegates
requests to the system-wide autofill service set by the user.

However, the current GV Autofill API does not leverage the complete range of
Gecko heuristics that handle many autofill/login scenarios.

The GV Login Storage API is meant to bridge that gap and provide an
intermediate solution for Fenix to enable feature-rich autofill/login support
without duplicating Gecko mechanics. As a storage-level API, it would also
enable easy integration with the existing Firefox Sync AC.

API Proposal A (deprecated)
---------------------------

Unified Login Storage API: session delegate

.. code:: java

  class LoginStorage {
      class Login {
          String guid;
          // @Fenix: currently called `hostname` in AsyncLoginsStorage.
          String origin;
          // @Fenix: currently called `formSubmitURL` in AsyncLoginsStorage
          String formActionOrigin;
          String httpRealm;
          String username;
          String password;
      }

      class Hint {
          // @Fenix: Automatically save the login and indicate this to the
          // user.
          int GENERATED;
          // @Fenix: Don’t prompt to save but allow the user to open UI to
          // save if they really want.
          int PRIVATE_MODE;
          // The data looks like it may be some other data (e.g. CC) entered
          // in a password field.
          // @Fenix: Don’t prompt to save but allow the user to open UI to
          // save if they want (e.g. in case the CC number is actually the
          // username for a credit card account)
          int LOW_CONFIDENCE;
          // TBD
      }

      interface Delegate {
          // Notify that the given login has been used for login.
          // @Fenix: call AsyncLoginsStorage.touch(login.guid).
          void onLoginUsed(Login login);

          // Request logins for the given domain.
          // @Fenix: return AsyncLoginsStorage.getByHostname(domain).
          GeckoResult<Login[]> onLoginRequest(String domain);

          // Request to save or update the given login.
          // The hint should help determining the appropriate user prompting
          // behavior.
          // @Fenix: Use the API from application-services/issues/1983 to
          // determine whether to show a Save or Update button on the
          // doorhanger, taking into account un/pw edits in the doorhanger.
          // When the user confirms the save/update,
          void onLoginSave(Login login, int hint);

          // TBD (next API iteration): handle autocomplete selection.
          // GeckoResult<Login> onLoginSelect(Login[] logins);
      }
  }

Extension of ``GeckoSession``

.. code:: java

  // Extending existing session class.
  class GeckoSession {
      // Set the login storage delegate for this session.
      void setLoginStorageDelegate(LoginStorage.Delegate delegate);

      LoginStorage.Delegate getLoginStorageDelegate();
  }

API Proposal B
--------------

Split Login Storage API: runtime storage delegate / session prompts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Split storing and prompting. Fetching and saving of logins is handled by the
runtime delegate, prompting for saving and (in future) autocompletion is
handled by the prompt delegate.

.. code:: java

  class LoginStorage {
      class Login {
          String guid;
          // @Fenix: currently called `hostname` in AsyncLoginsStorage.
          String origin;
          // @Fenix: currently called `formSubmitURL` in AsyncLoginsStorage
          String formActionOrigin;
          String httpRealm;
          String username;
          String password;
      }

      interface Delegate {
          // v2
          // Notify that the given login has been used for login.
          // @Fenix: call AsyncLoginsStorage.touch(login.guid).
          void onLoginUsed(Login login);

          // Request logins for the given domain.
          // @Fenix: return AsyncLoginsStorage.getByHostname(domain).
          GeckoResult<Login[]> onLoginFetch(String domain);

          // Request to save or update the given login.
          void onLoginSave(Login login);
      }
  }

Extension of ``GeckoRuntime``

.. code:: java

  // Extending existing runtime class.
  class GeckoRuntime {
      // Set the login storage delegate for this runtime.
      void setLoginStorageDelegate(LoginStorage.Delegate delegate);
  }

Extension of ``GeckoSession.PromptDelegate``

.. code:: java

  // Extending existing prompt delegate.
  class GeckoSession {
      interface PromptDelegate {
          class LoginStoragePrompt extends BasePrompt {
              class Type {
                  int SAVE;
                  // TBD: autocomplete selection.
                  // int SELECT;
              }

              class Hint {
                  // v2
                  // @Fenix: Automatically save the login and indicate this
                  // to the user.
                  int GENERATED;
                  // @Fenix: Don’t prompt to save but allow the user to open
                  // UI to save if they really want.
                  int PRIVATE_MODE;
                  // The data looks like it may be some other data (e.g. CC)
                  // entered in a password field
                  // @Fenix: Don’t prompt to save but allow the user to open
                  // UI to save if they want (e.g. in case the CC number is
                  // actually the username for a credit card account)
                  int LOW_CONFIDENCE;
                  // TBD
              }

              // Type
              int type;

              // Hint
              // The hint should help determining the appropriate user
              // prompting behavior.
              // @Fenix: Use the API from application-services/issues/1983 to
              // determine whether to show a Save or Update button on the
              // doorhanger, taking into account un/pw edits in the
              // doorhanger. When the user confirms the save/update.
              int hint;

              // For SAVE, it will hold the login to be stored or updated.
              // For SELECT, it will hold the logins for the autocomplete
              // selection.
              Login[] logins;

              // Confirm SAVE prompt: the login would include a user’s edits
              // to what will be saved.
              // v2
              // Confirm SELECT (autocomplete) prompt by providing the
              // selected login.
              PromptResponse confirm(Login login);

              // Dismiss request.
              PromptResponse dismiss();
          }

          GeckoResult<PromptResponse> onLoginStoragePrompt(
              GeckoSession session,
              LoginStoragePrompt prompt
          );
      }
  }
