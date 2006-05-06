<?php
/**
 * Get results and export them to .csv.  We should probably have some sort of date range option.
 * @package survey
 * @subpackage docs
 * @todo add date filters
 */

// Get our results.
$results = $app->getCsvExport();

// Our final output string.
$csv = array();

// Include our CSV file.
require_once('../lib/csv.php');

// Set up our headers.
$csv[] = array_keys($results[0]);

// Convert our results array to CSV format.
foreach ($results as $csv_row) {
    $csv_row['comments'] = preg_replace('/^(([A-Za-z0-9]+_+)|([A-Za-z0-9]+\-+)|([A-Za-z0-9]+\.+)|([A-Za-z0-9]+\++))*[A-Za-z0-9]+@((\w+\-+)|(\w+\.))*\w{1,63}\.[a-zA-Z]{2,6}$/','',$csv_row['comments']);
    $csv[] = $csv_row;
}

// Stream our resulting CSV array to the user.
csv_send_csv($csv);
exit;
?>
