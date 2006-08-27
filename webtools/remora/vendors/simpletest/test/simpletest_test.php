<?php
    // $Id: simpletest_test.php,v 1.1 2006/08/27 02:06:40 shaver%mozilla.org Exp $
    require_once(dirname(__FILE__) . '/../simpletest.php');

    SimpleTest::ignore('ShouldNeverBeRunEither');

    class ShouldNeverBeRun extends UnitTestCase {
        function testWithNoChanceOfSuccess() {
            $this->fail('Should be ignored');
        }
    }

    class ShouldNeverBeRunEither extends ShouldNeverBeRun { }
?>