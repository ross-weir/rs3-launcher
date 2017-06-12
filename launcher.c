#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>
#include <endian.h>
#include <unistd.h>
#include <limits.h>

#define FIFO_PREFIX "/tmp/RS2LauncherConnection_"

void print_message(const char *prefix, const char *msg, int length){
	printf("%smsg [ id=%02x%02x data=", prefix, msg[0], msg[1]);
	for (int i = 2; i < length; ++i){
		printf("%02x ", (unsigned char) msg[i]);
	}
	printf("] %iB\n", length-2);
}

/*
	launch parameters:
	0=executable name 
	1=cache folder  2=user folder
	3=pid
*/ 
int main(int argc, char *argv[]){
	
	// Size and assemble FIFO locations
	char in_fifo[sizeof(FIFO_PREFIX) + sizeof(argv[3]) + sizeof("_i")];
	char out_fifo[sizeof(FIFO_PREFIX) + sizeof(argv[3]) + sizeof("_o")];
	snprintf(in_fifo, sizeof in_fifo, "%s%s%s", FIFO_PREFIX, argv[3], "_i");
	snprintf(out_fifo, sizeof out_fifo, "%s%s%s", FIFO_PREFIX, argv[3], "_o");
	
	// Make FIFOs
	mkfifo(in_fifo, 0600);
	mkfifo(out_fifo, 0600);

	// Open FIFOs as files
	int fd_in = open(in_fifo, O_RDONLY);
	int fd_out = open(out_fifo, O_WRONLY);

	// store message size bytes	
	char buf[2];

	// Loop until unable to read a message
	while(read(fd_in, buf, sizeof buf)){
		// convert message size bytes to decimal
		uint16_t msg_len = le16toh(*(uint16_t *) buf);
		
		if(msg_len < 2){
			printf("Message size recieved is invalid.\n");
			break;
		}
		
		char msg[msg_len];
		if(!read(fd_in, msg, msg_len)){
			printf("Failed to read meaningful messaage.\n");
			break;
		}

		// handle message
		char msg_id[2] = {msg[0], msg[1]};
		uint16_t id = be16toh(*(uint16_t *) msg_id);
		print_message("GOT: ", msg, msg_len);
		if(id == 0) {
			// calculate reply size to make buffer
			int clen = strlen(argv[1]), ulen = strlen(argv[2]);
			uint16_t reply_size = htole16(17 + clen + ulen);
			char reply[reply_size];
			
			// declare variables to write in reply
			uint16_t msg_id = htobe16(1);
			char *bytes1 = "\x00\x02\x00\x00\x00\x00";
			char *bytes2 = "\x00\x03\x00";
			
			// zero and fill reply. 
               		 memset(reply, 0, reply_size); 
			memcpy(reply, &msg_id, 2);
			memcpy(reply+2, bytes1, 6);
			memcpy(reply+12, argv[1], clen);
			memcpy(reply+13+clen, argv[2], ulen);
			memcpy(reply+14+clen+ulen, bytes2, 3);
	
			// send reply size
			write(fd_out, &reply_size, 2);
			// send reply
			write(fd_out, reply, reply_size);
			print_message("SENT: ", reply, reply_size);			

		} else if (id == 0x0020){
			write(fd_out, "\4\0\0\v\0\1" ,6);
		}
	}
	printf("Ended communication with client.\n");

	// Close access to FIFOs
	close(fd_in);
	close(fd_out);

	// Delete FIFOs
	unlink(in_fifo);
	unlink(out_fifo);
}