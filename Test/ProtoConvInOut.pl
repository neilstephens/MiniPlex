#!/usr/bin/perl
use strict;
use warnings;
use Socket;
use IO::Select;
use Time::HiRes qw(sleep);

# Check for correct number of arguments
die "Usage: $0 <input_file> <snd_source_port> <snd_destination_ip> <snd_destination_port> <rcv_source_port> <rcv_destination_ip> <rcv_destination_port> <output_file> <diff_file>\n" 
    unless @ARGV == 9;

my ($input_file, $snd_source_port, $snd_dest_ip, $snd_dest_port, $rcv_source_port, $rcv_dest_ip, $rcv_dest_port, $output_file, $diff_file) = @ARGV;

# Fork to run send and receive operations concurrently
my $pid = fork();

die "Fork failed: $!" unless defined $pid;

if ($pid == 0) {
    # Child process - receive
    do_udp($output_file, $rcv_source_port, $rcv_dest_ip, $rcv_dest_port, "receive");
    exit 0;
} else {
    # Parent process - send
    # Small delay to ensure receiver starts first
    sleep(0.5);
    
    do_udp($input_file, $snd_source_port, $snd_dest_ip, $snd_dest_port, "send");
    
    # Wait for child process to complete
    waitpid($pid, 0);
    
    # Diff the input and output files
    if(system("diff", $diff_file, $output_file) != 0)
    {
		die "Test Failed. Diff does not pass";
	}
	print "Test Passed.\n";
	exit 0;
}

sub do_udp {
    my ($filename, $source_port, $dest_ip, $dest_port, $direction) = @_;
    
    # Create UDP socket
    socket(my $sock, PF_INET, SOCK_DGRAM, getprotobyname('udp')) 
        or die "Could not create socket: $!";

    # Bind to specific source port
    bind($sock, sockaddr_in($source_port, INADDR_ANY)) 
        or die "Could not bind to source port: $!";

    # Resolve destination address
    my $dest_addr = inet_aton($dest_ip) 
        or die "Could not resolve destination IP: $!";

    # Prepare destination socket address
    my $dest_sock_addr = sockaddr_in($dest_port, $dest_addr);
        
    if ($direction eq "send") {
        # Open input file
        open(my $fh, '<', $filename) 
            or die "Could not open file '$filename' for reading: $!";
            
        # Process each line
        while (my $hex_line = <$fh>)
        {
            chomp $hex_line;
            
            # Convert hex string to binary
            my $binary_data = pack("H*", $hex_line);
            
            # Send UDP packet
            send($sock, $binary_data, 0, $dest_sock_addr) 
                or die "Could not send packet: $!";
        }
        close($fh);
    }
    else #receive
    {
        # Open output file
        open(my $output_fh, '>', $filename) 
            or die "Could not open file '$filename' for writing: $!";
        
        # Create a selector for handling timeouts
        my $sel = IO::Select->new($sock);
        
        # Receive packets with a 2-second idle timeout
        while (1)
        {
            my @ready = $sel->can_read(2);  # 2-second timeout
            
            if (@ready) {
                my $packet;
                my $peer_addr = recv($sock, $packet, 65535, 0);
                
                if (defined $peer_addr) {
                    # Convert received binary data to hex
                    my $hex_packet = unpack("H*", $packet);
                    
                    # Write hex packet to file
                    print $output_fh "$hex_packet\n";
                }
            } else {
                # No packets received for 2 seconds, exit loop
                last;
            }
        }
        
        close($output_fh);
    }

    close($sock);
}
