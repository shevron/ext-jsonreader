--TEST--
Parse a simple short array
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--STDIN--
["Galahad", "Bedivere", "Lancelot", "Bors"]
--FILE--
<?php
$jr = new JSONReader;
$jr->open('php://stdin');
while ($jr->read()) {
  if ($jr->tokenType & JSONReader::VALUE) {
    echo "- {$jr->value}\n";
  }
}
$jr->close();
?>
--EXPECT--
- Galahad
- Bedivere
- Lancelot
- Bors

