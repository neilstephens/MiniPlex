# MiniPlex
```

USAGE: 

   ./MiniPlex  {-H|-T|-P} -p <port> [-l <localaddr>] [-r <trunkhost>] [-t
               <trunkport>] [-c <console log level>] [-f <file log level>]
               [-F <log filename>] [-x <numthreads>] [--] [--version] [-h]


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

   -r <trunkhost>,  --trunk_ip <trunkhost>
     Remote trunk ip address.

   -t <trunkport>,  --trunk_port <trunkport>
     Remote trunk port.

   -c <console log level>,  --console_logging <console log level>
     Console log level: off, critical, error, warn, info, debug, or trace.
     Default off.

   -f <file log level>,  --file_logging <file log level>
     File log level: off, critical, error, warn, info, debug, or trace.
     Default error.

   -F <log filename>,  --log_file <log filename>
     Log filename. Defaults to ./MiniPlex.log

   -x <numthreads>,  --concurrency <numthreads>
     A hint for the number of threads in thread pool. Defaults to detected
     hardware concurrency.

   --,  --ignore_rest
     Ignores the rest of the labeled arguments following this flag.

   --version
     Displays version information and exits.

   -h,  --help
     Displays usage information and exits.


   A minimal UDP/TCP multiplexer/hub/broker. A simple way to bolt-on
   rudimentary multi-cast/multi-path or combine connections.
   
```
