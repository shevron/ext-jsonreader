--TEST--
Parse a simple JSON object into an associative array
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--STDIN--
{
	"pure": "Sir Robin",
	"brave": "Sir Lancelot",
	"not-quite-so-brave": "Sir Robin",
	"aptly-named": "Sir Not-Appearing-in-this-Film"
}
--FILE--
<?php
$jr = new JSONReader;
$jr->open('php://stdin');
$arr = array();
while ($jr->read()) {
  if ($jr->tokenType == JSONReader::OBJECT_KEY) {
    $key = $jr->value;
    if (! $jr->read() || ! $jr->tokenType == JSONReader::VALUE) {
      echo "failed reading value of $key!\n";
      exit;
    }
    $value = $jr->value;

    $arr[$key] = $value;
  }
}
var_dump($arr);
--EXPECT--
array(4) {
  ["pure"]=>
  string(9) "Sir Robin"
  ["brave"]=>
  string(12) "Sir Lancelot"
  ["not-quite-so-brave"]=>
  string(9) "Sir Robin"
  ["aptly-named"]=>
  string(30) "Sir Not-Appearing-in-this-Film"
}

