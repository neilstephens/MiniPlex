#!/bin/bash

#Test MiniPlex permanent branches
# Same as trunk mode, except the trunk speaks first, to prove the perm branch still gets it

#Usage: <this_script> <MiniPlex trunk port> <MiniPlex perm port> <MiniPlex host> <MiniPlex port>

# Check if host and port are provided
if [ $# -ne 4 ]; then
    echo "Error: Please provide trunk-port perm-port host and port arguments"
    echo "Usage: $0 <MiniPlex trunk port> <MiniPlex perm port> <MiniPlex host> <MiniPlex port>"
    exit 1
fi

# Execute ncat commands and capture output
exec 3< <((sleep 0.5 && echo -n 3 && sleep 2 && echo -n 3 && sleep 0.5) | ncat -u -p $1 $3 $4)
sleep 0.1
exec 4< <((sleep 1 && echo -n 4 && sleep 2) | ncat -u -p $2 $3 $4)
sleep 0.1
exec 5< <((sleep 1.5 && echo -n 5 && sleep 1.5) | ncat -u $3 $4)
sleep 0.1
exec 6< <((sleep 2 && echo -n 6 && sleep 1) | ncat -u $3 $4)

sleep 6.1

# Read outputs
OUT3=$(cat <&3)
OUT4=$(cat <&4)
OUT5=$(cat <&5)
OUT6=$(cat <&6)

# Initialize error flag
ERROR_FOUND=0

# Check OUT3
if [ "$OUT3" != "456" ]; then
    echo "Error: OUT3 expected '456', got '$OUT3'"
    ERROR_FOUND=1
fi

# Check OUT4
if [ "$OUT4" != "33" ]; then
    echo "Error: OUT4 expected '33', got '$OUT4'"
    ERROR_FOUND=1
fi

# Check OUT5
if [ "$OUT5" != "3" ]; then
    echo "Error: OUT5 expected '3', got '$OUT5'"
    ERROR_FOUND=1
fi

# Check OUT6
if [ "$OUT6" != "3" ]; then
    echo "Error: OUT6 expected '3', got '$OUT6'"
    ERROR_FOUND=1
fi

# Exit with appropriate status
if [ $ERROR_FOUND -eq 1 ]; then
    exit 1
else
	echo "All tests passed."
    exit 0
fi
