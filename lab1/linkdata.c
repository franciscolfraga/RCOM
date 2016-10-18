#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <sys/signal.h>
#include <sys/types.h>
#include <strings.h>

#include "linkdata.h"
#include "alarm.h"
struct termios oldtio,newtio;
Frame reader;


void setuplink(char* sport, int sportfd,int id){
	//new termios
	printf("\tSetting up data link layer info...\n");
	if(setupTermios(sportfd)<0){
		printf("\n\tError on setup of termios!\n");
		exit(-1);
	}

	//ver boas práticas e mudar
	//setup da struct
	linkinfo.port = sport;
	linkinfo.baudRate = 4800; //default minha (depois costumizar)
	linkinfo.sequenceNumber = 0;
	linkinfo.timeout = 1;
	linkinfo.numTransmissions = 3;
	printf("\tAll set in data link layer info!\n");
	if(llopen(sportfd , id)<0){
		printf("\n\tError setting up llopen!\n");
		exit(-1);
	}

}


int setupTermios(int fd){
	//save old serial port settings, add control later
	printf("\tSaving current settings of termios...\n");
	tcgetattr(fd , &oldtio);
	printf("\tOld termios settings saved!\n");
	//setup of new Termios (asynchronous as asked)
	printf("\tSetting up new termios...\n");
	bzero(&newtio, sizeof(newtio));
    newtio.c_cflag = linkinfo.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR | ICRNL;
    newtio.c_oflag = 0;
    newtio.c_lflag = ICANON;
    newtio.c_cc[VMIN] = 1;
    newtio.c_cc[VTIME] = 0.5;
    tcflush(fd , TCIFLUSH);
    tcsetattr(fd , TCSANOW,&newtio);
    printf("\tNew termios setup done!\n");
    //add control later
    return 1;
}


int llopen(int fd , int id){
	int counter = 0;
	received=false;
	printf("\tSetting up connection\n");

		if(id == SENDER){
			//add alarm later
			while(counter < linkinfo.numTransmissions && received==false){
				if(	buzz==1 || counter == 0){
					printf("\tNr.%d buzz now\n",counter);
					buzz=0;
					counter++;
					setAlarm();
					sendcmd(fd,3,id); //DISC
				}
				//precisamos de checkar se a frame que recebemos é válida, fazer uma estrutura para a frame em si
				if(recmachine(fd,id)){
					received=true;
					printf("\tExchanging data...\n");
				}
			}
			if(counter <= linkinfo.numTransmissions && received==true){
				stopAlarm();
				printf("\tConnected with receiver!\n");
			}
			else{
				printf("\tError, reached max retries!\n");
				exit(-1);
			}
		}
		else if(id == RECEIVER){
			if(recmachine(fd , id))
				printf("\tConnected with sender!\n");
		}
	return 1;
}
int sendcmd(int fd, int frametype, int id) {
	unsigned char frame[FRAME_SIZE];

	frame[0] = FLAG;
	frame[4] = FLAG;

	switch(frametype) {
		case 1:
		//SET
			frame[1] = getA(1,id);
			frame[2] = CSET;
			frame[3] = frame[1] ^ frame[2];
			break;
		case 2:
		//UA
			frame[1] = getA(2,id);
			frame[2] = CUA;
			frame[3] = frame[1] ^ frame[2];
			break;
		case 3:
		//DISC
			frame[1] = getA(1,id);
			frame[2] = CDISC;
			frame[3] = frame[1] ^ frame[2];
			break;
		//ver rr e rej
		default:
			printf("\tERROR sending frame (unexpected frame)\n");
			exit(-1);
			break;
	}
	printf("\tFrame setup complete!\n");
	if(frame[1] == 0) {
		printf("\tERROR sending frame (unnexpected status)\n");
		exit(-1);
	}

	if (write(fd, frame, FRAME_SIZE) != FRAME_SIZE) {
		printf("\tERROR sending frame (size don't match)\n");
		exit(-1);
	}

	return 0;
}

unsigned char getA(int type,int id){
	switch(type){
		case 1:
			if(id == SENDER)
				return A03;
			else if(id==RECEIVER)
				return A01;
			else{
				printf("\tERROR setting up frame\n");
				exit(-1);
			}
		case 2:
			if(id==SENDER)
				return A01;
			else if( id == RECEIVER )
				return A03;
			else{
				printf("\tERROR setting up frame\n");
				exit(-1);
			}
	}
	return 0;
}

int recmachine(int fd , int id){
	int frameloading = 0 , state = 0;
	unsigned char info;
	//printf("\tEntering receive state machine...\n");
	//check prof state machine and update it (add prints), I will do the basics
	while(frameloading!=1){
		break;
		switch(state){
			case 0:
				info=callRead(fd);
				if(info==FLAG){
					reader.frame[state]=info;
					state++;
				}
				else
					state=0;
			case 1:
				info=callRead(fd);
				if(info==A01 || info==A03){
					reader.frame[state]=info;
					state++;
				}
			case 2:
				info=callRead(fd);
				if(info==CSET || info==CUA || info==CRR || info==CREJ || info==CDISC){
					reader.frame[state]=info;
					state++;
				}
			case 3:
				info=callRead(fd);
				if(info==(reader.frame[1]^reader.frame[2])){
					reader.frame[state]=info;
					state++;
				}
			case 4:
				info=callRead(fd);
				if(info==FLAG){
					reader.frame[state]=info;
					state++;
					reader.frame[state]='\0';
					frameloading=1;
					break;
				}
			default:
				state=0;
		}
	}
	return 0;
}

unsigned char callRead(int fd){
	unsigned char info;
	int res;
	res=read(fd,&info,1);
	if(res>1)
		printf("\tError, (corrupted info received) can't continue\n");
	return info;
}