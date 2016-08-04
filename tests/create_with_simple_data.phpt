--TEST--
SHMT: Test SHMT::create() method with simple data array
--FILE--
<?php
$file = tempnam(sys_get_temp_dir(), 'shmt_');
$array = ['foo' => 'bar', 'bar' => 'baz', 666 => 'evil', 'evil' => 666];
var_dump(SHMT::create($file, $array));
var_dump(file_exists($file));
var_dump(filesize($file) > 50);
--EXPECT--
bool(true)
bool(true)
bool(true)
