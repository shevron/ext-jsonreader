--TEST--
Check for jsonreader presence
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--FILE--
<?php 
echo "jsonreader extension is available\n";
$j = new JSONReader;
var_dump($j);
?>
--EXPECT--
jsonreader extension is available
object(JSONReader)#1 (0) {
}

