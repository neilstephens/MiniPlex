	# a0: pointer to data
	# a1: size of data
	# a2: pointer to src (output u64)
	# a3: pointer to dst (output u64)
	
    li t0, 10           # DNP3 link layer header is 10 bytes
	bltu a1, t0, err	# error, buffer size < 10
	
	lhu t0, 0(a0)		# load DNP3 start bytes
	li t1, 0x6405
    bne t0, t1, err     # error, start bytes != 0x05 0x64
	
	lbu t0, 2(a0) 		# load DNP3 len byte
	bgt t0, a1, err		# error, len > size of data - should be well less (doesn't inc CRCs etc)
	
	lhu t0, 6(a0)		# load DNP3 src addr
	lhu t1, 4(a0)		# load DNP3 dst addr
	
	sd t0, 0(a2)		# store src to output
	sd t1, 0(a3)		# store dst to output
	
	li a0, 0			# success return 0
	jr ra
err:
    li a0, -1			# error return -1
    jr ra
