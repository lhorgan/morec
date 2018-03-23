#include "act.h"
#include "slist.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fuse.h>
#include <stdlib.h>

int read_dir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset);

#define DIRSIZE 20
#define FPTLEN 40

static int   pages_fd   = -1;

static void* pages_base =  0;
inode* inode_table = 0;

int* inode_bitmap = 0;
int* page_bitmap = 0;

void storage_init(const char* path)
{
    pages_init(path); // create file for file system data
}

int get_stat(const char* path, struct stat* st)
{
    //printf("getting stat for %s\n", path);
    inode* node = get_inode_from_path(path);
    if(!node) {
        //printf("can't get stat metadata from this path\n");
        return -1;
    }

    //printf("got file data");
    memset(st, 0, sizeof(struct stat));
    st->st_uid = getuid();
    st->st_mode = node->mode;
    st->st_size = node->size;
    return 0;
}

int unlink(const char *path) {
    inode* uinode = get_inode_from_path(path);
    if(uinode) {
        if(uinode->data_pnum >= 0) {
            uinode->refs -= 1;
            inode* parent_inode = get_inode_from_path(level_up(path));
            if(parent_inode->dir_pnum >= 0)
            {
                int inode_idx = -1;
                printf("parent inode really is a directory\n");
                char* to_unlink = get_leaf(path);
                directory_entry* parent_dir = get_directory(parent_inode->dir_pnum);
                for(int i = 0; i < DIRSIZE; i++) {
                    if(strcmp(parent_dir[i].name, to_unlink) == 0) {
                        printf("file identified, removed from list\n");
                        strcpy(parent_dir[i].name, "\0");
                        inode_idx = parent_dir[i].inode_idx;
                        parent_dir[i].inode_idx = -1;
                        break;
                    }
                }
                if(inode_idx < 0) {
                    printf("file not found in parent directory for deletion\n");
                    return -1;
                }
                if(uinode->refs == 0) {
                    printf("no links to file remain, deleting\n");
                    if(uinode->dir_pnum >= 0) {
                        printf("freeing associated directory data\n");
                        //page_bitmap[inode->dir_pnum] = 1;
                        release_page(uinode->dir_pnum);
                    }
                    if(uinode->data_pnum >= 0) {
                        printf("freeing associated file data\n");
                        int* file_pnums = (int*)pages_get_page(uinode->data_pnum);
                        for(int x = 0; x < FPTLEN; x++) {
                            if(file_pnums[x] >= 0) {
                                release_page(file_pnums[x]);
                            }
                        }
                        release_page(uinode->data_pnum);
                    }
                    release_inode(inode_idx);
                }
                return 0;
            }
        }
        else {
            printf("Directories cannot yet be deleted.  Sorry.");
        }
    }

    return -1;
}

void release_inode(int inode_idx) {
    inode_bitmap[inode_idx] = 1;
    inode* node = get_inode(inode_idx);
    node->refs = -1;
    node->mode = 0;
    node->size = -1;
    node->dir_pnum = -1;
    node->data_pnum = -1;
}

void release_page(int pnum) {
    printf("releasing page %i\n", pnum);
    page_bitmap[pnum] = 1; // page is now free
    memset(pages_get_page(pnum), 0, 4096);
}

int get_data(const char* path, size_t size, off_t offset, char* res) {
    inode* file_inode = get_inode_from_path(path);
    printf("reading data from file!\n");
    //res = (char*)malloc(size);
    if(file_inode && file_inode->data_pnum >= 0) {
        int* page_nums = (int*)pages_get_page(file_inode->data_pnum);
        int page_nums_index = offset / 4096;
        int data_remaining = size;
        int off_in_page = offset - (4096 * page_nums_index);

        while(data_remaining) {
            printf("data remaining: %i\n", data_remaining);
            int page_num = page_nums[page_nums_index];
            if(page_num < 0) { // this page isn't pointing to anything yet
                printf("page does not exist :(\n");
                return -1;
            }
            char* data = (char*)(pages_get_page(page_num) + off_in_page);
            strncpy(res, data, min(data_remaining, 4096 - off_in_page));
            if(off_in_page + data_remaining > 4096) {
                ////printf("writing starts at %i and I have %i characters to write\n", off_in_page, data_remaining);
                data_remaining -= 4096 - off_in_page;
                res += 4096 - off_in_page;
                off_in_page = 0;
            }
            else {
                data_remaining -= data_remaining;
            }
            page_num++;
        }
        printf("READ SUCCESSFUL!\n");
        return size; // success
    }
    printf("someting went wrong\n");
    return -1; // error
}

// set up pages at the specified path
void pages_init(const char* path)
{
    int exists = 0;
    if(access(path, F_OK) != -1) {
        exists = 1;
    }

    pages_fd = open(path, O_CREAT | O_RDWR, 0644); // FUSE api call
    assert(pages_fd != -1);

    int rv = ftruncate(pages_fd, NUFS_SIZE);
    assert(rv == 0);

    pages_base = mmap(0, NUFS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, pages_fd, 0); // now we have our file system in a continous 1MB chunk of memory
    assert(pages_base != MAP_FAILED);

    // this should really only happen the first time
    // page bitmap
    if(!exists) {
        // well, then, let's create it!
        page_bitmap = (int*)pages_get_page(0); // page bitmap itself is on the first page (each page is 4096 bits)
        page_bitmap[0] = 0; // first page stores the page bitmap (so mark it as NOT EMPTY)F
        page_bitmap[1] = 0; // second page stores the inode bitmap (so mark it as NOT EMPTY)
        page_bitmap[2] = 0; // inodes themselves (so mark it as NOT EMPTY)
        page_bitmap[3] = 0; // root directory structure (so mark it as NOT EMPTY)
        for(int i = 4; i < 256; i++) { // remaining pages are free
            page_bitmap[i] = 1; // ie free = true
        }

        // inode bitmap
        inode_bitmap = (int*)pages_get_page(1);
        for(int i = 0; i < 256; i++) {
            inode_bitmap[i] = 1;
        }

        inode_table = (inode*)pages_get_page(2);
        inode_table[0].size = 0;
        inode_table[0].message = 513; // just for debugging
        inode_table[0].mode = 040755;

        inode_table[0].dir_pnum = 3;
        initialize_directory(3); // create the root directory here
        inode_bitmap[0] = 0; // the 0th inode is taken
    }
    else {
        //printf("No need to create!\n");
        page_bitmap = (int*)pages_get_page(0);
        inode_bitmap = (int*)pages_get_page(1);
        inode_table = (inode*)pages_get_page(2);
    }
}

void pages_free()
{
    int rv = munmap(pages_base, NUFS_SIZE);
    assert(rv == 0);
}

void* pages_get_page(int pnum)
{
    return pages_base + 4096 * pnum;
}

directory_entry* get_directory(int pnum) {
    return (directory_entry*)pages_get_page(pnum);
}

inode* get_inode(int idx) {
    return &inode_table[idx];
}

int pages_find_empty()
{
    int pnum = -1;
    for(int ii = 2; ii < PAGE_COUNT; ++ii) {
        if(page_bitmap[ii]) { // 1 free, 0 not free
            pnum = ii;
            break;
        }
    }
    return pnum;
}

int inode_get_empty() {
    for(int i = 0; i < 256; i++) {
        if(inode_bitmap[i] == 1) { // free
            return i;
        }
    }
    return -1;
}

// wrapper around create_directory that makes a directory at a given path
int create_directory(const char* path, mode_t mode) {
    char* parent_path = level_up(path); // figure out what containing directory this new directory belongs in (this is just a string operation)
    inode* parent_inode = get_inode_from_path(parent_path); // and resolve that path into a parent directory
    if(parent_inode && parent_inode->dir_pnum >= 0) { // the parent directory EXISTS and has an associated data page (dir_pnum)
        directory_entry* parent_dir = get_directory(parent_inode->dir_pnum);
        for(int i = 0; i < DIRSIZE; i++) { // try to find a free entry in the parent directory to create its new child directory
            if(parent_dir[i].inode_idx < 0) { // free entry
                strcpy(parent_dir[i].name, get_leaf(path));

                int inode_idx = inode_get_empty(); // pick a free inode on page 2
                inode_bitmap[inode_idx] = 0; // mark that inode down as taken
                parent_dir[i].inode_idx = inode_idx; // TODO, CHECK IF INODE IS ACTUALLY FREE

                int new_dir_pnum = pages_find_empty(); // find an empty page in which to store the directory's actual data (by referencing our page table on page 0)
                initialize_directory(new_dir_pnum); // create a blank directory (finally!)
                inode* new_dir_inode = get_inode(inode_idx);
                new_dir_inode->dir_pnum = new_dir_pnum;
                printf("dir pnum exists at %i\n", pages_get_page(new_dir_pnum));
                new_dir_inode->data_pnum = -1; // not a file, a directory
                new_dir_inode->refs = 1;
                new_dir_inode->size = 0;
                new_dir_inode->mode = 040755;

                return 0;
            }
        }
    }
    return -1; // error
}

// just create an empty directory on a page (page #pnum)
int initialize_directory(int pnum) {
    page_bitmap[pnum] = 0; // mark this page as taken in the page bitmap stored at page 0
    directory_entry* entries = (directory_entry*)pages_get_page(pnum); // the the memory at page #pnum into a directory_entry
    for(int i = 0; i < DIRSIZE; i++) {
        strcpy(entries[i].name, "\0"); // all files initially have the name null
        entries[i].inode_idx = -1; // and they point nowhere
    }
}

// similar to create_directory, but it creates... get this... a FILE!
int create_file(const char* path, mode_t mode) {
    char* parent_path = level_up(path);
    inode* parent_inode = get_inode_from_path(parent_path);

    if(parent_inode && parent_inode->dir_pnum >= 0) {
        directory_entry* parent_dir = get_directory(parent_inode->dir_pnum);

        for(int i = 0; i < DIRSIZE; i++) {
            if(parent_dir[i].inode_idx < 0) { // free entry

                strcpy(parent_dir[i].name, get_leaf(path));
                int new_file_inode_idx = inode_get_empty();
                inode_bitmap[new_file_inode_idx] = 0; // taken
                parent_dir[i].inode_idx = new_file_inode_idx;
                inode* new_file_inode = get_inode(new_file_inode_idx);

                int data_pnum = pages_find_empty();
                /* okay, so, yeah, this is a little inefficient
                even if the file is just one character, it needs a whole
                page as a page table*/
                initialize_file_page_table(data_pnum);
                new_file_inode->data_pnum = data_pnum;
                new_file_inode->refs = 1;
                new_file_inode->dir_pnum = -1; // not a directory, a file
                new_file_inode->size = 0;
                new_file_inode->mode = 0100777;

                return 0;
            }
        }
    }
    return -1; // error
}

void initialize_file_page_table(int pnum) {
    // take page #pnum as a page table just for a file, organizing it into chunks
    page_bitmap[pnum] = 0; // this page is now TAKEN
    int* pnums = (int*)pages_get_page(pnum);

    for(int i = 0; i < FPTLEN; i++) {
        pnums[i] = -1; // none of these entries point anywhere yet
    }
}

int truncate(const char* path, off_t size) {
    inode* file_inode = get_inode_from_path(path);
    if(file_inode && file_inode->data_pnum >= 0) {
        printf("truncating successful!\n");
        file_inode->size = size;
        return 0;
    }
    printf("truncate failed!\n");
    return -1;
}

/*
* all this does is spread file data over a bunch of different pages_get_page
* (if it's too big to fit on one single page, that is)
*/
int write_file(const char* path, const char* buf, size_t size, off_t offset) {
    inode* file_inode = get_inode_from_path(path);
    printf("writing data to file!\n");
    if(file_inode && file_inode->data_pnum >= 0) {
        file_inode->size += size;
        int* page_nums = (int*)pages_get_page(file_inode->data_pnum);
        int page_nums_index = offset / 4096;
        int data_remaining = size;
        int off_in_page = offset - (4096 * page_nums_index);

        data_remaining = size;
        while(data_remaining) {
            printf("data remaining: %i\n", data_remaining);
            int page_num = page_nums[page_nums_index];
            if(page_num < 0) { // this page isn't pointing to anything yet
                page_num = pages_find_empty(); // TODO: HANDLE PAGE NOT BEING FREE
                page_bitmap[page_num] = 0; // this page is taken now
                page_nums[page_nums_index] = page_num;
            }
            char* data = (char*)(pages_get_page(page_num) + off_in_page);
            strncpy(data, buf, min(data_remaining, 4096 - off_in_page));
            if(off_in_page + data_remaining > 4096) {
                ////printf("writing starts at %i and I have %i characters to write\n", off_in_page, data_remaining);
                data_remaining -= 4096 - off_in_page;
                buf += 4096 - off_in_page;
                off_in_page = 0;
            }
            else {
                data_remaining -= data_remaining;
            }
            page_num++;
        }
        printf("WRITE SUCCESSFUL!\n"); // We're very excited about this.
        return size; // success
    }
    printf("someting went wrong\n");
    return -1; // error
}

int min(int num1, int num2) {
    if(num2 < num1) {
        return num2;
    }
    return num1;
}

int read_dir(const char* path, void* buf, fuse_fill_dir_t filler, off_t offset) {
    inode* dir_inode = get_inode_from_path(path);
    if(dir_inode) {
        directory_entry* dir = get_directory(dir_inode->dir_pnum);
        for(int i = offset; i < DIRSIZE; i++) {
            if(dir[i].inode_idx != -1) {
                filler(buf, dir[i].name, 0, i + 1);
            }
        }
        return 0;
    }
    return -1;
}

char* level_up(const char* path) {
    int last_slash_index = 0;
    for(int i = 0; i < strlen(path); i++) {
        if(path[i] == '/') {
            last_slash_index = i;
        }
    }
    if(last_slash_index == 0) {
        char* parent_path = (char*)malloc(2);
        parent_path[0] = '/';
        parent_path[1] = 0;
        return parent_path;
    }
    else {
        char* parent_path = (char*)malloc(last_slash_index + 1);
        for(int i = 0; i < last_slash_index; i++) {
            parent_path[i] = path[i];
        }
        parent_path[last_slash_index] = 0;
        //printf("parent of %s is %s\n", path, parent_path);
        return parent_path;
    }
}

char* get_leaf(const char* path) {
    slist* split = s_split(path, '/');
    while(split->next) {
        split = split->next;
    }
    return split->data;
}

inode* get_inode_from_path(const char* path) {
    printf("Getting inode from path: %s\n", path);
    slist* split = s_split(path, '/')->next; // split path on /
    inode* curr_node = &inode_table[0]; // start at the root directory
    while(split) {
        if(curr_node->dir_pnum >= 0) { // this node points to a directory
            directory_entry* dir = get_directory(curr_node->dir_pnum);
            int entry_found = 0;
            for(int i = 0; i < DIRSIZE; i++) { // loop through all the entries in the directory pointed to by this inode
                if(dir[i].inode_idx != -1)
                    printf("ENTRY: %s, %i\n", dir[i].name, dir[i].inode_idx);
                if(strcmp(dir[i].name, split->data) == 0) { // current entry points to the inode for our path
                    //printf("entry found: %s\n", dir[i].name);
                    entry_found = 1;
                    curr_node = get_inode(dir[i].inode_idx);
                    //printf("the new current node has directory type: %i\n", curr_node->dir_pnum);
                    break;
                }
            }
            if(!entry_found) {
                printf("entry not found: %s\n", split->data);
                return 0;
            }
        }
        else { // can't search inside something that's not a directory
            //printf("boom %i bam %i\n", curr_node->dir_pnum, curr_node->data_pnum);
            printf("you're trying to search inside something that's not a directory\n");
            return 0;
        }
        split = split->next;
    }
    return curr_node;
}
