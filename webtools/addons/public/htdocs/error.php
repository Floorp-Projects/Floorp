<?php
    require_once(LIB.'/error.php');

    triggerError('Test Error<br>_SERVER:<pre>'.print_r($_SERVER, true).'</pre>', 'site-down.tpl');

?>
