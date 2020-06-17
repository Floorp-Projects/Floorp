module.exports = async function(context, commands) {
  await commands.navigate("https://www.example.com");
  await commands.wait.byTime(15000);

  // Fill in:
  let username = "some_user@mail.com";
  let passworld = "topsecretpassword";

  // We start by navigating to the login page.
  await commands.navigate("https://www.facebook.com");

  // When we fill in a input field/click on a link we wanna
  // try/catch that if the HTML on the page changes in the feature
  // sitespeed.io will automatically log the error in a user friendly
  // way, and the error will be re-thrown so you can act on it.
  await commands.wait.byTime(5000);

  // Add text into an input field, finding the field by id
  await commands.addText.bySelector(username, "input[name=email]");
  await commands.wait.byTime(2000);
  await commands.addText.bySelector(passworld, "input[name=pass]");
  await commands.wait.byTime(2000);

  // Start the measurement before we click on the
  // submit button. Sitespeed.io will start the video recording
  // and prepare everything.
  // Find the sumbit button and click it and then wait
  // for the pageCompleteCheck to finish
  await commands.measure.start("pageload");

  // There are two variants of the facebook login page:
  try {
    await commands.click.bySelectorAndWait("button[name=login]");
  } catch (e) {
    await commands.click.bySelectorAndWait("input[type=submit]");
  }

  // Stop and collect the measurement before the next page we want to measure
  await commands.measure.stop();
};
