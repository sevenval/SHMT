--TEST--
SHMT: Test SHMT::create() method with big data array
--FILE--
<?php
$sTmpFile = tempnam(sys_get_temp_dir(), 'shmt_');

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

var_dump(SHMT::create($sTmpFile, array_fill(0, 1 << 15, 7)));
var_dump(file_exists($sTmpFile));
var_dump(filesize($sTmpFile) > 1400000);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

unlink($sTmpFile);
?>
===DONE===
--EXPECT--
bool(true)
bool(true)
bool(true)
===DONE===
