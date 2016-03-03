#!/bin/bash

if [ ! -f /usr/lib64/libcurl.so ]
then
 	echo "creating ln libcurl.so..."
	ln -s  /usr/lib64/libcurl.so.4 /usr/lib64/libcurl.so
fi

export ETCD_IP=54.222.135.148
export ETCD_PORT=2379

./serviceBrokerRedis -c ./config/serviceBroker.conf -n start

#nohup ./serviceBrokerRedis -c ./config/serviceBroker.conf -n start 2>&1 &
