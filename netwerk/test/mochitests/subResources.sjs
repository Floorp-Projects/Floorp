const kTwoDays = 2 * 24 * 60 * 60;
const kInTwoDays = new Date().getTime() + kTwoDays * 1000;

function getDateInTwoDays() {
  let date2 = new Date(kInTwoDays);
  let days = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"];
  let months = [
    "Jan",
    "Feb",
    "Mar",
    "Apr",
    "May",
    "Jun",
    "Jul",
    "Aug",
    "Sep",
    "Oct",
    "Nov",
    "Dec",
  ];
  let day = date2.getUTCDate();
  if (day < 10) {
    day = "0" + day;
  }
  let month = months[date2.getUTCMonth()];
  let year = date2.getUTCFullYear();
  let hour = date2.getUTCHours();
  if (hour < 10) {
    hour = "0" + hour;
  }
  let minute = date2.getUTCMinutes();
  if (minute < 10) {
    minute = "0" + minute;
  }
  let second = date2.getUTCSeconds();
  if (second < 10) {
    second = "0" + second;
  }
  return (
    days[date2.getUTCDay()] +
    ", " +
    day +
    "-" +
    month +
    "-" +
    year +
    " " +
    hour +
    ":" +
    minute +
    ":" +
    second +
    " GMT"
  );
}

function handleRequest(aRequest, aResponse) {
  aResponse.setStatusLine(aRequest.httpVersion, 200);

  let suffix = " path=/; domain:.mochi.test";

  if (aRequest.queryString.includes("3")) {
    aResponse.setHeader(
      "Set-Cookie",
      "test3=value3; expires=Fri, 02-Jan-2037 00:00:01 GMT;" + suffix
    );
  } else if (aRequest.queryString.includes("4")) {
    let date2 = getDateInTwoDays();

    aResponse.setHeader(
      "Set-Cookie",
      "test4=value4; expires=" + date2 + ";" + suffix
    );
  }

  aResponse.setHeader("Content-Type", "text/javascript", false);
  aResponse.write("42;");
}
