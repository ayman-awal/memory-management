#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "cache.h"
#include "mdadm.h"

int check_cache = 0; // Initially cache is not created

static cache_entry_t *cache = NULL;
static int cache_size = 0;
static int clock = 0;
static int num_queries = 0;
static int num_hits = 0;

int starting_index = 0;  // First index in cache in which an entry will be placed
int replace_index;   // Global variable used in the lru process
int num_cache_items = 0;  // Global variable which keeps track of how many items are in the cache
int return_status;   // Global variable in which the return value is assigned


int lruHelper(cache_entry_t array[], int cache_size) {  // custom made function that helps with lru.
	int minAccessTime = array[0].access_time;            // Finds and returns the entry with the
                                                      // lowest access time in the main function body so it can be replaced by the new entry.
  if (cache_size == 1){       
		return array[0].access_time;	
	}
  else if (cache_size > 1){
		for (int i = 0; i < cache_size; i++){

      if (array[i].access_time < minAccessTime){
        minAccessTime = array[i].access_time;
      }
      else{
        continue;
      }
		}
	}
	return minAccessTime;
}

void reset_values(void){                  // Custom made function which resets the values of disk_num and block_num to -2
  for (int i = 0; i < cache_size; i++)   // so there's no issues with insert function
    {
      cache[i].disk_num = -2;
      cache[i].block_num = -2;
    }
}

int cache_create(int num_entries) {
  if (check_cache == 1)
  {
    return -1;
  }

  if (num_entries < 2 || num_entries > 4096)  // checks for invalid num_entries parameter
  {
    return -1;
  }

  else
  { 
    check_cache = 1; // Means cache is available
    cache = (cache_entry_t*) malloc(num_entries * sizeof(cache_entry_t));  // Dynamically allocating memory for the cache
    cache_size = num_entries;
    
    reset_values(); // Calling the function to reset
    
    return 1;
  }
  return 1;
}


int cache_destroy(void) {
  if (cache == 0)
  {
    return -1;
  }

  else
  {
    check_cache = 0; // Cache is no longer available
    free(cache); // Freeing memory

    // Setting these variables to its original default values
    cache = NULL;
    cache_size = 0;
    clock = 0;
    starting_index = 0;
    num_cache_items = 0;

    return 1;
  }
  //return -10;
}

int cache_lookup(int disk_num, int block_num, uint8_t *buf) {
  num_queries++; // incrementing num_queries as told

  if (check_cache == 0)
  {
    return -1;
  }

  if (num_cache_items == 0)
  {
    return -1;
  }
  
  if (disk_num < 0 || disk_num > 16) // checks for invalid disk_num parameter
  {
    return -1;
  }

  if (block_num < 0 || block_num > 255)  // Checks for invalid block_num parameter
  {
    return -1;
  }
  
  if (buf == NULL){  // Checks if buf is NULL or not
    return -1;
  }
  else {
    for (int i = 0; i < cache_size; i++){
      if (cache[i].disk_num == disk_num && cache[i].block_num == block_num){ // Checks if the disk number and block number                       
        memcpy(buf, cache[i].block, JBOD_BLOCK_SIZE);                       // are already in the cache or not
        num_hits++;               // Updating num_hits, clock and access_time
        clock++;
        cache[i].access_time = clock;

        return_status = 1;
        break;    // breaks the moment we get a disk and block number match
      }
      else {
        return_status = -1;
      }
    }
    return return_status;
  }
}

void cache_update(int disk_num, int block_num, const uint8_t *buf) {
  num_queries++;
  for (int i = 0; i < cache_size; i++) {

    if (cache[i].disk_num == disk_num && cache[i].block_num == block_num) {
      memcpy(cache[i].block, buf, JBOD_BLOCK_SIZE); // Copying data into the block
              // Updating the clock and access_time
      clock++;
      cache[i].access_time = clock; 
    }
  }
}

bool repeat_entry(int disk_num, int block_num){  // Custom made function used to check for repeated entries
  bool repeat_checker = false;

  for (int i = 0; i < cache_size; i++){
    if ((cache[i].disk_num == disk_num) && (cache[i].block_num == block_num)){
      repeat_checker = true;
      break; // breaks as soon as we find a match
    } 
    else{
      continue;
    }
  }
  return repeat_checker;
}

int cache_insert(int disk_num, int block_num, const uint8_t *buf) {
  num_queries++;

  if (check_cache == 0)
  {
    return -1;
  }

  if (cache_size == 0)
  {
    return -1;
  }

  if (repeat_entry(disk_num, block_num) == true){ // Same entry cannot be inserted again. The function call checks for that
    return -1;
  }
  
  if (buf == NULL)
  {
    return -1;
  }

  if (disk_num < 0 || disk_num > 16)  // Checks for invalid parameters
  {
    return -1;
  }

  if (block_num < 0 || block_num > 255)  // Checks for invalid parameters
  {
    return -1;
  }

  else {
    cache_entry_t entry; // Creating an object for the new entry
    clock++;  // Incrementing clock
    entry.access_time = clock; // Assigning access time to clock after incrementing it as told

    entry.block_num = block_num; // Assigning the block number 
    entry.disk_num = disk_num;   // and disk number

    memcpy(entry.block, buf, JBOD_BLOCK_SIZE);  // Performing memcopy from buffer to block 


    if(starting_index >= cache_size) { // When cache is full
      replace_index = lruHelper(cache, cache_size);  // lruHelper returns the access time

      for (int i = 0; i < cache_size; i++){
        if (cache[i].access_time == replace_index){ // Acquiring  the repective index
          cache[i] = entry; // Evicting the cache block and replacing it with new entry
        }
      }  
      return_status = 1;
    } 

    else { // When there is space in the cache
      cache[starting_index] = entry;
      starting_index++; // Incrementing starting_index since the next entry will placed in the following index in the cache
      num_cache_items++; // number of items in the cache has increased

      return_status = 1; // Updating return status
    }

    return return_status;
  }
}

bool cache_enabled(void){
  if (cache_size >= 2){
    return true;
  } 
  else if (check_cache == 1){
    return true;
  }
  else{
    return false;
  }
}

void cache_print_hit_rate(void) {
  fprintf(stderr, "Hit rate: %5.1f%%\n", 100 * (float) num_hits / num_queries);
} 