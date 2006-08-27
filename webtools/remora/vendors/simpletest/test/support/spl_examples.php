<?php
    // $Id: spl_examples.php,v 1.1 2006/08/27 02:06:40 shaver%mozilla.org Exp $

    class IteratorImplementation implements Iterator {
        function current() { }
        function next() { }
        function key() { }
        function valid() { }
        function rewind() { }
    }

    class IteratorAggregateImplementation implements IteratorAggregate {
        function getIterator() { }
    }
?>