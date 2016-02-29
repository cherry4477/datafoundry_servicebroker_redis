#!/bin/bash

if [ ! -f /usr/lib64/libcurl.so ]
then
 	echo "creating ln libcurl.so..."
	ln -s  /usr/lib64/libcurl.so.4 /usr/lib64/libcurl.so
fi

./serviceBrokerRedis -c ./config/serviceBroker.conf -n start

#nohup ./serviceBrokerRedis -c ./config/serviceBroker.conf -n start 2>&1 &
