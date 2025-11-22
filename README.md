# MiniPlex
[![MiniPlex Workflow](https://github.com/neilstephens/MiniPlex/actions/workflows/MiniPlex.yml/badge.svg)](https://github.com/neilstephens/MiniPlex/actions/workflows/MiniPlex.yml)

## Synopsis
```

MiniPlex: A minimal UDP multiplexer/hub/broker.
ProtoConv: Protocol adapter to convert between a stream and datagrams.

		In combination, a simple way to bolt-on rudimentary multi-cast/multi-path or combine connections for existing applications

USAGE: 

   ./MiniPlex  {-H|-T|-P|-X} -p <port> [-l <localaddr>] [-Z <rcv buf size>]
               [-o <timeout>] [-r <trunk host>] [-t <trunk port>] [-B
               <branch host>] ... [-b <branch port>] ... [-C <switchmode
               bytecode file>] [-c <console log level>] [-f <file log
               level>] [-F <log filename>] [-S <size in kB>] [-N <number of
               files>] [-x <num threads>] [-M] [-m <milliseconds>] [--]
               [--version] [-h]


Where: 

   -H,  --hub
     (OR required)  Hub mode: Forward datagrams from/to all endpoints.
         -- OR --
   -T,  --trunk
     (OR required)  Trunk mode: Forward frames from a 'trunk' to other
     endpoints. Forward datagrams from other endpoints to the trunk.
         -- OR --
   -P,  --prune
     (OR required)  Like Trunk mode, but limits flow to one (first in best
     dressed) branch
         -- OR --
   -X,  --switch
     (OR required)  Switch mode: forward datagrams based on application
     addresses.


   -p <port>,  --port <port>
     (required)  Local port to listen/receive on.

   -l <localaddr>,  --local <localaddr>
     Local ip address. Defaults to 0.0.0.0 for all ipv4 interfaces.

   -Z <rcv buf size>,  --so_rcvbuf <rcv buf size>
     Datagram socket receive buffer size.

   -o <timeout>,  --timeout <timeout>
     Milliseconds to keep an idle endpoint cached

   -r <trunk host>,  --trunk_ip <trunk host>
     Remote trunk ip address.

   -t <trunk port>,  --trunk_port <trunk port>
     Remote trunk port.

   -B <branch host>,  --branch_ip <branch host>  (accepted multiple times)
     Remote endpoint addresses to permanently cache. Use -b to provide
     respective ports in the same order.

   -b <branch port>,  --branch_port <branch port>  (accepted multiple
      times)
     Remote endpoint port to permanently cache. Use -B to provide
     respective addresses in the same order.

   -C <switchmode bytecode file>,  --byte_code <switchmode bytecode file>
     RV64IM RISC-V byte code file. Switch mode code for extracting src and
     dst addrs from packet data.

     Pre-conditions: a0=&buf, a1=buf_size, a2=&src, a3=&dst.
     Post-execution: result = a0 (success result==0, src/dst have been
     written).

   -c <console log level>,  --console_logging <console log level>
     Console log level: off, critical, error, warn, info, debug, or trace.
     Default critical.

   -f <file log level>,  --file_logging <file log level>
     File log level: off, critical, error, warn, info, debug, or trace.
     Default error.

   -F <log filename>,  --log_file <log filename>
     Log filename. Defaults to ./MiniPlex.log

   -S <size in kB>,  --log_size <size in kB>
     Roll the log file at this many kB. Defaults to 5000

   -N <number of files>,  --log_num <number of files>
     Keep this many log files when rolling the log. Defaults to 3

   -x <num threads>,  --concurrency <num threads>
     A hint for the number of threads in thread pool. Defaults to detected
     hardware concurrency.

   -M,  --benchmark
     Run a loopback test for fixed duration (see -m) and exit.

   -m <milliseconds>,  --benchmark_duration <milliseconds>
     Number of milliseconds to run the loopback benchmark test. Defaults to
     10000.

   --,  --ignore_rest
     Ignores the rest of the labeled arguments following this flag.

   --version
     Displays version information and exits.

   -h,  --help
     Displays usage information and exits.


=================================================================================


   ./ProtoConv  -l <local port> [-a <local addr>] -A <remote addr> -r
                <remote port> [-B <rcv buf size>] [-Q <max write queue
                size>] [-D <packet delimiter>] [-q <max out-of-order
                frames>] [-m <max out-of-order frame wait ms>] [-T <remote
                tcp host>] [-C <tcp is client>] [-k <tcp connection retry
                times>] [-t <remote tcp port>] [-s <serial devices>] ...
                [-b <serial bauds rates>] ... [-L <serial flow ctl
                settings>] ... [-Z <serial char sizes>] ... [-i <serial
                stop bits>] ... [-p <frame protocol>] [-c <console log
                level>] [-f <file log level>] [-F <log filename>] [-S <log
                file size in kB>] [-N <number of log files>] [-x
                <numthreads>] [--] [--version] [-h]


Where: 

   -l <local port>,  --localport <local port>
     (required)  Local port to listen/receive datagrams on.

   -a <local addr>,  --localaddr <local addr>
     Local ip address for datagrams. Defaults to 0.0.0.0 for all ipv4
     interfaces.

   -A <remote addr>,  --remoteaddr <remote addr>
     (required)  Remote ip address for datagrams.

   -r <remote port>,  --remoteport <remote port>
     (required)  Remote port for datagrams.

   -B <rcv buf size>,  --so_rcvbuf <rcv buf size>
     Datagram socket receive buffer size.

   -Q <max write queue size>,  --write_queue_size <max write queue size>
     Max number of messages to buffer in the stream writer queue before
     dropping (older) data.

   -D <packet delimiter>,  --packet_delimiter <packet delimiter>
     Use a packet delimiter (inserted in the stream with sequence and CRC)
     instead of protocol framing.

   -q <max out-of-order frames>,  --max_sequence_reorder <max out-of-order
      frames>
     Tolerance for frame re-ordering: max frames to buffer waiting for the
     next sequence number.

   -m <max out-of-order frame wait ms>,  --max_sequence_age_ms <max
      out-of-order frame wait ms>
     Tolerance for frame re-ordering: max time (ms) to wait for the next
     sequence number.

   -T <remote tcp host>,  --tcphost <remote tcp host>
     If converting TCP, this is the remote IP address for the connection.

   -C <tcp is client>,  --tcpisclient <tcp is client>
     If converting TCP, this is defines if it's a client or server
     connection.

   -k <tcp connection retry times>,  --tcpretrytimes <tcp connection retry
      times>
     Timing parameters for the tcp connection retry exponential backoff:
     '<MinRetryTime> <MaxRetryTime> <EstablishedResetTime>' in
     milliseconds

   -t <remote tcp port>,  --tcpport <remote tcp port>
     TCP port if converting TCP.

   -s <serial devices>,  --serialdevices <serial devices>  (accepted
      multiple times)
     List of serial devices, if converting serial

   -b <serial bauds rates>,  --serialbauds <serial bauds rates>  (accepted
      multiple times)
     List of serial board rates, if converting serial

   -L <serial flow ctl settings>,  --serialflowctl <serial flow ctl
      settings>  (accepted multiple times)
     List of serial flow control settings, if converting serial

   -Z <serial char sizes>,  --serialcharsize <serial char sizes>  (accepted
      multiple times)
     List of serial char sizes, if converting serial

   -i <serial stop bits>,  --serialstopbits <serial stop bits>  (accepted
      multiple times)
     List of serial stop bits settings, if converting serial

   -p <frame protocol>,  --frameprotocol <frame protocol>
     Parse stream frames based on this protocol

   -c <console log level>,  --console_logging <console log level>
     Console log level: off, critical, error, warn, info, debug, or trace.
     Default critical.

   -f <file log level>,  --file_logging <file log level>
     File log level: off, critical, error, warn, info, debug, or trace.
     Default error.

   -F <log filename>,  --log_file <log filename>
     Log filename. Defaults to ./ProtoConv.log

   -S <log file size in kB>,  --log_size <log file size in kB>
     Roll the log file at this many kB. Defaults to 5000

   -N <number of log files>,  --log_num <number of log files>
     Keep this many log files when rolling the log. Defaults to 3

   -x <numthreads>,  --concurrency <numthreads>
     A hint for the number of threads in thread pool. Defaults to detected
     hardware concurrency.

   --,  --ignore_rest
     Ignores the rest of the labeled arguments following this flag.

   --version
     Displays version information and exits.

   -h,  --help
     Displays usage information and exits.
   
```

## Get MiniPlex

You can download a pre-built binaries for various platforms from the github [Releases](https://github.com/neilstephens/MiniPlex/releases) section. Alternatively, simply build your own copy from source using the instructions below.

## Example Use Case

Suppose you have a network that doesn't support UDP multicast. Maybe it's disabled for security or performance, or maybe you like firewall policies to have strict point-to-point rules.
But you would still like to stream UDP packets to multiple endpoints. This is where MiniPlex can help. MiniPlex can convert a single unicast stream into multiple streams. For example:

### Stream some audio to multiple endpoints:

Run MiniPlex to listen on port 1234 in hub mode any incoming datagrams will be forwarded to all branches.
We specify two fixed branches on the command line, because they will just passively recieve data:

```
./MiniPlex -H -p 1234 -B 192.168.1.5 -b 1234 -B 192.168.1.6 -b 1234
```

Use vlc to stream out some unicast UDP audio:

```
vlc sftp://192.168.1.2/Music/ --sout="#std{access=udp, mux=ts, dst=127.0.0.1:1234}"
```

On the branch hosts (192.168.1.5 and 192.168.1.6 in this example) run vlc to recieve the the audio that is being forwarded by MiniPlex:

```
vlc udp://@:1234
```

### UDP Switch

See [these examples](Examples/SwitchBytecode/README.md) for how to use MiniPlex as a layer 4 UDP switch! 

## Build

Assuming
  * You've git cloned this repo into a directory called 'MiniPlex' (source directory),
  * Created an adjacent build directory called MiniPlex-bin,
  * Have the appropriate c++20 toolchain installed (earlier version might work, but aren't tested):
    * g++ >= 9.4.0, or
    * clang >= 10.0.0, or
    * MSVC >= VS 2019 v16.11

### Configure the build

```
cmake -S MiniPlex -B MiniPlex-bin
```
This will automatically clone and init the submodules repo dependencies for spdlog, tclap and asio and configure the default build system.

### Run the build
```
cmake --build MiniPlex-bin
```
You should have a MiniPlex executable in MiniPlex-bin. See the synopis above for how to use it.
