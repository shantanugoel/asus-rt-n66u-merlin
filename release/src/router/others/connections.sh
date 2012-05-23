#!/bin/sh
echo -e "Proto. \tSource \t\tPort \tDestination \tPort \tBytes \tState"
echo     "======================================================================"
cat /proc/net/nf_conntrack | grep "tcp" | grep "ESTABLISHED" | grep -v "UNREPLIED" | awk '{print $3 "\t" substr($7,5) "\t" substr($9,7) "\t" substr($13,5) "\t" substr($10,7) "\t" substr($12,7) "\t" substr($6,1)}' | sort
cat /proc/net/nf_conntrack | grep "tcp" | grep -v "ESTABLISHED" | grep -v "UNREPLIED" | awk '{print $3 "\t" substr($7,5) "\t" substr($9,7) "\t" substr($13,5) "\t" substr($10,7) "\t" substr($12,7) "\t" substr($6,1)}' | sort
echo
cat /proc/net/nf_conntrack | grep "udp" | grep -v "UNREPLIED" | awk '{print $3 "\t" substr($6,5) "\t" substr($8,7) "\t" substr($12,5) "\t" substr($9,7) "\t" substr($11,7)}' | sort

