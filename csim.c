#define _GNU_SOURCE
#include <getopt.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "cachelab.h"
#include <math.h>

typedef struct instruction {
    char operation;
    long long tag;
    long long set_id;
    long long address;
    int size;
} instruction;

typedef struct cacheline {
    int set_id;
    int time;
    long long *tags_array;
    int *recently_used;
} cacheline;

void print_instruction(instruction c){
    printf("operation: %c\ntag: 0x%llx\nset_id: 0x%llx\nsize: %d\n",
            c.operation,c.tag,c.set_id,c.size);
}

void print_cache(cacheline* cache, int s, int E){
    printf("Set\t");
    int i, j;

    for (i = 0; i < E; i++){
        printf("Way %d\t\t",i);
    }
    printf("\n-------------------------------------------------------------\n");
    for (i = 0; i < s; i++){
        printf("0x%x\t",cache[i].set_id);
        for (j = 0; j < E; j++){
            printf("%d, 0x%llx\t\t",(cache[i]).recently_used[j],\
                                    (cache[i]).tags_array[j]);
        }
        printf("\n");
    }
    printf("\n");
}

instruction* find_line(char* t, int s, int E, int b, char *line, \
                       FILE *trace_file, instruction* c){
    char operation, *line_pointer;
    long long address, mask1, mask2, all_ones = ~0;
    size_t length;
    int size;

    if (getline(&line, &length, trace_file) != -1) {
        line_pointer = line;
        
        if ((line[1] == 'S') || (line[1] == 'L') || (line[1] == 'M')) {
            operation = line[1];
            
            while (line_pointer[0] != ','){
                line_pointer++;
            }
            char* comma = line_pointer;
            size = atoi(++line_pointer);
            address = strtoll(line+3,&comma,16);
            
            (*c).size = size;
            (*c).address = address;
            (*c).operation = operation;
            (*c).tag = address >> (s + b);
            
            mask1 = all_ones << (s + b);
            mask2 = all_ones << b;
            (*c).set_id = (address & (mask1 ^ mask2)) >> b;
        } else if (line[0] == 'I'){
            (*c).operation = 'I';
        } else {
            (*c).operation = 'E';
        }
        return c;
    } else {
        (*c).operation = 'E';
        return c;
    }
}

int find_min_index(int* lst, int size){
    int index = 0;
    int min = lst[0];
    for (int i = 0; i < size; i++){
        if (lst[i] < min){
            min = lst[i];
            index = i;
        }
    }
    return index;
}

void simulate(char v,int s, int E,int b,char* t){
    char *line = NULL;
    size_t length = 1;
    FILE *trace_file= fopen(t,"r");
    instruction instruc;
    char hit = (char) length, miss, eviction;
    int lru, hits = 0, misses = 0, evictions = 0;
    

    find_line(t,s,E,b,line,trace_file,&instruc);
    int sets = pow(2,s);
    cacheline cache[sets];

    int ru[sets][E];
    int times[sets];
    for (int i = 0; i < sets; i++){
        cache[i].set_id = i;
        times[i] = 1;
        (cache[i]).tags_array = (long long*)malloc(sizeof(long long)*E);
        for (int j = 0; j < E; j++){
            ru[i][j] = 0;
        }
    }

    while (instruc.operation != 'E'){
        if (instruc.operation == 'I'){
            find_line(t,s,E,b,line,trace_file,&instruc);
            continue;
        }
        hit = 0;
        miss = 0;
        eviction = 0;
        for (int i = 0; i < E; i++){
            if ((cache[instruc.set_id].tags_array[i] == instruc.tag) && \
                ru[instruc.set_id][i] != 0) {
                hits++;
                ru[instruc.set_id][i] = times[instruc.set_id]++;
                hit = 1;
                break;
            }
        }
        if (!hit) {
            misses++;
            miss = 1;
            lru = find_min_index(ru[instruc.set_id],E);
            evictions += ru[instruc.set_id][lru] != 0;
            eviction = ru[instruc.set_id][lru] != 0;
            ru[instruc.set_id][lru] = times[instruc.set_id]++;
            cache[instruc.set_id].tags_array[lru] = instruc.tag;
        }

        if (v) {
            //print_cache(cache,s,E);
            printf("%c %llx,%d",instruc.operation,instruc.address,instruc.size);
            if (hit) {
                printf(" hit");
            } else if (eviction) {
                printf(" miss eviction");
            } else if (miss) {
                printf(" miss");
            }
            if (instruc.operation == 'M'){
                printf(" hit");
            }
            printf(" \n");
        } 
        if (instruc.operation == 'M'){
            hits++;
        }
        find_line(t,s,E,b,line,trace_file,&instruc);
    }
    printSummary(hits,misses,evictions);
}


int main(int argc, char *argv[])
{
    // Declare option variables
    int c;
    char verbose = 0;
    int set, associativity, block_bits;
    char* trace;

    // Set option variables
    while ((c = getopt(argc, argv, "vs:E:b:t:")) != -1){
        switch (c) {
        case 'v':
            verbose = 1;
            break;
        case 's':
            set = atoi(optarg);
            break;
        case 'E':
            associativity = atoi(optarg);
            break;
        case 'b':
            block_bits = atoi(optarg);
            break;
        case 't':
            trace = optarg;
            break;
        case '?':
            if (optopt == 'c')
                fprintf(stderr, "Option -%c requires an argument.\n", optopt);
            else
                fprintf(stderr, "Unknown option `-%c'.\n", optopt);
            exit(1);
        default:
            printf("?? getopt (%s) returned character code 0%o ??\n", optarg,c);
        }
    }

    
    simulate(verbose, set, associativity, block_bits, trace);

}
