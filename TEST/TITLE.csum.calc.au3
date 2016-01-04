$csum = 0x00

$mode = 0x04
$cmd1 = 0x00
$cmd2 = 0x23	;title = 0x21, artist = 0x23, album = 0x25

;~ $text = 'Somewhereinamerica'
;~ $text = 'Allure'
;~ $text = 'Barbed Wire'
$text = 'Kendrick Lamar, Ash Rise'
;~ $text = 'Kendrick Lamar feat Ash Riser'

;~ $length = 1 + 2 + StringLen($text)+1
$length = 29
ConsoleWrite('length = ' & hex($length, 2) & ' ('&$length&')' &@CRLF)

$csum += $length
$csum += $mode
$csum += $cmd1
$csum += $cmd2

$a = StringToASCIIArray($text)
for $c = 1 to StringLen($text)
	ConsoleWrite(Hex($a[$c-1], 2) & ', ')
	$csum += Asc(StringMid($text, $c, 1))
Next
ConsoleWrite(@CRLF)
$csum += 0x00	;null terminated string

;~ $csum = BitAND((0x100 - $csum), 0xFF)
$csum = 0x100 - BitAND($csum, 0xFF)

ConsoleWrite( 'csum = ' & hex($csum, 2) &@CRLF)

$count = 0
for $c in $a
	If $c = $csum Then $count += 1
Next
ConsoleWrite('count = ' & $count &@CRLF)
