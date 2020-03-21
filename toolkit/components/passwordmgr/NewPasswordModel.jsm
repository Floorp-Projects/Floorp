/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Machine learning model for identifying new password input elements
 * using Fathom.
 */

const EXPORTED_SYMBOLS = ["NewPasswordModel"];

ChromeUtils.defineModuleGetter(
  this,
  "fathom",
  "resource://gre/modules/third_party/fathom/fathom.jsm"
);

const {
  dom,
  element,
  out,
  rule,
  ruleset,
  score,
  type,
  utils: { identity, isVisible, min },
  clusters: { euclidean },
} = fathom;

/**
 * ----- Start of model -----
 *
 * Everything below this comment up to the "End of model" comment is copied from:
 * https://github.com/mozilla-services/fathom-login-forms/blob/bff6995c32e86b6b5810ff013632f68db8747346/new-password/rulesets.js#L6-L272
 * Deviations from that file:
 *   - Remove import statements, instead using ``ChromeUtils.defineModuleGetter`` and destructuring assignments above.
 *   - Set ``DEVELOPMENT`` constant to ``false``.
 */

// Whether this is running in the Vectorizer, rather than in-application, in a
// privileged Chrome context
const DEVELOPMENT = false;

const coefficients = {
  new: [
    ["hasPasswordLabel", 2.6237754821777344],
    ["hasNewLabel", 0.8804616928100586],
    ["hasConfirmLabel", 2.19775390625],
    ["hasConfirmEmailLabel", -2.51192307472229],
    ["closestLabelMatchesPassword", 0.8376041054725647],
    ["closestLabelMatchesNew", -0.3952447175979614],
    ["closestLabelMatchesConfirm", 1.6602904796600342],
    ["closestLabelMatchesConfirmEmail", -2.828238010406494],
    ["hasPasswordAriaLabel", 1.3898684978485107],
    ["hasNewAriaLabel", 0.25765886902809143],
    ["hasConfirmAriaLabel", 3.3060154914855957],
    ["hasPasswordPlaceholder", 1.9736918210983276],
    ["hasNewPlaceholder", 0.20559890568256378],
    ["hasConfirmPlaceholder", 3.1848690509796143],
    ["hasConfirmEmailPlaceholder", 0.07635760307312012],
    ["forgotPasswordInFormLinkInnerText", -2.125054359436035],
    ["forgotPasswordInFormLinkHref", -2.310042381286621],
    ["forgotPasswordInFormLinkTitle", -3.322892427444458],
    ["forgotPasswordInFormButtonInnerText", -4.083425521850586],
    ["forgotPasswordOnPageLinkInnerText", -0.851978600025177],
    ["forgotPasswordOnPageLinkHref", -1.1582040786743164],
    ["forgotPasswordOnPageLinkTitle", -1.5192012786865234],
    ["forgotPasswordOnPageButtonInnerText", 1.4093186855316162],
    ["idIsPassword1Or2", 1.994940161705017],
    ["nameIsPassword1Or2", 3.0895347595214844],
    ["idMatchesPassword", -0.8498945236206055],
    ["nameMatchesPassword", 0.9636800289154053],
    ["idMatchesPasswordy", 2.0097758769989014],
    ["nameMatchesPasswordy", 3.0705039501190186],
    ["classMatchesPasswordy", -0.2314695119857788],
    ["idMatchesLogin", -2.941408157348633],
    ["nameMatchesLogin", 0.9627101421356201],
    ["classMatchesLogin", -3.4055135250091553],
    ["containingFormHasLoginId", -2.660820245742798],
    ["formButtonIsRegistery", -0.07571711391210556],
    ["formButtonIsLoginy", -3.7707366943359375],
    ["hasAutocompleteCurrentPassword", -0.029503464698791504],
  ],
};

const biases = [["new", -3.197509765625]];

const passwordRegex = /password|passwort|رمز عبور|mot de passe|パスワード|비밀번호|암호|wachtwoord|senha|Пароль|parol|密码|contraseña|heslo/i;
const newRegex = /erstellen|create|choose|設定|신규/i;
const confirmRegex = /wiederholen|wiederholung|confirm|repeat|confirmation|verify|retype|repite|確認|の確認|تکرار|re-enter|확인|bevestigen|confirme|Повторите|tassyklamak|再次输入/i;
const emailRegex = /e-mail|email|ایمیل|メールアドレス|이메일|邮箱/i;
const forgotPasswordInnerTextRegex = /vergessen|forgot|oublié|dimenticata|Esqueceu|esqueci|Забыли|忘记|找回|Zapomenuté|lost|忘れた|忘れられた|忘れの方|재설정|찾기/i;
const forgotPasswordHrefRegex = /forgot|reset|recovery|change|lost|reminder|find/i;
const password1Or2Regex = /password1|password2/i;
const passwordyRegex = /pw|pwd|passwd|pass/i;
const loginRegex = /login|Войти|sign in|ورود|登录|Přihlásit se|Авторизоваться|signin|log in|sign\/in|sign-in|entrar|ログインする|로그인/i;
const registerButtonRegex = /create account|Zugang anlegen|Angaben prüfen|Konto erstellen|register|sign up|create an account|create my account|ثبت نام|登録|Cadastrar|Зарегистрироваться|Bellige alynmak/i;

function makeRuleset(coeffs, biases) {
  /**
   * Don't bother with the fairly expensive isVisible() call when we're in
   * production. We fire only when the user clicks an <input> field. They can't
   * very well click an invisible one.
   */
  function isVisibleInDev(fnodeOrElement) {
    return DEVELOPMENT ? isVisible(fnodeOrElement) : true;
  }

  function hasConfirmLabel(fnode) {
    return hasLabelMatchingRegex(fnode.element, confirmRegex);
  }

  function hasConfirmEmailLabel(fnode) {
    return (
      hasConfirmLabel(fnode) && hasLabelMatchingRegex(fnode.element, emailRegex)
    );
  }

  function hasLabelMatchingRegex(element, regex) {
    // Check element.labels
    const labels = element.labels;
    // TODO: Should I be concerned with multiple labels?
    if (labels !== null && labels.length) {
      return regex.test(labels[0].innerText);
    }

    // Check element.aria-labelledby
    let labelledBy = element.getAttribute("aria-labelledby");
    if (labelledBy !== null) {
      labelledBy = labelledBy
        .split(" ")
        .map(id => element.ownerDocument.getElementById(id));
      if (labelledBy.length === 1) {
        return regex.test(labelledBy[0].innerText);
      } else if (labelledBy.length > 1) {
        return regex.test(
          min(labelledBy, node => euclidean(node, element)).innerText
        );
      }
    }

    const parentElement = element.parentElement;
    // Check if the input is in a <td>, and, if so, check the innerText of the containing <tr>
    if (parentElement.tagName === "TD") {
      // TODO: How bad is the assumption that the <tr> won't be the parent of the <td>?
      return regex.test(parentElement.parentElement.innerText);
    }

    // Check if the input is in a <dd>, and, if so, check the innerText of the preceding <dt>
    if (parentElement.tagName === "DD") {
      return regex.test(parentElement.previousElementSibling.innerText);
    }
    return false;
  }

  function closestLabelMatchesConfirm(fnode) {
    return closestLabelMatchesRegex(fnode.element, confirmRegex);
  }

  function closestLabelMatchesConfirmEmail(fnode) {
    return (
      closestLabelMatchesConfirm(fnode) &&
      closestLabelMatchesRegex(fnode.element, emailRegex)
    );
  }

  function closestLabelMatchesRegex(element, regex) {
    const previousElementSibling = element.previousElementSibling;
    if (
      previousElementSibling !== null &&
      previousElementSibling.tagName === "LABEL"
    ) {
      return regex.test(previousElementSibling.innerText);
    }

    const nextElementSibling = element.nextElementSibling;
    if (nextElementSibling !== null && nextElementSibling.tagName === "LABEL") {
      return regex.test(nextElementSibling.innerText);
    }

    const closestLabelWithinForm = closestSelectorElementWithinElement(
      element,
      element.form,
      "label"
    );
    return containsRegex(
      regex,
      closestLabelWithinForm,
      closestLabelWithinForm => closestLabelWithinForm.innerText
    );
  }

  function containsRegex(regex, thingOrNull, thingToString = identity) {
    return thingOrNull !== null && regex.test(thingToString(thingOrNull));
  }

  function closestSelectorElementWithinElement(
    toElement,
    withinElement,
    querySelector
  ) {
    if (withinElement !== null) {
      let nodeList = Array.from(withinElement.querySelectorAll(querySelector));
      if (nodeList.length) {
        return min(nodeList, node => euclidean(node, toElement));
      }
    }
    return null;
  }

  function hasAriaLabelMatchingRegex(element, regex) {
    return containsRegex(regex, element.getAttribute("aria-label"));
  }

  function hasConfirmPlaceholder(fnode) {
    return hasPlaceholderMatchingRegex(fnode.element, confirmRegex);
  }

  function hasConfirmEmailPlaceholder(fnode) {
    return (
      hasConfirmPlaceholder(fnode) &&
      hasPlaceholderMatchingRegex(fnode.element, emailRegex)
    );
  }

  function hasPlaceholderMatchingRegex(element, regex) {
    return containsRegex(regex, element.getAttribute("placeholder"));
  }

  function forgotPasswordInAnchorPropertyWithinElement(
    property,
    element,
    ...regexes
  ) {
    return hasAnchorMatchingPredicateWithinElement(element, anchor => {
      const propertyValue = anchor[property];
      return regexes.every(regex => regex.test(propertyValue));
    });
  }

  function hasAnchorMatchingPredicateWithinElement(element, matchingPredicate) {
    if (element !== null) {
      const anchors = Array.from(element.querySelectorAll("a"));
      return anchors.some(matchingPredicate);
    }
    return false;
  }

  function forgotPasswordButtonWithinElement(element) {
    if (element !== null) {
      const buttons = Array.from(element.querySelectorAll("button"));
      return buttons.some(button => {
        const innerText = button.innerText;
        return (
          passwordRegex.test(innerText) &&
          forgotPasswordInnerTextRegex.test(innerText)
        );
      });
    }
    return false;
  }

  function containingFormHasLoginId(fnode) {
    const form = fnode.element.form;
    return containsRegex(loginRegex, form, form => form.id);
  }

  function testFormButtonsAgainst(element, stringRegex) {
    const form = element.form;
    if (form !== null) {
      let inputs = Array.from(
        form.querySelectorAll("input[type=submit],input[type=button]")
      );
      inputs = inputs.filter(input => {
        return stringRegex.test(input.value);
      });
      if (inputs.length) {
        return true;
      }

      let buttons = Array.from(form.querySelectorAll("button"));
      return buttons.some(button => {
        return (
          stringRegex.test(button.value) ||
          stringRegex.test(button.innerText) ||
          stringRegex.test(button.id)
        );
      });
    }
    return false;
  }

  function hasAutocompleteCurrentPassword(fnode) {
    return fnode.element.autocomplete === "current-password";
  }

  return ruleset(
    [
      rule(
        (DEVELOPMENT ? dom : element)(
          'input[type=text],input[type=password],input[type=""],input:not([type])'
        ).when(isVisibleInDev),
        type("new")
      ),
      rule(
        type("new"),
        score(fnode => hasLabelMatchingRegex(fnode.element, passwordRegex)),
        { name: "hasPasswordLabel" }
      ),
      rule(
        type("new"),
        score(fnode => hasLabelMatchingRegex(fnode.element, newRegex)),
        { name: "hasNewLabel" }
      ),
      rule(type("new"), score(hasConfirmLabel), { name: "hasConfirmLabel" }),
      rule(type("new"), score(hasConfirmEmailLabel), {
        name: "hasConfirmEmailLabel",
      }),
      rule(
        type("new"),
        score(fnode => closestLabelMatchesRegex(fnode.element, passwordRegex)),
        { name: "closestLabelMatchesPassword" }
      ),
      rule(
        type("new"),
        score(fnode => closestLabelMatchesRegex(fnode.element, newRegex)),
        { name: "closestLabelMatchesNew" }
      ),
      rule(type("new"), score(closestLabelMatchesConfirm), {
        name: "closestLabelMatchesConfirm",
      }),
      rule(type("new"), score(closestLabelMatchesConfirmEmail), {
        name: "closestLabelMatchesConfirmEmail",
      }),
      rule(
        type("new"),
        score(fnode => hasAriaLabelMatchingRegex(fnode.element, passwordRegex)),
        { name: "hasPasswordAriaLabel" }
      ),
      rule(
        type("new"),
        score(fnode => hasAriaLabelMatchingRegex(fnode.element, newRegex)),
        { name: "hasNewAriaLabel" }
      ),
      rule(
        type("new"),
        score(fnode => hasAriaLabelMatchingRegex(fnode.element, confirmRegex)),
        { name: "hasConfirmAriaLabel" }
      ),
      rule(
        type("new"),
        score(fnode =>
          hasPlaceholderMatchingRegex(fnode.element, passwordRegex)
        ),
        { name: "hasPasswordPlaceholder" }
      ),
      rule(
        type("new"),
        score(fnode => hasPlaceholderMatchingRegex(fnode.element, newRegex)),
        { name: "hasNewPlaceholder" }
      ),
      rule(type("new"), score(hasConfirmPlaceholder), {
        name: "hasConfirmPlaceholder",
      }),
      rule(type("new"), score(hasConfirmEmailPlaceholder), {
        name: "hasConfirmEmailPlaceholder",
      }),
      rule(
        type("new"),
        score(fnode =>
          forgotPasswordInAnchorPropertyWithinElement(
            "innerText",
            fnode.element.form,
            passwordRegex,
            forgotPasswordInnerTextRegex
          )
        ),
        { name: "forgotPasswordInFormLinkInnerText" }
      ),
      rule(
        type("new"),
        score(fnode =>
          forgotPasswordInAnchorPropertyWithinElement(
            "href",
            fnode.element.form,
            new RegExp(passwordRegex.source + "|" + passwordyRegex.source, "i"),
            forgotPasswordHrefRegex
          )
        ),
        { name: "forgotPasswordInFormLinkHref" }
      ),
      rule(
        type("new"),
        score(fnode =>
          forgotPasswordInAnchorPropertyWithinElement(
            "title",
            fnode.element.form,
            passwordRegex,
            forgotPasswordInnerTextRegex
          )
        ),
        { name: "forgotPasswordInFormLinkTitle" }
      ),
      rule(
        type("new"),
        score(fnode => forgotPasswordButtonWithinElement(fnode.element.form)),
        { name: "forgotPasswordInFormButtonInnerText" }
      ),
      rule(
        type("new"),
        score(fnode =>
          forgotPasswordInAnchorPropertyWithinElement(
            "innerText",
            fnode.element.ownerDocument,
            passwordRegex,
            forgotPasswordInnerTextRegex
          )
        ),
        { name: "forgotPasswordOnPageLinkInnerText" }
      ),
      rule(
        type("new"),
        score(fnode =>
          forgotPasswordInAnchorPropertyWithinElement(
            "href",
            fnode.element.ownerDocument,
            new RegExp(passwordRegex.source + "|" + passwordyRegex.source, "i"),
            forgotPasswordHrefRegex
          )
        ),
        { name: "forgotPasswordOnPageLinkHref" }
      ),
      rule(
        type("new"),
        score(fnode =>
          forgotPasswordInAnchorPropertyWithinElement(
            "title",
            fnode.element.ownerDocument,
            passwordRegex,
            forgotPasswordInnerTextRegex
          )
        ),
        { name: "forgotPasswordOnPageLinkTitle" }
      ),
      rule(
        type("new"),
        score(fnode =>
          forgotPasswordButtonWithinElement(fnode.element.ownerDocument)
        ),
        { name: "forgotPasswordOnPageButtonInnerText" }
      ),
      rule(
        type("new"),
        score(fnode => password1Or2Regex.test(fnode.element.id)),
        { name: "idIsPassword1Or2" }
      ),
      rule(
        type("new"),
        score(fnode => password1Or2Regex.test(fnode.element.name)),
        { name: "nameIsPassword1Or2" }
      ),
      rule(
        type("new"),
        score(fnode => passwordRegex.test(fnode.element.id)),
        { name: "idMatchesPassword" }
      ),
      rule(
        type("new"),
        score(fnode => passwordRegex.test(fnode.element.name)),
        { name: "nameMatchesPassword" }
      ),
      rule(
        type("new"),
        score(fnode => passwordyRegex.test(fnode.element.id)),
        { name: "idMatchesPasswordy" }
      ),
      rule(
        type("new"),
        score(fnode => passwordyRegex.test(fnode.element.name)),
        { name: "nameMatchesPasswordy" }
      ),
      rule(
        type("new"),
        score(fnode => passwordyRegex.test(fnode.element.className)),
        { name: "classMatchesPasswordy" }
      ),
      rule(
        type("new"),
        score(fnode => loginRegex.test(fnode.element.id)),
        { name: "idMatchesLogin" }
      ),
      rule(
        type("new"),
        score(fnode => loginRegex.test(fnode.element.name)),
        { name: "nameMatchesLogin" }
      ),
      rule(
        type("new"),
        score(fnode => loginRegex.test(fnode.element.className)),
        { name: "classMatchesLogin" }
      ),
      rule(type("new"), score(containingFormHasLoginId), {
        name: "containingFormHasLoginId",
      }),
      rule(
        type("new"),
        score(fnode =>
          testFormButtonsAgainst(fnode.element, registerButtonRegex)
        ),
        { name: "formButtonIsRegistery" }
      ),
      rule(
        type("new"),
        score(fnode => testFormButtonsAgainst(fnode.element, loginRegex)),
        { name: "formButtonIsLoginy" }
      ),
      rule(type("new"), score(hasAutocompleteCurrentPassword), {
        name: "hasAutocompleteCurrentPassword",
      }),
      rule(type("new"), out("new")),
    ],
    coeffs,
    biases
  );
}

/*
 * ----- End of model -----
 */

this.NewPasswordModel = {
  type: "new",
  rules: makeRuleset([...coefficients.new], biases),
};
