---
layout: post
title: "☀️ Google Summer of Code: Fretboard - A/B Testing Framework for Android"
date: 2018-08-10 14:48:00 +0200
author: fernando
---

## Overview
Firefox for Android (codenamed Fennec) started using [KeepSafe's Switchboard library](https://github.com/KeepSafe/Switchboard) and server component in order to run A/B testing or staged rollout of features, but since then the code changed significantly. It used a special [switchboard server](https://github.com/mozilla-services/switchboard-server) that decided which experiments a client is part of and returned a simplified list for the client to consume. However, this required the client to send data (including a unique id) to the server.

To avoid this Firefox moved to using [Kinto](https://github.com/Kinto/kinto) as storage and server of the experiment configuration. Clients now download the whole list of experiments and decide locally what experiments they are enrolled in (the configuration for Fennec [looks like this](https://firefox.settings.services.mozilla.com/v1/buckets/fennec/collections/experiments/records)).

The purpose of this Google Summer of Code project, which is called Fretboard, was to develop an A/B testing framework written in Kotlin based on the existing code from Fennec, but making it independent of both the server used and the local storage mechanism, allowing it to be used on other apps, such as Firefox Focus, which started to need to do some A/B tests.

The source code implemented as part of this project is located at the [android-components](https://github.com/mozilla-mobile/android-components) GitHub repo, more specifically [here](https://github.com/mozilla-mobile/android-components/tree/main/components/service/fretboard)

## Features
This is a basic and non-exhaustive list of features, for all details you can view the README [here](https://github.com/mozilla-mobile/android-components/blob/main/components/service/fretboard/README.md)

* Query if a device is part of a specific experiment
* Get experiment associated metadata
* Override a specific experiment (force activate / deactivate it)
* Override device values (such as the appId, version, etc)
* Update experiment list from the server manually or using JobScheduler 
* Update experiment list using WorkManager (blocked waiting for a non-alpha version of WorkManager to be released by Google)
* Default source implementation for Kinto
    * Uses diff requests to reduce bandwith usage
    * Support for certificate pinning (pending security review)
    * Support for validating the signature of the downloaded experiment collection (pending security review)
* Default storage implementation using a flat JSON file

## Filters
Fretboard allows you to specify the following filters:
- Buckets: Every user is in one of 100 buckets (0-99). For every experiment you can set up a min and max value (0 <= min <= max <= 100). The bounds are [min, max).
    - Both max and min are optional. For example, specifying only min = 0 or only max = 100 includes all users
    - 0-100 includes all users (as opposed to 0-99)
    - 0-0 includes no users (as opposed to just bucket 0)
    - 0-1 includes just bucket 0
    - Users will always stay in the same bucket. An experiment targeting 0-25 will always target the same 25% of users
- appId (regex): The app ID (package name)
- version (regex): The app version
- country (regex): country, pulled from the default locale
- lang (regex): language, pulled from the default locale
- device (regex): Android device name
- manufacturer (regex): Android device manufacturer
- region: custom region, different from the one from the default locale (like a GeoIP, or something similar).
- release channel: release channel of the app (alpha, beta, etc)

## Merged commits
* [Issue #17: Run code quality tools: detekt, ktlint and codecov](https://github.com/mozilla-mobile/android-components/commit/cb4a3685f7ec3d79cf356fba7a95d04f5373c9f7)
* [Remove codecov uploading from taskcluster](https://github.com/mozilla-mobile/android-components/commit/79f49c7a61ee2560bb6e4233586d2b0fc10a978f)
* [Issue #20: Upload test coverage results to codecov](https://github.com/mozilla-mobile/android-components/commit/71a165d094fb777113f06d1e2ac27f2c082bba29)
* [Issue #3: Implement client for loading (partial) experiment configuration from server, Issue #4: Implement storage for saving experiment configuration to disk](https://github.com/mozilla-mobile/android-components/commit/7996c0c11a40939cac73914b83fc2ec578927596)
* [Make experiments variable private and fetch experiments from local storage when updating](https://github.com/mozilla-mobile/android-components/commit/63c9792d9c98b7b5cb12aa62fec792710fa116d0)
* [Make loadExperiments and updateExperiments synchronized and guard against storage file not found](https://github.com/mozilla-mobile/android-components/commit/cd812f3fb63b853d842c7af87c69587bec5f45d6)
* [Issue #5: Schedule frequent updates of experiments configuration](https://github.com/mozilla-mobile/android-components/commit/fcf13f7b794966f2f111ebaccc2a468465c72fff)
* [Issue #6: Implement code for evaluating experiment configuration and bucketing users](https://github.com/mozilla-mobile/android-components/commit/b4d9e41b749f34ac75cdb01e6c5150368fcdcba3)
* [Changed uuid type to String](https://github.com/mozilla-mobile/android-components/commit/b9057672604a675f7d9edddc5d6707e07cef0b78)
* [Add RegionProvider](https://github.com/mozilla-mobile/android-components/commit/af80d363dd7a522f43990550a7096aa5a68c9b3e)
* [Issue #7: Implement simple API for checking if an installation is part of a specific experiment](https://github.com/mozilla-mobile/android-components/commit/93b2fb9407926a7910b353265667938f2daaea63)
* [Issue #29: Add a more Kotlin idiomatic method for checking experiments](https://github.com/mozilla-mobile/android-components/commit/1c7ca693e29356d54f6bf354fe7506aab18dca80)
* [Issue #12: Implement simple API for getting experiment metadata](https://github.com/mozilla-mobile/android-components/commit/36b17e1e2d8d87992dab5b3e8d72e463032a827a)
* [Issue #32: JSONExperimentParserTest is not deterministic](https://github.com/mozilla-mobile/android-components/commit/57d91cfcce0f028a1a3523bc390966f99ad2caa5)
* [Issue #37: Add "export TERM=dumb" to taskcluster script](https://github.com/mozilla-mobile/android-components/commit/0c4ce5d8632c777d535aaeea4678c5cdac42380c)
* [Issue #38: detekt is configured to only run on 'fretboard' module](https://github.com/mozilla-mobile/android-components/commit/d5ebdda10e04d5429adcb0952d2838e38b7d8f89)
* [Issue #41: Add test for IOException on HttpURLConnectionHttpClient](https://github.com/mozilla-mobile/android-components/commit/267c633217b920d2d76d134884e439c7b5efdcb6)
* [Issue #14: Add mechanism for overriding the local experiment configuration](https://github.com/mozilla-mobile/android-components/commit/1f745bcfa8d879145f4514bf431ef9f5eb3978af)
* [Issue #46: Rename AtomicFileExperimentStorage to FlatFileExperimentStorage](https://github.com/mozilla-mobile/android-components/commit/3adeb6c7bc4071f90601b46adc0e418a6edb4828)
* [Issue #53: Change FlatFileExperimentStorage instrumentation tests to use File and remove temp file when done](https://github.com/mozilla-mobile/android-components/commit/e788b0c394ecbe7cfa788bd0628c75d64a85fd6d)
* [Issue #56: Rename FlatFileExperimentStorage package to flatfile](https://github.com/mozilla-mobile/android-components/commit/a824b5d4c0318ce865e3980e0723af6e8c462939)
* [Issue #50: Make FlatFileExperimentStorage receive a File](https://github.com/mozilla-mobile/android-components/commit/2e5103b1984156ad5cf8b3011813894dd25f516f)
* [Issue #432: Fretboard: Kinto delete diffs might lead to crash](https://github.com/mozilla-mobile/android-components/commit/ee164fc8121c1cb338be1e28c00b99d2077a7799)
* [Issue #435: Fretboard: Documentation and guides](https://github.com/mozilla-mobile/android-components/commit/ae40fbeee03009e836455604453bf32678fc06ca)
* [Issue #460: Fretboard: Update documentation](https://github.com/mozilla-mobile/android-components/commit/050b98f41743ccad319bdb8263b1067944971431)
* [Issue #456: Fretboard: Allow filtering by release channel](https://github.com/mozilla-mobile/android-components/commit/43b3938ee9386a33c34f658017f5ae289d2a3e97)
* [Issue #464: Let app access a list of experiments](https://github.com/mozilla-mobile/android-components/commit/5a999c4a482c7b6f6bb7641da9da6031ba4f4568)
* [Issue #466: Fretboard: Move JSONExtensions into support-ktx](https://github.com/mozilla-mobile/android-components/commit/0acf5a5ca79a743d768b5fe80fd04ba35ba5f5ec)
* [Issue #115: Core ping: Report experiments](https://github.com/mozilla-mobile/android-components/commit/0a22a10b5ee7aa081996992aa1567e9abca17105)
* [Issue #485: Remove List.toJSONArray extension method](https://github.com/mozilla-mobile/android-components/commit/c4005e7676050ca19ba8cbd8c7a4c0e37e15073b)
* [Issue #487: Fretboard: Add helper method to get active experiments](https://github.com/mozilla-mobile/android-components/commit/ed825a67c1f54724aa8577bdb4c5874e91968e36)
* [Issue #501: Move JSON extensions to package android.org.json](https://github.com/mozilla-mobile/android-components/commit/a230066b4b568fd2642b27c82f8417afe9abeb54)
* [Issue #504: Fretboard: Require network for JobScheduler-based scheduler](https://github.com/mozilla-mobile/android-components/commit/17630ffd5d8b64c2233a1f3f41f12f6ab73d1cf4)
* [Issue #526: fretboard: README: Explain buckets with examples](https://github.com/mozilla-mobile/android-components/commit/5db98276dd7aa935ab5bec86fba04a40b9ea302a)
* [Issue #524: Fretboard: Add kdoc to Experiment properties](https://github.com/mozilla-mobile/android-components/commit/b8b94f5d421717c0c488656d0838f3478d9f37ad)
* [Issue #542: Fretboard: Remove deleted RegionProvider from README](https://github.com/mozilla-mobile/android-components/commit/6e856a4ddde730058326f2eb4c2441f3c0e8cda9)
* [Issue #541: Fretboard: ExperimentDescriptor should use the experiment name instead of the id](https://github.com/mozilla-mobile/android-components/commit/a9d6ba5863c12411f6b5b9a3cf9bc7dc5900e806)
* [Issue #555: Fretboard: Add test for non HTTP url for HttpURLConnectionHttpClient](https://github.com/mozilla-mobile/android-components/commit/170b8743635f7e6c1cc5b133518d48a86df96271)
* [Issue #557: Fretboard: ExperimentEvaluator: Add tests for empty values and release channel](https://github.com/mozilla-mobile/android-components/commit/7c7bfe00b3bd9192c3b74c53506fa96ff1534dd7)
* [Issue #559: Fretboard: Add tests for ExperimentPayload](https://github.com/mozilla-mobile/android-components/commit/da483522ce9ecfea8b4c2bb00fcd772cbba3537d)
* [Issue #561: Fretboard: Add more tests for Fretboard class](https://github.com/mozilla-mobile/android-components/commit/6aa253a6bb6cd6f898cf96cba9e3973895a54dca)
* [Issue #563: Fretboard: Make kinto properties private](https://github.com/mozilla-mobile/android-components/commit/80d1bbf100448bccca64f1a84c881fc7e1a50795)
* [Issue #565: Fretboard: Handle JSON exceptions on Kinto](https://github.com/mozilla-mobile/android-components/commit/0ba81f51500a063a71b8db35540fb9aeab15218b)
* [Issue #570: Fretboard: More idiomatic Kotlin on FlatFileExperimentStorage and on ExperimentSerializer](https://github.com/mozilla-mobile/android-components/commit/40d000b7a8815cb6b8786e49b501f8e86bee7ef4)
* [Issue #572: Fretboard: Complete kdoc](https://github.com/mozilla-mobile/android-components/commit/c874c343cab18fe64f45ba8eb9a726f829fe06b9)
* [Issue #577: Fretboard: Pass original exception to ExperimentDownloadException](https://github.com/mozilla-mobile/android-components/pull/579)
* [Issue #576: Fretboard: Log ExperimentDownloadException](https://github.com/mozilla-mobile/android-components/pull/582)
* [Issue #590: Fretboard: Blog post](https://github.com/mozilla-mobile/android-components/commit/75a87f8774e2848769e7d18e0253b7bf49464d68)
* [Issue #434: Fretboard: Verify signatures of experiments collection](https://github.com/mozilla-mobile/android-components/commit/f65aad40e6f8ec84009a3f809f816c1c969c7082)

## Open pull requests
There are two open pull requests. The first one is open pending a review from the security team and the last one is waiting for a non-alpha version of WorkManager to be released by Google:

1. [Issue #433: Fretboard: Certificate pinning](https://github.com/mozilla-mobile/android-components/pull/446)
2. [Issue #493: Use WorkManager in fretboard](https://github.com/mozilla-mobile/android-components/pull/503)

## Project progress and difficulties faced
Prior to starting the project, I became familiar with Kinto and the diff response format, as well as with the existing code of the Switchboard fork from Fennec. I also thought it was a good idea to send a pull request to Firefox Focus because this library was going to be integrated into it, and also to become familiar with a code review process at Mozilla. I looked at the issue list and discovered a problem with display cutouts, so I sent two pull requests to address the issue: [the first one](https://github.com/mozilla-mobile/focus-android/pull/2264) and [the second one](https://github.com/mozilla-mobile/focus-android/pull/2278).

At the beginning of the project it became necessary to familiarize myself with tools like [Taskcluster](https://github.com/taskcluster), which I had never used (although I used similar tools like [Travis CI](https://travis-ci.org/)).

The most difficult pull requests for me were the ones related to certificate pinning and experiment collection signature verification. For the first one I had to broaden my knowledge about it, as well as research how to properly implement it on Android, avoiding common mistakes.
For the second one the most difficult part was to understand what algorithm Mozilla was using to validate the signatures, and how it worked. I discovered from the Kinto collection `mode` field that it was `p384ecdsa`, and then I had to research how to properly implement it in Kotlin. For this later I needed the help of Julien Vehent and Franziskus Kiefer, which pointed me to a [great talk](https://www.youtube.com/watch?v=b2kPo8YdLTw&t=0s&list=WL&index=2) and also a [Go](https://github.com/mozilla-services/autograph/blob/master/tools/autograph-monitor/contentsignature.go#L35) and [C++](https://searchfox.org/mozilla-central/source/security/manager/ssl/ContentSignatureVerifier.cpp) implementation. After seeing the two implementations I realized my solution wasn't working because I didn't know that the signature actually contained two values concatenated (r and s), which then needed to be encoded using [DER syntax](https://crypto.stackexchange.com/questions/1795/how-can-i-convert-a-der-ecdsa-signature-to-asn-1)

Overall I think I learned a lot doing this project and I really loved working with Mozilla.

## Extending Fretboard
Right now there is an [ongoing discussion](https://github.com/mozilla-mobile/android-components/issues/454) about enhancing Fretboard with an expression language (being that JEXL/CEL/etc) for the matchers values instead of regular expressions like it's using now.

## Thanks to
I would like to thank my mentor [Sebastian Kaspari](https://mozillians.org/en-US/u/sebastian.kaspari/) for all the help and guidance, for being so friendly and available to talk at any moment I needed, as well as reviewing my pull requests quickly.

I would also like to thank Franziskus Kiefer and Julien Vehent for helping me understand the signature validation system used by Kinto.

## Related links
* [Summer of Code project](https://summerofcode.withgoogle.com/projects/#6511592707981312)
* [Summer of Code proposal](https://summerofcode.withgoogle.com/serve/6444466999656448/)
