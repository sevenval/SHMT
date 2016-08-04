--TEST--
SHMT: Test SHMT::create() method with big data array
--FILE--
<?php
$file = tempnam(sys_get_temp_dir(), 'shmt_');
$array = array_fill(0, 1 << 15, 7);
var_dump(SHMT::create($file, $array));
var_dump(file_exists($file));
var_dump(filesize($file) > 1400000);
unlink($file);
--EXPECT--
bool(true)
bool(true)
bool(true)
