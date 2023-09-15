/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Fathom ML model for identifying sign up <forms>
 *
 * This is developed out-of-tree at https://github.com/mozilla-services/fathom-login-forms,
 * where there is also over a GB of training, validation, and
 * testing data. To make changes, do your edits there (whether adding new
 * training pages, adding new rules, or both), retrain and evaluate as
 * documented at https://mozilla.github.io/fathom/training.html, paste the
 * coefficients emitted by the trainer into the ruleset, and finally copy the
 * ruleset's "CODE TO COPY INTO PRODUCTION" section to this file's "CODE FROM
 * TRAINING REPOSITORY" section.
 */

import {
  dom,
  out,
  rule,
  ruleset,
  score,
  type,
  element,
  utils,
} from "resource://gre/modules/third_party/fathom/fathom.mjs";

let { isVisible, attributesMatch, setDefault } = utils;

const DEVELOPMENT = false;

/**
 * --- START OF CODE FROM TRAINING REPOSITORY ---
 */
const coefficients = {
  form: new Map([
    ["formAttributesMatchRegisterRegex", 0.4614015519618988],
    ["formAttributesMatchLoginRegex", -2.608457326889038],
    ["formAttributesMatchSubscriptionRegex", -3.253319501876831],
    ["formAttributesMatchLoginAndRegisterRegex", 3.6423728466033936],
    ["formHasAcNewPassword", 2.214113473892212],
    ["formHasAcCurrentPassword", -0.43707895278930664],
    ["formHasEmailField", 1.760241150856018],
    ["formHasUsernameField", 1.1527059078216553],
    ["formHasPasswordField", 1.6670876741409302],
    ["formHasFirstOrLastNameField", 0.9517516493797302],
    ["formHasRegisterButton", 1.574048638343811],
    ["formHasLoginButton", -1.1688978672027588],
    ["formHasSubscribeButton", -0.26299405097961426],
    ["formHasContinueButton", 2.3797709941864014],
    ["formHasTermsAndConditionsHyperlink", 1.764896035194397],
    ["formHasPasswordForgottenHyperlink", -0.32138824462890625],
    ["formHasAlreadySignedUpHyperlink", 3.160510301589966],
    ["closestElementIsEmailLabelLike", 1.0336143970489502],
    ["formHasRememberMeCheckbox", -1.2176686525344849],
    ["formHasSubcriptionCheckbox", 0.6100747585296631],
    ["docTitleMatchesRegisterRegex", 0.680654764175415],
    ["docTitleMatchesEditProfileRegex", -4.104133605957031],
    ["closestHeaderMatchesRegisterRegex", 1.3462989330291748],
    ["closestHeaderMatchesLoginRegex", -0.1804502159357071],
    ["closestHeaderMatchesSubscriptionRegex", -1.3057124614715576],
  ]),
};

const biases = [["form", -4.402400970458984]];

const loginRegex =
  /login|log-in|log_in|log in|signon|sign-on|sign_on|sign on|signin|sign-in|sign_in|sign in|einloggen|anmelden|logon|log-on|log_on|log on|Войти|ورود|登录|Přihlásit se|Přihlaste|Авторизоваться|Авторизация|entrar|ログイン|로그인|inloggen|Συνδέσου|accedi|ログオン|Giriş Yap|登入|connecter|connectez-vous|Connexion|Вход|inicia/i;
const registerRegex =
  /regist|sign up|signup|sign-up|sign_up|join|new|登録|neu|erstellen|設定|신규|Créer|Nouveau|baru|nouă|nieuw|create[a-zA-Z\s]+account|create[a-zA-Z\s]+profile|activate[a-zA-Z\s]+account|Zugang anlegen|Angaben prüfen|Konto erstellen|ثبت نام|登録|注册|cadastr|Зарегистрироваться|Регистрация|Bellige alynmak|تسجيل|ΕΓΓΡΑΦΗΣ|Εγγραφή|Créer mon compte|Créer un compte|Mendaftar|가입하기|inschrijving|Zarejestruj się|Deschideți un cont|Создать аккаунт|ร่วม|Üye Ol|ساخت حساب کاربری|Schrijf je|S'inscrire/i;
const emailRegex = /mail/i;
const usernameRegex = /user|member/i;
const nameRegex = /first|last|middle/i;
const subscriptionRegex =
  /subscri|trial|offer|information|angebote|probe|ニュースレター|abonn|promotion|news/i;
const termsAndConditionsRegex =
  /terms|condition|rules|policy|privacy|nutzungsbedingungen|AGB|richtlinien|datenschutz|términos|condiciones/i;
const pwForgottenRegex =
  /forgot|reset|set password|vergessen|vergeten|oublié|dimenticata|Esqueceu|esqueci|Забыли|忘记|找回|Zapomenuté|lost|忘れた|忘れられた|忘れの方|재설정|찾기|help|فراموشی| را فراموش کرده اید|Восстановить|Unuttu|perdus|重新設定|recover|remind|request|restore|trouble|olvidada/i;
const continueRegex =
  /continue|go on|weiter|fortfahren|ga verder|next|continuar/i;
const rememberMeRegex =
  /remember|stay|speichern|merken|bleiben|auto_login|auto-login|auto login|ricordami|manter|mantenha|savelogin|keep me logged in|keep me signed in|save email address|save id|stay signed in|次回からログオンIDの入力を省略する|メールアドレスを保存する|を保存|아이디저장|아이디 저장|로그인 상태 유지|lembrar|mantenha-me conectado|Запомни меня|запомнить меня|Запомните меня|Не спрашивать в следующий раз|下次自动登录|记住我|recordar|angemeldet bleiben/i;
const alreadySignedUpRegex = /already|bereits|schon|ya tienes cuenta/i;
const editProfile = /edit/i;

function createRuleset(coeffs, biases) {
  let descendantsCache;
  let surroundingNodesCache;

  /**
   * Check document characteristics
   */
  function docTitleMatchesRegisterRegex(fnode) {
    const docTitle = fnode.element.ownerDocument.title;
    return checkValueAgainstRegex(docTitle, registerRegex);
  }
  function docTitleMatchesEditProfileRegex(fnode) {
    const docTitle = fnode.element.ownerDocument.title;
    return checkValueAgainstRegex(docTitle, editProfile);
  }

  /**
   * Check header
   */
  function closestHeaderMatchesLoginRegex(fnode) {
    return closestHeaderMatchesPredicate(fnode.element, header =>
      checkValueAgainstRegex(header.innerText, loginRegex)
    );
  }
  function closestHeaderMatchesRegisterRegex(fnode) {
    return closestHeaderMatchesPredicate(fnode.element, header =>
      checkValueAgainstRegex(header.innerText, registerRegex)
    );
  }
  function closestHeaderMatchesSubscriptionRegex(fnode) {
    return closestHeaderMatchesPredicate(fnode.element, header =>
      checkValueAgainstRegex(header.innerText, subscriptionRegex)
    );
  }

  /**
   * Check checkboxes
   */
  function formHasRememberMeCheckbox(fnode) {
    return elementHasRegexMatchingCheckbox(fnode.element, rememberMeRegex);
  }
  function formHasSubcriptionCheckbox(fnode) {
    return elementHasRegexMatchingCheckbox(fnode.element, subscriptionRegex);
  }

  /**
   * Check input fields
   */
  function formHasFirstOrLastNameField(fnode) {
    const acValues = ["name", "given-name", "family-name"];
    return elementHasPredicateMatchingInput(
      fnode.element,
      elem =>
        atLeastOne(acValues.filter(ac => elem.autocomplete == ac)) ||
        inputFieldMatchesPredicate(elem, attr =>
          checkValueAgainstRegex(attr, nameRegex)
        )
    );
  }
  function formHasEmailField(fnode) {
    return elementHasPredicateMatchingInput(
      fnode.element,
      elem =>
        elem.autocomplete == "email" ||
        elem.type == "email" ||
        inputFieldMatchesPredicate(elem, attr =>
          checkValueAgainstRegex(attr, emailRegex)
        )
    );
  }
  function formHasUsernameField(fnode) {
    return elementHasPredicateMatchingInput(
      fnode.element,
      elem =>
        elem.autocomplete == "username" ||
        inputFieldMatchesPredicate(elem, attr =>
          checkValueAgainstRegex(attr, usernameRegex)
        )
    );
  }
  function formHasPasswordField(fnode) {
    const acValues = ["current-password", "new-password"];
    return elementHasPredicateMatchingInput(
      fnode.element,
      elem =>
        atLeastOne(acValues.filter(ac => elem.autocomplete == ac)) ||
        elem.type == "password"
    );
  }

  /**
   * Check autocomplete values
   */
  function formHasAcCurrentPassword(fnode) {
    return inputFieldMatchesSelector(
      fnode.element,
      "autocomplete=current-password"
    );
  }
  function formHasAcNewPassword(fnode) {
    return inputFieldMatchesSelector(
      fnode.element,
      "autocomplete=new-password"
    );
  }

  /**
   * Check hyperlinks within form
   */
  function formHasTermsAndConditionsHyperlink(fnode) {
    return elementHasPredicateMatchingHyperlink(
      fnode.element,
      termsAndConditionsRegex
    );
  }
  function formHasPasswordForgottenHyperlink(fnode) {
    return elementHasPredicateMatchingHyperlink(
      fnode.element,
      pwForgottenRegex
    );
  }
  function formHasAlreadySignedUpHyperlink(fnode) {
    return elementHasPredicateMatchingHyperlink(
      fnode.element,
      alreadySignedUpRegex
    );
  }

  /**
   * Check labels
   */
  function closestElementIsEmailLabelLike(fnode) {
    return elementHasPredicateMatchingInput(fnode.element, elem =>
      previousSiblingLabelMatchesRegex(elem, emailRegex)
    );
  }

  /**
   * Check buttons
   */
  function formHasRegisterButton(fnode) {
    return elementHasPredicateMatchingButton(
      fnode.element,
      button =>
        checkValueAgainstRegex(button.innerText, registerRegex) ||
        buttonMatchesPredicate(button, attr =>
          checkValueAgainstRegex(attr, registerRegex)
        )
    );
  }
  function formHasLoginButton(fnode) {
    return elementHasPredicateMatchingButton(
      fnode.element,
      button =>
        checkValueAgainstRegex(button.innerText, loginRegex) ||
        buttonMatchesPredicate(button, attr =>
          checkValueAgainstRegex(attr, loginRegex)
        )
    );
  }
  function formHasContinueButton(fnode) {
    return elementHasPredicateMatchingButton(
      fnode.element,
      button =>
        checkValueAgainstRegex(button.innerText, continueRegex) ||
        buttonMatchesPredicate(button, attr =>
          checkValueAgainstRegex(attr, continueRegex)
        )
    );
  }
  function formHasSubscribeButton(fnode) {
    return elementHasPredicateMatchingButton(
      fnode.element,
      button =>
        checkValueAgainstRegex(button.innerText, subscriptionRegex) ||
        buttonMatchesPredicate(button, attr =>
          checkValueAgainstRegex(attr, subscriptionRegex)
        )
    );
  }

  /**
   * Check form attributes
   */
  function formAttributesMatchRegisterRegex(fnode) {
    return formMatchesPredicate(fnode.element, attr =>
      checkValueAgainstRegex(attr, registerRegex)
    );
  }
  function formAttributesMatchLoginRegex(fnode) {
    return formMatchesPredicate(fnode.element, attr =>
      checkValueAgainstRegex(attr, loginRegex)
    );
  }
  function formAttributesMatchSubscriptionRegex(fnode) {
    return formMatchesPredicate(fnode.element, attr =>
      checkValueAgainstRegex(attr, subscriptionRegex)
    );
  }
  function formAttributesMatchLoginAndRegisterRegex(fnode) {
    return formMatchesPredicate(fnode.element, attr =>
      checkValueAgainstAllRegex(attr, [registerRegex, loginRegex])
    );
  }

  /**
   * HELPER FUNCTIONS
   */
  function elementMatchesPredicate(element, predicate, additional = []) {
    return attributesMatch(
      element,
      predicate,
      ["id", "name", "className"].concat(additional)
    );
  }
  function formMatchesPredicate(element, predicate) {
    return elementMatchesPredicate(element, predicate, ["action"]);
  }
  function inputFieldMatchesPredicate(element, predicate) {
    return elementMatchesPredicate(element, predicate, ["placeholder"]);
  }
  function inputFieldMatchesSelector(element, selector) {
    return atLeastOne(getElementDescendants(element, `input[${selector}]`));
  }
  function buttonMatchesPredicate(element, predicate) {
    return elementMatchesPredicate(element, predicate, [
      "value",
      "id",
      "title",
    ]);
  }
  function elementHasPredicateMatchingDescendant(element, selector, predicate) {
    const matchingElements = getElementDescendants(element, selector);
    return matchingElements.some(predicate);
  }
  function elementHasPredicateMatchingHeader(element, predicate) {
    return (
      elementHasPredicateMatchingDescendant(
        element,
        "h1,h2,h3,h4,h5,h6",
        predicate
      ) ||
      elementHasPredicateMatchingDescendant(
        element,
        "div[class*=heading],div[class*=header],div[class*=title],header",
        predicate
      )
    );
  }
  function elementHasPredicateMatchingButton(element, predicate) {
    return elementHasPredicateMatchingDescendant(
      element,
      "button,input[type=submit],input[type=button]",
      predicate
    );
  }
  function elementHasPredicateMatchingInput(element, predicate) {
    return elementHasPredicateMatchingDescendant(element, "input", predicate);
  }
  function elementHasPredicateMatchingHyperlink(element, regexExp) {
    return elementHasPredicateMatchingDescendant(
      element,
      "a",
      link =>
        previousSiblingLabelMatchesRegex(link, regexExp) ||
        checkValueAgainstRegex(link.innerText, regexExp) ||
        elementMatchesPredicate(
          link,
          attr => checkValueAgainstRegex(attr, regexExp),
          ["href"]
        ) ||
        nextSiblingLabelMatchesRegex(link, regexExp)
    );
  }
  function elementHasRegexMatchingCheckbox(element, regexExp) {
    return elementHasPredicateMatchingDescendant(
      element,
      "input[type=checkbox], div[class*=checkbox]",
      box =>
        elementMatchesPredicate(box, attr =>
          checkValueAgainstRegex(attr, regexExp)
        ) || nextSiblingLabelMatchesRegex(box, regexExp)
    );
  }

  function nextSiblingLabelMatchesRegex(element, regexExp) {
    let nextElem = element.nextElementSibling;
    if (nextElem && nextElem.tagName == "LABEL") {
      return checkValueAgainstRegex(nextElem.innerText, regexExp);
    }
    let closestElem = closestElementFollowing(element, "label");
    return closestElem
      ? checkValueAgainstRegex(closestElem.innerText, regexExp)
      : false;
  }

  function previousSiblingLabelMatchesRegex(element, regexExp) {
    let previousElem = element.previousElementSibling;
    if (previousElem && previousElem.tagName == "LABEL") {
      return checkValueAgainstRegex(previousElem.innerText, regexExp);
    }
    let closestElem = closestElementPreceding(element, "label");
    return closestElem
      ? checkValueAgainstRegex(closestElem.innerText, regexExp)
      : false;
  }
  function getElementDescendants(element, selector) {
    const selectorToDescendants = setDefault(
      descendantsCache,
      element,
      () => new Map()
    );

    return setDefault(selectorToDescendants, selector, () =>
      Array.from(element.querySelectorAll(selector))
    );
  }

  function clearCache() {
    descendantsCache = new WeakMap();
    surroundingNodesCache = new WeakMap();
  }
  function closestHeaderMatchesPredicate(element, predicate) {
    return (
      elementHasPredicateMatchingHeader(element, predicate) ||
      closestHeaderAboveMatchesPredicate(element, predicate)
    );
  }
  function closestHeaderAboveMatchesPredicate(element, predicate) {
    let closestHeader = closestElementPreceding(element, "h1,h2,h3,h4,h5,h6");

    if (closestHeader !== null) {
      if (predicate(closestHeader)) {
        return true;
      }
    }
    closestHeader = closestElementPreceding(
      element,
      "div[class*=heading],div[class*=header],div[class*=title],header"
    );
    return closestHeader ? predicate(closestHeader) : false;
  }
  function closestElementPreceding(element, selector) {
    return getSurroundingNodes(element, selector).precedingNode;
  }
  function closestElementFollowing(element, selector) {
    return getSurroundingNodes(element, selector).followingNode;
  }
  function getSurroundingNodes(element, selector) {
    const selectorToSurroundingNodes = setDefault(
      surroundingNodesCache,
      element,
      () => new Map()
    );

    return setDefault(selectorToSurroundingNodes, selector, () => {
      let elements = getElementDescendants(element.ownerDocument, selector);
      let followingIndex = closestFollowingNodeIndex(elements, element);
      let precedingIndex = followingIndex - 1;
      let preceding = precedingIndex < 0 ? null : elements[precedingIndex];
      let following =
        followingIndex == elements.length ? null : elements[followingIndex];
      return { precedingNode: preceding, followingNode: following };
    });
  }
  function closestFollowingNodeIndex(elements, element) {
    let low = 0;
    let high = elements.length;
    while (low < high) {
      let i = (low + high) >>> 1;
      if (
        element.compareDocumentPosition(elements[i]) &
        Node.DOCUMENT_POSITION_PRECEDING
      ) {
        low = i + 1;
      } else {
        high = i;
      }
    }
    return low;
  }

  function checkValueAgainstAllRegex(value, regexExp = []) {
    return regexExp.every(reg => checkValueAgainstRegex(value, reg));
  }

  function checkValueAgainstRegex(value, regexExp) {
    return value ? regexExp.test(value) : false;
  }
  function atLeastOne(iter) {
    return iter.length >= 1;
  }

  /**
   * CREATION OF RULESET
   */
  const rules = ruleset(
    [
      rule(
        DEVELOPMENT ? dom("form").when(isVisible) : element("form"),
        type("form").note(clearCache)
      ),
      // Check form attributes
      rule(type("form"), score(formAttributesMatchRegisterRegex), {
        name: "formAttributesMatchRegisterRegex",
      }),
      rule(type("form"), score(formAttributesMatchLoginRegex), {
        name: "formAttributesMatchLoginRegex",
      }),
      rule(type("form"), score(formAttributesMatchSubscriptionRegex), {
        name: "formAttributesMatchSubscriptionRegex",
      }),
      rule(type("form"), score(formAttributesMatchLoginAndRegisterRegex), {
        name: "formAttributesMatchLoginAndRegisterRegex",
      }),
      // Check autocomplete attributes
      rule(type("form"), score(formHasAcCurrentPassword), {
        name: "formHasAcCurrentPassword",
      }),
      rule(type("form"), score(formHasAcNewPassword), {
        name: "formHasAcNewPassword",
      }),
      // Check input fields
      rule(type("form"), score(formHasEmailField), {
        name: "formHasEmailField",
      }),
      rule(type("form"), score(formHasUsernameField), {
        name: "formHasUsernameField",
      }),
      rule(type("form"), score(formHasPasswordField), {
        name: "formHasPasswordField",
      }),
      rule(type("form"), score(formHasFirstOrLastNameField), {
        name: "formHasFirstOrLastNameField",
      }),
      // Check buttons
      rule(type("form"), score(formHasRegisterButton), {
        name: "formHasRegisterButton",
      }),
      rule(type("form"), score(formHasLoginButton), {
        name: "formHasLoginButton",
      }),
      rule(type("form"), score(formHasContinueButton), {
        name: "formHasContinueButton",
      }),
      rule(type("form"), score(formHasSubscribeButton), {
        name: "formHasSubscribeButton",
      }),
      // Check hyperlinks
      rule(type("form"), score(formHasTermsAndConditionsHyperlink), {
        name: "formHasTermsAndConditionsHyperlink",
      }),
      rule(type("form"), score(formHasPasswordForgottenHyperlink), {
        name: "formHasPasswordForgottenHyperlink",
      }),
      rule(type("form"), score(formHasAlreadySignedUpHyperlink), {
        name: "formHasAlreadySignedUpHyperlink",
      }),
      // Check labels
      rule(type("form"), score(closestElementIsEmailLabelLike), {
        name: "closestElementIsEmailLabelLike",
      }),
      // Check checkboxes
      rule(type("form"), score(formHasRememberMeCheckbox), {
        name: "formHasRememberMeCheckbox",
      }),
      rule(type("form"), score(formHasSubcriptionCheckbox), {
        name: "formHasSubcriptionCheckbox",
      }),
      // Check header
      rule(type("form"), score(closestHeaderMatchesRegisterRegex), {
        name: "closestHeaderMatchesRegisterRegex",
      }),
      rule(type("form"), score(closestHeaderMatchesLoginRegex), {
        name: "closestHeaderMatchesLoginRegex",
      }),
      rule(type("form"), score(closestHeaderMatchesSubscriptionRegex), {
        name: "closestHeaderMatchesSubscriptionRegex",
      }),
      // Check doc title
      rule(type("form"), score(docTitleMatchesRegisterRegex), {
        name: "docTitleMatchesRegisterRegex",
      }),
      rule(type("form"), score(docTitleMatchesEditProfileRegex), {
        name: "docTitleMatchesEditProfileRegex",
      }),
      rule(type("form"), out("form")),
    ],
    coeffs,
    biases
  );
  return rules;
}

/**
 * --- END OF CODE FROM TRAINING REPOSITORY ---
 */

export const SignUpFormRuleset = {
  type: "form",
  rules: createRuleset([...coefficients.form], biases),
};
