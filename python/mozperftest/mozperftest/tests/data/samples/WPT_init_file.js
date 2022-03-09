// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.
/* eslint-env node */
"use strict";

module.exports = {
  setUp,
  tearDown,
  test,
  owner: "Performance Testing Team",
  name: "WebpageTest Parameter list",
  description: "These are the arguments used when running the webpagetest Layer",
  options: {
        test_parameters : {
            location: "ec2-us-west-1",
            browser: "Firefox",
            connection: "Cable",
            timeout_limit: 21600,
            wait_between_requests: 5,
            statistics: ["average", "median", "standardDeviation"],
            label: "",
            runs: 3,
            fvonly: 0,
            private: 1,
            web10: 0,
            script: "",
            block: "",
            video: 1,
            tcpdump: 0,
            noimages: 0,
            keepua: 1,
            uastring: "",
            htmlbody: 0,
            custom: "",
            ignoreSSL: 0,
            appendua: "",
            injectScript: "",
            disableAVIF: 0,
            disableWEBP: 0,
            disableJXL: 0,
        },
        test_list: ["Google.com", "Youtube.com", "Facebook.com", "Qq.com",
         "Baidu.com", "Sohu.com",  "360.cn", "Jd.com", "Amazon.com", "Yahoo.com", "Zoom.us",
         "Weibo.com", "Sina.com.cn", "Live.com", "Reddit.com", "Netflix.com",
         "Microsoft.com", "Instagram.com", "Panda.tv", "Google.com.hk", "Csdn.net",
         "Bing.com", "Vk.com", "Yahoo.co.jp", "Twitter.com", "Naver.com", "Canva.com", "Ebay.com",
         "Force.com", "Amazon.in", "Adobe.com", "Aliexpress.com", "Linkedin.com", "Tianya.cn",
         "Yy.com", "Huanqiu.com", "Amazon.co.jp", "Okezone.com"]
    },
};
