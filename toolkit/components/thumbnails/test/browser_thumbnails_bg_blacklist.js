/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function checkBlacklist(url, expect, desc) {
  is(BackgroundPageThumbs.isBlacklistedUrl(url), expect, `${url} blacklisted? ${desc}`);
}

function runTests() {
  checkBlacklist("invalid", false, "invalid url");
  checkBlacklist("https://tv", false, "no host");
  checkBlacklist("http://example.com", false, "not blacklisted");

  checkBlacklist("https://www.twitch.tv", true, "yes blacklisted");
  checkBlacklist("https://twitch.tv", true, "without subdomain");
  checkBlacklist("https://go.twitch.tv", true, "with other subdomain");
  checkBlacklist("https://foo.bar.go.twitch.tv", true, "with more subdomains");
  checkBlacklist("http://go.twitch.tv", true, "with http protocol");
  checkBlacklist("https://go.twitch.tv/somepage", true, "with path");
}
