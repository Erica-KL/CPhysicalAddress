 /*
 * part2_shared_memory.c
 *
 * Creates TWO processes that share a piece of memory allocated from
 * the heap (via mmap with MAP_SHARED | MAP_ANONYMOUS, which is the
 * POSIX-portable way to share heap-style memory across fork()).
 *
 * Why MAP_ANONYMOUS instead of plain malloc?
 *   malloc() returns memory that is private copy-on-write after fork().
 *   MAP_SHARED | MAP_ANONYMOUS creates a region that *both* parent and
 *   child see the same physical pages for — true shared memory without
 *   a backing file.
 *
 * Each process prints:
 *   - its PID
 *   - the virtual address of the shared region
 *   - the physical address of the shared region (via /proc/self/pagemap)
 *
 * Key observation:
 *   The virtual addresses will typically DIFFER between processes
 *   (modern kernels use ASLR), but the PHYSICAL addresses will be
 *   IDENTICAL — confirming that both processes map to the same RAM page.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <errno.h>

/* Same translation function as Part 1 */
uintptr_t virtual_to_physical(uintptr_t vaddr)
{
    long page_size = sysconf(_SC_PAGESIZE);

    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0) { perror("open pagemap"); return 0; }

    uintptr_t vpn    = vaddr / (uintptr_t)page_size;
    off_t     offset = (off_t)(vpn * 8);

    if (lseek(fd, offset, SEEK_SET) == (off_t)-1) {
        perror("lseek"); close(fd); return 0;
    }

    uint64_t entry = 0;
    ssize_t  n     = read(fd, &entry, sizeof(entry));
    close(fd);

    if (n != sizeof(entry)) return 0;
    if (!(entry & (1ULL << 63))) {
        fprintf(stderr, "[PID %d] Page not present\n", getpid());
        return 0;
    }

    uint64_t  pfn         = entry & ((1ULL << 55) - 1);
    uintptr_t page_offset = vaddr & (uintptr_t)(page_size - 1);
    return (uintptr_t)(pfn * (uint64_t)page_size) + page_offset;
}

void print_shared_info(const char *role, int *shared_mem)
{
    uintptr_t vaddr = (uintptr_t)shared_mem;
    uintptr_t paddr = virtual_to_physical(vaddr);

    printf("[%s | PID %d]\n", role, getpid());
    printf("  Shared memory value      : %d\n",   *shared_mem);
    printf("  Virtual  address         : 0x%016lx\n", (unsigned long)vaddr);
    if (paddr)
        printf("  Physical address         : 0x%016lx\n", (unsigned long)paddr);
    else
        printf("  Physical address         : (unavailable — needs root)\n");
    printf("\n");
    fflush(stdout);
}

int main(void)
{
    long page_size = sysconf(_SC_PAGESIZE);

    /* Allocate ONE page of shared anonymous memory */
    int *shared_mem = (int *)mmap(
        NULL,
        (size_t)page_size,
        PROT_READ | PROT_WRITE,
        MAP_SHARED | MAP_ANONYMOUS,
        -1, 0);

    if (shared_mem == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    *shared_mem = 1234;   /* initialise before fork */

    printf("=== Shared memory: virtual-to-physical comparison ===\n\n");

    /* Touch the page NOW so the kernel faults it in before fork() */
    volatile int dummy = *shared_mem;
    (void)dummy;

    pid_t child = fork();

    if (child < 0) {
        perror("fork");
        munmap(shared_mem, (size_t)page_size);
        return 1;
    }

    if (child == 0) {
        /* ---- CHILD ---- */
        /* Modify the shared value so we can confirm both processes see it */
        *shared_mem = 5678;
        print_shared_info("CHILD", shared_mem);
        _exit(0);

    } else {
        /* ---- PARENT ---- */
        /* Wait briefly so the child writes first */
        wait(NULL);
        print_shared_info("PARENT", shared_mem);

        printf("--- Observations ---\n");
        printf("1. The value written by the child (%d) is visible in the parent,\n"
               "   confirming the memory IS truly shared.\n", *shared_mem);
        printf("2. Virtual addresses may differ (ASLR), but physical addresses\n"
               "   should be IDENTICAL, proving both processes map to the same\n"
               "   physical RAM page.\n");
    }

    munmap(shared_mem, (size_t)page_size);
    return 0;
}