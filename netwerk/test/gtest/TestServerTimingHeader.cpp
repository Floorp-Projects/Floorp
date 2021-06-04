#include "gtest/gtest.h"

#include "mozilla/Unused.h"
#include "mozilla/net/nsServerTiming.h"
#include <string>
#include <vector>

using namespace mozilla;
using namespace mozilla::net;

void testServerTimingHeader(
    const char* headerValue,
    std::vector<std::vector<std::string>> expectedResults) {
  nsAutoCString header(headerValue);
  ServerTimingParser parser(header);
  parser.Parse();

  nsTArray<nsCOMPtr<nsIServerTiming>> results =
      parser.TakeServerTimingHeaders();

  ASSERT_EQ(results.Length(), expectedResults.size());

  unsigned i = 0;
  for (const auto& header : results) {
    std::vector<std::string> expectedResult(expectedResults[i++]);
    nsCString name;
    mozilla::Unused << header->GetName(name);
    ASSERT_TRUE(name.Equals(expectedResult[0].c_str()));

    double duration;
    mozilla::Unused << header->GetDuration(&duration);
    ASSERT_EQ(duration, atof(expectedResult[1].c_str()));

    nsCString description;
    mozilla::Unused << header->GetDescription(description);
    ASSERT_TRUE(description.Equals(expectedResult[2].c_str()));
  }
}

TEST(TestServerTimingHeader, HeaderParsing)
{
  // Test cases below are copied from
  // https://cs.chromium.org/chromium/src/third_party/WebKit/Source/platform/network/HTTPParsersTest.cpp

  testServerTimingHeader("", {});
  testServerTimingHeader("metric", {{"metric", "0", ""}});
  testServerTimingHeader("metric;dur", {{"metric", "0", ""}});
  testServerTimingHeader("metric;dur=123.4", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric;dur=\"123.4\"", {{"metric", "123.4", ""}});

  testServerTimingHeader("metric;desc", {{"metric", "0", ""}});
  testServerTimingHeader("metric;desc=description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric;desc=\"description\"",
                         {{"metric", "0", "description"}});

  testServerTimingHeader("metric;dur;desc", {{"metric", "0", ""}});
  testServerTimingHeader("metric;dur=123.4;desc", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric;dur;desc=description",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric;dur=123.4;desc=description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric;desc;dur", {{"metric", "0", ""}});
  testServerTimingHeader("metric;desc;dur=123.4", {{"metric", "123.4", ""}});
  testServerTimingHeader("metric;desc=description;dur",
                         {{"metric", "0", "description"}});
  testServerTimingHeader("metric;desc=description;dur=123.4",
                         {{"metric", "123.4", "description"}});

  // special chars in name
  testServerTimingHeader("aB3!#$%&'*+-.^_`|~",
                         {{"aB3!#$%&'*+-.^_`|~", "0", ""}});

  // delimiter chars in quoted description
  testServerTimingHeader("metric;desc=\"descr;,=iption\";dur=123.4",
                         {{"metric", "123.4", "descr;,=iption"}});

  // whitespace
  testServerTimingHeader("metric ; ", {{"metric", "0", ""}});
  testServerTimingHeader("metric , ", {{"metric", "0", ""}});
  testServerTimingHeader("metric ; dur = 123.4 ; desc = description",
                         {{"metric", "123.4", "description"}});
  testServerTimingHeader("metric ; desc = description ; dur = 123.4",
                         {{"metric", "123.4", "description"}});

  // multiple entries
  testServerTimingHeader(
      "metric1;dur=12.3;desc=description1,metric2;dur=45.6;"
      "desc=description2,metric3;dur=78.9;desc=description3",
      {{"metric1", "12.3", "description1"},
       {"metric2", "45.6", "description2"},
       {"metric3", "78.9", "description3"}});
  testServerTimingHeader("metric1,metric2 ,metric3, metric4 , metric5",
                         {{"metric1", "0", ""},
                          {"metric2", "0", ""},
                          {"metric3", "0", ""},
                          {"metric4", "0", ""},
                          {"metric5", "0", ""}});

  // quoted-strings
  // metric;desc=\ --> ''
  testServerTimingHeader("metric;desc=\\", {{"metric", "0", ""}});
  // metric;desc=" --> ''
  testServerTimingHeader("metric;desc=\"", {{"metric", "0", ""}});
  // metric;desc=\\ --> ''
  testServerTimingHeader("metric;desc=\\\\", {{"metric", "0", ""}});
  // metric;desc=\" --> ''
  testServerTimingHeader("metric;desc=\\\"", {{"metric", "0", ""}});
  // metric;desc="\ --> ''
  testServerTimingHeader("metric;desc=\"\\", {{"metric", "0", ""}});
  // metric;desc="" --> ''
  testServerTimingHeader("metric;desc=\"\"", {{"metric", "0", ""}});
  // metric;desc=\\\ --> ''
  testServerTimingHeader("metric;desc=\\\\\\", {{"metric", "0", ""}});
  // metric;desc=\\" --> ''
  testServerTimingHeader("metric;desc=\\\\\"", {{"metric", "0", ""}});
  // metric;desc=\"\ --> ''
  testServerTimingHeader("metric;desc=\\\"\\", {{"metric", "0", ""}});
  // metric;desc=\"" --> ''
  testServerTimingHeader("metric;desc=\\\"\"", {{"metric", "0", ""}});
  // metric;desc="\\ --> ''
  testServerTimingHeader("metric;desc=\"\\\\", {{"metric", "0", ""}});
  // metric;desc="\" --> ''
  testServerTimingHeader("metric;desc=\"\\\"", {{"metric", "0", ""}});
  // metric;desc=""\ --> ''
  testServerTimingHeader("metric;desc=\"\"\\", {{"metric", "0", ""}});
  // metric;desc=""" --> ''
  testServerTimingHeader("metric;desc=\"\"\"", {{"metric", "0", ""}});
  // metric;desc=\\\\ --> ''
  testServerTimingHeader("metric;desc=\\\\\\\\", {{"metric", "0", ""}});
  // metric;desc=\\\" --> ''
  testServerTimingHeader("metric;desc=\\\\\\\"", {{"metric", "0", ""}});
  // metric;desc=\\"\ --> ''
  testServerTimingHeader("metric;desc=\\\\\"\\", {{"metric", "0", ""}});
  // metric;desc=\\"" --> ''
  testServerTimingHeader("metric;desc=\\\\\"\"", {{"metric", "0", ""}});
  // metric;desc=\"\\ --> ''
  testServerTimingHeader("metric;desc=\\\"\\\\", {{"metric", "0", ""}});
  // metric;desc=\"\" --> ''
  testServerTimingHeader("metric;desc=\\\"\\\"", {{"metric", "0", ""}});
  // metric;desc=\""\ --> ''
  testServerTimingHeader("metric;desc=\\\"\"\\", {{"metric", "0", ""}});
  // metric;desc=\""" --> ''
  testServerTimingHeader("metric;desc=\\\"\"\"", {{"metric", "0", ""}});
  // metric;desc="\\\ --> ''
  testServerTimingHeader("metric;desc=\"\\\\\\", {{"metric", "0", ""}});
  // metric;desc="\\" --> '\'
  testServerTimingHeader("metric;desc=\"\\\\\"", {{"metric", "0", "\\"}});
  // metric;desc="\"\ --> ''
  testServerTimingHeader("metric;desc=\"\\\"\\", {{"metric", "0", ""}});
  // metric;desc="\"" --> '"'
  testServerTimingHeader("metric;desc=\"\\\"\"", {{"metric", "0", "\""}});
  // metric;desc=""\\ --> ''
  testServerTimingHeader("metric;desc=\"\"\\\\", {{"metric", "0", ""}});
  // metric;desc=""\" --> ''
  testServerTimingHeader("metric;desc=\"\"\\\"", {{"metric", "0", ""}});
  // metric;desc="""\ --> ''
  testServerTimingHeader("metric;desc=\"\"\"\\", {{"metric", "0", ""}});
  // metric;desc="""" --> ''
  testServerTimingHeader("metric;desc=\"\"\"\"", {{"metric", "0", ""}});

  // duplicate entry names
  testServerTimingHeader(
      "metric;dur=12.3;desc=description1,metric;dur=45.6;"
      "desc=description2",
      {{"metric", "12.3", "description1"}, {"metric", "45.6", "description2"}});

  // non-numeric durations
  testServerTimingHeader("metric;dur=foo", {{"metric", "0", ""}});
  testServerTimingHeader("metric;dur=\"foo\"", {{"metric", "0", ""}});

  // unrecognized param names
  testServerTimingHeader(
      "metric;foo=bar;desc=description;foo=bar;dur=123.4;foo=bar",
      {{"metric", "123.4", "description"}});

  // duplicate param names
  testServerTimingHeader("metric;dur=123.4;dur=567.8",
                         {{"metric", "123.4", ""}});
  testServerTimingHeader("metric;desc=description1;desc=description2",
                         {{"metric", "0", "description1"}});
  testServerTimingHeader("metric;dur=foo;dur=567.8", {{"metric", "", ""}});

  // unspecified param values
  testServerTimingHeader("metric;dur;dur=123.4", {{"metric", "0", ""}});
  testServerTimingHeader("metric;desc;desc=description", {{"metric", "0", ""}});

  // param name case
  testServerTimingHeader("metric;DuR=123.4;DeSc=description",
                         {{"metric", "123.4", "description"}});

  // nonsense
  testServerTimingHeader("metric=foo;dur;dur=123.4,metric2",
                         {{"metric", "0", ""}, {"metric2", "0", ""}});
  testServerTimingHeader("metric\"foo;dur;dur=123.4,metric2",
                         {{"metric", "0", ""}});

  // nonsense - return zero entries
  testServerTimingHeader(" ", {});
  testServerTimingHeader("=", {});
  testServerTimingHeader("[", {});
  testServerTimingHeader("]", {});
  testServerTimingHeader(";", {});
  testServerTimingHeader(",", {});
  testServerTimingHeader("=;", {});
  testServerTimingHeader(";=", {});
  testServerTimingHeader("=,", {});
  testServerTimingHeader(",=", {});
  testServerTimingHeader(";,", {});
  testServerTimingHeader(",;", {});
  testServerTimingHeader("=;,", {});

  // Invalid token
  testServerTimingHeader("met=ric", {{"met", "0", ""}});
  testServerTimingHeader("met ric", {{"met", "0", ""}});
  testServerTimingHeader("met[ric", {{"met", "0", ""}});
  testServerTimingHeader("met]ric", {{"met", "0", ""}});
  testServerTimingHeader("metric;desc=desc=123, metric2",
                         {{"metric", "0", "desc"}, {"metric2", "0", ""}});
  testServerTimingHeader("met ric;desc=de sc  , metric2",
                         {{"met", "0", "de"}, {"metric2", "0", ""}});

  // test cases from https://w3c.github.io/server-timing/#examples
  testServerTimingHeader(
      " miss, ,db;dur=53, app;dur=47.2 ",
      {{"miss", "0", ""}, {"db", "53", ""}, {"app", "47.2", ""}});
  testServerTimingHeader(" customView, dc;desc=atl ",
                         {{"customView", "0", ""}, {"dc", "0", "atl"}});
  testServerTimingHeader(" total;dur=123.4 ", {{"total", "123.4", ""}});

  // test cases for comma in quoted string
  testServerTimingHeader(
      "     metric ; desc=\"descr\\\"\\\";,=iption\";dur=123.4",
      {{"metric", "123.4", "descr\"\";,=iption"}});
  testServerTimingHeader(
      " metric2;dur=\"123.4\";;desc=\",;\\\",;,\";;,  metric  ;  desc = \" "
      "\\\", ;\\\" \"; dur=123.4,",
      {{"metric2", "123.4", ",;\",;,"}, {"metric", "123.4", " \", ;\" "}});
}
