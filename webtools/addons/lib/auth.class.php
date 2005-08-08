<?php
/**
 * Authentication and Session handling class.
 * This class circumvents basic PHP Sessions and uses MySQL to store all session data.
 * The reason this was done was to preserve sessions across different LVS nodes by utilizing the application latyer.
 *
 * @package amo
 * @subpackage lib
 */
?>
