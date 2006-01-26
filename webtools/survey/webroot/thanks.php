<?php
/**
 * Thank you page on survey completion.
 * Since the input is all optional, on any submit they will see this.
 * @package survey
 * @subpackage docs
 */
require_once(HEADER);

echo <<<THANKS
<h1>Thanks!</h1>
<p>Thank you for taking the time to help us make Firefox and Thunderbird better products.  We hope to see you soon.</p>
THANKS;

require_once(FOOTER);
?>
