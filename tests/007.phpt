--TEST--
Test that we can modify the max nesting level through an INI setting 
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--STDIN--
[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[
[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[
[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[
	"Some Value"
]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]
]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]
]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]
--FILE--
<?php
ini_set('jsonreader.max_depth', 128);
$rdr = new JSONReader();
$rdr->open('php://stdin');
while ($rdr->read()) {
  if ($rdr->tokenType & JSONReader::VALUE) echo "FOUND IT at {$rdr->currentDepth}!\n";
}
$rdr->close();
?>
--EXPECTF--
FOUND IT at 96!

