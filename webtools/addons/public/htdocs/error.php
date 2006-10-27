<?php
    require_once(LIB.'/error.php');

    $error = 'Test Error<br>';
    $error .= 'Generated: '.date("r");
    $error .= '<pre>'.print_r($_SERVER, true).'</pre>';

    triggerError($error, 'site-down.tpl');

?>
