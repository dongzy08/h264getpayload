//h264GetPayload.exe MCU_31.pcap_1 MCU_31in.h264_2 MCU_31in.h264rl_3 5006_4 5002_5 106_6 aaa.txt_7
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "h264frame.h"

typedef long long LONGLONG;
typedef unsigned int UINT;
typedef unsigned char BYTE;

int main(int argc, char * argv[])
{
	FILE * pppfile = fopen(argv[7],"wb");
	FILE * lostfile = fopen("lostSeq.txt","wb");

	FILE *infile = fopen(argv[1], "rb");
	if (infile == NULL)
	{
		printf("open input file failed.\n");
		return -1;
	}
	FILE * outfile = fopen(argv[2], "wb");
	if (outfile == 0)
	{
		fclose(infile);
		printf("open output file failed.\n");
		return -1;
	}
	FILE * rtpfile = fopen(argv[3], "wb");
	if (rtpfile == 0)
	{
		fclose(infile);
		fclose(outfile);
		printf("open rtpfile file failed.\n");
		return -1;
	}
	LONGLONG loffset = 0;
	UINT udpLen = 0;
	UINT rtpSn = 0;
	UINT lastRtpSn = 0;
	bool seqnumStartFlag = true;
	
	UINT timeStamp = 0, lastStamp = 0, curStamp = 0;

	UINT srcPort = atoi(argv[4]);
	UINT dstPort = atoi(argv[5]);
	UINT payloadType = atoi(argv[6]);

	BYTE * pTemp = new BYTE[1];
	BYTE * pData = new BYTE[4];
	BYTE * pType = new BYTE[1];
	BYTE * pPort = new BYTE[4]; 
	BYTE * pRTPPacket = NULL;
	memset(pType,0,1);
	memcpy(pType,(void *)&payloadType,1);
	memset(pPort,0,4);
	memcpy(pPort,(void *)&srcPort,2);
	memcpy(pPort+2,(void *)&dstPort,2);

	

	//reordering the data seq.
	*pTemp=*pPort;
	*pPort=*(pPort+1);
	*(pPort+1)=*pTemp;
	*pTemp=*(pPort+2);
	*(pPort+2)=*(pPort+3);
	*(pPort+3)=*pTemp;

	H264Frame* _rxH264Frame = new H264Frame();
	bool _gotIFrame = false;
	bool _gotAGoodFrame = false;
	int _frameCounter = 0;
	int _skippedFrameCounter = 0;

	while (!feof (infile))
	{
		fseek(infile,loffset,SEEK_SET);
		fread(pData,1,2,infile);
		fread(pData,1,2,infile);
		if(memcmp(pData,(pPort+2),2)==0) //use udp port to find rtp header.
		{
			//get udp length
			memset(pData,0,4);
			fseek(infile,loffset+4, SEEK_SET);
			fread(pData,1,4,infile);
			*pTemp=*pData;
			*pData=*(pData+1);
			*(pData+1)=*pTemp;
			memcpy(&udpLen,pData,2);

			//get rtp payload type.
			memset(pData,0,4);
			fseek(infile,loffset+9,SEEK_SET);
			fread(pData,1,1,infile);
			*pData=pData[0] & 0x7f;

			if(memcmp(pData,pType,1)==0)
			{
				//get rtp seq.
				memset(pData,0,4);
				fseek(infile,loffset+10,SEEK_SET);
				fread(pData,1,2,infile);
				*pTemp=*pData;
				*pData=*(pData+1);
				*(pData+1)=*pTemp;
				memcpy(&rtpSn,pData,2);
				//drop dup rtp packet
				if((rtpSn == lastRtpSn) && !seqnumStartFlag) 
				{
					loffset = loffset + udpLen;
				}
				else
				{
					//test lost rtp
					if (rtpSn > lastRtpSn+1)
						fprintf(lostfile,"lost rtp begin sequence number: %d           end sequence number: %d     interval  sequence number: %d \r\n",lastRtpSn, rtpSn, rtpSn - lastRtpSn);
					lastRtpSn = rtpSn;
					//get rtp timestamp
					memset(pData,0,4);
					fseek(infile,loffset+12,SEEK_SET);
					fread(pData,1,4,infile);							
					memcpy(&timeStamp,pData,4);

					curStamp = pData[0]*256*256*256 + pData[1]*256*256 + pData[2]*256 + pData[3];
					if((lastStamp > curStamp) && !seqnumStartFlag)
					{
						lastStamp = curStamp;
					}

					if(seqnumStartFlag == true)
						seqnumStartFlag = false;

					fseek(infile,loffset+8,SEEK_SET);
					UINT rtpLen = udpLen-8;							
					pRTPPacket = new BYTE[rtpLen];
					fread(pRTPPacket,1,rtpLen,infile);
					fprintf(pppfile,"rtp sequence number: %d           ",rtpSn);
					fprintf(pppfile,"rtp length: %d\n",rtpLen);

					//get h264 frame from rtp
					RTPFrame srcRTP(pRTPPacket, rtpLen); 

					unsigned int flags;
					if (!_rxH264Frame->SetFromRTPFrame(srcRTP, flags))  // 1.get the whole frame, otherwise drop it.
					{
						_rxH264Frame->BeginNewFrame();
						flags = (_gotAGoodFrame ? requestIFrame : 0);
						_gotAGoodFrame = false;
						delete []pRTPPacket;
						pRTPPacket=NULL;
						loffset=loffset+rtpLen+8;
					}
					if (srcRTP.GetMarker()==1) // 2.is marker, is whole frame 
					{
						if (_rxH264Frame->GetFrameSize()==0) // 3.empty frame
						{
							_rxH264Frame->BeginNewFrame();
							_skippedFrameCounter++;
							flags = (_gotAGoodFrame ? requestIFrame : 0);
							_gotAGoodFrame = false;
							delete []pRTPPacket;
							pRTPPacket=NULL;
							loffset=loffset+rtpLen+8;
						}
						else 
						{
							if (_gotIFrame == 0) // 4.ensure the first frame is I-frame 
							{
								if (!_rxH264Frame->IsSync()) //drop frames except I-frame
								{
									_rxH264Frame->BeginNewFrame();
									_gotAGoodFrame = false;
									delete []pRTPPacket;
									pRTPPacket=NULL;
									loffset=loffset+rtpLen+8;
									continue;
								}
								_gotIFrame = 1;
								continue;
							}
						}
						// 5.read file 
						int Framesize=_rxH264Frame->GetFrameSize();
						fwrite(_rxH264Frame->GetFramePtr(),1, _rxH264Frame->GetFrameSize(),outfile);
						fwrite(&Framesize,1,4,rtpfile);
						fwrite(_rxH264Frame->GetFramePtr(),1,Framesize,rtpfile);
						_rxH264Frame->BeginNewFrame();
					} 
					//end
					delete []pRTPPacket;
					pRTPPacket=NULL;
					loffset=loffset+rtpLen+8;
				}
			}

		}
		loffset++;

	}
	getchar();
	fflush(infile);
	fflush(outfile);
	fflush(rtpfile);
    fflush(pppfile);
	fflush(lostfile);
	fclose(infile);
	fclose(outfile);
	fclose(rtpfile);
    fclose(pppfile);
	fclose(lostfile);
	if(pData!=NULL)
		delete []pData;
	pData=NULL;    

	if (pRTPPacket!=NULL)
		delete []pRTPPacket;
	pRTPPacket=NULL;

	//if(infile!=NULL)
	//	delete infile;
	//infile=NULL;

	//if(outfile!=NULL)
	//	delete outfile;
	//outfile=NULL;

	//if (rtpfile!=NULL)
	//	delete rtpfile;
	//rtpfile=NULL;

	return 0;
}
