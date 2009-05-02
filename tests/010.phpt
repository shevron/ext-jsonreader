--TEST--
Check that the JSONReaderException class is available
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--FILE--
<?php 
try {
  throw new JSONReaderException("Testing", 123);
} catch (Exception $e) {
  echo "{$e->getMessage()} {$e->getCode()}\n";
}
?>
--EXPECT--
Testing 123

