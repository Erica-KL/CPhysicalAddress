#include <stdio.h> //fflush
#include <stdlib.h> //exit
#include <stdint.h> //unit64_t
#include <fcntl.h> //O_RDONLY
#include <unistd.h> //reed lseek close getpagesize
#include <sys/mman.h> //mmap munmap MAP_SHARED MAP_ANONYMUS
#include <sys/wait.h> //wait()

uint64_t virt_to_phys(void *addr) { //from pointer to physical address
    uint64_t page_size = getpagesize(); // should be 4096 again
    uint64_t vpn    = (uint64_t)addr / page_size; //which page? divide by 4096
    uint64_t offset = (uint64_t)addr % page_size; // byte position aka the remainder

    int fd = open("/proc/self/pagemap", O_RDONLY); //open read only 
    if (fd < 0) { perror("open"); return 0; } //return 0 for failure

    lseek(fd, vpn * 8, SEEK_SET); //seek 8 bytes

    uint64_t entry;
    read(fd, &entry, 8); //page entry= 8 read one 8bit entry the one found prior
    close(fd); //close

    if (!(entry & (1ULL << 63))) return 0;//in ram? if not no physical address

    uint64_t pfn = entry & ((1ULL << 55) - 1); //take the page frame number (PFN) 0-55
    return (pfn * page_size) + offset;//find the phyiscal address
}

void print_info(const char *role, int *shared_mem) { //prints info shared memory pointer
    uint64_t vaddr = (uint64_t)shared_mem;//convert pointer to integer for virtual
    uint64_t paddr = virt_to_phys(shared_mem); //convert virtual address to physical address

    printf("[%s | PID %d]\n", role, getpid()); //feedback
    printf("  Value            : %d\n",           *shared_mem); //print value
    printf("  Virtual address  : 0x%016lx\n",     vaddr); //print virtual address
    printf("  Physical address : 0x%016lx\n\n",   paddr); //print physical address
    fflush(stdout); //flush output
}

int main(void) {
    long page_size = getpagesize(); //get page size

    int *shared_mem = mmap(NULL, page_size,  // allocate page of shared memory
                           PROT_READ | PROT_WRITE, //read and write
                           MAP_SHARED | MAP_ANONYMOUS, //backed by RAM
                           -1, 0); //place holder for anonymous
    if (shared_mem == MAP_FAILED) { perror("mmap"); return 1; } //returns map_failed on failure

    *shared_mem = 1234;// write value and fault to ensure physical value

    pid_t child = fork(); //create child process so it runs doesnt stop parent from running parent will wait
    if (child < 0) { perror("fork"); return 1; } //PID to parent 0 to child except -1 if error

    if (child == 0) { //fork to child
        *shared_mem = 5678; //write value to shared memory
        print_info("CHILD", shared_mem); //prints it out (should come first because wait)
        _exit(0);//exit
    } else {
        wait(NULL);//wait for fork to finish
        print_info("PARENT", shared_mem);//print parent output
    }

    munmap(shared_mem, page_size); //release memory mapping
    return 0;//exit
}