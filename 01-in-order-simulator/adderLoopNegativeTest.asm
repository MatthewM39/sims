	lw      0       1       pos1
	noop
	noop
	lw      0       2       neg100
	noop
	noop
	lw      0       3       pos100
	noop
	noop
	sw      0       3       pos100
	noop
	noop
	lw      0       4       pos100
	noop
	noop
start	add     1       2       2
	beq     2       3       done
	beq     0       0       start
	noop
	noop
done	halt
pos1	.fill   1
neg100	.fill   -100
pos100   .fill   100
stAddr	.fill   start                   will contain the address of start (2)