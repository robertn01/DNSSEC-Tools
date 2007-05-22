#!/usr/bin/perl
#
# Copyright 2004-2006 SPARTA, Inc.  All rights reserved.  See the COPYING
# file distributed with this software for details
#

use Net::DNS;
use Net::DNS::SEC::Tools::conf;
use Net::SMTP;
use Getopt::Long;
use Sys::Syslog;
use IO::File;
use Net::DNS::SEC::Tools::BootStrap;

######################################################################
# detect needed perl module requirements
#
dnssec_tools_load_mods('Net::DNS::SEC' => "");

########################################################
# Defaults

my %opts = (
        t => 3600, # default to one hour
        v => 1,
        c => 0
);

my $daemon = 1; # default is to run as a daemon

########################################################
# main

# Parse command-line options
LocalGetOptions(\%opts,
		['GUI:screen',           'TrustMan Options'],
		['d|domain=s',
		 'Comma separated list of new domain names to check'],

		['GUI:separator',        'Daemon Options:'],
		['t|sleeptime=i',        'Time to sleep between checks'],
		['f|foreground|fg',
		 'Run in the foreground, checking only once'],

		['GUI:separator',        'Configuration file options:'],
		['c|config',             'Read the dnssec-tools config file'],
		['o|outfile=s',          'Output configuration file'],
		['n|named_conf_file=s',  'The named.conf file to read'],
		['k|dnsval_conf_file=s', 'The dnsval.conf file to read'],

		['GUI:separator',        'Output options:'],
		['L|syslog',             'Log results to syslog'],
		['p|print',              'Print results to stdout'],
		['N|no_error',           'Display good results, not just bad'],
		['v|verbose',            'Turn on Verbose Output'],

		['GUI:separator',        'EMail output options:'],
		['m|mail_contact_addr=s', 'E-Mail results to address'],
		['s|smtp_server=s',     'The SMTP server to send mail through'],
                ['V|version', 'Version'],
		);

# Parse the dnssec-tools.conf file
my %dtconf = parseconfig();

my $ncfile = $opts{'n'} ? $opts{'n'}
                        : $dtconf{'tanamedconffile'};

my $dvfile = $opts{'k'} ? $opts{'k'}
                        : $dtconf{'tadnsvalconffile'};

my $contactaddr = $opts{'m'} ? $opts{'m'}
                             : $dtconf{'tacontact'};

my $smtpserver =  $opts{'s'} ? $opts{'s'}
                             : $dtconf{'tasmtpserver'};

my $sleeptime = $opts{'t'} ? $opts{'t'}
                           : $dtconf{'tasleeptime'};

if ((!$contactaddr) && (!$opts{'L'}) && (!$opts{'p'})) {
    usage();
}

if ($opts{'f'}) {
    $daemon = 0;
    &checkkeys;
} elsif ($opts{'c'}) {
    my $conffile = getconffile();
    my $didnconf = 0;
    my $didvconf = 0;
    my $didtime = 0;
    my $didcontact = 0;
    my $didsmtp = 0;
    open(CONF,$conffile) or die "unable to open \"$conffile\".";
    usage () unless $opts{'o'};
    open(OUT,">$opts{'o'}") or die "unable to open \"$opts{'o'}\" for writing.";
    while(<CONF>) {
        next if (/^tasleeptime/ && ($opts{'t'}));
        next if (/^tasmtpserver/ && ($opts{'s'}));
        next if (/^tacontact/ && ($opts{'m'}));
        next if (/^tanamedconffile/ && ($opts{'n'}));
        next if (/^tadnsvalconffile/ && ($opts{'k'}));
        print OUT $_;
    }
    if ($opts{'t'}) {
        print OUT "tasleeptime\t" . $sleeptime . "\n";
    }
    if ($opts{'s'}) {
        print OUT "tasmtpserver\t" . $smtpserver . "\n";
    }
    if ($opts{'m'}) {
        print OUT "tacontact\t" . $contactaddr . "\n";
    }
    if ($opts{'n'}) {
        print OUT "tanamedconffile\t" . $ncfile . "\n";
    }
    if ($opts{'k'}) {
        print OUT "tadnsvalconffile\t" . $dvfile . "\n";
    }
    close (OUT);
    close (CONF);

} else {
    &daemonize;
    while (1) {
        &checkkeys;
        sleep($sleeptime);
    }
} 

sub show_version {
    print STDERR "Version: 0.7\n";
    print STDERR "DNSSEC-Tools Version: 1.2\n";
    exit(1);
}

sub usage {
    print "TrustMan [-d domain] [-L] [-f] [-c] [-v] [-V]\n";
    print "\t[-o outfile] [-m mailcontact] [-s smtpserver]\n";
    print "\t[-t secs] [-n named_conf_file] [-k dnsval_conf_file]\n";
    print "\tuse the -f option to run in the foreground.\n";
    print "\tUse -L to log to syslog; this can be in addition to mail.\n";
    print "\tIf a domain is not specified, all domains in the key_containing_files will be checked.\n";
    print "\tIf no key_containing_files are specified, dnssec-tools.conf will be
parsed for appropriate files.\n";
    print "\tWhen running the configure option (-c or --config), you MUST specify an output file (-o).\n";
    exit(1);
}

sub checkkeys {
    my %keystorage;
    my @baddomains;
    my @gooddomains;
    my @maybe_dotted_domains;
    my @domains;
    my $named_conf_pat = "trusted-keys";
    my $dnsval_conf_pat = "trust-anchor";
    push @maybe_dotted_domains, split(/,/,$opts{'d'}) if ($opts{'d'});
    foreach my $d (@maybe_dotted_domains) {
        $d =~ s/\.$//;
        push @domains, $d;
    }
    my $res = Net::DNS::Resolver->new;
    # needed to ensure that +dnssec is used on subsequent queries
    my $setdnssec = $res->dnssec(1);
    
    read_conf_file($named_conf_pat, \%keystorage, $ncfile) if ($ncfile);
    read_conf_file($dnsval_conf_pat, \%keystorage, $ncfile) if ($ncfile);

# if a domain is specified on the command line, we will only
# check that domain. Otherwise, check all domains found in config files.
    if (!exists ($domains[0])) {
        foreach my $k (keys(%keystorage)) {
            push @domains, $k;
        }
    }
    
    if (!@domains) {
        print "No domains to check, exiting....\n";
        exit(1);
    }

    foreach my $d (@domains) {
        my $rrsigquery = $res->query($d,"RRSIG");
        foreach my $rr ($rrsigquery->answer) {
            my $ottl = $rr->orgttl;
            my $sigexp = $rr->sigexpiration;
            my $domainname = $rr->name;
        # do I need the sig?
            my $sig = $rr->sig;
        }
        my $query = $res->query($d,"DNSKEY");

        if ($query) {
            foreach my $keyrec (grep { $_->type eq 'DNSKEY' } $query->answer) {
                next if (!($keyrec->flags & 1));
                my $ttl = $keyrec->ttl;
                my $key = $keyrec->key;
                $key =~ s/\s+//g; # remove all spaces   
                $nonmatch = compare_keys(\%keystorage, $d, $keyrec, $key);
                if ($nonmatch) {
                    push @baddomains, $d;
                } else {
                    push @gooddomains, $d;
                }
            }
            
        } else {
# TODO see notes under Active Refresh for how to handle query failure
            print "query failed for domain " . $d . ": " . $res->errorstring . "\n";
        }
    }
    if (@baddomains) {
        if ($contactaddr) { # mail it
            mailcontact(0,$smtpserver,$contactaddr,@baddomains);
        }
        foreach my $d (@baddomains) {
            if ($opts{'L'}) { # log it to syslog
                openlog('TrustMan','pid','user') || warn "could not open syslog";
                syslog('warning', %s, "DNSKEY mismatch for zone $d");
                closelog();
            }
            if ($opts{'p'}) {
                # write to stdout
                print "DNSKEY mismatch for zone $d\n";
            }
        }
    } elsif ($opts{'N'}) {
        if ($contactaddr) { # mail it
            mailcontact(1,$smtpserver,$contactaddr,@domains);
        }
        foreach my $d (@gooddomains) {
            if ($opts{'L'}) { # log it to syslog
                openlog('TrustMan','pid','user') || warn "could not open syslog";
                syslog('warning', %s, "DNSKEY okay for zone $d");
                closelog();
            }
            if ($opts{'p'}) {
                # write to stdout
                print "DNSKEY okay for zone $d\n";
            }
        }
    }
        
}

#######################################################################
# read_conf_file()
#
# reads in a config file pointed to by $file
# looks for trust anchors using $pat and stores key
# information in $storage
#
sub read_conf_file {
    my ($pat, $storage, $file) = @_;
    Verbose("reading and parsing trust keys from $file\n");
    # regexp pulled from Fast.pm
    my $pat_maybefullname = qr{[-\w\$\d*]+(?:\.[-\w\$\d]+)*\.?};

    my $fh = new IO::File;
    if (!$fh->open("<$file")) {
	print STDERR "Could not open named configuration file: $file\n";
	exit (1);
    }
    while (<$fh>) {
	if (/$pat {/) {
	    while (<$fh>) {
		last if (/^\s*};/);
		if (/\s*($pat_maybefullname)\s+(256|257)\s+(\d+)\s+(\d+)\s+\"(.+)\"\s*;/) {
                    my $domainname = $1;
                    $domainname =~ s/\.$//;
		    push @{$storage->{$domainname}},
		      { flags => $2,
			protocol => $3,
			algorithm => $4,
			key => $5 };
		    $storage->{$domainname}[$#{$storage->{$domainname}}]{key} =~ s/\s+//g;
		}
	    }
	}
    }
    $fh->close;
}

######################################################################
# mailcontact()
#  - emails a contact address with the error output
sub mailcontact {
    my ($ok,$smtp,$contact,@domains) = @_;
    my $fromaddr = 'TrustMan@localhost';

    # set up the SMTP object and required data
    my $message = Net::SMTP->new($smtp) || die "failed to create smtp message";
    $message->mail($fromaddr);
    $message->to(split(/,\s*/,$contact));
    $message->data();

    # create headers
    $message->datasend("To: " . $contact . "\n");
    $message->datasend("From: " . $fromaddr . "\n");

    # create the body of the message: the warning
    if ($ok) {
        $message->datasend("Subject: TrustMan all clear\n\n");
        $message->datasend("TrustMan detected no DNSKEY mismatches for the following zones: \n\n");
    } else {
        $message->datasend("Subject: TrustMan warning -- DNSKEY mismatches\n\n");
        $message->datasend("TrustMan has detected DNSKEY mismatches for the following zones: \n\n");
    }
    foreach my $d (@domains) {
        $message->datasend("\t" . $d . "\n");
    }
    if (!$ok) {
        $message->datasend("\n\nYou should verify mismatched keys manually.\n\n");
    }

    # finish and send the message
    $message->dataend();
    $message->quit;
}

#######################################################################
# compare_keys()
#
# compares the contents of two keys to see if the new one ($domain,
# $rec, and $keyin) matches the cached one previously remembered (in
# $storage->{$domain} )
#
sub compare_keys {
    my ($storage, $domain, $rec, $keyin) = @_;
    if (!exists($storage->{$domain})) {
# What would nonexistence of this really mean?
#	print STDERR "  Found a key for $domain; previously we had none cached\n";
    }
    my $keys = $storage->{$domain};
    foreach my $key (@$keys) {
	if ($key->{'flags'} eq $rec->flags &&
	    $key->{'protocol'} eq $rec->protocol &&
	    $key->{'algorithm'} eq $rec->algorithm &&
	    $key->{'key'} eq $keyin) {
	    # the key exactly matches a stored key
	    $key->{'found'} = 1;
	    return 0;
	}
    }
    return 1;
}

#######################################################################
# Verbose()
#
# prints something(s) to STDERR only if -v was specified.
#
sub Verbose {
    print STDERR @_ if ($opts{'v'});
}

####################################################################
# daemonize
# 
# run as a daemon

sub daemonize {
  chdir '/' or die "Can't chdir to /: $!";
  open STDIN, '/dev/null' or die "Can't read /dev/null: $!";
  open STDERR, '>/dev/null' or die "Can't write to /dev/null: $!";
  defined(my $pid = fork) or die "Can't fork: $!";
  exit if $pid;
  setsid or die "Can't start a new session: $!";
  umask 0;
}

#######################################################################
# Getopt::GUI::Long portability
#
# will be used in a near-future version

sub LocalGetOptions {
    if (eval {require Getopt::GUI::Long;}) {
	require Getopt::GUI::Long;
	import Getopt::GUI::Long;
	Getopt::GUI::Long::Configure(qw(display_help no_ignore_case));
	return GetOptions(@_);
    }
    require Getopt::Long;
    import Getopt::Long;
    Getopt::Long::Configure(qw(auto_help no_ignore_case));
    GetOptions(LocalOptionsMap(@_));
}

sub LocalOptionsMap {
    my ($st, $cb, @opts) = ((ref($_[0]) eq 'HASH') 
			    ? (1, 1, $_[0]) : (0, 2));
    for (my $i = $st; $i <= $#_; $i += $cb) {
	if ($_[$i]) {
	    next if (ref($_[$i]) eq 'ARRAY' && $_[$i][0] =~ /^GUI:/);
	    push @opts, ((ref($_[$i]) eq 'ARRAY') ? $_[$i][0] : $_[$i]);
	    push @opts, $_[$i+1] if ($cb == 2);
	}
    }
    return @opts;
}

=pod

=head1 NAME

TrustMan - manage keys used as trust anchors

=head1 SYNOPSIS

  TrustMan [options]

=head1 DESCRIPTION

B<TrustMan> runs by default as a daemon to verify if keys stored locally in
configuration files like B<named.conf> still match the same keys as fetched
from the zone where they are defined.  If mismatches are detected, the daemon
notifies the contact person defined in the config file or on the command line
by mail.

B<TrustMan> can also be run in the foreground (I<-f>) to run this check a
single time.

B<TrustMan> can also be used to set up configuration data in the file
B<dnssec-tools.conf> for later use by the daemon, making fewer command line
arguments necessary. Configuration data are stored in B<dnssec-tools.conf>.
The current version requires the B<dnssec-tools.conf> file to be edited by
hand to add values for the contact person's email address (I<tacontact>) and the
SMTP server (I<tasmtpserver>).  Also, the location of B<named.conf>
and B<dnsval.conf> must also be added to that file, if necessary.

=head1 OPTIONS

=over #indent

=item -f

Run in the foreground.

=item -c

Create a configure file for B<TrustMan> from the command line options given.

=item -o

Output file for configuration.

=item -k 

A B<dnsval.conf> file to read.

=item -n 

A B<named.conf> file to read.

=item -d

The domain to check (supersedes configuration file.)

=item -t

The number of seconds to sleep between checks.  Default is 3600 (one hour.)

=item -m

Mail address for the contact person to whom reports should be sent.

=item -p

Log messages to I<stdout>.

=item -L

Log messages to B<syslog>.

=item -s

SMTP server B<TrustMan> should use to send reports.

=item -N

Send report when there are no errors.

=item -v

Verbose.

=item -V

Version.

=back #unindent
=head1 PRE-REQUISITES

=head1 COPYRIGHT

Copyright 2006 SPARTA, Inc.  All rights reserved.
See the COPYING file included with the DNSSEC-Tools package for details.

=head1 SEE ALSO

B<dnssec-tools.conf(5)>,
B<dnsval.conf(5)>,
B<named.conf(5)>

=cut
