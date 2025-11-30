	# a0: pointer to packet
	# a1: size of packet
	# a2: pointer to src (output u64)
	# a3: pointer to dst (output u64)

#
#	WireGuard (WG) sessions are established by a handshake which establishes 32bit sender and receiver IDs
#	After the first two handshake messages, packets only contain the receiver ID
#	So, we need to keep a record of the sender/receiver pairs from the handshake
#	Here we use a simple 'hash' table (the 'hash' is really just the 16 LSBits of an ID) to store the pairs
#	Hash collisions aren't really a problem, a session will get dropped and it will just change IDs and restart
#

# macro to lookup a hash table entry
.macro LOOKUP addr key
	li t2, 0xFFFF
	and t2, \key, t2	# take 'hash' (lower 16)
	la t3, hash_table	# load the address of the hash table
	li t4, 6			# size of an entry (2byte receiver msbs, 4byte sender)
	mul \addr, t4, t2	# get the offset for the hash
	add \addr, \addr, t3# address of the hash table entry
.endm

start:
	li t0, 12           # All WG packets are more than 12 bytes
	bltu a1, t0, err	# error, buffer size < 12
	
	lbu t0, 0(a0)		# load WG packet type
	li t1, 4
	beq t0, t1, trans	# transport message
	li t1, 1
	beq t0, t1, init	# initiation packet
	li t1, 2
	beq t0, t1, resp	# initiation response
	li t1, 3
	beq t0, t1, cookie	# under load - cookie
	j err				# error, start bytes invalid
init:
	lwu t0, 4(a0)		# load sender
	li t1, 0x123456789	# bogus receiver - 33 bits
	j success
resp:
	lwu t0, 4(a0)		# load sender
	lwu t1, 8(a0)		# load receiver
	LOOKUP t5, t1		# lookup receiver
	srli t2, t1, 16		# get most significant bytes of receiver
	sh t2, 0(t5)		# store the receiver msbs
	sd t0, 2(t5)		# store the sender
	LOOKUP t5, t0		# lookup sender
	srli t2, t0, 16		# get most significant bytes of sender
	sh t2, 0(t5)		# store the sender msbs
	sd t1, 2(t5)		# store the receiver
	j success
cookie:
trans:
	lwu t1, 4(a0)		# load receiver
	LOOKUP t5, t1		# lookup receiver
	lhu t2, 0(t5)		# load msbs from table
	srli t3, t1, 16		# get receiver msbs
	bne t2, t3, err		# error, 'hash' collision - drop and it'll switch addrs
	lwu t0, 2(t5)		# load sender
success:
	sd t0, 0(a2)		# store src to output
	sd t1, 0(a3)		# store dst to output
	li a0, 0			# success return 0
	jr ra
err:
    li a0, -1			# error return -1
    jr ra
hash_table:
	.space 399360, 0	# 64k 6 byte slots
