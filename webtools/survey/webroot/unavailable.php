<?php
/**
 * Thank you page on survey completion.
 * Since the input is all optional, on any submit they will see this.
 * @package survey
 * @subpackage docs
 */
require_once(HEADER);

echo <<<THANKS
<h1>Survey Temporarily Unavailable</h1>
<p>We're sorry, the uninstall survey is not available at this time.  Please try again later.</p>
THANKS;

require_once(FOOTER);
?>
