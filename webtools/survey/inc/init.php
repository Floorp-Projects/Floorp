<?php
/**
 * Init script.
 *
 * @package survey
 * @subpackage inc
 */

/**
 * Require config and required libraries.
 */
require_once('config.php');
require_once(ROOT_PATH.'/lib/sql.class.php');
require_once(ROOT_PATH.'/lib/survey.class.php');

/**
 * Set runtime options.
 */

/**
 * Configure database.
 */
class Survey_SQL extends SQL {
    function Survey_SQL() {
        require_once("DB.php");
        $dsn = array (
            'phptype'  => 'mysql',
            'dbsyntax' => 'mysql',
            'username' => DB_USER,
            'password' => DB_PASS,
            'hostspec' => DB_HOST,
            'database' => DB_NAME,
            'port'        => DB_PORT
        );
        $this->connect($dsn);

        // Test connection; display "gone fishing" on failure.
        if (DB::isError($this->db)) {
            die('Unable to connect to database.  Try again in a few minutes.');
        }
    }
}

/**
 * Set global variables.
 */
$db = new Survey_SQL();
$app = new Survey();

/**
 * Set up arrays to hold cleaned inputs.
 */
$clean = array();
$html = array();
$sql = array();
?>
