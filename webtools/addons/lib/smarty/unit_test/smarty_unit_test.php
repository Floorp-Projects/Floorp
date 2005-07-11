<?php

require_once 'test_cases.php';
require_once 'PHPUnit.php';

$suite = new PHPUnit_TestSuite("SmartyTest");
$result = PHPUnit::run($suite);

echo $result -> toString();
?>
