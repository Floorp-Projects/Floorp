/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

export const kHandlerListVersion = 1;

export const kHandlerList = {
  default: {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
        ],
      },
    },
  },
  cs: {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Seznam",
            uriTemplate: "https://email.seznam.cz/newMessageScreen?mailto=%s",
          },
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
        ],
      },
    },
  },
  "es-CL": {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
          {
            name: "Outlook",
            uriTemplate:
              "https://outlook.live.com/default.aspx?rru=compose&to=%s",
          },
        ],
      },
    },
  },
  "ja-JP-mac": {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Yahoo!メール",
            uriTemplate: "https://mail.yahoo.co.jp/compose/?To=%s",
          },
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
        ],
      },
    },
  },
  ja: {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Yahoo!メール",
            uriTemplate: "https://mail.yahoo.co.jp/compose/?To=%s",
          },
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
        ],
      },
    },
  },
  kk: {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Яндекс.Почта",
            uriTemplate: "https://mail.yandex.ru/compose?mailto=%s",
          },
          {
            name: "Mail.Ru",
            uriTemplate: "https://e.mail.ru/cgi-bin/sentmsg?mailto=%s",
          },
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
        ],
      },
    },
  },
  ltg: {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
          {
            name: "inbox.lv mail",
            uriTemplate: "https://mail.inbox.lv/compose?to=%s",
          },
        ],
      },
    },
  },
  lv: {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
          {
            name: "inbox.lv mail",
            uriTemplate: "https://mail.inbox.lv/compose?to=%s",
          },
        ],
      },
    },
  },
  pl: {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Poczta Interia.pl",
            uriTemplate: "https://poczta.interia.pl/mh/?mailto=%s",
          },
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
        ],
      },
    },
  },
  ru: {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Яндекс.Почту",
            uriTemplate: "https://mail.yandex.ru/compose?mailto=%s",
          },
          {
            name: "Mail.Ru",
            uriTemplate: "https://e.mail.ru/cgi-bin/sentmsg?mailto=%s",
          },
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
        ],
      },
    },
  },
  uk: {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
          {
            name: "Outlook",
            uriTemplate:
              "https://outlook.live.com/default.aspx?rru=compose&to=%s",
          },
        ],
      },
    },
  },
  uz: {
    schemes: {
      mailto: {
        handlers: [
          {
            name: "Gmail",
            uriTemplate: "https://mail.google.com/mail/?extsrc=mailto&url=%s",
          },
          {
            name: "Mail.Ru",
            uriTemplate: "https://e.mail.ru/cgi-bin/sentmsg?mailto=%s",
          },
        ],
      },
    },
  },
};
