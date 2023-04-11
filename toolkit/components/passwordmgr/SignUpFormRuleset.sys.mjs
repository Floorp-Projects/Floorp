/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Machine learning model for identifying sign up scenario forms
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
  clusters,
} from "resource://gre/modules/third_party/fathom/fathom.mjs";

let { isVisible, attributesMatch, min, setDefault } = utils;
let { euclidean } = clusters;

const DEVELOPMENT = false;

/**
 * --- START OF RULESET ---
 */
const coefficients = {
  signup: new Map([
    ["formMethodIsPost", -0.7543129920959473],
    ["formAttributesMatchRegisterRegex", 1.7264184951782227],
    ["formAttributesMatchLoginRegex", -0.9329696297645569],
    ["formAttributesMatchNewsletterRegex", -2.206372022628784],
    ["formHasAcNewPassword", 1.5736613273620605],
    ["formHasAcCurrentPassword", -0.12390841543674469],
    ["formHasAcEmail", 0.6157014966011047],
    ["formHasAcUsername", -0.7274730801582336],
    ["formHasAcTel", -1.1380716562271118],
    ["formHasEmailField", 1.7712397575378418],
    ["formHasUsernameField", 1.311736822128296],
    ["formHasPasswordField", 1.3901746273040771],
    ["formHasEmailAndExtraNameField", 1.0147123336791992],
    ["formHasFirstOrLastNameFields", 0.6080058217048645],
    ["formHasBirthdayFields", 1.2841497659683228],
    ["formHasPhoneField", 0.8433054685592651],
    ["formHasRegisterButton", 1.1531426906585693],
    ["formHasLoginButton", -1.1045818328857422],
    ["formHasSubscribeButton", -2.290463447570801],
    ["formHasContinueButton", 2.2985446453094482],
    ["closestElementIsEmailLabelLike", 0.7776230573654175],
    ["closestElementIsNewPasswordLabelLike", 0.814133882522583],
    ["formHasTermsAndConditionsCheckbox", 0.6034160852432251],
    ["formHasRememberMeCheckbox", -2.623626470565796],
    ["formHasSubcriptionCheckbox", 0.5114848613739014],
    ["docTitleMatchesRegisterRegex", 1.2882719039916992],
    ["docTitleMatchesEditProfileRegex", -2.381089687347412],
    ["docHasRegisterOrPasswordForgottenHyperlink", -1.792033076286316],
    ["docHasLoginHyperlink", -0.621324896812439],
    ["closestHeaderMatchesRegisterRegex", 1.826246738433838],
    ["closestHeaderMatchesLoginRegex", -0.7161067128181458],
    ["closestHeaderMatchesNewsletterRegex", -1.6708611249923706],
  ]),
};

const biases = [["signup", -2.210782766342163]];

const loginRegex = /login|log-in|log_in|log in|signon|sign-on|sign_on|sign on|signin|sign-in|sign_in|sign in|einloggen|anmelden|logon|log-on|lgon_on|log on|Войти|ورود|登录|Přihlásit se|Přihlaste|Авторизоваться|Авторизация|entrar|ログイン|로그인|inloggen|Συνδέσου|accedi|ログオン|Giriş Yap|登入|connecter|connectez-vous|Connexion|Вход|inicia|inloggen/gi;
const registerRegex = /create|regist[a-z]|sign up|signup|sign-up|sign_up|join|new|登録|neu|erstellen|choose|設定|신규|Créer|Nouveau|baru|nouă|nieuw|create[a-zA-Z\s]+account|activate[a-zA-Z\s]+account|Zugang anlegen|Angaben prüfen|Konto erstellen|ثبت نام|登録|注册|cadastr|Зарегистрироваться|Регистрация|Bellige alynmak|تسجيل|ΕΓΓΡΑΦΗΣ|Εγγραφή|Créer mon compte|Créer un compte|Mendaftar|가입하기|inschrijving|Zarejestruj się|Deschideți un cont|Создать аккаунт|ร่วม|Üye Ol|ساخت حساب کاربری|Schrijf je|S'inscrire/gi;
const emailRegex = /mail/gi;
const usernameRegex = /user|name|member/gi;
const newPasswordRegex = /new|create/gi;
//const confirmAttrRegex = /confirm|retype|again|bevestigen|wiederhol|repeat|confirmation|verify|retype|repite|確認|の確認|تکرار|re-enter|확인|bevestigen|Повторите|tassyklamak|再次输入|ještě jednou|gentag|re-type|Répéter|conferma|Repetaţi|reenter|再入力|재입력|Ulangi|Bekræft/gi;
const nameRegex = /first|last|middle/gi;
const birthdateRegex = /birth|year|yyyy/gi;
const phoneRegex = /phone|mobile|tel|number/gi;
const newsletterRegex = /subscri|newsletter|trial|offer|information|angebote|probe|ニュースレター|abonn[a-z]/gi;
const termsAndConditionsRegex = /accept|agree|read|terms|condition|rules|policy|privacy|akzeptier|gelesen|nutzungbedingungen|AGB|términos|condiciones|/gi;
const pwForgottenRegex = /forgot|reset|set password|vergessen|vergeten|oublié|dimenticata|Esqueceu|esqueci|Забыли|忘记|找回|Zapomenuté|lost|忘れた|忘れられた|忘れの方|재설정|찾기|help|فراموشی| را فراموش کرده اید|Восстановить|Unuttu|perdus|重新設定|recover|remind|request|restore|trouble|olvidada/gi;
const continueRegex = /continue|go on|weiter|fortfahren|ga verder|next|continuar/gi;
const rememberMeRegex = /remember|stay|speichern|merken|bleiben|auto_login|auto-login|auto login|ricordami|manter|mantenha|savelogin|keep me logged in|keep me signed in|save email address|save id|stay signed in|次回からログオンIDの入力を省略する|メールアドレスを保存する|を保存|아이디저장|아이디 저장|로그인 상태 유지|lembrar|mantenha-me conectado|Запомни меня|запомнить меня|Запомните меня|Не спрашивать в следующий раз|下次自动登录|记住我|recordar/gi;
const alreadySignedUpRegex = /already|bereits|schon|ya tienes cuenta/gi;
const editProfile = /edit|profile/gi;

function createRuleset(coeffs, biases) {
  let elementToSelectors;

  /**
   * Check document characteristics
   */
  function docHasLoginHyperlink(fnode) {
    const links = getElementDescendants(fnode.element.ownerDocument, "a");

    return links.some(
      link =>
        checkValueAgainstRegex(link.innerText, loginRegex) ||
        checkValueAgainstRegex(link.innerText, alreadySignedUpRegex)
    );
  }
  function docHasRegisterOrPasswordForgottenHyperlink(fnode) {
    const links = getElementDescendants(fnode.element.ownerDocument, "a");

    return links.some(
      link =>
        checkValueAgainstRegex(link.innerText, registerRegex) ||
        checkValueAgainstRegex(link.innerText, pwForgottenRegex)
    );
  }
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
    return (
      headerInFormMatchesRegex(fnode.element, loginRegex) ||
      closestHeaderAboveMatchesRegex(fnode.element, loginRegex)
    );
  }

  function closestHeaderMatchesRegisterRegex(fnode) {
    return (
      headerInFormMatchesRegex(fnode.element, registerRegex) ||
      closestHeaderAboveMatchesRegex(fnode.element, registerRegex)
    );
  }
  function closestHeaderMatchesNewsletterRegex(fnode) {
    return (
      headerInFormMatchesRegex(fnode.element, newsletterRegex) ||
      closestHeaderAboveMatchesRegex(fnode.element, newsletterRegex)
    );
  }

  /**
   * Check Checkboxes
   */
  function formHasRememberMeCheckbox(fnode) {
    return checkboxInFormMatchesRegex(fnode.element, rememberMeRegex);
  }
  function formHasSubcriptionCheckbox(fnode) {
    return checkboxInFormMatchesRegex(fnode.element, newsletterRegex);
  }
  function formHasTermsAndConditionsCheckbox(fnode) {
    return checkboxInFormMatchesRegex(fnode.element, termsAndConditionsRegex);
  }

  /**
   * Check input fields
   */
  function formHasPhoneField(fnode) {
    return formContainsRegexMatchingElement(fnode.element, "input", phoneRegex);
  }

  function formHasBirthdayFields(fnode) {
    return formContainsRegexMatchingElement(
      fnode.element,
      "input,select",
      birthdateRegex
    );
  }
  function formHasFirstOrLastNameFields(fnode) {
    return formContainsRegexMatchingElement(fnode.element, "input", nameRegex);
  }
  function formHasEmailAndExtraNameField(fnode) {
    const possibleFields = getElementDescendants(
      fnode.element,
      "input[type=email],input[type=text]"
    );
    let containsEmail = false;
    let containsAnyName = false;

    for (const field of possibleFields) {
      if (
        attributesMatch(
          field,
          attr => checkValueAgainstRegex(attr, emailRegex),
          ["id", "name", "className", "placeholder"]
        )
      ) {
        if (containsAnyName) {
          return true;
        } else if (!containsEmail) {
          containsEmail = true;
        }
      } else if (
        attributesMatch(
          field,
          attr =>
            checkValueAgainstRegex(attr, usernameRegex) ||
            checkValueAgainstRegex(attr, nameRegex)
        )
      ) {
        if (containsEmail) {
          return true;
        } else if (!containsAnyName) {
          containsAnyName = true;
        }
      }
    }
    return false;
  }
  function formHasEmailField(fnode) {
    return atLeastOne(getEmailInputElements(fnode.element));
  }
  function formHasUsernameField(fnode) {
    return formContainsRegexMatchingElement(
      fnode.element,
      "input",
      usernameRegex
    );
  }
  function formHasPasswordField(fnode) {
    return checkInputFieldsForAttr(fnode.element, "type=password");
  }

  /**
   * Check autocomplete values
   */
  function formHasAcUsername(fnode) {
    return checkInputFieldsForAttr(fnode.element, "autocomplete=username");
  }
  function formHasAcEmail(fnode) {
    return checkInputFieldsForAttr(fnode.element, "autocomplete=email");
  }
  function formHasAcCurrentPassword(fnode) {
    return checkInputFieldsForAttr(
      fnode.element,
      "autocomplete=current-password"
    );
  }
  function formHasAcNewPassword(fnode) {
    return checkInputFieldsForAttr(fnode.element, "autocomplete=new-password");
  }
  function formHasAcTel(fnode) {
    return checkInputFieldsForAttr(fnode.element, "autocomplete*=tel");
  }

  /**
   * Check labels
   */
  function closestElementIsNewPasswordLabelLike(fnode) {
    const passwordFields = getElementDescendants(
      fnode.element,
      "input[type=password][autocomplete=new-password]"
    );
    return closestElementIsRegexMatchingLabel(passwordFields, newPasswordRegex);
  }
  function closestElementIsEmailLabelLike(fnode) {
    const emailFields = getEmailInputElements(fnode.element);
    return closestElementIsRegexMatchingLabel(emailFields, emailRegex);
  }

  /**
   * Check buttons
   */
  function formHasRegisterButton(fnode) {
    const buttons = getButtons(fnode.element);
    return buttons.some(button =>
      checkValueAgainstRegex(button.innerText, registerRegex)
    );
  }
  function formHasLoginButton(fnode) {
    const buttons = getButtons(fnode.element);
    return buttons.some(button =>
      checkValueAgainstRegex(button.innerText, loginRegex)
    );
  }
  function formHasContinueButton(fnode) {
    const buttons = getButtons(fnode.element);
    return buttons.some(button =>
      checkValueAgainstRegex(button.innerText, continueRegex)
    );
  }
  function formHasSubscribeButton(fnode) {
    const buttons = getButtons(fnode.element);
    return buttons.some(button =>
      checkValueAgainstRegex(button.innerText, newsletterRegex)
    );
  }

  /**
   * Check form attributes
   */
  function formAttributesMatchRegisterRegex(fnode) {
    return attributesMatch(
      fnode.element,
      attr => checkValueAgainstRegex(attr, registerRegex),
      ["action", "id", "name", "className"]
    );
  }

  function formAttributesMatchLoginRegex(fnode) {
    return attributesMatch(
      fnode.element,
      attr => checkValueAgainstRegex(attr, loginRegex),
      ["action", "id", "name", "className"]
    );
  }
  function formAttributesMatchNewsletterRegex(fnode) {
    return attributesMatch(fnode.element, attr =>
      checkValueAgainstRegex(attr, newsletterRegex)
    );
  }
  function formMethodIsPost(fnode) {
    return fnode.element.method === "post";
  }

  /**
   * HELPER FUNCTIONS
   */
  function closestElementIsRegexMatchingLabel(elements, regexExp) {
    return elements.some(elem => {
      const previousElem = elem.previousElementSibling;
      const closestLabel = closestSelectorElementWithinElement(
        elem,
        previousElem,
        "label"
      );
      return (
        closestLabel && checkValueAgainstRegex(closestLabel.innerText, regexExp)
      );
    });
  }
  function formContainsRegexMatchingElement(element, selector, regexExp) {
    const matchingElements = getElementDescendants(element, selector);

    return matchingElements.some(elem =>
      attributesMatch(elem, attr => checkValueAgainstRegex(attr, regexExp))
    );
  }
  function checkInputFieldsForAttr(element, attr) {
    return atLeastOne(getElementDescendants(element, `input[${attr}]`));
  }
  function headerInFormMatchesRegex(element, regexExp) {
    const headers = getElementDescendants(
      element,
      "h1,h2,h3,h4,h5,h6,div[class*=heading],div[class*=header],div[class*=title],header"
    );
    return headers.some(head =>
      checkValueAgainstRegex(head.innerText, regexExp)
    );
  }
  function checkboxInFormMatchesRegex(element, regexExp) {
    const checkboxes = getElementDescendants(element, "input[type=checkbox]");
    return checkboxes.some(box =>
      attributesMatch(box, attr => checkValueAgainstRegex(attr, regexExp))
    );
  }
  function getEmailInputElements(element) {
    const possibleEmailFields = getElementDescendants(element, "input");
    return possibleEmailFields.filter(field => {
      return attributesMatch(
        field,
        attr => checkValueAgainstRegex(attr, emailRegex),
        ["type", "id", "name", "className", "placeholder", "innerText"]
      );
    });
  }
  function getButtons(element) {
    return getElementDescendants(
      element,
      "button,input[type=submit],input[type=button]"
    );
  }
  function getElementDescendants(element, selector) {
    const selectorToDescendants = setDefault(
      elementToSelectors,
      element,
      () => new Map()
    );

    return setDefault(selectorToDescendants, selector, () =>
      Array.from(element.querySelectorAll(selector))
    );
  }
  function clearCache() {
    elementToSelectors = new WeakMap();
  }
  function closestSelectorElementWithinElement(
    toElement,
    withinElement,
    querySelector
  ) {
    if (withinElement) {
      let matchingElements = Array.from(
        withinElement.querySelectorAll(querySelector)
      );
      if (matchingElements.length) {
        return min(matchingElements, match => euclidean(match, toElement));
      }
    }
    return null;
  }
  function closestHeaderAboveMatchesRegex(element, regex) {
    const closestHeader = closestElementAbove(
      element,
      "h1,h2,h3,h4,h5,h6,div[class*=heading],div[class*=header],div[class*=title],header"
    );

    if (closestHeader == null) {
      return false;
    }
    return checkValueAgainstRegex(closestHeader.innerText, regex);
  }
  function closestElementAbove(element, selector) {
    let elements = Array.from(
      getElementDescendants(element.ownerDocument, selector)
    );
    for (let i = elements.length - 1; i >= 0; --i) {
      if (
        element.compareDocumentPosition(elements[i]) &
        Node.DOCUMENT_POSITION_PRECEDING
      ) {
        return elements[i];
      }
    }
    return null;
  }

  function checkValueAgainstRegex(value, regexExp) {
    const lowerCaseValue = value ? value.toLowerCase() : "";
    return regexExp.test(lowerCaseValue);
  }
  function atLeastOne(iter) {
    return iter.length >= 1;
  }

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
      rule(type("form"), score(formMethodIsPost), { name: "formMethodIsPost" }),
      rule(type("form"), score(formAttributesMatchLoginRegex), {
        name: "formAttributesMatchLoginRegex",
      }),
      rule(type("form"), score(formAttributesMatchNewsletterRegex), {
        name: "formAttributesMatchNewsletterRegex",
      }),
      // Check autocomplete attributes
      rule(type("form"), score(formHasAcCurrentPassword), {
        name: "formHasAcCurrentPassword",
      }),
      rule(type("form"), score(formHasAcNewPassword), {
        name: "formHasAcNewPassword",
      }),
      rule(type("form"), score(formHasAcTel), {
        name: "formHasAcTel",
      }),
      rule(type("form"), score(formHasAcUsername), {
        name: "formHasAcUsername",
      }),
      rule(type("form"), score(formHasAcEmail), {
        name: "formHasAcEmail",
      }),
      // Check input fields
      rule(type("form"), score(formHasEmailField), {
        name: "formHasEmailField",
      }),
      rule(type("form"), score(formHasUsernameField), {
        name: "formHasUsernameField",
      }),
      rule(type("form"), score(formHasEmailAndExtraNameField), {
        name: "formHasEmailAndExtraNameField",
      }),
      rule(type("form"), score(formHasPasswordField), {
        name: "formHasPasswordField",
      }),
      rule(type("form"), score(formHasFirstOrLastNameFields), {
        name: "formHasFirstOrLastNameFields",
      }),
      rule(type("form"), score(formHasBirthdayFields), {
        name: "formHasBirthdayFields",
      }),
      rule(type("form"), score(formHasPhoneField), {
        name: "formHasPhoneField",
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
      // Check labels
      rule(type("form"), score(closestElementIsEmailLabelLike), {
        name: "closestElementIsEmailLabelLike",
      }),
      rule(type("form"), score(closestElementIsNewPasswordLabelLike), {
        name: "closestElementIsNewPasswordLabelLike",
      }),
      // Check checkboxes
      rule(type("form"), score(formHasTermsAndConditionsCheckbox), {
        name: "formHasTermsAndConditionsCheckbox",
      }),
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
      rule(type("form"), score(closestHeaderMatchesNewsletterRegex), {
        name: "closestHeaderMatchesNewsletterRegex",
      }),
      // Check doc characteristics
      rule(type("form"), score(docTitleMatchesRegisterRegex), {
        name: "docTitleMatchesRegisterRegex",
      }),
      rule(type("form"), score(docTitleMatchesEditProfileRegex), {
        name: "docTitleMatchesEditProfileRegex",
      }),
      rule(type("form"), score(docHasLoginHyperlink), {
        name: "docHasLoginHyperlink",
      }),
      rule(type("form"), score(docHasRegisterOrPasswordForgottenHyperlink), {
        name: "docHasRegisterOrPasswordForgottenHyperlink",
      }),
      rule(type("form"), out("form")),
    ],
    coeffs,
    biases
  );
  return rules;
}

/**
 * --- END OF RULESET ---
 */

export const SignUpFormRuleset = {
  type: "form",
  rules: createRuleset([...coefficients.signup], biases),
};
