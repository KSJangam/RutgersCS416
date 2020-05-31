#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
//Assume the address space is 32 bits, so the max memory size is 4GB
#define PGSIZE 4096

// Maximum size of virtual memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024

// Size of "physcial memory"
#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef unsigned int pte_t;

// Represents a page directory entry
typedef unsigned int pde_t;

#define TLB_ENTRIES 512

//Structure to represents TLB
struct tlb {
  unsigned int va;
  unsigned int pa;
  int valid;
  int size;
   
};
struct tlb tlb_store;


void set_physical_mem();
pte_t* translate( void *va);
int page_map( void *va, void* pa);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *a_malloc(unsigned int num_bytes);
void a_free(void *va, int size);
void put_value(void *va, void *val, int size);
void get_value(void *va, void *val, int size);
void mat_mult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();

#endif
