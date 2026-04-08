#include <stdio.h>    // printf
#include <stdlib.h>   // malloc free exit
#include <stdint.h>   // uint64_t for pointer math
#include <fcntl.h>    // O_RDONLY for open
#include <unistd.h>   // close read lseek getpagesize

uint64_t virt_to_phys(void *addr) {
    uint64_t page_size = getpagesize();      //should be 4096 if not perhaps problem 
    uint64_t vpn    = (uint64_t)addr / page_size;  //find page index by dividing by 4096
    uint64_t offset = (uint64_t)addr % page_size;  // byte position within that page

    int fd = open("/proc/self/pagemap", O_RDONLY);   //read only entry
    if (fd < 0) { perror("open"); exit(1); }  // if this fails halt

    lseek(fd, vpn * 8, SEEK_SET);   //multiply by 8 to access right entry 

    uint64_t entry; 
    read(fd, &entry, 8);   //open and read entry
    close(fd); // close

    if (!(entry & (1ULL << 63))) {     // check bit if page is in ram
        printf("page not in RAM (swapped out)\n");  // womp womp no physical address
        return 0;
    }

    uint64_t pfn = entry & ((1ULL << 55) - 1);  //take the Page Frame number bits and zero out the rest (55>)
    return (pfn * page_size) + offset;     // Page Frame number aka (PFN) * 4096 + offset = physical address
}

void my_function() {
    int local_var = 42;   // non-pointer, lives on the STACK
    int *heap_ptr = malloc(sizeof(int));   // pointer is in stack and the object is in HEAP
    *heap_ptr = 99;  //store 99 as long as malloc not null

    printf("local_var | virtual: %p | physical: 0x%lx\n", //print for local variable 
           &local_var, virt_to_phys(&local_var));
    printf("*heap_ptr | virtual: %p | physical: 0x%lx\n", // print both virtual and physical address of heap memory
           heap_ptr,   virt_to_phys(heap_ptr));   

    free(heap_ptr); //prevent leak
}

int main() {
    my_function();
    return 0; //Exit 
}

