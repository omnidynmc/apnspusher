#!/bin/bash

botname='apnspusher'
sleepfor=10

svn update
if [ $? -ne 0 ]; then
  echo "svn update failed, bailing"
  exit 1
fi

make makeall
if [ $? -ne 0 ]; then
  echo "makeall failed, bailing"
  exit 1
fi

if test -r $botname.pid; then
  botpid=`cat $botname.pid`
  kill -INT $botpid
  while `kill -CHLD $botpid >/dev/null 2>&1`
  do
      # it's still going
      echo "Still running, waiting $sleepfor seconds"
      sleep $sleepfor
  done
fi

echo ""
echo "Shutdown detected, restarting"
echo "Stale $botname.pid file (erasing it)"
rm -f $botname.pid

./$botname.botchk
