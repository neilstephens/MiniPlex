#VXLAN Header:
#   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#   |R|R|R|R|I|R|R|R|            Reserved                           |
#   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#   |                VXLAN Network Identifier (VNI) |   Reserved    |
#   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#
#   Inner Ethernet Header:
#   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#   |             Inner Destination MAC Address                     |
#   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#   | Inner Destination MAC Address | Inner Source MAC Address      |
#   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#   |                Inner Source MAC Address                       |
#   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

# We'll load in little-endian, even though network byte order is reversed
# We're combining MACs and VNI anyway to form hybrid addresses

get_addrs:
	#check we have 5x32+16 bytes to work with:
	li t0, 176
	bltu a1, t0, err
	
	#load 2 bytes of the VNI (LSBs)
	lh t0, 40(a0)
	slli t0, t0, 48 # shift up to add to MACs
	
	#load the inner eth dst MAC
	ld t1, 64(a0)
	and t1, t1, t0 # combine VNI
	
	#load the inner eth src MAC
	ld t2, 112(a0)
	and t2, t2, t0 # combine VNI
	
	#write outputs
	sd t2, 0(a2) #src
	sd t1, 0(a4) #dst
	
	li a0, 0
	jr ra
err:
	li a0, -1
	jr ra