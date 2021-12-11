const VIDEO = atob(
  "GkXfo49CgoR3ZWJtQoeBAkKFgQIYU4BnI0nOEU2bdKpNu" +
    "4tTq4QVSalmU6yBL027i1OrhBZUrmtTrIGmTbuLU6uEHF" +
    "O7a1OsgdEVSalm8k2ApWxpYmVibWwyIHYwLjkuNyArIGx" +
    "pYm1hdHJvc2thMiB2MC45LjhXQZ5ta2NsZWFuIDAuMi41" +
    "IGZyb20gTGF2ZjUyLjU3LjFzpJC/CoyJjSbckmOytS9Se" +
    "y3cRImIQK9AAAAAAABEYYgEHiZDUAHwABZUrmumrqTXgQ" +
    "FzxYEBnIEAIrWcg3VuZIaFVl9WUDiDgQHgh7CCAUC6gfA" +
    "cU7trzbuMs4EAt4f3gQHxggEju42zggMgt4f3gQHxgrZd" +
    "u46zggZAt4j3gQHxgwFba7uOs4IJYLeI94EB8YMCAR+7j" +
    "rOCDIC3iPeBAfGDAqggH0O2dSBibueBAKNbiYEAAIDQdw" +
    "CdASpAAfAAAAcIhYWIhYSIAIIb347n/c/Z0BPBfjv7f+I" +
    "/6df275Wbh/XPuZ+qv9k58KrftA9tvkP+efiN/ovmd/if" +
    "9L/ef2z+Xv+H/rv+39xD/N/zL+nfid71363e9X+pf6/+q" +
    "+x7+W/0P/H/233Zf6n/Wv696APuDf0b+YdcL+2HsUfxr+" +
    "gfP/91P+F/yH9K+H79Z/7j/Xvhj/VjVsvwHXt/n/qz9U/" +
    "w749+c/g="
);

function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  if (aRequest.queryString.includes("result")) {
    aResponse.write(getState("hints") || 0);
    setState("hints", "0");
  } else {
    let hints = parseInt(getState("hints") || 0) + 1;
    setState("hints", hints.toString());

    aResponse.setHeader("Content-Type", "video/webm", false);
    aResponse.setHeader(
      "Cache-Control",
      "public, max-age=604800, immutable",
      false
    );
    aResponse.write(VIDEO);
  }
}
