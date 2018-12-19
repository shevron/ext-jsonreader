--TEST--
Test that an exception is thrown when ERRMODE_EXCEPT is set 
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--STDIN--
{ 12: "<= Numeric keys are not valid in JSON!" }
--FILE--
<?php
$rdr = new JSONReader(array(
  JSONReader::ATTR_ERRMODE => JSONReader::ERRMODE_EXCEPT
));
$rdr->open('php://stdin');
try {
  while ($rdr->read()) {
    echo "- $rdr->tokenType\n";
  }
} catch (JSONReaderException $e) {
  echo "EX: {$e->getCode()} {$e->getMessage()}\n";
}
$rdr->close();
?>
--EXPECTF--
- %d
EX: %d [%s] Unexpected character in input: '1' (0x31)

