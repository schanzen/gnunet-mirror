#!/bin/sh
cat log | grep "Mesh for peer" > __tmp_peers
cat __tmp_peers | while read line; do
    PEER=`echo $line | sed -e 's/.*\[\(....\)\].*/\1/'`
    PID=`echo $line | sed -e 's/.*mesh-\([0-9]*\).*/\1/'`
    echo "$PID => $PEER"
    cat log | sed -e "s/mesh-$PID/MESH $PEER/" > __tmp_log
    mv __tmp_log log
done 

rm __tmp_peers

cat log | sed -e 's/mesh-api-/mesh-api-                                            /g' > __tmp_log
mv __tmp_log log

kwrite log --geometry 960x1140-960 &

