#!/usr/bin/perl
use strict;
use warnings;
use IPC::Open3;
use Symbol qw(gensym);

my %procs = (
    '01' => { command => './SendDNP3.pl 20011 127.0.0.1 20005 01 02 50 10' },
    '02' => { command => './SendDNP3.pl 20012 127.0.0.1 20005 02 01 50 10' },
    '03' => { command => './SendDNP3.pl 20013 127.0.0.1 20005 03 04 50 10' },
    '04' => { command => './SendDNP3.pl 20014 127.0.0.1 20005 04 03 50 10' },
    '05' => { command => './SendDNP3.pl 20015 127.0.0.1 20005 05 06 50 10' },
    '06' => { command => './SendDNP3.pl 20016 127.0.0.1 20005 06 05 50 10' },
    '07' => { command => './SendDNP3.pl 20017 127.0.0.1 20005 07 08 50 10' },
    '08' => { command => './SendDNP3.pl 20018 127.0.0.1 20005 08 07 50 10' },
    '09' => { command => './SendDNP3.pl 20019 127.0.0.1 20005 09 10 50 10' },
    '10' => { command => './SendDNP3.pl 20020 127.0.0.1 20005 10 09 50 10' },
);

# Launch all commands
for my $id (keys %procs) {
    my $p = $procs{$id};
    ($p->{in}, $p->{out}, $p->{err}) = (undef, undef, gensym());
    $p->{pid} = open3($p->{in}, $p->{out}, $p->{err}, $p->{command});
}

# Wait and collect output
for my $id (keys %procs) {
    my $p = $procs{$id};
    waitpid($p->{pid}, 0);
    $p->{stdout} = do { local $/; readline($p->{out}) };
    $p->{stderr} = do { local $/; readline($p->{err}) };
    $p->{exit} = $? >> 8;
}

# Display results
my $success = 1;
for my $id (sort keys %procs) {
    my $p = $procs{$id};
    print "[$id] (exit: $p->{exit})\n";
    print "STDOUT:\n$p->{stdout}\n" if $p->{stdout};
    print "STDERR:\n$p->{stderr}\n" if $p->{stderr};
    
    $p->{stdout} =~ /^0x[0-9A-F]{4}->(0x[0-9A-F]{4}) : ([0-9]+)/;
    my $top_addr = hex($1);
    my $top_count = int($2);
    my $self_addr = int($id);
    
    if($top_addr != $self_addr or $top_count < 49)
    {
		print "Top address ($top_addr) != Self address ($self_addr) or Top count ($top_count) < 49\n";
		$success = 0;
	}
}

die "FAILED\n" if($success == 0);
print "SUCCESS\n" if($success == 1);
exit(0);
