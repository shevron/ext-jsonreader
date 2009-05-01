--TEST--
parse a multi-dimentional JSON array and use the currentDepth property
--SKIPIF--
<?php if (!extension_loaded("jsonreader")) print "skip"; ?>
--STDIN--
[ 
  ["Windows XP", "Windows Vista", "Windows 7"],
  ["AIX", "Solaris", "OpenSolaris",
    ["FreeBSD", "NetBSD", "OpenBSD", "Darwin"],
    ["Gentoo Linux", "Slackware", "Mandriva",
      ["Debian GNU/Linux", "Ubuntu Linux"],
      ["RHEL", "CentOS", "Fedora Linux"],
      ["SLES", "OpenSUSE"]
    ]
  ],
  "AS/400",
  "Mac OS",
  "DOS"
]
--FILE--
<?php

$reader = new JSONReader; 
$reader->open('php://stdin');

while ($reader->read()) {
  if ($reader->tokenType == JSONReader::VALUE) {
    // print indent
    echo str_repeat("  ", $reader->currentDepth);
    echo "* $reader->value\n";
  } elseif ($reader->tokenType == JSONReader::ARRAY_START) {
    echo str_repeat("-", $reader->currentDepth) . ">\n";
  }
}

$reader->close();

?>
--EXPECT--
->
-->
    * Windows XP
    * Windows Vista
    * Windows 7
-->
    * AIX
    * Solaris
    * OpenSolaris
--->
      * FreeBSD
      * NetBSD
      * OpenBSD
      * Darwin
--->
      * Gentoo Linux
      * Slackware
      * Mandriva
---->
        * Debian GNU/Linux
        * Ubuntu Linux
---->
        * RHEL
        * CentOS
        * Fedora Linux
---->
        * SLES
        * OpenSUSE
  * AS/400
  * Mac OS
  * DOS

