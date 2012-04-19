#!/usr/bin/perl -w
use strict;
use warnings;
use File::Basename;

# ToolDir , search path , sh file

my $ToolHelp = shift;
my $FilePath = shift;
my $TempFilePath = shift;
my $DictEnum = shift;
my $DictNoFound = shift;
my $ModelName = shift;
my $HtmlPrepOutput = shift;
my $ShFile = ">".$TempFilePath;

my $FH;
open($FH, $ShFile);
printf $FH "#!/bin/sh\n";

my $location = $FilePath;
my $n = 0;
my $exclude_n = 0;
readsub($location);
close $FH;
print "\nFound $n file(s)!\n";
print "\nExclude $exclude_n file(s)!\n";
exit;


#
# add exclude list here
#
sub excludelist {
	my $fn = shift;
	$fn = lc($fn);
	if ($fn eq "jquery.js") { return 1; }
	if ($fn eq "svg.js") { return 1; }
	if ($fn eq "tablesorter.js") { return 1; }
	return 0;
}

#
# add extension here
#
sub ext_filter {
	my $ext = shift;
	$ext = lc($ext);	
	if ($ext eq "asp") { return 1; }
	if ($ext eq "htm") { return 1; }
	if ($ext eq "js") { return 1; }
	return 0;
}

#
# add NEW preprocessor command here
#
sub addcommand {
	my $fn = shift;
	$ModelName = uc($ModelName);
	 printf $FH $ToolHelp."/LnxHtmlPrep"." ".$fn." ".$ModelName." ".$HtmlPrepOutput."\n";
	 printf $FH $ToolHelp."/rmvcomments.pl"." ".$fn."\n";
	 printf $FH $ToolHelp."/LnxRmvTabs"." ".$fn."\n";
	 printf $FH $ToolHelp."/LnxHtmlEnumDict"." ".$fn." ".$DictEnum." ".$DictNoFound."\n";
}

sub readsub {
  my ($file_t) = @_;
  if (-f $file_t) {                  #if its a file
	 my $ext = ($file_t =~ m/([^.]+)$/)[0];
	 #print $ext;
	 my ($ret_ext)=&ext_filter($ext);
	 if ($ret_ext)
	 {
		 my $filename = basename( $file_t );
		 my ($ret_exclude)=&excludelist($filename);
		 if ($ret_exclude)
		 {
		 printf $file_t;
		 printf " in exclude list, skipped\n";
		 $exclude_n++;
		 }
		 else
		 {
		 print $file_t;	 
		 print "\n";
		 addcommand($file_t);
		 }
		$n++;                           #the total number of files		 
	 }
  } 
  if (-d $file_t) {                  #if its a directory
     opendir(AA,$file_t) || return;
     my @list = readdir(AA);
     closedir (AA);
     my $file_to_act;
     foreach $file_to_act (sort @list) {
       if ($file_to_act =~ /^\.|\.$/) { next; }
       else { readsub("$file_t/$file_to_act"); }
     }
  }
}

