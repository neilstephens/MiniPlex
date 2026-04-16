#!/usr/bin/perl
use strict;
use warnings;
use IPC::Open3;
use Symbol qw(gensym);

my $DNP3Flows = "./Test/OneToOneDNP3Flows.pl";
if($ARGV[0])
{
	$DNP3Flows = $ARGV[0];
}

my $procs = do $DNP3Flows or die "Failed to load DNP3 send commands from $DNP3Flows";

# Launch all commands
for my $id (keys %{$procs}) {
    my $p = $procs->{$id};
    ($p->{in}, $p->{out}, $p->{err}) = (undef, undef, gensym());
    $p->{pid} = open3($p->{in}, $p->{out}, $p->{err}, $p->{command});
}

# Wait and collect output
for my $id (keys %{$procs}) {
    my $p = $procs->{$id};
    waitpid($p->{pid}, 0);
    $p->{stdout} = do { local $/; readline($p->{out}) };
    $p->{stderr} = do { local $/; readline($p->{err}) };
    $p->{exit} = $? >> 8;
}

# Display results
my $success = 1;
for my $id (sort keys %{$procs}) {
    my $p = $procs->{$id};
    print "[$id] (exit: $p->{exit})\n";
    print "STDOUT:\n$p->{stdout}\n" if $p->{stdout};
    print "STDERR:\n$p->{stderr}\n" if $p->{stderr};
    
    $p->{stdout} =~ /^(0x[0-9A-F]{4}->0x[0-9A-F]{4}) : ([0-9]+)/;
    my $top_flow = $1;
    my $top_count = int($2);
    my $expected_flow = $id;
    
    if($top_flow ne $expected_flow or $top_count < 49)
    {
		print "Top flow ($top_flow) != Expected flow ($expected_flow) or Top count ($top_count) < 49\n";
		$success = 0;
	}
}

die "FAILED\n" if($success == 0);
print "SUCCESS\n" if($success == 1);
exit(0);
