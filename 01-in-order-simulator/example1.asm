	lw      0       1       five
	lw      0       2        neg1
	noop
start	add     1       2       1
	beq     0       1       done
	beq     0       0       start
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
done	halt
five	.fill   5
neg1	.fill   -1
stAddr	.fill   start                   will contain the address of start (2)
