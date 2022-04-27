
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"
#include "net.h"

int mount_check = 0;

int minimum_value(int a , int b){
  if(a > b){
    return b;
  }
  else{
    return a;
  }
}
/*
void translate_address(uint32_t addr, int *disk,int *block, int *offset){
  int remainder;
  *disk = addr / JBOD_DISK_SIZE;
  remainder = addr % JBOD_DISK_SIZE;
  *block = remainder /JBOD_BLOCK_SIZE;
  *offset = addr % JBOD_BLOCK_SIZE; 
}

int32_t encode_operation(jbod_cmd_t command, int disk, int block){
  int32_t op;
  op = command << 26 | disk << 22 | block;
  return op;
}

void seek_operation(int disk,int block){
    int disk_to_seek;
    disk_to_seek = encode_operation(JBOD_SEEK_TO_DISK, disk,0);
    jbod_client_operation(disk_to_seek,NULL);
    int block_to_seek;
    block_to_seek = encode_operation(JBOD_SEEK_TO_BLOCK,0,block);
    jbod_client_operation(block_to_seek,NULL);
  }
*/
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

