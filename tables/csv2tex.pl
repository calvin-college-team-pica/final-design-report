#!/usr/bin/perl

use strict;
use warnings;

use utf8;

use autodie qw(open close);
use English qw( -no_match_vars );
use Getopt::Long;
use Pod::Usage;
use Carp;

use Text::CSV;
use File::Basename;
use Cwd;

use Template;

use LaTeX::Table;
use LaTeX::Encode;
#use LaTeX::Driver;

use version; our $VERSION = qv('1.0.0');

my ( $infile, $outfile, $outfiletex, $help, $man, $version );

my $sep_char     = q{,};
my $latex_encode = 0;
my $landscape    = 0;
my $outputlatex  = 1;
my $theme        = 0;
my $title        = 0;
my $coldef       = 0;
my $header       = 0;
my $caption      = "default";
my $fontsize     = '';

my $options_ok = GetOptions(
    'in=s'         => \$infile,
    'out=s'        => \$outfile,
    'sep_char=s'   => \$sep_char,
    'caption=s'    => \$caption,
    'fontsize=s'   => \$fontsize,
    'latex_encode' => \$latex_encode,
    'landscape'    => \$landscape,
    'outputlatex'  => \$outputlatex,
    'theme=s'      => \$theme,
    'title=s'      => \$title,
    'coldef=s'     => \$coldef,
    'header=s'     => \$header,
    'help|?'       => \$help,
    'version|v'    => \$version,
    'man'          => \$man,
) or pod2usage(2);

if ($version) {
    print "$PROGRAM_NAME $VERSION\n" or croak q{Can't print to stdout.};
    exit;
}

if ($man) {
    pod2usage( -exitstatus => 0, -verbose => 2 );
}
if ( $help || !defined $infile ) {
    pod2usage(1);
}
if ( !defined $outfile ) {
    $outfile = q{./} . fileparse( $infile, qw(csv txt dat) ) . 'pdf';
    $outfiletex = fileparse( $infile, qw(csv txt dat) ) . 'tex';
}
else {
    $outfiletex = fileparse( $outfile, qw(pdf) ) . 'tex';
}

my $csv = Text::CSV->new(
    {   binary           => 1,
        sep_char         => $sep_char,
        allow_whitespace => 1
    }
);

my @header;
my @data;
my @rowcolor = ['\rowcolor{blue}'];
my $columns = $coldef =~ s/([[:ascii:]])/$1/g;
$columns = ($columns-1) / 2;
print "No. Columns: " . $columns . "\n";;

if ($header) {
    my $status = $csv->parse($header);
    @header = [ $csv->fields() ];
}

my $line_number = 0;

open my $IN, '<', $infile;
while ( my $line = <$IN> ) {
    chomp $line;
    my $status = $csv->parse($line);
    if ( !$header && $line_number == 0 ) {
        @header = [ $csv->fields() ];
        unshift(@header, ['\rowcolor{blue}']);
        unshift(@header, ['\hline']);
        push(@header, ['\hline']);
    }
    else {
        push @data, [ $csv->fields() ];
        print "mod: " . ($line_number-1) % $columns . "\n";
#        if ( (($line_number-1) % $columns) == 0){
            print "Adding hline\n";
            push(@data, ['\hline']);
#        }
    }
    $line_number++;
}
close $IN;
push( @data, ['\hline']);

my $table = LaTeX::Table->new(
    {   header      => \@header,
        data        => \@data,
        type        => 'longtable',
        environment => 'longtable',
        coldef      => $coldef,
        caption_top => 1,
        caption     => $caption,
        fontsize    => $fontsize,
        label       => $outfiletex,
        filename    => $outfiletex,
    }
);
$table->set_theme('plain');
$table->generate;


# vim: ft=perl sw=4 ts=4 expandtab

