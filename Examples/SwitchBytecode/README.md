# Switching Address Bytecode
TLDR; skip to the 'Producing Bytecode' section if you just want to know how to implement MiniPlex Switch mode and don't need the background.

## Background
Hub mode (and trunk mode to a lesser extent) in MiniPlex can be inefficient when there are a lot of endpoints connecting to the hub:
* all the branches are receiving all the datagrams
* good for true broadcast data
* not so good if only a subset of the messages from the trunk or other branches are intended for them...

Prune mode is efficient, because there's only one active branch, but if more than one application is 'behind' a branch, it's an all-or-nothing affair; if one application fails on that path, then MiniPlex won't try another branch until (if ever) ALL the activity on that branch stops.

That's where application layer addressing comes in. If datagrams contain application data with source and destination addresses, then in theory MiniPlex could be smart and only send datagrams where they need to go. Enter MiniPlex 'Switch' mode.

## Switching
'Switch' and 'Hub' modes in MinPlex are directly analagous to Ethernet switches and hubs, except they operate on a different layer of the OSI model:
* Ethernet switches operate at layer 1/2 in the OSI model
  * Physical ports at layer 1, Ethernet frames at layer 2
  * They read src and dst MAC addrs from the frame headers
  * They associate MAC (L2) addrs with ports (L1), and forward packets based on that
* MiniPlex operates at layer 3/4 of the OSI model:
  * UDP endpoints (IP:Port) at layer 3, datagram payloads at layer 4
  * To forward the right datagrams to the right endpoints,
    * Layer 4 addresses need  to be associated with endpoints
    * But layer 4 is the *application* payload, and MiniPlex is application agnostic
  * So MiniPlex relies on **you** telling it how to extract src/dst addresses for **your** specific application.

That's why, if you turn on 'Switch' mode (-X) in MiniPlex, you also have to provide a 'mini program' (bytecode) that MiniPlex can use to extract addressing from the datagrams that it will be switching (it looks for 'switch.bin' by default).

## Producing Bytecode
In Switch mode, under the hood, MiniPlex has a tiny RISC-V virtual machine that runs your bytecode on each datagram to extract the source and destination addresses, so it can forward the datagrams to the correct endpoints.

### Virtual architecture
Your 'mini program' bytecode is simply a series of RV64IM instructions.

RV64IM is a varient of the Reduced Instruction Set Computer - version 5(V) - spec (RISC-V). It's the basic integer instructions (I), plus multiply and divide instructions (M), with 64bit registers.

#### Completely unaffiliated links:
* [cheatsheet](https://www.cl.cam.ac.uk/teaching/1617/ECAD+Arch/files/docs/RISCVGreenCardv8-20151013.pdf) + [another cheatsheet](https://www.cs.sfu.ca/~ashriram/Courses/CS295/assets/notebooks/RISCV/RISCV_CARD.pdf)
* [brief listing of instructions and pseudocode](https://marks.page/riscv/)
* [online assembler](https://ret.futo.org/riscv/)
* [official docs](https://riscv.org/specifications/)

### Bytecode Interface
With a handful of instructions, you can unlock efficient datagram forwarding in MiniPlex for any applications that contain source and destination information in the datagram payload.

Simply store the raw bytes for those instruction in a file and pass that to MiniPlex with the -C (--byte_code) option.

Before your program is called, MiniPlex pre-loads the first 4 argument registers (a0-a3) with: the datagram buffer virtual memory address, buffer size, and output virtual memory addresses for src and dst to be loaded into. Put in C syntax, that means your program has an entry point signature that looks like:

```
int64_t getAddrs (uint8_t* buf, uint64_t n, uint64_t* src, uint64_t* dst);
```

Then your instructions extract (load) the addresses from the datagram buffer and store them at specified addresses, using whatever extra logic or checking that's required for the application data in question. Finally, storing zero in a0 to indicate success; any other value will be treated as an error and src/dst won't be used.

Note, for simply checking, loading and storing addresses for simple protocols, C would be overkill. A few instructions in assembly language converted to bytecode would do the job (check out https://ret.futo.org/riscv/ for how easy that is).

### Example - DNP3
Here's an example for the DNP3 SCADA protocol.

Assembly:
```
    li t0, 10           # DNP3 link layer header is 10 bytes
	bltu a1, t0, err	# error, buffer size < 10
	
	lhu t0, 0(a0)		# load DNP3 start bytes
	li t1, 0x6405
    bne t0, t1, err     # error, start bytes != 0x05 0x64
	
	lhu t0, 6(a0)		# load DNP3 src addr
	lhu t1, 4(a0)		# load DNP3 dst addr
	
	sd t0, 0(a2)		# store src to output
	sd t1, 0(a3)		# store dst to output
	
	li a0, 0			# success return 0
	jr ra
err:
    li a0, -1			# error return -1
    jr ra
``` 
Assembled to bytecode:
```
93 02 a0 00
63 e6 55 02
83 52 05 00
37 63 00 00
1b 03 53 40
63 9e 62 00
83 52 65 00
03 53 45 00
23 30 56 00
23 b0 66 00
13 05 00 00
67 80 00 00
13 05 f0 ff
67 80 00 00
```
The bytecode is displayed in HEX encoding above; save it to a file (in binary, not hex encoded), and you can use it to switch forward multidrop DNP3 datagrams in unlimited topologies!

### Example - VXLAN
Ever think VXLAN multicast and VTEP flood lists and all that guff is overkill? And you want a simple VXLAN fabric with simple, yet dynamic, point-point UDP? No problem! Point all your VTEPs at MiniPlex running this Switch mode bytecode:

Assembly:
```
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
```
Bytecode:
```
93 02 00 0b
63 e6 55 02
83 12 85 02
93 92 02 03
03 33 05 04
33 73 53 00
83 33 05 07
b3 f3 53 00
23 30 76 00
23 30 67 00
13 05 00 00
67 80 00 00
13 05 f0 ff
67 80 00 00
```

### Example - WireGuard

[WireGuard](https://www.wireguard.com/papers/wireguard.pdf) (WG) runs exclusively on UDP, and sessions use 32bit sender and receiver IDs. So it's a perfect candidate for MiniPlex Switch mode.

Using MiniPlex as a central WireGuard switch, you could have a completely ephemeral VPN where all endpoints are dynamic (no fixed addresses and all initiators). MiniPlex would be the only fixed point, and it just forwards encrypted traffic, so the only risk is denial of service even if it's compromised.

It's an interesting [Example](SwitchWireGuard.s), because sessions are established by a handshake which exchanges sender and receiver IDs. Following the handshake, packets only container the receiver ID, so the example showcases the ability for bytecode to use persistent memory for stateful protocols.

Disclaimer: this example hasn't been performance tested. Obvioulsy, funnelling a WG VPN through a userspace app is going to create a performance constraint - your miliage may vary.