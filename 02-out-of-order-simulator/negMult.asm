	lw      0       1       one
	beq      0       1       done
	lw      0       2       two
	mult      1       2       3
	noop
	noop
	noop
	noop
	noop
done	halt
one		.fill   1
two		.fill   -1