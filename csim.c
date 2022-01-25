// Main File:        csim.c
// This File:        csim.c
// Other Files:      NA
// Semester:         CS 354 Fall 2021
// Instructor:       deppeler
//
// Author:           Alexander Dudin
// Email:            Dudin@wisc.edu
// CS Login:         Dudin
//
/////////////////////////// OTHER SOURCES OF HELP //////////////////////////////
//                   Fully acknowledge and credit all sources of help,
//                   other than Instructors and TAs.
//
// Persons:          Identify persons by name, relationship to you, and email.
//                   Describe in detail the the ideas and help they provided.
//
// Online sources:   Avoid web searches to solve your problems, but if you do
//                   search, be sure to include Web URLs and description of
//                   of any information you find.
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
//
// Copyright 2013,2019-2020
// Posting or sharing this file is prohibited, including any changes/additions.
//
////////////////////////////////////////////////////////////////////////////////

/*
 * csim.c:  
 * A cache simulator that can replay traces (from Valgrind) and output
 * statistics for the number of hits, misses, and evictions.
 * The replacement policy is LRU.
 *
 * Implementation and assumptions:
 *  1. Each load/store can cause at most 1 cache miss plus a possible eviction.
 *  2. Instruction loads (I) are ignored.
 *  3. Data modify (M) is treated as a load followed by a store to the same
 *  address. Hence, an M operation can result in two cache hits, or a miss and a
 *  hit plus a possible eviction.
 */  

#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

/******************************************************************************/
/* DO NOT MODIFY THESE VARIABLES **********************************************/

//Globals set by command line args.
int b = 0; //number of block (b) bits
int s = 0; //number of set (s) bits
int E = 0; //number of lines per set

//Globals derived from command line args.
int B; //block size in bytes: B = 2^b
int S; //number of sets: S = 2^s

//Global counters to track cache statistics in access_data().
int hit_cnt = 0;
int miss_cnt = 0;
int evict_cnt = 0;

//Global to control trace output
int verbosity = 0; //print trace if set
/******************************************************************************/
  
  
//Type mem_addr_t: Use when dealing with addresses or address masks.
typedef unsigned long long int mem_addr_t;

//Type cache_line_t: Use when dealing with cache lines.
//TODO - COMPLETE THIS TYPE
typedef struct cache_line {                    
    char valid;
    mem_addr_t tag;
    int full; // 1 if yes 0 if not 
    //Add a data member as needed by your implementation for LRU tracking.
} cache_line_t;

//Type cache_set_t: Use when dealing with cache sets
//Note: Each set is a pointer to a heap array of one or more cache lines.
typedef cache_line_t* cache_set_t;
//Type cache_t: Use when dealing with the cache.
//Note: A cache is a pointer to a heap array of one or more sets.
typedef cache_set_t* cache_t;

// Create the cache (i.e., pointer var) we're simulating.
cache_t cache;  

/* TODO - COMPLETE THIS FUNCTION
 * init_cache:
 * Allocates the data structure for a cache with S sets and E lines per set.
 * Initializes all valid bits and tags with 0s.
 */                    
void init_cache() {   
    S = pow(2,s);// sets S 
    B = pow(2,b); //Sets B
    //alloacates the needed ammound of sets
    cache_t sets = malloc(sizeof(cache_set_t)*S);
    if (sets == NULL){
        printf("Malloc did not successfully allocate data to the cache");
        exit(1);
    }
    for(int i = 0 ; i < S; i ++){
        sets[i]= calloc(E,sizeof(cache_line_t));// allocates the needed number of lines and sets everything to zero 
        if(sets[i] == NULL){
             printf("calloc did not successfully allocate data to the cache");
             exit(1);
        }
    }
    cache = sets; 
}
  

/* TODO - COMPLETE THIS FUNCTION 
 * free_cache:
 * Frees all heap allocated memory used by the cache.
 */                    
void free_cache() {         
        for(int set =0; set<S;set++){
                free((cache[set]));
                cache[set]= NULL;
        }
        free(cache);
        cache = NULL;
}
// the next three functions implemnt the queue for three cases there is room there isnt room and there is a hit

/*
* makes the set into a queue where the least recently used goes to the bottom of the 
*set then everything gets moved down 
*adn the most recently acessed or added goes to the top
*/
void implemntQueuefull(mem_addr_t addr, cache_set_t set){
     mem_addr_t tag = addr >> (s + b);
    for(int i = E-1;i>0;i--){
        (set+i)->tag = (set+(i-1))->tag; 
        (set+i)->valid = (set+(i-1))->valid; 
        (set+i)->full = (set+(i-1))->full; 
    }
    
    (set+0)->tag = tag;
    (set+0)->full =1;
    (set+0) ->valid = '1';
}
/*
* makes the set into a queue where the least recently used goes to the bottom of the 
*set then everything gets moved down 
*adn the most recently acessed or added goes to the top
*/
void implementQueueroom(mem_addr_t addr, cache_set_t set, int linefree){
     mem_addr_t tag = addr >> (s + b);

    for(int i =linefree; i >0;i--){
        (set+i)->tag = (set+(i-1))->tag; 
        (set+i)->valid = (set+(i-1))->valid; 
        (set+i)->full = (set+(i-1))->full; 
    }
    (set+0)->tag = tag;
    (set+0)->full =1;
    (set+0) ->valid = '1';
}
/*
* makes the set into a queue where the least recently used goes to the bottom of the 
*set then everything gets moved down 
*adn the most recently acessed or added goes to the top
*/
void implementQueuehit(mem_addr_t addr, cache_set_t set){
    int line = E;
     mem_addr_t tag = addr >> (s + b);
    int hit = E;
    cache_line_t hit_l ;
    for(int i =0;i<E;i++){
        if(set[i].valid == '0'){
            line = i;
        }else if(set[i].tag == tag && set[i].valid == '1'){
            hit = i;
            hit_l = set[i];
        }
    }
    if(line != E){
    for(int i =line;i>0;i--){
        if(i==hit){
            (set+i)->tag = (set+(i-1))->tag; 
            (set+i)->valid = (set+(i-1))->valid; 
            (set+i)->full = (set+(i-1))->full; 
        }
    }
    set[0] = hit_l;
    }else{
        for(int i = hit; i >0;i--){
            (set+i)->tag = (set+(i-1))->tag; 
            (set+i)->valid = (set+(i-1))->valid; 
            (set+i)->full = (set+(i-1))->full; 
        }
        set[0] = hit_l;
    }
}

/* TODO - COMPLETE THIS FUNCTION 
 * access_data:
 * Simulates data access at given "addr" memory address in the cache.
 *
 * If already in cache, increment hit_cnt
 * If not in cache, cache it (set tag), increment miss_cnt
 * If a line is evicted, increment evict_cnt
 */                    
void access_data(mem_addr_t addr) {      
     cache_line_t *line;
     mem_addr_t tag = addr >> (s + b);// uses right shifts to get the tag
    mem_addr_t set = (addr - (tag << (s + b))) >> b;// uses left and right shifts to get the set
    cache_set_t setInCache = *(cache + set);
     int yes = 0;// if there was a hit
     int lin = 0;//empty line
    for(int e = 0 ;e < E; e ++){
        line  = (setInCache +e);
        if (line->valid == '1'){//checks if its valid
            if(line->tag == tag){
                yes+=1;
                hit_cnt++; 
                implementQueuehit(addr,setInCache);// calls helper
                break;
            }
        }else{
            lin = e;
            break;
        }
    }
    if(yes == 0){
      if(line->full == 0){
          //if there is only one line 
          if(E ==1){
            (line+0)->tag = tag;
            (line+0)->full =1;
            (line+0) ->valid = '1';
          }else{
          implementQueueroom(addr,setInCache,lin);// calls helper
          }
          miss_cnt+=1;
      }else{
          // if there is only one line 
          if(E ==1){
            (line+0)->tag = tag;
            (line+0)->full =1;
            (line+0) ->valid = '1';
          }else{
          implemntQueuefull(addr,setInCache);// calls helper
          }
          miss_cnt+=1;
          evict_cnt+=1;
      }
    }
    
}
  

  
/* TODO - FILL IN THE MISSING CODE
 * replay_trace:
 * Replays the given trace file against the cache.
 *
 * Reads the input trace file line by line.
 * Extracts the type of each memory access : L/S/M
 * TRANSLATE each "L" as a load i.e. 1 memory access
 * TRANSLATE each "S" as a store i.e. 1 memory access
 * TRANSLATE each "M" as a load followed by a store i.e. 2 memory accesses 
 */                    
void replay_trace(char* trace_fn) {           
    char buf[1000];  
    mem_addr_t addr = 0;
    unsigned int len = 0;
    FILE* trace_fp = fopen(trace_fn, "r"); 

    if (!trace_fp) { 
        fprintf(stderr, "%s: %s\n", trace_fn, strerror(errno));
        exit(1);   
    }

    while (fgets(buf, 1000, trace_fp) != NULL) {
        if (buf[1] == 'S' || buf[1] == 'L' || buf[1] == 'M') {
            sscanf(buf+3, "%llx,%u", &addr, &len);
      
            if (verbosity)
                printf("%c %llx,%u ", buf[1], addr, len);

            // TODO - MISSING CODE
            // GIVEN: 1. addr has the address to be accessed
            //        2. buf[1] has type of acccess(S/L/M)
            // call access_data function here depending on type of access
                if(buf[1] == 'S' || buf[1] == 'L' ){
                    access_data(addr);
                }else if(buf[1] == 'M' ){
                    access_data(addr);
                     access_data(addr);
                }
            if (verbosity)
                printf("\n");
        }
    }

    fclose(trace_fp);
}  
  
  
/*
 * print_usage:
 * Print information on how to use csim to standard output.
 */                    
void print_usage(char* argv[]) {                 
    printf("Usage: %s [-hv] -s <num> -E <num> -b <num> -t <file>\n", argv[0]);
    printf("Options:\n");
    printf("  -h         Print this help message.\n");
    printf("  -v         Optional verbose flag.\n");
    printf("  -s <num>   Number of s bits for set index.\n");
    printf("  -E <num>   Number of lines per set.\n");
    printf("  -b <num>   Number of b bits for block offsets.\n");
    printf("  -t <file>  Trace file.\n");
    printf("\nExamples:\n");
    printf("  linux>  %s -s 4 -E 1 -b 4 -t traces/yi.trace\n", argv[0]);
    printf("  linux>  %s -v -s 8 -E 2 -b 4 -t traces/yi.trace\n", argv[0]);
    exit(0);
}  
  
  
/*
 * print_summary:
 * Prints a summary of the cache simulation statistics to a file.
 */                    
void print_summary(int hits, int misses, int evictions) {                
    printf("hits:%d misses:%d evictions:%d\n", hits, misses, evictions);
    FILE* output_fp = fopen(".csim_results", "w");
    assert(output_fp);
    fprintf(output_fp, "%d %d %d\n", hits, misses, evictions);
    fclose(output_fp);
}  
  
  
/*
 * main:
 * Main parses command line args, makes the cache, replays the memory accesses
 * free the cache and print the summary statistics.  
 */                    
int main(int argc, char* argv[]) {                      
    char* trace_file = NULL;
    char c;
    
    // Parse the command line arguments: -h, -v, -s, -E, -b, -t 
    while ((c = getopt(argc, argv, "s:E:b:t:vh")) != -1) {
        switch (c) {
            case 'b':
                b = atoi(optarg);
                break;
            case 'E':
                E = atoi(optarg);
                break;
            case 'h':
                print_usage(argv);
                exit(0);
            case 's':
                s = atoi(optarg);
                break;
            case 't':
                trace_file = optarg;
                break;
            case 'v':
                verbosity = 1;
                break;
            default:
                print_usage(argv);
                exit(1);
        }
    }

    //Make sure that all required command line args were specified.
    if (s == 0 || E == 0 || b == 0 || trace_file == NULL) {
        printf("%s: Missing required command line argument\n", argv[0]);
        print_usage(argv);
        exit(1);
    }
    
    //Initialize cache.
    init_cache();

    //Replay the memory access trace.
    replay_trace(trace_file);

    //Free memory allocated for cache.
    free_cache();

    //Print the statistics to a file.
    //DO NOT REMOVE: This function must be called for test_csim to work.
    print_summary(hit_cnt, miss_cnt, evict_cnt);
    return 0;   
}  





// 202109
