#!/usr/bin/perl
use strict;
use warnings;
use IO::Socket::INET;
use IO::Select;
use Time::HiRes qw(sleep time);

if (@ARGV != 7) {
    die "Usage: $0 local_port dest_ip dest_port DNP3_src DNP3_dst count duration_sec\n";
}

my ($local_port, $dest_ip, $dest_port, $DNP3_src, $DNP3_dst, $count, $duration) = @ARGV;

for my $p ($local_port, $dest_port) {
    die "Port must be integer 0..65535\n" unless $p =~ /^\d+$/ && $p >= 0 && $p <= 65535;
}
for my $v ($DNP3_src, $DNP3_dst) {
    die "DNP3 addr values must be integer 0..65535\n" unless $v =~ /^\d+$/ && $v >= 0 && $v <= 65535;
}

#printf("DNP3_src = 0x%04X\n", $DNP3_src);
#printf("DNP3_dst = 0x%04X\n", $DNP3_dst);

# Build payload
my $DNP3_start = 0x0564;
my $DNP3_ctl_len = 0x05c9;
my $payload = pack('n n v v', $DNP3_start, $DNP3_ctl_len, $DNP3_dst, $DNP3_src);

# Compute CRC
my $crc = computeCRC($payload);

# Debug: print CRC
#printf("CRC = 0x%04X\n", $crc);

# Append CRC
$payload .= pack('v', $crc);

# Debug: print final buffer as hex
#print "Final buffer (hex): ";
#print unpack("H*", $payload), "\n";

# Create UDP socket
my $sock = IO::Socket::INET->new(
    Proto     => 'udp',
    LocalPort => $local_port,
    PeerAddr  => $dest_ip,
    PeerPort  => $dest_port,
) or die "Cannot create UDP socket: $!\n";

# Statistics tracking
my %packet_counts;  # Key: "src:dst", Value: count
my $total_received = 0;
my $total_sent = 0;
my $crc_errors = 0;

# Create select object for non-blocking reads
my $select = IO::Select->new($sock);

# Fork to handle sending and receiving concurrently
my $pid = fork();
die "Fork failed: $!\n" unless defined $pid;

if ($pid == 0) {
    # Child process: Send packets
    for my $i (1..$count) {
        my $sent = $sock->send($payload);
        if (defined $sent) {
            #print "Sent packet $i ($sent bytes) to $dest_ip:$dest_port\n";
            $total_sent++;
        } else {
            warn "Send $i failed: $!\n";
        }
        sleep($duration/$count) if $i < $count;  # Don't sleep after last packet
    }
    exit(0);
} else {
    # Parent process: Receive packets
    my $last_packet_time = time();
    my $timeout = 2.0;  # 2 seconds of silence
    
    while (1) {
        my $elapsed = time() - $last_packet_time;
        my $remaining = $timeout - $elapsed;
        
        # Check if we've had 2 seconds of silence
        if ($remaining <= 0) {
            last;
        }
        
        # Wait for data with remaining timeout
        my @ready = $select->can_read($remaining);
        
        if (@ready) {
            my $recv_data;
            my $peer_addr = $sock->recv($recv_data, 1024);
            
            if (defined $peer_addr) {
                $last_packet_time = time();
                $total_received++;
                
                my ($peer_port, $peer_ip) = sockaddr_in($peer_addr);
                my $peer_ip_str = inet_ntoa($peer_ip);
                
                # Parse the received packet
                if (length($recv_data) >= 10) {
                    my ($start, $ctl_len, $dst, $src, $recv_crc) = unpack('n n v v v', $recv_data);
                    
                    # Verify CRC
                    my $data_for_crc = substr($recv_data, 0, 8);
                    my $computed_crc = computeCRC($data_for_crc);
                    
                    if ($computed_crc == $recv_crc) {
                        #printf("Received valid packet from %s:%d - DNP3 src=0x%04X dst=0x%04X\n",
                        #       $peer_ip_str, $peer_port, $src, $dst);
                        
                        # Track statistics
                        my $key = sprintf("0x%04X->0x%04X", $src, $dst);
                        $packet_counts{$key}++;
                    } else {
                        printf("Received packet with CRC error from %s:%d (expected 0x%04X, got 0x%04X)\n",
                               $peer_ip_str, $peer_port, $computed_crc, $recv_crc);
                        $crc_errors++;
                    }
                } else {
                    printf("Received short packet from %s:%d (%d bytes)\n",
                           $peer_ip_str, $peer_port, length($recv_data));
                }
            }
        }
    }
    
    # Wait for child to finish
    waitpid($pid, 0);
    
    for my $addr (sort {$packet_counts{$b}<=>$packet_counts{$a}} keys %packet_counts) {
        printf("%s : %d\n", $addr, $packet_counts{$addr});
    }
    $sock->close();
}


# ----------------- CRC FUNCTION -----------------
sub computeCRC {
    my ($buf) = @_;
    my @bytes = unpack('C*', $buf);

    my $crc = 0;
    for my $dataOctet (@bytes) {
        for (0..7) {
            my $temp = ($crc ^ $dataOctet) & 1;
            $crc >>= 1;
            $dataOctet >>= 1;
            if ($temp) {
                $crc ^= 0xA6BC;
            }
        }
    }
    $crc = (~$crc) & 0xFFFF;
    return $crc;
}
