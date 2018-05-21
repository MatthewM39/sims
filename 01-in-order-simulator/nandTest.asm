	lw      0       1       one
	lw      0       2       two
	lw      0       3       three
	noop
	noop
	nand      0       1       4
	nand      1       2       5
	nand      1       3       6
	noop
	noop
	halt
one		.fill   1
two		.fill   2
three 	.fill   3