/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.util.Log;
import androidx.annotation.AnyThread;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

/**
 * The translations controller coordinates the session and runtime messaging between GeckoView and
 * the translations toolkit. Initial runtime component will be added in ToDo: bug 1852313.
 */
public class TranslationsController {
  private static final boolean DEBUG = false;
  private static final String LOGTAG = "TranslationsController";

  /**
   * Session translation coordinates session messaging between the translations toolkit actor and
   * GeckoView.
   *
   * <p>Performs translations actions that are dependent on the page.
   */
  public static class SessionTranslation {

    // Events Dispatched to Toolkit Translations
    private static final String TRANSLATE_EVENT = "GeckoView:Translations:Translate";
    private static final String RESTORE_PAGE_EVENT = "GeckoView:Translations:RestorePage";

    // Events Dispatched from Toolkit Translations
    private static final String ON_OFFER_EVENT = "GeckoView:Translations:Offer";
    private static final String ON_STATE_CHANGE_EVENT = "GeckoView:Translations:StateChange";

    private final GeckoSession mSession;
    private final SessionTranslation.Handler mHandler;

    /**
     * Construct a new translations session.
     *
     * @param session that will be dispatching and receiving events.
     */
    public SessionTranslation(final GeckoSession session) {
      mSession = session;
      mHandler = new SessionTranslation.Handler(mSession);
    }

    /**
     * Handler for receiving messages about translations.
     *
     * @return associated session handler
     */
    @AnyThread
    public @NonNull Handler getHandler() {
      return mHandler;
    }

    /**
     * Translates the session's current page based on criteria.
     *
     * <p>Currently when translating, the necessary language models will be automatically
     * downloaded.
     *
     * <p>ToDo: bug 1853055 will adjust this flow to add an option for automatic/non-automatic
     * downloads.
     *
     * @param fromLanguage BCP 47 language tag that the page should be translated from. Usually will
     *     be the suggested detected language or user specified.
     * @param toLanguage BCP 47 language tag that the page should be translated to. Usually will be
     *     the suggested preference language (will be added in ToDo: bug 1852313) or user specified.
     * @param options no-op, ToDo: bug 1853055 will add options
     * @return if translate process begins or exceptionally if an issue occurs.
     */
    @AnyThread
    public @NonNull GeckoResult<Void> translate(
        @NonNull final String fromLanguage,
        @NonNull final String toLanguage,
        @Nullable final TranslationOptions options) {
      if (DEBUG) {
        Log.d(
            LOGTAG,
            "Translate page requested - fromLanguage: "
                + fromLanguage
                + " toLanguage: "
                + toLanguage
                + " options: "
                + options);
      }
      final GeckoBundle bundle = new GeckoBundle(2);
      bundle.putString("fromLanguage", fromLanguage);
      bundle.putString("toLanguage", toLanguage);
      // ToDo: bug 1853055 - Translate options will be configured in a later iteration.
      return mSession.getEventDispatcher().queryVoid(TRANSLATE_EVENT, bundle);
    }

    /**
     * Convenience method for calling {@link #translate(String, String, TranslationOptions)} with a
     * translation pair.
     *
     * @param translationPair the object with a from and to language
     * @param options no-op, ToDo: bug 1853055 will add options
     * @return if translate process begins or exceptionally if an issue occurs.
     */
    @AnyThread
    public @NonNull GeckoResult<Void> translate(
        @NonNull final TranslationPair translationPair,
        @Nullable final TranslationOptions options) {
      return translate(translationPair.fromLanguage, translationPair.toLanguage, options);
    }

    /**
     * Restores a page to the original or pre-translated state.
     *
     * @return if page restoration process begins or exceptionally if an issue occurs.
     */
    @AnyThread
    public @NonNull GeckoResult<Void> restoreOriginalPage() {
      if (DEBUG) {
        Log.d(LOGTAG, "Restore translated page requested");
      }
      return mSession.getEventDispatcher().queryVoid(RESTORE_PAGE_EVENT);
    }

    /**
     * Options available for translating. The options available for translating. Will be developed
     * in ToDo: bug 1853055.
     *
     * <p>Options (default):
     *
     * <p>downloadModel (true) - Downloads any models automatically that are needed for translation.
     */
    @AnyThread
    public static class TranslationOptions {
      /** If the model should be automatically downloaded or stopped. */
      public final @NonNull boolean downloadModel;

      /**
       * Options for translation.
       *
       * @param builder that populated the translation options
       */
      protected TranslationOptions(final @NonNull Builder builder) {
        this.downloadModel = builder.mDownloadModel;
      }

      /** Builder for making translation options. */
      @AnyThread
      public static class Builder {
        /* package */ boolean mDownloadModel = true;

        /**
         * Build setter for the option for downloading a model.
         *
         * @param downloadModel should the model be automatically download or not
         * @return the model to download for the translation options
         */
        public @NonNull Builder downloadModel(final @NonNull boolean downloadModel) {
          mDownloadModel = downloadModel;
          return this;
        }

        /**
         * Final call to build the specified options.
         *
         * @return a constructed translation options
         */
        @AnyThread
        public @NonNull TranslationOptions build() {
          return new TranslationOptions(this);
        }
      }
    }

    /**
     * The translations session delegate is used for receiving translation events and information.
     */
    @AnyThread
    public interface Delegate {
      /**
       * onOfferTranslate occurs when a page should be offered for translation.
       *
       * <p>An offer should occur when all conditions are met:
       *
       * <p>* The page is not in the user's preferred language
       *
       * <p>* The page language is eligible for translation
       *
       * <p>* The host hasn't been offered for translation in this session
       *
       * <p>* No user preferences indicate that translation shouldn't be offered
       *
       * <p>* It is possible to translate
       *
       * <p>Usual use-case is to show a pop-up recommending a translation.
       *
       * @param session The associated GeckoSession.
       */
      default void onOfferTranslate(@NonNull final GeckoSession session) {}

      /**
       * onExpectedTranslate occurs when it is likely the user will want to translate and it is
       * feasible. For example, if the page is in a different language than the user preferred
       * language or languages.
       *
       * <p>Usual use-case is to add a toolbar option for translate.
       *
       * @param session The associated GeckoSession.
       */
      default void onExpectedTranslate(@NonNull final GeckoSession session) {}

      /**
       * onTranslationStateChange occurs when new information about the translation state is
       * available. This includes information when first visiting the page and after calls to
       * translate.
       *
       * @param session The associated GeckoSession.
       * @param translationState The state of the translation as reported by the translation engine.
       */
      default void onTranslationStateChange(
          @NonNull final GeckoSession session, @Nullable TranslationState translationState) {}
    }

    /** Translation pair is the from language and to language set on the translation state. */
    public static class TranslationPair {
      /** Language the page is translated from originally. */
      public final @Nullable String fromLanguage;

      /** Language the page is translated to. */
      public final @Nullable String toLanguage;

      /**
       * Requested translation pair constructor.
       *
       * @param fromLanguage original language of page (detected or specified)
       * @param toLanguage translated to language of page (detected or specified)
       */
      public TranslationPair(
          @Nullable final String fromLanguage, @Nullable final String toLanguage) {
        this.fromLanguage = fromLanguage;
        this.toLanguage = toLanguage;
      }

      @Override
      public String toString() {
        return "TranslationPair {"
            + "fromLanguage='"
            + fromLanguage
            + '\''
            + ", toLanguage='"
            + toLanguage
            + '\''
            + '}';
      }

      /**
       * Convenience method for deserializing translation state information.
       *
       * @param bundle contains translation pair information.
       * @return translation pair
       */
      /* package */
      static @Nullable TranslationPair fromBundle(final GeckoBundle bundle) {
        if (bundle == null) {
          return null;
        }
        return new TranslationPair(
            bundle.getString("fromLanguage"), bundle.getString("toLanguage"));
      }
    }

    /** DetectedLanguages is information that was detected about the page or user preferences. */
    public static class DetectedLanguages {

      /** The user's preferred language tag */
      public final @Nullable String userLangTag;

      /** If the engine supports the document language. */
      public final @NonNull Boolean isDocLangTagSupported;

      /** Detected language tag of page. */
      public final @Nullable String docLangTag;

      /**
       * DetectedLanguages constructor.
       *
       * @param userLangTag - the user's preferred language tag
       * @param isDocLangTagSupported - if the engine supports the document language for translation
       * @param docLangTag - the document's detected language tag
       */
      public DetectedLanguages(
          @Nullable final String userLangTag,
          @NonNull final Boolean isDocLangTagSupported,
          @Nullable final String docLangTag) {
        this.userLangTag = userLangTag;
        this.isDocLangTagSupported = isDocLangTagSupported;
        this.docLangTag = docLangTag;
      }

      @Override
      public String toString() {
        return "DetectedLanguages {"
            + "userLangTag='"
            + userLangTag
            + '\''
            + ", isDocLangTagSupported="
            + isDocLangTagSupported
            + ", docLangTag='"
            + docLangTag
            + '\''
            + '}';
      }

      /**
       * Convenience method for deserializing detected language state information.
       *
       * @param bundle contains detected language information.
       * @return detected language information
       */
      /* package */
      static @Nullable DetectedLanguages fromBundle(final GeckoBundle bundle) {
        if (bundle == null) {
          return null;
        }
        return new DetectedLanguages(
            bundle.getString("userLangTag"),
            bundle.getBoolean("isDocLangTagSupported", false),
            bundle.getString("docLangTag"));
      }
    }

    /** The representation of the translation state. */
    public static class TranslationState {
      /** The language pair to translate. */
      public final @Nullable TranslationPair requestedTranslationPair;

      /** If an error state occurred. */
      public final @Nullable String error;

      /** Detected information about preferences and page information. */
      public final @Nullable DetectedLanguages detectedLanguages;

      /** If the translation engine is ready for use or will need to be loaded. */
      public final @NonNull Boolean isEngineReady;

      /**
       * Translation State constructor.
       *
       * @param requestedTranslationPair the language pair to translate
       * @param error if an error occurred
       * @param detectedLanguages detected language
       * @param isEngineReady if the engine is ready for translations
       */
      public TranslationState(
          final @Nullable TranslationPair requestedTranslationPair,
          final @Nullable String error,
          final @Nullable DetectedLanguages detectedLanguages,
          final @NonNull Boolean isEngineReady) {
        this.requestedTranslationPair = requestedTranslationPair;
        this.error = error;
        this.detectedLanguages = detectedLanguages;
        this.isEngineReady = isEngineReady;
      }

      @Override
      public String toString() {
        return "TranslationState {"
            + "requestedTranslationPair="
            + requestedTranslationPair
            + ", error='"
            + error
            + '\''
            + ", detectedLanguages="
            + detectedLanguages
            + ", isEngineReady="
            + isEngineReady
            + '}';
      }

      /**
       * Convenience method for deserializing translation state information.
       *
       * @param bundle contains information about translation state.
       * @return translation state
       */
      /* package */
      static @Nullable TranslationState fromBundle(final GeckoBundle bundle) {
        if (bundle == null) {
          return null;
        }
        return new TranslationState(
            TranslationPair.fromBundle(bundle.getBundle("requestedTranslationPair")),
            bundle.getString("error"),
            DetectedLanguages.fromBundle(bundle.getBundle("detectedLanguages")),
            bundle.getBoolean("isEngineReady", false));
      }
    }

    /* package */ static class Handler extends GeckoSessionHandler<SessionTranslation.Delegate> {

      private final GeckoSession mSession;

      private Handler(final GeckoSession session) {
        super(
            "GeckoViewTranslations",
            session,
            new String[] {
              ON_OFFER_EVENT, ON_STATE_CHANGE_EVENT,
            });
        mSession = session;
      }

      @Override
      public void handleMessage(
          final Delegate delegate,
          final String event,
          final GeckoBundle message,
          final EventCallback callback) {
        if (DEBUG) {
          Log.d(LOGTAG, "handleMessage " + event);
        }
        if (delegate == null) {
          Log.w(LOGTAG, "The translations session delegate is not set.");
          return;
        }
        if (ON_OFFER_EVENT.equals(event)) {
          delegate.onOfferTranslate(mSession);
          return;
        } else if (ON_STATE_CHANGE_EVENT.equals(event)) {
          final GeckoBundle data = message.getBundle("data");
          final TranslationState translationState = TranslationState.fromBundle(data);
          delegate.onTranslationStateChange(mSession, translationState);
          if (translationState != null
              && translationState.detectedLanguages != null
              && translationState.detectedLanguages.docLangTag != null
              && translationState.detectedLanguages.userLangTag != null
              && translationState.detectedLanguages.isDocLangTagSupported)
          // Also check if engine is supported when runtime functions are added in ToDo: bug 1852313
          {
            delegate.onExpectedTranslate(mSession);
          }
          return;
        }
      }
    }
  }
}
