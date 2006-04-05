<?php
/**
 * memcache configuration.
 * 
 * The memcache_config array lists all possible memcached servers to use in case the default server does not have the appropriate key.
 */
require_once('../../public/inc/config.php');

$memcacheConnected = false;

$cache = new Memcache();

foreach ($memcache_config as $host=>$options) {
    if ($cache->addServer($host, $options['port'], $options['persistent'], $options['weight'], $options['timeout'], $options['retry_interval'])) {
        $memcacheConnected = true;
    }
}

if ($memcacheConnected) {
    echo '<pre>';
    print_r($cache->getExtendedStats());
    echo '<pre>';
} else {
    die("Unable to connect to any servers.");
}
?>
