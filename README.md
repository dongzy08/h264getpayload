# h264getpayload
get h264 raw data from rtp stream in *.pcap.
usage:
./hp.mak
then get ./bin/hp
run:
./bin/run.sh 
will run the test demo.
in run.sh, I write:
./hp 1.pcap 1.h264 1.h264.rl 5002 5004 106 l.txt
1.pcap: the pcap by tcpdump.
1.h264: output h264 raw data file name.
1.h264.rl: output other.
5002: rtp source port.
5004: rtp dest port.
106: rtp payload type.
1.txt: log.

also, it will output lostSeq.txt to test weather this rtp stream is ok.

