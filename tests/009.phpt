--TEST--
Test that the constructor takes the right argument(s)
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--FILE--
<?php
// Expected to be OK
$rdr = new JSONReader(array());
$rdr = new JSONReader();
$rdr = new JSONReader(array('fakeoption' => 'somevalue'));

// Expected to fail
echo 'String: ';
$rdr = new JSONReader('somestring');
echo "NULL: ";
$rdr = new JSONReader(null);
?>
--EXPECTF--
String: 
Warning: JSONReader::__construct() expects parameter 1 to be array, string given in %s on line %d
NULL: 
Warning: JSONReader::__construct() expects parameter 1 to be array, null given in %s on line %d

