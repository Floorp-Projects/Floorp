<?php
/**
 * Maintenance script for addons.mozilla.org.
 *
 * The purpose of this document is to perform periodic tasks that should not be
 * done everytime a download occurs in install.php.  This should reduce
 * unnecessary DELETE and UPDATE queries and lighten the load on the database
 * backend.
 *
 * This script should not ever be accessed over HTTP, and instead run via cron.
 * Only sysadmins should be responsible for operating this script.
 *
 * @package amo
 * @subpackage bin
 */


// Before doing anything, test to see if we are calling this from the command
// line.  If this is being called from the web, HTTP environment variables will
// be automatically set by Apache.  If these are found, exit immediately.
if (isset($_SERVER['HTTP_HOST'])) {
    exit;
}

// If we get here, we're on the command line, which means we can continue.
require_once('config.php');

/**
 *  * Get time as a float.
 *   * @return float
 *    */
function getmicrotime() {
    list($usec, $sec) = explode(" ", microtime());
    return ((float)$usec + (float)$sec);
}

// Start our timer.
$start = getmicrotime();

// Connect to our database.
$connection = mysql_connect(DB_HOST, DB_USER, DB_PASS);
if (!is_resource($connection)) {
    die('Could not connect: ' . mysql_error());
} else {
    mysql_select_db(DB_NAME, $connection);
}

// Get our action.
$action = isset($_SERVER['argv'][1]) ? $_SERVER['argv'][1] : '';

// Perform specified task.  If a task is not properly defined, exit.
switch ($action) {

    /**
     * Update weekly addon counts.
     */
    case 'weekly':
        // Get 7 day counts from the download table.
        $seven_day_count_sql = "
            SELECT
                downloads.ID as ID,
                COUNT(downloads.ID) as seven_day_count
            FROM
                `downloads`
            WHERE
                `date` >= DATE_SUB(NOW(), INTERVAL 7 DAY)
            GROUP BY
                downloads.ID
            ORDER BY
                downloads.ID
        ";

        echo 'Retrieving seven-day counts from `downloads` ...'."\n";
        $seven_day_count_result = mysql_query($seven_day_count_sql, $connection) 
            or trigger_error('MySQL Error '.mysql_errno().': '.mysql_error()."", 
                             E_USER_NOTICE);

        $affected_rows = mysql_num_rows($seven_day_count_result);
    
        if ($affected_rows > 0 ) {
            $seven_day_counts = array();
            while ($row = mysql_fetch_array($seven_day_count_result)) {
                $seven_day_counts[$row['ID']] = ($row['seven_day_count']>0) ? $row['seven_day_count'] : 0;
            }

            echo 'Updating seven day counts in `main` ...'."\n";

            foreach ($seven_day_counts as $id=>$seven_day_count) {
                $seven_day_count_update_sql = "
                    UPDATE `main` SET `downloadcount`='{$seven_day_count}' WHERE `id`='{$id}'
                ";

                $seven_day_count_update_result = mysql_query($seven_day_count_update_sql, $connection) 
                    or trigger_error('mysql error '.mysql_errno().': '.mysql_error()."", 
                                     E_USER_NOTICE);
            }
        }
    break;



    /**
     * Update all total download counts.
     */
    case 'total':
        // Get uncounted hits from the download table.
        $uncounted_hits_sql = "
            SELECT
                downloads.ID as ID,
                COUNT(downloads.ID) as count
            FROM
                downloads
            WHERE
                `counted`=0
            GROUP BY
                downloads.ID
            ORDER BY
                downloads.ID
        ";

        echo 'Retrieving uncounted downloads ...'."\n";

        $uncounted_hits_result = mysql_query($uncounted_hits_sql, $connection) 
            or trigger_error('MySQL Error '.mysql_errno().': '.mysql_error()."", 
                             E_USER_NOTICE);
        $affected_rows = mysql_num_rows($uncounted_hits_result);

        if ($affected_rows > 0) {
            $uncounted_hits = array();
            while ($row = mysql_fetch_array($uncounted_hits_result)) {
                $uncounted_hits[$row['ID']] = ($row['count'] > 0) ? $row['count'] : 0;
            }

            echo 'Updating download totals ...'."\n";

            foreach ($uncounted_hits as $id=>$hits) {
                $uncounted_update_sql = "
                    UPDATE `main` SET `TotalDownloads`=`TotalDownloads`+{$hits} WHERE `ID`='{$id}'
                ";
                $uncounted_update_result = mysql_query($uncounted_update_sql, $connection) 
                    or trigger_error('MySQL Error '.mysql_errno().': '.mysql_error()."", 
                                     E_USER_NOTICE);
            }


            // If we get here, we've counted everything and we can mark stuff for
            // deletion.
            //
            // Mark the downloads we just counted as counted if:
            //      a) it is a day count that is more than 8 days old
            //      b) it is a download log that has not been counted
            //
            // We may lose a couple counts, theoretically, but it's negligible - we're not
            // NASA (yes, THE NASA).
            $counted_update_sql = "
                UPDATE
                    `downloads`
                SET
                    `counted`=1
            ";
            $counted_update_result = mysql_query($counted_update_sql, $connection) 
                or trigger_error('MySQL Error '.mysql_errno().': '.mysql_error()."", 
                                 E_USER_NOTICE);
        }
    break;



    /**
     * Garbage collection for all records that are older than 8 days.
     */
    case 'gc':
        echo 'Starting garbage collection ...'."\n";
        $gc_sql = "
            DELETE FROM
                `downloads`
            WHERE
                `date` < DATE_SUB(NOW(), INTERVAL 8 DAY)
        ";
        $gc_result = mysql_query($gc_sql, $connection) 
            or trigger_error('MySQL Error '.mysql_errno().': '.mysql_error()."", 
                             E_USER_NOTICE);

        // This is unreliable, but it's not a big deal.
        $affected_rows = mysql_affected_rows();
    break;



    /**
     * Unknown command.
     */
    default:
        echo 'Command not found. Exiting ...'."\n";
        exit;
    break;
}
// End switch.



// How long did it take to run?
$exectime = getmicrotime() - $start;



// Display script output.
echo 'Affected rows: '.$affected_rows.'    ';
echo 'Time: '.$exectime."\n";
echo 'Exiting ...'."\n";



exit;
?>
