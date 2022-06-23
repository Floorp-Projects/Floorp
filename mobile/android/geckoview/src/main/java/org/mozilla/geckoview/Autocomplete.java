/* -*- Mode: Java; c-basic-offset: 4; tab-width: 20; indent-tabs-mode: nil; -*-
 * vim: ts=4 sw=4 expandtab:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

package org.mozilla.geckoview;

import android.util.Log;
import androidx.annotation.AnyThread;
import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.UiThread;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import org.mozilla.gecko.EventDispatcher;
import org.mozilla.gecko.util.BundleEventListener;
import org.mozilla.gecko.util.EventCallback;
import org.mozilla.gecko.util.GeckoBundle;

/**
 * The Autocomplete API provides a way to leverage Gecko's input form handling for autocompletion.
 *
 * <p>The API is split into two parts: 1. Storage-level delegates. 2. User-prompt delegates.
 *
 * <p>The storage-level delegates connect Gecko mechanics to the app's storage, e.g., retrieving and
 * storing of login entries.
 *
 * <p>The user-prompt delegates propagate decisions to the app that could require user choice, e.g.,
 * saving or updating of login entries or the selection of a login entry out of multiple options.
 *
 * <p>Throughout the documentation, we will refer to the filling out of input forms using two terms:
 * 1. Autofill: automatic filling without user interaction. 2. Autocomplete: semi-automatic filling
 * that requires user prompting for the selection.
 *
 * <h2>Examples</h2>
 *
 * <h3>Autocomplete/Fetch API</h3>
 *
 * <p>GeckoView loads <code>https://example.com</code> which contains (for the purpose of this
 * example) elements resembling a login form, e.g.,
 *
 * <pre><code>
 *   &lt;form&gt;
 *     &lt;input type=&quot;text&quot; placeholder=&quot;username&quot;&gt;
 *     &lt;input type=&quot;password&quot; placeholder=&quot;password&quot;&gt;
 *     &lt;input type=&quot;submit&quot; value=&quot;submit&quot;&gt;
 *   &lt;/form&gt;
 * </code></pre>
 *
 * <p>With the document parsed and the login input fields identified, GeckoView dispatches a <code>
 * StorageDelegate.onLoginFetch(&quot;example.com&quot;)</code> request to fetch logins for the
 * given domain.
 *
 * <p>Based on the provided login entries, GeckoView will attempt to autofill the login input
 * fields, if there is only one suitable login entry option.
 *
 * <p>In the case of multiple valid login entry options, GeckoView dispatches a <code>
 * GeckoSession.PromptDelegate.onLoginSelect</code> request, which allows for user-choice
 * delegation.
 *
 * <p>Based on the returned login entries, GeckoView will attempt to autofill/autocomplete the login
 * input fields.
 *
 * <h3>Update API</h3>
 *
 * <p>When the user submits some login input fields, GeckoView dispatches another <code>
 * StorageDelegate.onLoginFetch(&quot;example.com&quot;)</code> request to check whether the
 * submitted login exists or whether it's a new or updated login entry.
 *
 * <p>If the submitted login is already contained as-is in the collection returned by <code>
 * onLoginFetch</code>, then GeckoView dispatches <code>StorageDelegate.onLoginUsed</code> with the
 * submitted login entry.
 *
 * <p>If the submitted login is a new or updated entry, GeckoView dispatches a sequence of requests
 * to save/update the login entry, see the Save API example.
 *
 * <h3>Save API</h3>
 *
 * <p>The user enters new or updated (password) login credentials in some login input fields and
 * submits explicitely (submit action) or by navigation. GeckoView identifies the entered
 * credentials and dispatches a <code>GeckoSession.PromptDelegate.onLoginSave(session, request)
 * </code> with the provided credentials.
 *
 * <p>The app may dismiss the prompt request via <code>
 * return GeckoResult.fromValue(prompt.dismiss())</code> which terminates this saving request, or
 * confirm it via <code>return GeckoResult.fromValue(prompt.confirm(login))</code> where <code>login
 * </code> either holds the credentials originally provided by the prompt request (<code>
 * prompt.logins[0]</code>) or a new or modified login entry.
 *
 * <p>The login entry returned in a confirmed save prompt is used to request for saving in the
 * runtime delegate via <code>StorageDelegate.onLoginSave(login)</code>. If the app has already
 * stored the entry during the prompt request handling, it may ignore this storage saving request.
 * <br>
 *
 * @see GeckoRuntime#setAutocompleteStorageDelegate <br>
 * @see GeckoSession#setPromptDelegate <br>
 * @see GeckoSession.PromptDelegate#onLoginSave <br>
 * @see GeckoSession.PromptDelegate#onLoginSelect
 */
public class Autocomplete {
  private static final String LOGTAG = "Autocomplete";
  private static final boolean DEBUG = false;

  protected Autocomplete() {}

  /** Holds credit card information for a specific entry. */
  public static class CreditCard {
    private static final String GUID_KEY = "guid";
    private static final String NAME_KEY = "name";
    private static final String NUMBER_KEY = "number";
    private static final String EXP_MONTH_KEY = "expMonth";
    private static final String EXP_YEAR_KEY = "expYear";

    /** The unique identifier for this login entry. */
    public final @Nullable String guid;

    /** The full name as it appears on the credit card. */
    public final @NonNull String name;

    /** The credit card number. */
    public final @NonNull String number;

    /** The expiration month. */
    public final @NonNull String expirationMonth;

    /** The expiration year. */
    public final @NonNull String expirationYear;

    // For tests only.
    @AnyThread
    protected CreditCard() {
      guid = null;
      name = "";
      number = "";
      expirationMonth = "";
      expirationYear = "";
    }

    @AnyThread
    /* package */ CreditCard(final @NonNull GeckoBundle bundle) {
      guid = bundle.getString(GUID_KEY);
      name = bundle.getString(NAME_KEY, "");
      number = bundle.getString(NUMBER_KEY, "");
      expirationMonth = bundle.getString(EXP_MONTH_KEY, "");
      expirationYear = bundle.getString(EXP_YEAR_KEY, "");
    }

    @Override
    @AnyThread
    public String toString() {
      final StringBuilder builder = new StringBuilder("CreditCard {");
      builder
          .append("guid=")
          .append(guid)
          .append(", name=")
          .append(name)
          .append(", number=")
          .append(number)
          .append(", expirationMonth=")
          .append(expirationMonth)
          .append(", expirationYear=")
          .append(expirationYear)
          .append("}");
      return builder.toString();
    }

    @AnyThread
    /* package */ @NonNull
    GeckoBundle toBundle() {
      final GeckoBundle bundle = new GeckoBundle(7);
      bundle.putString(GUID_KEY, guid);
      bundle.putString(NAME_KEY, name);
      bundle.putString(NUMBER_KEY, number);
      if (expirationMonth != null) {
        bundle.putString(EXP_MONTH_KEY, expirationMonth);
      }
      if (expirationYear != null) {
        bundle.putString(EXP_YEAR_KEY, expirationYear);
      }

      return bundle;
    }

    public static class Builder {
      private final GeckoBundle mBundle;

      @AnyThread
      /* package */ Builder(final @NonNull GeckoBundle bundle) {
        mBundle = new GeckoBundle(bundle);
      }

      @AnyThread
      @SuppressWarnings("checkstyle:javadocmethod")
      public Builder() {
        mBundle = new GeckoBundle(7);
      }

      /**
       * Finalize the {@link CreditCard} instance.
       *
       * @return The {@link CreditCard} instance.
       */
      @AnyThread
      public @NonNull CreditCard build() {
        return new CreditCard(mBundle);
      }

      /**
       * Set the unique identifier for this credit card entry.
       *
       * @param guid The unique identifier string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder guid(final @Nullable String guid) {
        mBundle.putString(GUID_KEY, guid);
        return this;
      }

      /**
       * Set the name for this credit card entry.
       *
       * @param name The full name as it appears on the credit card.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder name(final @Nullable String name) {
        mBundle.putString(NAME_KEY, name);
        return this;
      }

      /**
       * Set the number for this credit card entry.
       *
       * @param number The credit card number string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder number(final @Nullable String number) {
        mBundle.putString(NUMBER_KEY, number);
        return this;
      }

      /**
       * Set the expiration month for this credit card entry.
       *
       * @param expMonth The expiration month string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder expirationMonth(final @Nullable String expMonth) {
        mBundle.putString(EXP_MONTH_KEY, expMonth);
        return this;
      }

      /**
       * Set the expiration year for this credit card entry.
       *
       * @param expYear The expiration year string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder expirationYear(final @Nullable String expYear) {
        mBundle.putString(EXP_YEAR_KEY, expYear);
        return this;
      }
    }
  }

  /** Holds address information for a specific entry. */
  public static class Address {
    private static final String GUID_KEY = "guid";
    private static final String NAME_KEY = "name";
    private static final String GIVEN_NAME_KEY = "givenName";
    private static final String ADDITIONAL_NAME_KEY = "additionalName";
    private static final String FAMILY_NAME_KEY = "familyName";
    private static final String ORGANIZATION_KEY = "organization";
    private static final String STREET_ADDRESS_KEY = "streetAddress";
    private static final String ADDRESS_LEVEL1_KEY = "addressLevel1";
    private static final String ADDRESS_LEVEL2_KEY = "addressLevel2";
    private static final String ADDRESS_LEVEL3_KEY = "addressLevel3";
    private static final String POSTAL_CODE_KEY = "postalCode";
    private static final String COUNTRY_KEY = "country";
    private static final String TEL_KEY = "tel";
    private static final String EMAIL_KEY = "email";
    private static final byte bundleCapacity = 14;

    /** The unique identifier for this address entry. */
    public final @Nullable String guid;

    /** The full name. */
    public final @NonNull String name;

    /** The given (first) name. */
    public final @NonNull String givenName;

    /** An additional name, if available. */
    public final @NonNull String additionalName;

    /** The family name. */
    public final @NonNull String familyName;

    /** The name of the company, if applicable. */
    public final @NonNull String organization;

    /** The (multiline) street address. */
    public final @NonNull String streetAddress;

    /** The level 1 (province) address. Note: Only use if streetAddress is not provided. */
    public final @NonNull String addressLevel1;

    /** The level 2 (city/town) address. Note: Only use if streetAddress is not provided. */
    public final @NonNull String addressLevel2;

    /**
     * The level 3 (suburb/sublocality) address. Note: Only use if streetAddress is not provided.
     */
    public final @NonNull String addressLevel3;

    /** The postal code. */
    public final @NonNull String postalCode;

    /** The country string in ISO 3166. */
    public final @NonNull String country;

    /** The telephone number string. */
    public final @NonNull String tel;

    /** The email address. */
    public final @NonNull String email;

    // For tests only.
    @AnyThread
    protected Address() {
      guid = null;
      name = "";
      givenName = "";
      additionalName = "";
      familyName = "";
      organization = "";
      streetAddress = "";
      addressLevel1 = "";
      addressLevel2 = "";
      addressLevel3 = "";
      postalCode = "";
      country = "";
      tel = "";
      email = "";
    }

    @AnyThread
    /* package */ Address(final @NonNull GeckoBundle bundle) {
      guid = bundle.getString(GUID_KEY);
      name = bundle.getString(NAME_KEY, "");
      givenName = bundle.getString(GIVEN_NAME_KEY, "");
      additionalName = bundle.getString(ADDITIONAL_NAME_KEY, "");
      familyName = bundle.getString(FAMILY_NAME_KEY, "");
      organization = bundle.getString(ORGANIZATION_KEY, "");
      streetAddress = bundle.getString(STREET_ADDRESS_KEY, "");
      addressLevel1 = bundle.getString(ADDRESS_LEVEL1_KEY, "");
      addressLevel2 = bundle.getString(ADDRESS_LEVEL2_KEY, "");
      addressLevel3 = bundle.getString(ADDRESS_LEVEL3_KEY, "");
      postalCode = bundle.getString(POSTAL_CODE_KEY, "");
      country = bundle.getString(COUNTRY_KEY, "");
      tel = bundle.getString(TEL_KEY, "");
      email = bundle.getString(EMAIL_KEY, "");
    }

    @Override
    @AnyThread
    public String toString() {
      final StringBuilder builder = new StringBuilder("Address {");
      builder
          .append("guid=")
          .append(guid)
          .append(", givenName=")
          .append(givenName)
          .append(", additionalName=")
          .append(additionalName)
          .append(", familyName=")
          .append(familyName)
          .append(", organization=")
          .append(organization)
          .append(", streetAddress=")
          .append(streetAddress)
          .append(", addressLevel1=")
          .append(addressLevel1)
          .append(", addressLevel2=")
          .append(addressLevel2)
          .append(", addressLevel3=")
          .append(addressLevel3)
          .append(", postalCode=")
          .append(postalCode)
          .append(", country=")
          .append(country)
          .append(", tel=")
          .append(tel)
          .append(", email=")
          .append(email)
          .append("}");
      return builder.toString();
    }

    @AnyThread
    /* package */ @NonNull
    GeckoBundle toBundle() {
      final GeckoBundle bundle = new GeckoBundle(bundleCapacity);
      bundle.putString(GUID_KEY, guid);
      bundle.putString(NAME_KEY, name);
      bundle.putString(GIVEN_NAME_KEY, givenName);
      bundle.putString(ADDITIONAL_NAME_KEY, additionalName);
      bundle.putString(FAMILY_NAME_KEY, familyName);
      bundle.putString(ORGANIZATION_KEY, organization);
      bundle.putString(STREET_ADDRESS_KEY, streetAddress);
      bundle.putString(ADDRESS_LEVEL1_KEY, addressLevel1);
      bundle.putString(ADDRESS_LEVEL2_KEY, addressLevel2);
      bundle.putString(ADDRESS_LEVEL3_KEY, addressLevel3);
      bundle.putString(POSTAL_CODE_KEY, postalCode);
      bundle.putString(COUNTRY_KEY, country);
      bundle.putString(TEL_KEY, tel);
      bundle.putString(EMAIL_KEY, email);

      return bundle;
    }

    public static class Builder {
      private final GeckoBundle mBundle;

      @AnyThread
      /* package */ Builder(final @NonNull GeckoBundle bundle) {
        mBundle = new GeckoBundle(bundle);
      }

      @AnyThread
      @SuppressWarnings("checkstyle:javadocmethod")
      public Builder() {
        mBundle = new GeckoBundle(bundleCapacity);
      }

      /**
       * Finalize the {@link Address} instance.
       *
       * @return The {@link Address} instance.
       */
      @AnyThread
      public @NonNull Address build() {
        return new Address(mBundle);
      }

      /**
       * Set the unique identifier for this address entry.
       *
       * @param guid The unique identifier string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder guid(final @Nullable String guid) {
        mBundle.putString(GUID_KEY, guid);
        return this;
      }

      /**
       * Set the full name for this address entry.
       *
       * @param name The full name string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder name(final @Nullable String name) {
        mBundle.putString(NAME_KEY, name);
        return this;
      }

      /**
       * Set the given name for this address entry.
       *
       * @param givenName The given name string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder givenName(final @Nullable String givenName) {
        mBundle.putString(GIVEN_NAME_KEY, givenName);
        return this;
      }

      /**
       * Set the additional name for this address entry.
       *
       * @param additionalName The additional name string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder additionalName(final @Nullable String additionalName) {
        mBundle.putString(ADDITIONAL_NAME_KEY, additionalName);
        return this;
      }

      /**
       * Set the family name for this address entry.
       *
       * @param familyName The family name string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder familyName(final @Nullable String familyName) {
        mBundle.putString(FAMILY_NAME_KEY, familyName);
        return this;
      }

      /**
       * Set the company name for this address entry.
       *
       * @param organization The company name string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder organization(final @Nullable String organization) {
        mBundle.putString(ORGANIZATION_KEY, organization);
        return this;
      }

      /**
       * Set the street address for this address entry.
       *
       * @param streetAddress The street address string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder streetAddress(final @Nullable String streetAddress) {
        mBundle.putString(STREET_ADDRESS_KEY, streetAddress);
        return this;
      }

      /**
       * Set the level 1 address for this address entry.
       *
       * @param addressLevel1 The level 1 address string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder addressLevel1(final @Nullable String addressLevel1) {
        mBundle.putString(ADDRESS_LEVEL1_KEY, addressLevel1);
        return this;
      }

      /**
       * Set the level 2 address for this address entry.
       *
       * @param addressLevel2 The level 2 address string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder addressLevel2(final @Nullable String addressLevel2) {
        mBundle.putString(ADDRESS_LEVEL2_KEY, addressLevel2);
        return this;
      }

      /**
       * Set the level 3 address for this address entry.
       *
       * @param addressLevel3 The level 3 address string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder addressLevel3(final @Nullable String addressLevel3) {
        mBundle.putString(ADDRESS_LEVEL3_KEY, addressLevel3);
        return this;
      }

      /**
       * Set the postal code for this address entry.
       *
       * @param postalCode The postal code string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder postalCode(final @Nullable String postalCode) {
        mBundle.putString(POSTAL_CODE_KEY, postalCode);
        return this;
      }

      /**
       * Set the country code for this address entry.
       *
       * @param country The country string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder country(final @Nullable String country) {
        mBundle.putString(COUNTRY_KEY, country);
        return this;
      }

      /**
       * Set the telephone number for this address entry.
       *
       * @param tel The telephone number string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder tel(final @Nullable String tel) {
        mBundle.putString(TEL_KEY, tel);
        return this;
      }

      /**
       * Set the email address for this address entry.
       *
       * @param email The email address string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder email(final @Nullable String email) {
        mBundle.putString(EMAIL_KEY, email);
        return this;
      }
    }
  }

  /** Holds login information for a specific entry. */
  public static class LoginEntry {
    private static final String GUID_KEY = "guid";
    private static final String ORIGIN_KEY = "origin";
    private static final String FORM_ACTION_ORIGIN_KEY = "formActionOrigin";
    private static final String HTTP_REALM_KEY = "httpRealm";
    private static final String USERNAME_KEY = "username";
    private static final String PASSWORD_KEY = "password";

    /** The unique identifier for this login entry. */
    public final @Nullable String guid;

    /** The origin this login entry applies to. */
    public final @NonNull String origin;

    /**
     * The origin this login entry was submitted to. This only applies to form-based login entries.
     * It's derived from the action attribute set on the form element.
     */
    public final @Nullable String formActionOrigin;

    /**
     * The HTTP realm this login entry was requested for. This only applies to non-form-based login
     * entries. It's derived from the WWW-Authenticate header set in a HTTP 401 response, see
     * RFC2617 for details.
     */
    public final @Nullable String httpRealm;

    /** The username for this login entry. */
    public final @NonNull String username;

    /** The password for this login entry. */
    public final @NonNull String password;

    // For tests only.
    @AnyThread
    protected LoginEntry() {
      guid = null;
      origin = "";
      formActionOrigin = null;
      httpRealm = null;
      username = "";
      password = "";
    }

    @AnyThread
    /* package */ LoginEntry(final @NonNull GeckoBundle bundle) {
      guid = bundle.getString(GUID_KEY);
      origin = bundle.getString(ORIGIN_KEY, "");
      formActionOrigin = bundle.getString(FORM_ACTION_ORIGIN_KEY);
      httpRealm = bundle.getString(HTTP_REALM_KEY);
      username = bundle.getString(USERNAME_KEY, "");
      password = bundle.getString(PASSWORD_KEY, "");
    }

    @Override
    @AnyThread
    public String toString() {
      final StringBuilder builder = new StringBuilder("LoginEntry {");
      builder
          .append("guid=")
          .append(guid)
          .append(", origin=")
          .append(origin)
          .append(", formActionOrigin=")
          .append(formActionOrigin)
          .append(", httpRealm=")
          .append(httpRealm)
          .append(", username=")
          .append(username)
          .append(", password=")
          .append(password)
          .append("}");
      return builder.toString();
    }

    @AnyThread
    /* package */ @NonNull
    GeckoBundle toBundle() {
      final GeckoBundle bundle = new GeckoBundle(6);
      bundle.putString(GUID_KEY, guid);
      bundle.putString(ORIGIN_KEY, origin);
      bundle.putString(FORM_ACTION_ORIGIN_KEY, formActionOrigin);
      bundle.putString(HTTP_REALM_KEY, httpRealm);
      bundle.putString(USERNAME_KEY, username);
      bundle.putString(PASSWORD_KEY, password);

      return bundle;
    }

    public static class Builder {
      private final GeckoBundle mBundle;

      @AnyThread
      /* package */ Builder(final @NonNull GeckoBundle bundle) {
        mBundle = new GeckoBundle(bundle);
      }

      @AnyThread
      @SuppressWarnings("checkstyle:javadocmethod")
      public Builder() {
        mBundle = new GeckoBundle(6);
      }

      /**
       * Finalize the {@link LoginEntry} instance.
       *
       * @return The {@link LoginEntry} instance.
       */
      @AnyThread
      public @NonNull LoginEntry build() {
        return new LoginEntry(mBundle);
      }

      /**
       * Set the unique identifier for this login entry.
       *
       * @param guid The unique identifier string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder guid(final @Nullable String guid) {
        mBundle.putString(GUID_KEY, guid);
        return this;
      }

      /**
       * Set the origin this login entry applies to.
       *
       * @param origin The origin string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder origin(final @NonNull String origin) {
        mBundle.putString(ORIGIN_KEY, origin);
        return this;
      }

      /**
       * Set the origin this login entry was submitted to.
       *
       * @param formActionOrigin The form action origin string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder formActionOrigin(final @Nullable String formActionOrigin) {
        mBundle.putString(FORM_ACTION_ORIGIN_KEY, formActionOrigin);
        return this;
      }

      /**
       * Set the HTTP realm this login entry was requested for.
       *
       * @param httpRealm The HTTP realm string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder httpRealm(final @Nullable String httpRealm) {
        mBundle.putString(HTTP_REALM_KEY, httpRealm);
        return this;
      }

      /**
       * Set the username for this login entry.
       *
       * @param username The username string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder username(final @NonNull String username) {
        mBundle.putString(USERNAME_KEY, username);
        return this;
      }

      /**
       * Set the password for this login entry.
       *
       * @param password The password string.
       * @return This {@link Builder} instance.
       */
      @AnyThread
      public @NonNull Builder password(final @NonNull String password) {
        mBundle.putString(PASSWORD_KEY, password);
        return this;
      }
    }
  }

  @Retention(RetentionPolicy.SOURCE)
  @IntDef(
      flag = true,
      value = {UsedField.PASSWORD})
  public @interface LSUsedField {}

  // Sync with UsedField in GeckoViewAutocomplete.jsm.
  /** Possible login entry field types for {@link StorageDelegate#onLoginUsed}. */
  public static class UsedField {
    /** The password field of a login entry. */
    public static final int PASSWORD = 1;

    protected UsedField() {}
  }

  /**
   * Implement this interface to handle runtime login storage requests. Login storage events include
   * login entry requests for autofill and autocompletion of login input fields. This delegate is
   * attached to the runtime via {@link GeckoRuntime#setAutocompleteStorageDelegate}.
   */
  public interface StorageDelegate {
    /**
     * Request login entries for a given domain. While processing the web document, we have
     * identified elements resembling login input fields suitable for autofill. We will attempt to
     * match the provided login information to the identified input fields.
     *
     * @param domain The domain string for the requested logins.
     * @return A {@link GeckoResult} that completes with an array of {@link LoginEntry} containing
     *     the existing logins for the given domain.
     */
    @UiThread
    default @Nullable GeckoResult<LoginEntry[]> onLoginFetch(@NonNull final String domain) {
      return null;
    }

    /**
     * Request login entries for all domains.
     *
     * @return A {@link GeckoResult} that completes with an array of {@link LoginEntry} containing
     *     the existing logins.
     */
    @UiThread
    default @Nullable GeckoResult<LoginEntry[]> onLoginFetch() {
      return null;
    }

    /**
     * Request credit card entries. While processing the web document, we have identified elements
     * resembling credit card input fields suitable for autofill. We will attempt to match the
     * provided credit card information to the identified input fields.
     *
     * @return A {@link GeckoResult} that completes with an array of {@link CreditCard} containing
     *     the existing credit cards.
     */
    @UiThread
    default @Nullable GeckoResult<CreditCard[]> onCreditCardFetch() {
      return null;
    }

    /**
     * Request address entries. While processing the web document, we have identified elements
     * resembling address input fields suitable for autofill. We will attempt to match the provided
     * address information to the identified input fields.
     *
     * @return A {@link GeckoResult} that completes with an array of {@link Address} containing the
     *     existing addresses.
     */
    @UiThread
    default @Nullable GeckoResult<Address[]> onAddressFetch() {
      return null;
    }

    /**
     * Request saving or updating of the given login entry. This is triggered by confirming a {@link
     * GeckoSession.PromptDelegate#onLoginSave onLoginSave} request.
     *
     * @param login The {@link LoginEntry} as confirmed by the prompt request.
     */
    @UiThread
    default void onLoginSave(@NonNull final LoginEntry login) {}

    /**
     * Request saving or updating of the given credit card entry. This is triggered by confirming a
     * {@link GeckoSession.PromptDelegate#onCreditCardSave onCreditCardSave} request.
     *
     * @param creditCard The {@link CreditCard} as confirmed by the prompt request.
     */
    @UiThread
    default void onCreditCardSave(@NonNull CreditCard creditCard) {}

    /**
     * Request saving or updating of the given address entry. This is triggered by confirming a
     * {@link GeckoSession.PromptDelegate#onAddressSave onAddressSave} request.
     *
     * @param address The {@link Address} as confirmed by the prompt request.
     */
    @UiThread
    default void onAddressSave(@NonNull Address address) {}

    /**
     * Notify that the given login was used to autofill login input fields. This is triggered by
     * autofilling elements with unmodified login entries as provided via {@link #onLoginFetch}.
     *
     * @param login The {@link LoginEntry} that was used for the autofilling.
     * @param usedFields The login entry fields used for autofilling. A combination of {@link
     *     UsedField}.
     */
    @UiThread
    default void onLoginUsed(@NonNull final LoginEntry login, @LSUsedField final int usedFields) {}
  }

  /**
   * Abstract base class for Autocomplete options. Extended by {@link Autocomplete.SaveOption} and
   * {@link Autocomplete.SelectOption}.
   */
  public abstract static class Option<T> {
    /* package */ static final String VALUE_KEY = "value";
    /* package */ static final String HINT_KEY = "hint";

    public final @NonNull T value;
    public final int hint;

    @SuppressWarnings("checkstyle:javadocmethod")
    public Option(final @NonNull T value, final int hint) {
      this.value = value;
      this.hint = hint;
    }

    @AnyThread
    /* package */ abstract @NonNull GeckoBundle toBundle();
  }

  /** Abstract base class for saving options. Extended by {@link Autocomplete.LoginSaveOption}. */
  public abstract static class SaveOption<T> extends Option<T> {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(
        flag = true,
        value = {Hint.NONE, Hint.GENERATED, Hint.LOW_CONFIDENCE})
    public @interface SaveOptionHint {}

    /** Hint types for login saving requests. */
    public static class Hint {
      public static final int NONE = 0;

      /** Auto-generated password. Notify but do not prompt the user for saving. */
      public static final int GENERATED = 1 << 0;

      /**
       * Potentially non-login data. The form data entered may be not login credentials but other
       * forms of input like credit card numbers. Note that this could be valid login data in same
       * cases, e.g., some banks may expect credit card numbers in the username field.
       */
      public static final int LOW_CONFIDENCE = 1 << 1;

      protected Hint() {}
    }

    @SuppressWarnings("checkstyle:javadocmethod")
    public SaveOption(final @NonNull T value, final @SaveOptionHint int hint) {
      super(value, hint);
    }
  }

  /** Abstract base class for saving options. Extended by {@link Autocomplete.LoginSelectOption}. */
  public abstract static class SelectOption<T> extends Option<T> {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(
        flag = true,
        value = {
          Hint.NONE,
          Hint.GENERATED,
          Hint.INSECURE_FORM,
          Hint.DUPLICATE_USERNAME,
          Hint.MATCHING_ORIGIN
        })
    public @interface SelectOptionHint {}

    /** Hint types for selection requests. */
    public static class Hint {
      public static final int NONE = 0;

      /**
       * Auto-generated password. A new password-only login entry containing a secure generated
       * password.
       */
      public static final int GENERATED = 1 << 0;

      /**
       * Insecure context. The form or transmission mechanics are considered insecure. This is the
       * case when the form is served via http or submitted insecurely.
       */
      public static final int INSECURE_FORM = 1 << 1;

      /**
       * The username is shared with another login entry. There are multiple login entries in the
       * options that share the same username. You may have to disambiguate the login entry, e.g.,
       * using the last date of modification and its origin.
       */
      public static final int DUPLICATE_USERNAME = 1 << 2;

      /**
       * The login entry's origin matches the login form origin. The login was saved from the same
       * origin it is being requested for, rather than for a subdomain.
       */
      public static final int MATCHING_ORIGIN = 1 << 3;
    }

    @SuppressWarnings("checkstyle:javadocmethod")
    public SelectOption(final @NonNull T value, final @SelectOptionHint int hint) {
      super(value, hint);
    }

    @Override
    public String toString() {
      final StringBuilder builder = new StringBuilder("SelectOption {");
      builder.append("value=").append(value).append(", ").append("hint=").append(hint).append("}");
      return builder.toString();
    }
  }

  /** Holds information required to process login saving requests. */
  public static class LoginSaveOption extends SaveOption<LoginEntry> {
    /**
     * Construct a login save option.
     *
     * @param value The {@link LoginEntry} login entry to be saved.
     * @param hint The {@link Hint} detailing the type of the option.
     */
    /* package */ LoginSaveOption(final @NonNull LoginEntry value, final @SaveOptionHint int hint) {
      super(value, hint);
    }

    /**
     * Construct a login save option.
     *
     * @param value The {@link LoginEntry} login entry to be saved.
     */
    public LoginSaveOption(final @NonNull LoginEntry value) {
      this(value, Hint.NONE);
    }

    @Override
    /* package */ @NonNull
    GeckoBundle toBundle() {
      final GeckoBundle bundle = new GeckoBundle(2);
      bundle.putBundle(VALUE_KEY, value.toBundle());
      bundle.putInt(HINT_KEY, hint);
      return bundle;
    }
  }

  /** Holds information required to process address saving requests. */
  public static class AddressSaveOption extends SaveOption<Address> {
    /**
     * Construct a address save option.
     *
     * @param value The {@link Address} address entry to be saved.
     * @param hint The {@link Hint} detailing the type of the option.
     */
    /* package */ AddressSaveOption(final @NonNull Address value, final @SaveOptionHint int hint) {
      super(value, hint);
    }

    /**
     * Construct an address save option.
     *
     * @param value The {@link Address} address entry to be saved.
     */
    public AddressSaveOption(final @NonNull Address value) {
      this(value, Hint.NONE);
    }

    @Override
    /* package */ @NonNull
    GeckoBundle toBundle() {
      final GeckoBundle bundle = new GeckoBundle(2);
      bundle.putBundle(VALUE_KEY, value.toBundle());
      bundle.putInt(HINT_KEY, hint);
      return bundle;
    }
  }

  /** Holds information required to process credit card saving requests. */
  public static class CreditCardSaveOption extends SaveOption<CreditCard> {
    /**
     * Construct a credit card save option.
     *
     * @param value The {@link CreditCard} credit card entry to be saved.
     * @param hint The {@link Hint} detailing the type of the option.
     */
    /* package */ CreditCardSaveOption(
        final @NonNull CreditCard value, final @SaveOptionHint int hint) {
      super(value, hint);
    }

    /**
     * Construct a credit card save option.
     *
     * @param value The {@link CreditCard} credit card entry to be saved.
     */
    public CreditCardSaveOption(final @NonNull CreditCard value) {
      this(value, Hint.NONE);
    }

    @Override
    /* package */ @NonNull
    GeckoBundle toBundle() {
      final GeckoBundle bundle = new GeckoBundle(2);
      bundle.putBundle(VALUE_KEY, value.toBundle());
      bundle.putInt(HINT_KEY, hint);
      return bundle;
    }
  }

  /** Holds information required to process login selection requests. */
  public static class LoginSelectOption extends SelectOption<LoginEntry> {
    /**
     * Construct a login select option.
     *
     * @param value The {@link LoginEntry} login entry selection option.
     * @param hint The {@link Hint} detailing the type of the option.
     */
    /* package */ LoginSelectOption(
        final @NonNull LoginEntry value, final @SelectOptionHint int hint) {
      super(value, hint);
    }

    /**
     * Construct a login select option.
     *
     * @param value The {@link LoginEntry} login entry selection option.
     */
    public LoginSelectOption(final @NonNull LoginEntry value) {
      this(value, Hint.NONE);
    }

    /* package */ static @NonNull LoginSelectOption fromBundle(final @NonNull GeckoBundle bundle) {
      final int hint = bundle.getInt("hint");
      final LoginEntry value = new LoginEntry(bundle.getBundle("value"));

      return new LoginSelectOption(value, hint);
    }

    @Override
    /* package */ @NonNull
    GeckoBundle toBundle() {
      final GeckoBundle bundle = new GeckoBundle(2);
      bundle.putBundle(VALUE_KEY, value.toBundle());
      bundle.putInt(HINT_KEY, hint);
      return bundle;
    }
  }

  /** Holds information required to process credit card selection requests. */
  public static class CreditCardSelectOption extends SelectOption<CreditCard> {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(
        flag = true,
        value = {Hint.NONE, Hint.INSECURE_FORM})
    public @interface CreditCardSelectHint {}

    /** Hint types for credit card selection requests. */
    public static class Hint {
      public static final int NONE = 0;

      /**
       * Insecure context. The form or transmission mechanics are considered insecure. This is the
       * case when the form is served via http or submitted insecurely.
       */
      public static final int INSECURE_FORM = 1 << 1;
    }

    /**
     * Construct a credit card select option.
     *
     * @param value The {@link LoginEntry} credit card entry selection option.
     * @param hint The {@link Hint} detailing the type of the option.
     */
    /* package */ CreditCardSelectOption(
        final @NonNull CreditCard value, final @CreditCardSelectHint int hint) {
      super(value, hint);
    }

    /**
     * Construct a credit card select option.
     *
     * @param value The {@link CreditCard} credit card entry selection option.
     */
    public CreditCardSelectOption(final @NonNull CreditCard value) {
      this(value, Hint.NONE);
    }

    /* package */ static @NonNull CreditCardSelectOption fromBundle(
        final @NonNull GeckoBundle bundle) {
      final int hint = bundle.getInt("hint");
      final CreditCard value = new CreditCard(bundle.getBundle("value"));

      return new CreditCardSelectOption(value, hint);
    }

    @Override
    /* package */ @NonNull
    GeckoBundle toBundle() {
      final GeckoBundle bundle = new GeckoBundle(2);
      bundle.putBundle(VALUE_KEY, value.toBundle());
      bundle.putInt(HINT_KEY, hint);
      return bundle;
    }
  }

  /** Holds information required to process address selection requests. */
  public static class AddressSelectOption extends SelectOption<Address> {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(
        flag = true,
        value = {Hint.NONE, Hint.INSECURE_FORM})
    public @interface AddressSelectHint {}

    /** Hint types for credit card selection requests. */
    public static class Hint {
      public static final int NONE = 0;

      /**
       * Insecure context. The form or transmission mechanics are considered insecure. This is the
       * case when the form is served via http or submitted insecurely.
       */
      public static final int INSECURE_FORM = 1 << 1;
    }

    /**
     * Construct a credit card select option.
     *
     * @param value The {@link LoginEntry} credit card entry selection option.
     * @param hint The {@link Hint} detailing the type of the option.
     */
    /* package */ AddressSelectOption(
        final @NonNull Address value, final @AddressSelectHint int hint) {
      super(value, hint);
    }

    /**
     * Construct a address select option.
     *
     * @param value The {@link Address} address entry selection option.
     */
    public AddressSelectOption(final @NonNull Address value) {
      this(value, Hint.NONE);
    }

    /* package */ static @NonNull AddressSelectOption fromBundle(
        final @NonNull GeckoBundle bundle) {
      final int hint = bundle.getInt("hint");
      final Address value = new Address(bundle.getBundle("value"));

      return new AddressSelectOption(value, hint);
    }

    @Override
    /* package */ @NonNull
    GeckoBundle toBundle() {
      final GeckoBundle bundle = new GeckoBundle(2);
      bundle.putBundle(VALUE_KEY, value.toBundle());
      bundle.putInt(HINT_KEY, hint);
      return bundle;
    }
  }

  /* package */ static final class StorageProxy implements BundleEventListener {
    private static final String FETCH_LOGIN_EVENT = "GeckoView:Autocomplete:Fetch:Login";
    private static final String FETCH_CREDIT_CARD_EVENT = "GeckoView:Autocomplete:Fetch:CreditCard";
    private static final String FETCH_ADDRESS_EVENT = "GeckoView:Autocomplete:Fetch:Address";
    private static final String SAVE_LOGIN_EVENT = "GeckoView:Autocomplete:Save:Login";
    private static final String SAVE_CREDIT_CARD_EVENT = "GeckoView:Autocomplete:Save:CreditCard";
    private static final String SAVE_ADDRESS_EVENT = "GeckoView:Autocomplete:Save:Address";
    private static final String USED_LOGIN_EVENT = "GeckoView:Autocomplete:Used:Login";

    private @Nullable StorageDelegate mDelegate;

    public StorageProxy() {}

    private void registerListener() {
      EventDispatcher.getInstance().dispatch("GeckoView:StorageDelegate:Attached", null);
      EventDispatcher.getInstance()
          .registerUiThreadListener(
              this,
              FETCH_LOGIN_EVENT,
              FETCH_CREDIT_CARD_EVENT,
              FETCH_ADDRESS_EVENT,
              SAVE_LOGIN_EVENT,
              SAVE_CREDIT_CARD_EVENT,
              SAVE_ADDRESS_EVENT,
              USED_LOGIN_EVENT);
    }

    private void unregisterListener() {
      EventDispatcher.getInstance()
          .unregisterUiThreadListener(
              this,
              FETCH_LOGIN_EVENT,
              FETCH_CREDIT_CARD_EVENT,
              FETCH_ADDRESS_EVENT,
              SAVE_LOGIN_EVENT,
              SAVE_CREDIT_CARD_EVENT,
              SAVE_ADDRESS_EVENT,
              USED_LOGIN_EVENT);
    }

    public synchronized void setDelegate(final @Nullable StorageDelegate delegate) {
      if (mDelegate == delegate) {
        return;
      }
      if (mDelegate != null) {
        unregisterListener();
      }

      mDelegate = delegate;

      if (mDelegate != null) {
        registerListener();
      }
    }

    public synchronized @Nullable StorageDelegate getDelegate() {
      return mDelegate;
    }

    @Override // BundleEventListener
    public synchronized void handleMessage(
        final String event, final GeckoBundle message, final EventCallback callback) {
      if (DEBUG) {
        Log.d(LOGTAG, "handleMessage " + event);
      }

      if (mDelegate == null) {
        if (callback != null) {
          callback.sendError("No StorageDelegate attached");
        }
        return;
      }

      if (FETCH_LOGIN_EVENT.equals(event)) {
        final String domain = message.getString("domain");
        final GeckoResult<Autocomplete.LoginEntry[]> result =
            domain != null ? mDelegate.onLoginFetch(domain) : mDelegate.onLoginFetch();

        if (result == null) {
          callback.sendSuccess(new GeckoBundle[0]);
          return;
        }

        callback.resolveTo(
            result.map(
                logins -> {
                  if (logins == null) {
                    return new GeckoBundle[0];
                  }

                  // This is a one-liner with streams (API level 24).
                  final GeckoBundle[] loginBundles = new GeckoBundle[logins.length];
                  for (int i = 0; i < logins.length; ++i) {
                    loginBundles[i] = logins[i].toBundle();
                  }

                  return loginBundles;
                }));
      } else if (FETCH_CREDIT_CARD_EVENT.equals(event)) {
        final GeckoResult<Autocomplete.CreditCard[]> result = mDelegate.onCreditCardFetch();

        if (result == null) {
          callback.sendSuccess(new GeckoBundle[0]);
          return;
        }

        callback.resolveTo(
            result.map(
                creditCards -> {
                  if (creditCards == null) {
                    return new GeckoBundle[0];
                  }

                  // This is a one-liner with streams (API level 24).
                  final GeckoBundle[] creditCardBundles = new GeckoBundle[creditCards.length];
                  for (int i = 0; i < creditCards.length; ++i) {
                    creditCardBundles[i] = creditCards[i].toBundle();
                  }

                  return creditCardBundles;
                }));
      } else if (FETCH_ADDRESS_EVENT.equals(event)) {
        final GeckoResult<Autocomplete.Address[]> result = mDelegate.onAddressFetch();

        if (result == null) {
          callback.sendSuccess(new GeckoBundle[0]);
          return;
        }

        callback.resolveTo(
            result.map(
                addresses -> {
                  if (addresses == null) {
                    return new GeckoBundle[0];
                  }

                  // This is a one-liner with streams (API level 24).
                  final GeckoBundle[] addressBundles = new GeckoBundle[addresses.length];
                  for (int i = 0; i < addresses.length; ++i) {
                    addressBundles[i] = addresses[i].toBundle();
                  }

                  return addressBundles;
                }));
      } else if (SAVE_LOGIN_EVENT.equals(event)) {
        final GeckoBundle loginBundle = message.getBundle("login");
        final LoginEntry login = new LoginEntry(loginBundle);

        mDelegate.onLoginSave(login);
      } else if (SAVE_CREDIT_CARD_EVENT.equals(event)) {
        final GeckoBundle creditCardBundle = message.getBundle("creditCard");
        final CreditCard creditCard = new CreditCard(creditCardBundle);

        mDelegate.onCreditCardSave(creditCard);
      } else if (SAVE_ADDRESS_EVENT.equals(event)) {
        final GeckoBundle addressBundle = message.getBundle("address");
        final Address address = new Address(addressBundle);

        mDelegate.onAddressSave(address);
      } else if (USED_LOGIN_EVENT.equals(event)) {
        final GeckoBundle loginBundle = message.getBundle("login");
        final LoginEntry login = new LoginEntry(loginBundle);
        final int fields = message.getInt("usedFields");

        mDelegate.onLoginUsed(login, fields);
      }
    }
  }
}
