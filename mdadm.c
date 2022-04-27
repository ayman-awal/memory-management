#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "net.h"

int mount_check = 0; // Variable which checks for the status of the mount


int32_t bitshift(jbod_cmd_t command, int disk, int block){ // Custom made function for bitshifting while using syscalls
  int32_t op;

  op = command << 26 | disk << 22 | block;
  return op;
}

int minimum_value(int a , int b){  // Custom made function for finding the minimum value
  if(a > b){
    return b;
  }
  else{
    return a;
  }
}

void get_address(uint32_t addr, int *disk,int *block, int *offset){ // Custom made function for acquiring the address
  int remainder;

  // Getting the remainder by doing modulo
  remainder = addr % JBOD_DISK_SIZE;

  // Getting the block
  *block = remainder /JBOD_BLOCK_SIZE;

  // Getting the disk by dividing with disk size
  *disk = addr / JBOD_DISK_SIZE;

  // Getting the offset by modulo
  *offset = addr % JBOD_BLOCK_SIZE; 
}

void seek_operation(int disk,int block){
    int opDisk;  // op variables for disk and block
    int opBlock;

    opDisk = bitshift(JBOD_SEEK_TO_DISK, disk,0);  // syscall for seeking the disk
    jbod_client_operation(opDisk,NULL);

    opBlock = bitshift(JBOD_SEEK_TO_BLOCK,0,block);  // syscall for seeking the block
    jbod_client_operation(opBlock,NULL);
  }

int mdadm_mount(void) {
  int32_t mount_op;
  int mounted_successful;
  
  if (mount_check == 1){  // returns fail if system is already mounted
   return -1;
 }

  mount_op = JBOD_MOUNT << 26;  // bitshifting

  mounted_successful = jbod_client_operation(mount_op,NULL); // sys call for mounting

  if (mounted_successful == 0){
    mount_check = 1;  // Sets the mount_check variable to 1
    return 1;
  }
  else{
    return -1;
  }
}

int mdadm_unmount(void) {
  int32_t unmount_op;
  int unmounted_successful;
  
  if(mount_check== 0){  // Returns fail if the system is already unmounted
    return -1;
  }

  unmount_op = JBOD_UNMOUNT << 26;  // bitshifting

  unmounted_successful = jbod_client_operation(unmount_op,NULL); // syscall for unmounting
  
  if(unmounted_successful == 0){
    mount_check = 0;  // Sets the mount_check to 0, meaning it is unmounted
    return 1;
  }
  return 0;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  int diskID;     // Creating the necessary variables
  int blockID;
  int offsetBytes;
  int modified_length;
  int bytes_so_far = 0;  // starting off with 0 bytes

  uint8_t tempbuf[256];  // creating the temporary buffer
  uint32_t return_length;  // we will return in the end
  
  if (mount_check == 0){
    return -1;
  }
  if(addr < 0 || (addr + len > JBOD_DISK_SIZE * JBOD_NUM_DISKS)){
    return -1;
  }
  
  if(len < 0 || len > 1024){
    return -1;
  }
  if(len != 0 && buf == NULL){
    return -1;
  }

  get_address(addr, &diskID, &blockID, &offsetBytes);  // Does function call to get the address
  seek_operation(diskID,blockID);

  return_length = len;
  modified_length = len;
  
  while(modified_length > 0){   
    int bytesToBuf;
    int decrease;
     
    if(cache_lookup(diskID,blockID,tempbuf) == -1){ // Looking up 
      int32_t opRead; // creating op variable for read

      opRead = bitshift(JBOD_READ_BLOCK,diskID,blockID);  // Sys call for read
      jbod_client_operation(opRead,tempbuf);

      cache_insert(diskID,blockID,tempbuf); // Inserting data in cache through function call
    }
    
    if (modified_length <= 256){
      int bytes_from_block;

      bytes_from_block = 256 - offsetBytes;  // calculating bytes from offset
      
      decrease = minimum_value(bytes_from_block, modified_length);
      // updating the length to be modified and the bytes to go to the buffer
      modified_length = modified_length - decrease;
      bytesToBuf = decrease;  
    }

    else if(modified_length == len) {
      int bytes_from_block;
      
      bytes_from_block = 256 - offsetBytes;

      decrease = minimum_value(bytes_from_block, modified_length);
      modified_length = modified_length - decrease;
      bytesToBuf = decrease;
    }

    else{
      modified_length = modified_length - 256;
      bytesToBuf = 256;
    }

    memcpy(buf + bytes_so_far,tempbuf, bytesToBuf);

    bytes_so_far = bytes_so_far + bytesToBuf;
    get_address(addr + bytes_so_far,&diskID, &blockID,&offsetBytes);

    seek_operation(diskID,blockID); // Calls function to seek for disk and block
    
  } 
  return return_length;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  int diskID;   // Creating the necessary variables
  int blockID;
  int offsetBytes;
  int modified_length;
  int bytes_so_far = 0;

  uint8_t tempbuf[256]; // creating a temporary buffer

  if (mount_check == 0){
    return -1;
  }

  if(addr < 0 || addr + len > JBOD_DISK_SIZE * JBOD_NUM_DISKS){
    return -1;
  }
  
  if(len < 0 || len > 1024){
    return -1;
  }
  if(len != 0 && buf == NULL){
    return -1;
  }

  get_address(addr, &diskID, &blockID, &offsetBytes);
  seek_operation(diskID,blockID);

  modified_length = len;

  while(modified_length > 0){
    int bytesToBuf;
    int decrease;
    
    if (modified_length <= 256){
      int bytes_from_block;
      
      bytes_from_block = 256 - offsetBytes;

      decrease = minimum_value(bytes_from_block, modified_length);
      modified_length = modified_length - decrease;
      bytesToBuf = decrease;
    }

    else if(modified_length == len){
      int bytes_from_block;
      
      bytes_from_block = 256 - offsetBytes;

      decrease = minimum_value(bytes_from_block, modified_length);
      modified_length = modified_length - decrease;
      bytesToBuf = decrease;
    }

    else{
      bytesToBuf = 256;
      modified_length = modified_length - 256;
    }

    if(cache_lookup(diskID,blockID, tempbuf) == -1){
      int32_t opRead;

      opRead = bitshift(JBOD_READ_BLOCK, diskID,blockID);
      jbod_client_operation(opRead,tempbuf);

      cache_insert(diskID,blockID,tempbuf);
    }
    
    memcpy(&tempbuf[offsetBytes],buf + bytes_so_far,bytesToBuf);
    
    bytes_so_far = bytes_so_far + bytesToBuf;

    int32_t opWrite;
    opWrite = bitshift(JBOD_WRITE_BLOCK, diskID,blockID);
    seek_operation(diskID,blockID);
    
    jbod_client_operation(opWrite,tempbuf);
    
    if (cache_enabled() == true){
      cache_update(diskID,blockID,tempbuf);
    }
    
    get_address(addr + bytes_so_far,&diskID, &blockID,&offsetBytes);
    seek_operation(diskID,blockID);
    
  }
  return len;

}