
/*
Timings:

For simple tests it takes around 2 seconds
For linear tests it takes around 7 seconds
For random tests it takes around 60 seconds
*/


#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include "net.h"
#include "jbod.h"

/* the client socket descriptor for the connection to the server */
int cli_sd = -1;

/* attempts to read n (len) bytes from fd; returns true on success and false on failure. 
It may need to call the system call "read" multiple times to reach the given size len. 
*/

static bool nread(int fd, int len, uint8_t *buf) {
  int bytesRead;  
  int index = 0;  // initial index
  int ogLen = len; // copying the value of len in the variable

  if (cli_sd == -1){
    return false;
  }
  
  while (bytesRead != ogLen){
    bytesRead = read(fd, &buf[index], len);  // calling the read function
    len = len - bytesRead;  // updating len and index
    index = bytesRead;
  }
  return true;
} 

/* attempts to write n bytes to fd; returns true on success and false on failure 
It may need to call the system call "write" multiple times to reach the size len.
*/


static bool nwrite(int fd, int len, uint8_t *buf) {
  int bytesWritten;
  int index = 0;
  int ogLen = len;

  if (cli_sd == -1){
    return false;
  }

  while (bytesWritten != ogLen){
    bytesWritten = write(fd, &buf[index], len);  // calling the write funtion
    index = bytesWritten; // updating the index and len variables
    len -= bytesWritten;
  }
  return true;
} 

/* Through this function call the client attempts to receive a packet from sd 
(i.e., receiving a response from the server.). It happens after the client previously 
forwarded a jbod operation call via a request message to the server.  
It returns true on success and false on failure. 
The values of the parameters (including op, ret, block) will be returned to the caller of this function: 

op - the address to store the jbod "opcode"  
ret - the address to store the return value of the server side calling the corresponding jbod_operation function.
block - holds the received block content if existing (e.g., when the op command is JBOD_READ_BLOCK)

In your implementation, you can read the packet header first (i.e., read HEADER_LEN bytes first), 
and then use the length field in the header to determine whether it is needed to read 
a block of data from the server. You may use the above nread function here.  
*/

static bool recv_packet(int sd, uint32_t *op, uint16_t *ret, uint8_t *block) {
  uint8_t tempbuff[HEADER_LEN];  // Creating temporary buffers and other necessary variables
  uint16_t ogLen; // stores the value of the original length
  uint32_t tempOP;
  uint16_t tempRet;
  
  nread(sd, HEADER_LEN, tempbuff); // calling nread function call

  memcpy(&ogLen, tempbuff, 2);  // memcpy to ogLen variable created
  memcpy(&tempOP, tempbuff + 2, 4);  // memcpy to tempOP variable created
  memcpy(&tempRet, tempbuff + 6, 2);  // memcpy to tempRet variable created

  ogLen = ntohs(ogLen); // converting ogLen to hostbyte
  *op = ntohl(tempOP);  // converting tempOP to hostbyte
  *ret = ntohs(tempRet); // converting tempRet to hostbyte

  if (ogLen == (HEADER_LEN + JBOD_BLOCK_SIZE)) {
    nread(sd, JBOD_BLOCK_SIZE, block);
  }
  return true;
} 


/* The client attempts to send a jbod request packet to sd (i.e., the server socket here); 
returns true on success and false on failure. 

op - the opcode. 
block- when the command is JBOD_WRITE_BLOCK, the block will contain data to write to the server jbod system;
otherwise it is NULL.

The above information (when applicable) has to be wrapped into a jbod request packet (format specified in readme).
You may call the above nwrite function to do the actual sending.  
*/


static bool send_packet(int sd, uint32_t op, uint8_t *block) {
  uint8_t tempbuff[HEADER_LEN]; // Creating the temporary buffer
  int cmd;
  uint16_t len;

  cmd = op >> 26; // Deconstructing the op to get the command

  if (cmd == 5){ // if the command is write
    len = HEADER_LEN + JBOD_BLOCK_SIZE;  // We include header_len and block size
  } 
  else{ // Otherwise we only use the headerlen
    len = HEADER_LEN;
  }

  len = htons(len);  // Converting to hostbyte
  op = htonl(op);
  
  memcpy(tempbuff, &len, 2); // copying data to the buffer
  memcpy(&tempbuff[2], &op, 4);

  nwrite(sd,HEADER_LEN,tempbuff); // nwrite function call

  if (cmd == 5){
    nwrite(sd,JBOD_BLOCK_SIZE,block); // calling nwrite
  }
  return true;
}

/* attempts to connect to server and set the global cli_sd variable to the
 * socket; returns true if successful and false if not. 
 * this function will be invoked by tester to connect to the server at given ip and port.
 * you will not call it in mdadm.c
*/
bool jbod_connect(const char *ip, uint16_t port) {
  struct sockaddr_in caddr;

  cli_sd = socket(AF_INET, SOCK_STREAM, 0); // creating the socket
  if (cli_sd == -1){
    return false;
  }

  // setting up variables for connection
  caddr.sin_family = AF_INET;
  caddr.sin_port = htons(port);
  if (inet_aton(ip, &caddr.sin_addr) == 0){
    return false;
  }

  // connecting socket to the server
  if (connect(cli_sd, (const struct sockaddr *)&caddr, sizeof(caddr)) == -1){
    return false;
  }
  
  return true;  // returns true if connection is succesful
}

/* disconnects from the server and resets cli_sd */

void jbod_disconnect(void) {
    close(cli_sd); // closing the socket and updating the cli_sd
    cli_sd = -1;
  
} 

/* sends the JBOD operation to the server (use the send_packet function) and receives 
(use the recv_packet function) and processes the response. 

The meaning of each parameter is the same as in the original jbod_operation function. 
return: 0 means success, -1 means failure.
*/

int jbod_client_operation(uint32_t op, uint8_t *block) {
  uint16_t returnValue;

  if (send_packet(cli_sd, op, block) != true) {
    return -1;
  }
  
  if (recv_packet(cli_sd, &op, &returnValue, block) != true){
    return -1;
  }
  
  return returnValue;
}