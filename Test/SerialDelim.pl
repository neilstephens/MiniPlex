#!/usr/bin/perl
use strict;
use warnings;
use Socket;

# Check for correct number of arguments
die "Usage: $0 <input_file> <snd_source_port> <snd_destination_ip> <snd_destination_port> <rcv_source_port> <rcv_destination_ip> <rcv_destination_port> <outpur_file>\n" 
    unless @ARGV == 8;

my ($input_file, $snd_source_port, $snd_dest_ip, $snd_dest_port, $rcv_source_port, $rcv_dest_ip, $rcv_dest_port, $output_file) = @ARGV;

#TODO: fork or exec etc, something to make these two calls execute concurrently:
do_udp($output_file, $rcv_source_port, $rcv_dest_ip, $rcv_dest_port);
do_udp($input_file, $snd_source_port, $snd_dest_ip, $snd_dest_port);

#TODO: diff the input_file and output_file


sub do_udp
{
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
		
	if($direction eq "send")
	{
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
			
			print "Sent: $hex_line\n";
			close($fh);
		}
	}
	else
	{
		# Open output file
		open(my $fh, '>', $filename) 
			or die "Could not open file '$filename' for writing: $!";
		
		# Read packets
		#TODO: read packets until idle for 2 sec
		#  write to file in hex format, one packet per line
	}

	close($sock);
}
