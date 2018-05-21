start	beq      0       1       next
	lw	 0       1       val
next	beq      0       1       start
	beq      0       1       done
	noop
	noop
done	halt
val	.fill   1 