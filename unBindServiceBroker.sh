#!/bin/bash

#set -x 
if [ $# -ne 1 ]
then
	echo "param is missing"
	exit
else

	port=$1
fi

pid=`ps -ef|grep redis.${port}.conf|grep -v grep|awk '{print $2}'`

kill $pid


