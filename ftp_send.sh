#!/bin/bash
# $1 IP, $2 port, $3 file name
ftp -vi $1 $2 <<< "put $PWD/$3"
