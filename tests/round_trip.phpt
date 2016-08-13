--TEST--
SHMT: Test round trip
--INI--
max_execution_time=90
--FILE--
<?php
function _getRandomValue()
{
	/* Return a random number if 0 */
	if (!mt_rand(0, 1)) {
		return mt_rand();
	}

	/* Otherwise, return a random string with a random length */
	$iLength = mt_rand(1, 200);

	$sValue = '';
	while (strlen($sValue) < $iLength) {
		$sValue .= chr(mt_rand(32, 126));
	}

	return $sValue;
}

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

$aValues = $aShuffled = [];
while (count($aValues) < 500000) {
	$aValues[_getRandomValue()] = _getRandomValue();
}

/* Create SHMT with the original order of values */

$sTmpFile = tempnam(sys_get_temp_dir(), 'shmt_');

var_dump(SHMT::create($sTmpFile, $aValues));

/* Shuffle the values */

$aKeys = array_keys($aValues);
shuffle($aKeys);

foreach($aKeys as $mKey) {
	$aShuffled[$mKey] = $aValues[$mKey];
}

/* Check results in a random order */

var_dump('RUN');

foreach ($aShuffled as $mKey => $mValue) {
	$oSHMT = new SHMT($sTmpFile);

	if ($oSHMT->get($mKey) !== (string)$mValue) {
		var_dump('FAIL');
	}
}

var_dump('END');

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

unlink($sTmpFile);
?>
===DONE===
--EXPECT--
bool(true)
string(3) "RUN"
string(3) "END"
===DONE===
