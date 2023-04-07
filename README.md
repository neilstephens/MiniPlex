# MiniPlex
[![MiniPlex Workflow](https://github.com/neilstephens/MiniPlex/actions/workflows/MiniPlex.yml/badge.svg)](https://github.com/neilstephens/MiniPlex/actions/workflows/MiniPlex.yml)

## Synopsis
```

USAGE: 

   ./MiniPlex  {-H|-T|-P} -p <port> [-l <localaddr>] [-o <timeout>] [-r
               <trunkhost>] [-t <trunkport>] [-B <branchhost>] ... [-b
               <branchport>] ... [-c <console log level>] [-f <file log
               level>] [-F <log filename>] [-S <size in kB>] [-N <number of
               files>] [-x <numthreads>] [-M] [-m <milliseconds>] [--]
               [--version] [-h]


Where: 

   -H,  --hub
     (OR required)  Hub/Star mode: Forward datagrams from/to all endpoints.
         -- OR --
   -T,  --trunk
     (OR required)  Trunk mode: Forward frames from a 'trunk' to other
     endpoints. Forward datagrams from other endpoints to the trunk.
         -- OR --
   -P,  --prune
     (OR required)  Like Trunk mode, but limits flow to one (first in best
     dressed) branch


   -p <port>,  --port <port>
     (required)  Local port to listen/receive on.

   -l <localaddr>,  --local <localaddr>
     Local ip address. Defaults to 0.0.0.0 for all ipv4 interfaces.

   -o <timeout>,  --timeout <timeout>
     Milliseconds to keep an idle endpoint cached

   -r <trunkhost>,  --trunk_ip <trunkhost>
     Remote trunk ip address.

   -t <trunkport>,  --trunk_port <trunkport>
     Remote trunk port.

   -B <branchhost>,  --branch_ip <branchhost>  (accepted multiple times)
     Remote endpoint addresses to permanently cache. Use -b to provide
     respective ports in the same order.

   -b <branchport>,  --branch_port <branchport>  (accepted multiple times)
     Remote endpoint port to permanently cache. Use -B to provide
     respective addresses in the same order.

   -c <console log level>,  --console_logging <console log level>
     Console log level: off, critical, error, warn, info, debug, or trace.
     Default off.

   -f <file log level>,  --file_logging <file log level>
     File log level: off, critical, error, warn, info, debug, or trace.
     Default error.

   -F <log filename>,  --log_file <log filename>
     Log filename. Defaults to ./MiniPlex.log

   -S <size in kB>,  --log_size <size in kB>
     Roll the log file at this many kB. Defaults to 5000

   -N <number of files>,  --log_num <number of files>
     Keep this many log files when rolling the log. Defaults to 3

   -x <numthreads>,  --concurrency <numthreads>
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


   A minimal UDP/TCP multiplexer/hub/broker. A simple way to bolt-on
   rudimentary multi-cast/multi-path or combine connections.
   
```

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
