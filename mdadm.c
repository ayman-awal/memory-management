#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "net.h"

int mount_check = 0;


int32_t bitshift(jbod_cmd_t command, int disk, int block){
  int32_t op;

  op = command << 26 | disk << 22 | block;
  return op;
}

int minimum_value(int a , int b){
  if(a > b){
    return b;
  }
  else{
    return a;
  }
}

void get_address(uint32_t addr, int *disk,int *block, int *offset){
  int remainder;

  remainder = addr % JBOD_DISK_SIZE;
  *block = remainder /JBOD_BLOCK_SIZE;
  *disk = addr / JBOD_DISK_SIZE;
  *offset = addr % JBOD_BLOCK_SIZE; 
}

void seek_operation(int disk,int block){
    int opDisk;

    opDisk = bitshift(JBOD_SEEK_TO_DISK, disk,0);
    jbod_client_operation(opDisk,NULL);

    int opBlock;

    opBlock = bitshift(JBOD_SEEK_TO_BLOCK,0,block);
    jbod_client_operation(opBlock,NULL);
  }

int mdadm_mount(void) {
  int32_t mount_op;
  
  if (mount_check == 1){
   return -1;
 }

  mount_op = JBOD_MOUNT << 26;
  int mounted_successful;
  mounted_successful = jbod_client_operation(mount_op,NULL);

  if (mounted_successful == 0){
    mount_check = 1;
    return 1;
  }
  else{
    return -1;
  }
}

int mdadm_unmount(void) {
  int32_t unmount_op;
  
  if(mount_check== 0){
    return -1;
  }

  unmount_op = JBOD_UNMOUNT << 26;
  int unmounted_successful;
  unmounted_successful = jbod_client_operation(unmount_op,NULL);
  
  if(unmounted_successful == 0){
    mount_check = 0;
    return 1;
  }
  return 0;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  int diskID;
  int blockID;
  int offsetBytes;
  int modified_length;

  uint8_t tempbuf[256];
  uint32_t return_length;
  
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

  get_address(addr, &diskID, &blockID, &offsetBytes);
  seek_operation(diskID,blockID);

  return_length = len;
  modified_length = len;
  int bytes_so_far = 0;
  
  
  while(modified_length > 0){   
    int bytesToBuf;
    int reduce;
     
    if(cache_lookup(diskID,blockID,tempbuf) == -1){
      int32_t opRead;

      opRead = bitshift(JBOD_READ_BLOCK,diskID,blockID);
      jbod_client_operation(opRead,tempbuf);

      cache_insert(diskID,blockID,tempbuf);
    }
    
    if (modified_length <= 256){
      int bytes_from_block;

      bytes_from_block = 256 - offsetBytes;
      
      reduce = minimum_value(bytes_from_block, modified_length);
      modified_length = modified_length - reduce;
      bytesToBuf = reduce;
    }

    else if(modified_length == len) {
      int bytes_from_block;
      
      bytes_from_block = 256 - offsetBytes;

      reduce = minimum_value(bytes_from_block, modified_length);
      modified_length = modified_length - reduce;
      bytesToBuf = reduce;
    }

    else{
      modified_length = modified_length - 256;
      bytesToBuf = 256;
    }

    memcpy(buf + bytes_so_far,tempbuf, bytesToBuf);

    bytes_so_far = bytes_so_far + bytesToBuf;
    get_address(addr + bytes_so_far,&diskID, &blockID,&offsetBytes);
    seek_operation(diskID,blockID);
    
  } 
  return return_length;
}