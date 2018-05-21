	lw      0       1       pos1
	lw      0       2       neg100
	lw      0       3       pos100
	sw      0       3       pos100
	lw      0       4       pos100
start	add     1       2       2
	beq     2       3       done
	beq     0       0       start
	noop
	noop
	noop
	noop
	noop
	noop
	noop
	noop
done	halt
pos1	.fill   1
neg100	.fill   -10
pos100   .fill   100
stAddr	.fill   start                   will contain the address of start (2)