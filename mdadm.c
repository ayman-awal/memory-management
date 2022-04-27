// #include <stdio.h>
// #include <string.h>
// #include <assert.h>

// #include "mdadm.h"
// #include "jbod.h"

// #include "cache.h"
// #include "net.h"

// jbod_error_t mount_check = JBOD_ALREADY_UNMOUNTED; // Initially linear device is unmounted

// // Creating necessary global variables for clean code


// uint32_t bitshift(uint32_t command, uint32_t disk, uint32_t reserved, uint32_t block)
// {
//   uint32_t tempBlock, tempReserved, tempDisk, tempCommand, jbodVar;
//   tempBlock = 0;    // temp variable for block
//   tempCommand = 0;  // temp variable for command
//   tempDisk = 0;     // temp variable for disk
//   tempReserved = 0; // temp variable for reserved
//   jbodVar = 0;

//   tempCommand = command << 26;  // shifting bytes for command
//   tempDisk = disk << 22;        // shifting bytes for disk
//   tempReserved = reserved << 8; // shifting bytes for reserved
//   tempBlock = block;

//   jbodVar = tempCommand | tempDisk | tempReserved | tempBlock;
//   return jbodVar;
// }

// int mdadm_mount(void){
//   if (mount_check == 3)
//   { // When linear device is not mounted
//     mount_check = JBOD_ALREADY_MOUNTED;
//     uint32_t op_mount = 0;
//     op_mount = JBOD_MOUNT << 26;
//     jbod_client_operation(op_mount, NULL); // Syscall during mount
//     return 1;
//   }
//   else { // When linear device is mounted
//     return -1;
//   }
// }

// int mdadm_unmount(void)
// {
//   if (mount_check == 2)
//   { // When linear device is not mounted
//     mount_check = JBOD_ALREADY_UNMOUNTED;
//     uint32_t op_unmount = 0;
//     op_unmount = JBOD_UNMOUNT << 26;
//     jbod_client_operation(op_unmount, NULL); // Syscall during unmount
//     return 1;
//   }
//   else
//   { // When linear device is mounted
//     return -1;
//   }
// }

// int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf)
// {
//   int diskID;
//   int blockID;
//   int offsetBytes;
//   int size_reading;
//   int bytes_read;

//   uint32_t tempAddr;
//   uint8_t temp_buffer[256];
//   uint32_t opDisk;
//   uint32_t opBlock;
//   uint32_t opRead;

//   if (mount_check == 3)
//   { // Check if linear device is mounted or not
//     return -1;
//   }

//   if (len > 1024)
//   { // Len cannot be more than 1024
//     return -1;
//   }

//   if (addr > (JBOD_DISK_SIZE * JBOD_NUM_DISKS) || (addr + len) > (JBOD_DISK_SIZE * JBOD_NUM_DISKS))
//   { // Addr cannot be more than (JBOD_DISK_SIZE * JBOD_NUM_DISKS), neither can (addr + len)
//     return -1;
//   }

//   if (buf == NULL && len != 0)
//   {
//     return -1;
//   }

//   if (len == 0 && buf == NULL)
//   {
//     return 0;
//   }

//   if (len == -1)
//   { // Returning fail when len equals to -1
//     return -1;
//   }

//   else
//   {
//     size_reading = 0;  // the size of bytes to be read
//     bytes_read = 0;   // the total bytes already read

//     while (len > 0) // as long as the len is more than 0
//     {
//       tempAddr = addr % 256;  // the temporary addresss
//       diskID = addr / 65536;   // Calculates the diskID
//       blockID = addr / 256;  // Calculates the blockID
//       offsetBytes = 256 - tempAddr;  // Calculates the offset

//       if (len < offsetBytes){
//         size_reading = len;

//         if (cache_enabled()){ // There is a cache

//           if (cache_lookup(diskID, blockID, temp_buffer) == 1){
//             memcpy(buf + bytes_read, temp_buffer, size_reading);
//           } 
//           else{
//             opDisk = bitshift(JBOD_SEEK_TO_DISK, diskID, 0, 0);
//             jbod_client_operation(opDisk, NULL);   // syscall for seeking the disk

//             opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
//             jbod_client_operation(opBlock, NULL);  // syscall for seeking the block

//             opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
//             jbod_client_operation(opRead, temp_buffer);  // syscall for readng the block
            
//             cache_insert(diskID, blockID, temp_buffer);
//           }
//         }
//         else { // There is no cache
          
//           opDisk = bitshift(JBOD_SEEK_TO_DISK, diskID, 0, 0);
//           jbod_client_operation(opDisk, NULL);   // syscall for seeking the disk

//           opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
//           jbod_client_operation(opBlock, NULL);  // syscall for seeking the block

//           opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
//           jbod_client_operation(opRead, temp_buffer);  // syscall for readng the block
          
//           memcpy(buf + bytes_read, temp_buffer, size_reading); // doing the mem copy
//         }
//       }
//       else { // when len is greater than or equal to the offset
//         size_reading = offsetBytes; // offset is the number of bytes to be read
        
//         if (cache_enabled()) { // There is a cache
          
//           if (cache_lookup(diskID, blockID, temp_buffer) == 1) {
//             memcpy(buf + bytes_read, temp_buffer, size_reading);
//           }
//           else {
//             opDisk = bitshift(JBOD_SEEK_TO_DISK, diskID, 0, 0);
//             jbod_client_operation(opDisk, NULL);   // syscall for seeking the disk

//             opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
//             jbod_client_operation(opBlock, NULL);  // syscall for seeking the block

//             opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
//             jbod_client_operation(opRead, temp_buffer);  // syscall for readng the block
//             cache_insert(diskID, blockID, temp_buffer);
//           }
//         }
//         else { // There is no cache
//           opDisk = bitshift(JBOD_SEEK_TO_DISK, diskID, 0, 0);
//           jbod_client_operation(opDisk, NULL);   // syscall for seeking the disk

//           opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
//           jbod_client_operation(opBlock, NULL);  // syscall for seeking the block

//           opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
//           jbod_client_operation(opRead, temp_buffer);  // syscall for readng the block
          
//           memcpy(buf + bytes_read, temp_buffer, size_reading);  // doing the mem copy
//         }
//       }
      
//       // updating the variables in the loop
//       addr += size_reading;

//       // calculating blockID and diskID
//       blockID = addr / 256;
//       diskID = addr / 65536;

//       // calculating the tempAddr to get the offset
//       tempAddr = addr % 256;
//       offsetBytes = 256 - tempAddr;

//       bytes_read += size_reading;  
//       len -= size_reading; // decreasing the length each iteration

//       size_reading = 0; // resetting the size of bytes to be read each iteration
//     }
//     len = bytes_read;
//   }
//   return len;
// }

// int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf)
// {
//   int diskID;
//   int blockID;
//   int offsetBytes;
//   int size_reading;
//   int bytes_read;

//   uint32_t tempAddr;
//   uint8_t temp_buffer[256];
//   uint32_t opDisk;
//   uint32_t opBlock;
//   uint32_t opRead;
//   uint32_t opWrite;

//   if (mount_check == 3)
//   {
//     return -1;
//   }

//   if (len > 1024)
//   { // Len cannot be more than 1024
//     return -1;
//   }

//   if (addr > (JBOD_DISK_SIZE * JBOD_NUM_DISKS) || (addr + len) > (JBOD_DISK_SIZE * JBOD_NUM_DISKS))
//   { // Addr cannot be more than (JBOD_DISK_SIZE * JBOD_NUM_DISKS), neither can (addr + len)
//     return -1;
//   }

//   if (buf == NULL && len != 0)
//   {
//     return -1;
//   }

//   if (len == 0 && buf == NULL)
//   {
//     return 0;
//   }

//   if (len == -1)
//   { // Returning fail when len equals to -1
//     return -1;
//   }

//   else 
//   {
//     size_reading = 0;   // the size of bytes to be read
//     bytes_read = 0;     // the total bytes already read

//     while (len > 0)  // as long as the len is more than 0
//     {
//       tempAddr = addr % 256;  // the temporary addresss
//       diskID = addr / 65536;   // Calculates the diskID
//       blockID = addr / 256;  // Calculates the blockID
//       offsetBytes = 256 - tempAddr;  // Calculates the offset

//       opDisk = bitshift(JBOD_SEEK_TO_DISK, diskID, 0, 0);
//       jbod_client_operation(opDisk, NULL);  // syscall for seeking disk

//       opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
//       jbod_client_operation(opBlock, NULL);  // syscall for seeking block

//       // op created for write
//       //opWrite = bitshift(JBOD_WRITE_BLOCK, diskID, 0, blockID);
      
//       // op created for read
//       //opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
      

//       if (len < offsetBytes){
//         size_reading = len;  // len is the reading size (size to be read)

//         if (cache_enabled()) { // If cache exists
//           if (cache_lookup(diskID, blockID, temp_buffer) == 1){
//             memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);
//             cache_update(diskID, blockID, temp_buffer);
            
//             opWrite = bitshift(JBOD_WRITE_BLOCK, diskID, 0, blockID);
//             jbod_client_operation(opWrite, temp_buffer);  // syscall for write
//           }
//           else {
//             opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);

//             jbod_client_operation(opRead, temp_buffer);  // syscall for read
//             memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);
            
//             opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
//             jbod_client_operation(opBlock, NULL);  // syscall for seeking block
//             jbod_client_operation(opWrite, temp_buffer);  // syscall for write

//             cache_insert(diskID, blockID, temp_buffer);
//           }
//         }

//         else {
//           opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);

//           jbod_client_operation(opRead, temp_buffer);  // syscall for read
//           memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);

//           opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID); 
//           jbod_client_operation(opBlock, NULL);  // syscall for seeking block
          
//           opWrite = bitshift(JBOD_WRITE_BLOCK, diskID, 0, blockID);
//           jbod_client_operation(opWrite, temp_buffer);  // syscall for write
//         }
//       }

//       else {
//         size_reading = offsetBytes;  // the offset bytes will be read

//         if (cache_enabled()) { // If cache exists
//           if (cache_lookup(diskID, blockID, temp_buffer) == 1){
//             memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);
//             cache_update(diskID, blockID, temp_buffer);
            
//             opWrite = bitshift(JBOD_WRITE_BLOCK, diskID, 0, blockID);
//             jbod_client_operation(opWrite, temp_buffer);  // syscall for write
//           }
//           else {
//             opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
//             jbod_client_operation(opRead, temp_buffer); // syscall for read

//             memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);

//             opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
//             jbod_client_operation(opBlock, NULL); // syscall for seeking block

//             opWrite = bitshift(JBOD_WRITE_BLOCK, diskID, 0, blockID);
//             jbod_client_operation(opWrite, temp_buffer); // syscall for write

//             cache_insert(diskID, blockID, temp_buffer);
//           }
//         } 

//         else{ // If cache does not exist
//           opRead = bitshift(JBOD_READ_BLOCK, diskID, 0, blockID);
//             jbod_client_operation(opRead, temp_buffer); // syscall for read

//             memcpy(temp_buffer + tempAddr, buf + bytes_read, size_reading);

//             opBlock = bitshift(JBOD_SEEK_TO_BLOCK, 0, 0, blockID);
//             jbod_client_operation(opBlock, NULL); // syscall for seeking block

//             opWrite = bitshift(JBOD_WRITE_BLOCK, diskID, 0, blockID);
//             jbod_client_operation(opWrite, temp_buffer); // syscall for write

//         }
//       }
//         // updating the variables in the loop

//       addr += size_reading;

//       // calculating blockID and diskID
//       blockID = addr / 256;
//       diskID = addr / 65536;

//       // calculating the tempAddr to get the offset
//       tempAddr = addr % 256;
//       offsetBytes = 256 - tempAddr;

//       bytes_read += size_reading;  
//       len -= size_reading; // decreasing the length each iteration

//       size_reading = 0; // resetting the size of bytes to be read each iteration
//     }
//     len = bytes_read;
//   }
//   return len;
// }
