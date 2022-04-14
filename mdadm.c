#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

#include "cache.h"

int mount_check = 0; // Initially linear device is unmounted

// Creating necessary global variables for clean code
int diskID;
int blockID;
int offsetBytes;
int size_reading;
int bytes_read;

uint8_t temp_buffer[256]; // creating a buffer
uint32_t tempAddr;

// Creating op variables
uint32_t opDisk;
uint32_t opBlock;
uint32_t opRead;
//uint32_t opCacheRead;
uint32_t opWrite;

int bitshift(uint32_t command, uint32_t disk, uint32_t reserved, uint32_t block)
{
  uint32_t tempBlock, tempReserved, tempDisk, tempCommand, jbodVar;
  tempBlock = 0;    // temp variable for block
  tempCommand = 0;  // temp variable for command
  tempDisk = 0;     // temp variable for disk
  tempReserved = 0; // temp variable for reserved
  jbodVar = 0;

  tempCommand = command << 26;  // shifting bytes for command
  tempDisk = disk << 22;        // shifting bytes for disk
  tempReserved = reserved << 8; // shifting bytes for reserved
  tempBlock = block;

  jbodVar = tempCommand | tempDisk | tempReserved | tempBlock;
  return jbodVar;
}

int mdadm_mount(void){
  if (mount_check == 0)
  { // When linear device is not mounted
    mount_check = 1;
    jbod_operation(JBOD_MOUNT, NULL); // Syscall during mount
    return 1;
  }
  else if (mount_check == 1)
  { // When linear device is mounted
    return -1;
  }
  return 1;
}

int mdadm_unmount(void)
{
  if (mount_check == 0)
  { // When linear device is not mounted
    return -1;
  }
  else
  { // When linear device is mounted
    mount_check = 0;
    jbod_operation(JBOD_UNMOUNT, NULL); // Syscall during unmount
    return 1;
  }
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf)
{
  if (mount_check == 0)
  { // Check if linear device is mounted or not
    return -1;
  }

  if (len > 1024)
  { // Len cannot be more than 1024
    return -1;
  }

  if (addr > (JBOD_DISK_SIZE * JBOD_NUM_DISKS) || (addr + len) > (JBOD_DISK_SIZE * JBOD_NUM_DISKS))
  { // Addr cannot be more than (JBOD_DISK_SIZE * JBOD_NUM_DISKS), neither can (addr + len)
    return -1;
  }

  if (buf == NULL && len != 0)
  {
    return -1;
  }

  if (len == 0 && buf == NULL)
  {
    return 0;
  }

  if (len == -1)
  { // Returning fail when len equals to -1
    return -1;
  }

  else
  {
    size_reading = 0;  // the size of bytes to be read
    bytes_read = 0;   // the total bytes already read

    while (len > 0) // as long as the len is more than 0
    {
      tempAddr = addr % 256;  // the temporary addresss
      diskID = addr / 65536;   // Calculates the diskID
      blockID = addr / 256;  // Calculates the blockID
      offsetBytes = 256 - tempAddr;  // Calculates the offset

      /*
      opDisk = bitshift(JBOD_SEEK_TO_DISK, diskID, 0, 0);
      jbod_operation(opDisk, NULL);   // syscall for seeking the disk

      opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
      jbod_operation(opBlock, NULL);  // syscall for seeking the block

      opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
      jbod_operation(opRead, temp_buffer);  // syscall for readng the block
      */
      
      if (len < offsetBytes){
        size_reading = len;

        if (cache_enabled() == true){ // There is a cache

          if (cache_lookup(diskID, blockID, temp_buffer) == 1){
            memcpy(buf + bytes_read, temp_buffer, size_reading);
          } 
          else{
            opDisk = bitshift(JBOD_SEEK_TO_DISK, diskID, 0, 0);
            jbod_operation(opDisk, NULL);   // syscall for seeking the disk

            opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
            jbod_operation(opBlock, NULL);  // syscall for seeking the block

            opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
            jbod_operation(opRead, temp_buffer);  // syscall for readng the block
            
            cache_insert(diskID, blockID, temp_buffer);
          }
        }
        else { // There is no cache
          size_reading = len;  // len is the number of bytes to be read
          
          opDisk = bitshift(JBOD_SEEK_TO_DISK, diskID, 0, 0);
          jbod_operation(opDisk, NULL);   // syscall for seeking the disk

          opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
          jbod_operation(opBlock, NULL);  // syscall for seeking the block

          opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
          jbod_operation(opRead, temp_buffer);  // syscall for readng the block
          memcpy(buf + bytes_read, temp_buffer, size_reading); // doing the mem copy
        }
      }
      else { // when len is greater than or equal to the offset
        size_reading = offsetBytes; // offset is the number of bytes to be read
        
        if (cache_enabled() == true) { // There is a cache
          
          if (cache_lookup(diskID, blockID, temp_buffer) == 1) {
            memcpy(buf + bytes_read, temp_buffer, size_reading);
          }
          else {
            opDisk = bitshift(JBOD_SEEK_TO_DISK, diskID, 0, 0);
            jbod_operation(opDisk, NULL);   // syscall for seeking the disk

            opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
            jbod_operation(opBlock, NULL);  // syscall for seeking the block

            opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
            jbod_operation(opRead, temp_buffer);  // syscall for readng the block
            cache_insert(diskID, blockID, temp_buffer);
          }
        }
        else { // There is no cache
          size_reading = offsetBytes;
          opDisk = bitshift(JBOD_SEEK_TO_DISK, diskID, 0, 0);
          jbod_operation(opDisk, NULL);   // syscall for seeking the disk

          opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
          jbod_operation(opBlock, NULL);  // syscall for seeking the block

          opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
          jbod_operation(opRead, temp_buffer);  // syscall for readng the block
          memcpy(buf + bytes_read, temp_buffer, size_reading);  // doing the mem copy
        }
      }
      
      // updating the variables in the loop
      addr += size_reading;

      // calculating blockID and diskID
      blockID = addr / 256;
      diskID = addr / 65536;

      // calculating the tempAddr to get the offset
      tempAddr = addr % 256;
      offsetBytes = 256 - tempAddr;

      bytes_read += size_reading;  
      len -= size_reading; // decreasing the length each iteration

      size_reading = 0; // resetting the size of bytes to be read each iteration
    }
    return bytes_read;
  }
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf)
{
  if (mount_check == 0)
  {
    return -1;
  }

  if (len > 1024)
  { // Len cannot be more than 1024
    return -1;
  }

  if (addr > (JBOD_DISK_SIZE * JBOD_NUM_DISKS) || (addr + len) > (JBOD_DISK_SIZE * JBOD_NUM_DISKS))
  { // Addr cannot be more than (JBOD_DISK_SIZE * JBOD_NUM_DISKS), neither can (addr + len)
    return -1;
  }

  if (buf == NULL && len != 0)
  {
    return -1;
  }

  if (len == 0 && buf == NULL)
  {
    return 0;
  }

  if (len == -1)
  { // Returning fail when len equals to -1
    return -1;
  }

  else 
  {
    size_reading = 0;   // the size of bytes to be read
    bytes_read = 0;     // the total bytes already read

    while (len > 0)  // as long as the len is more than 0
    {
      tempAddr = addr % 256;  // the temporary addresss
      diskID = addr / 65536;   // Calculates the diskID
      blockID = addr / 256;  // Calculates the blockID
      offsetBytes = 256 - tempAddr;  // Calculates the offset

      opDisk = bitshift(JBOD_SEEK_TO_DISK, diskID, 0, 0);
      jbod_operation(opDisk, NULL);  // syscall for seeking disk

      opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
      jbod_operation(opBlock, NULL);  // syscall for seeking block

      // op created for write
      opWrite = bitshift(JBOD_WRITE_BLOCK, diskID, 0, blockID);
      
      // op created for read
      opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
      

      if (len < offsetBytes){
        size_reading = len;  // len is the reading size (size to be read)

        if (cache_enabled() == true) { // If cache exists
          if (cache_lookup(diskID, blockID, temp_buffer) == 1){
            memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);
            cache_update(diskID, blockID, temp_buffer);
            
            jbod_operation(opWrite, temp_buffer);  // syscall for write
          }
          else {
            jbod_operation(opRead, temp_buffer);  // syscall for read
            jbod_operation(opBlock, NULL);  // syscall for seeking block

            memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);
            jbod_operation(opWrite, temp_buffer);  // syscall for write

            cache_insert(diskID, blockID, temp_buffer);
          }
        }

        else {
          jbod_operation(opRead, temp_buffer);  // syscall for read
          jbod_operation(opBlock, NULL);  // syscall for seeking block

          memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);
          jbod_operation(opWrite, temp_buffer);  // syscall for write
        }
      }

      else {
        size_reading = offsetBytes;  // the offset bytes will be read

        if (cache_enabled() == true) { // If cache exists
          if (cache_lookup(diskID, blockID, temp_buffer) == 1){
            memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);
            cache_update(diskID, blockID, temp_buffer);
            
            jbod_operation(opWrite, temp_buffer);  // syscall for write
          }
          else {
            jbod_operation(opRead, temp_buffer);  // syscall for read
            jbod_operation(opBlock, NULL);  // syscall for seeking block

            memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);
            jbod_operation(opWrite, temp_buffer);  // syscall for write

            cache_insert(diskID, blockID, temp_buffer);
          }
        } 

        else{ // If cache does not exist
          jbod_operation(opRead, temp_buffer);  // syscall for read
          jbod_operation(opBlock, NULL);  // syscall for seeking block

          memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);
          jbod_operation(opWrite, temp_buffer);  // syscall for write
        }
      }
        // updating the variables in the loop

      addr += size_reading;

      // calculating blockID and diskID
      blockID = addr / 256;
      diskID = addr / 65536;

      // calculating the tempAddr to get the offset
      tempAddr = addr % 256;
      offsetBytes = 256 - tempAddr;

      bytes_read += size_reading;  
      len -= size_reading; // decreasing the length each iteration

      size_reading = 0; // resetting the size of bytes to be read each iteration
    }
    return bytes_read;
  }

  // return len;
}