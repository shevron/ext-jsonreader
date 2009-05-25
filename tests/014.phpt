--TEST--
Test that we can get the current struct using currentStruct 
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--STDIN--
[{
  "key": 12, 
  "array": [1, 2, [3], {"a" : "B"}]
}]
--FILE--
<?php
$rdr = new JSONReader();
$rdr->open('php://stdin');
do {
  var_dump($rdr->currentStruct);
} while ($rdr->read());
$rdr->close();
?>
--EXPECT--
NULL
int(1)
int(2)
int(2)
int(2)
int(2)
int(1)
int(1)
int(1)
int(1)
int(1)
int(1)
int(2)
int(2)
int(2)
int(1)
int(2)
int(1)
NULL

