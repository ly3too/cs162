/*
 * mm_alloc.c
 *
 * Stub implementations of the mm_* routines.
 */

#include "mm_alloc.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include "sglib.h"

/* info for memory block*/
typedef struct block_info{
    union{
        struct block_info *next;
        struct block_info *right;
    };

    union{
        struct block_info *prev;
        struct block_info *left;
    };
    void *start;
    size_t size;
    char colorbit;
} block_info_t;
typedef block_info_t block_dl;
block_info_t *free_blocks_list = NULL;
// block_info_t *free_blocks_end = NULL;
#define COMP_BLOCK(x, y) ((x->start)-(y->start))
SGLIB_DEFINE_DL_LIST_PROTOTYPES(block_dl, COMP_BLOCK, prev, next);
SGLIB_DEFINE_DL_LIST_FUNCTIONS(block_dl, COMP_BLOCK, prev, next);
block_info_t *last = NULL;

typedef block_info_t block_tree;
block_info_t *allocated_tree_root = NULL;
SGLIB_DEFINE_RBTREE_PROTOTYPES(block_tree, left, right, colorbit, COMP_BLOCK);
SGLIB_DEFINE_RBTREE_FUNCTIONS(block_tree, left, right, colorbit, COMP_BLOCK);

/* page linked list*/
typedef struct page_node {
    struct page_node *next;
    struct page_node *prev;
    block_info_t data[0];
} page_node_t;
page_node_t *page_start = NULL;
page_node_t *page_end = NULL;
page_node_t *cur_page = NULL;
size_t cur_indx = 0;
long max_indx = 0;

long pagesize = 0;

void handle_error(const char *err) {
    perror(err);
    exit(EXIT_FAILURE);
}

/* must align to a page*/
void *get_n_pages(int n) {
    void *start = sbrk(0);
    if (start == (void *)-1)
        handle_error("sbrk");

    if (((long)start) % pagesize == 0) {
        if (sbrk(pagesize*n) != (void *)-1) {
            return start;
        } else {
            perror("cannot allocated new page");
            return NULL;
        }
    } else {
        return NULL;
    }
}

/* align to next page and return allocated block info*/
block_info_t *align_to_page(block_info_t *info) {
    void *start = sbrk(0);
    if (start == (void *)-1)
        handle_error("sbrk");
    if (((long)start) % pagesize == 0) {
        info -> start = NULL;
        info -> size = 0;
    } else {
        if (brk((void *)((long)(start+pagesize+1)&(~(pagesize-1))))) {
            handle_error("brk");
        }
        void *end = sbrk(0);
        info -> start = start;
        info -> size = end - start;
    }
    return info;
}

/* get new entry to store mem block info */
block_info_t *get_new_entry() {
    block_info_t *new_entry;

    // needs to allocated new pages for entry
    if (cur_page == NULL) {
        void *new_page = get_n_pages(1);
        if (!new_page) {
            return NULL;
        }

        // add new page into page list
        cur_page = (page_node_t *)new_page;
        cur_page -> next = NULL;
        if (page_end == NULL) {
            page_start = page_end = cur_page;
        } else {
            page_end -> next = cur_page;
            page_end = cur_page;
        }
    }

    new_entry = cur_page -> data + cur_indx;
    if ((++cur_indx) >= max_indx) {
        cur_indx = 0;
        cur_page = cur_page -> next;
    }
    return new_entry;
}

block_info_t *remove_last_entry() {
	if (cur_page == NULL) {
		cur_page = page_end;
		cur_indx = max_indx - 1;
	} else {
		if (cur_indx == 0) {
			cur_page = cur_page -> prev;
			cur_indx = max_indx - 1;
		} else {
			--cur_indx;
		}
	}
	return cur_page -> data + cur_indx;
}

void try_to_merge_list() {
	sglib_block_dl_sort(&free_blocks_list);
	block_info_t *cur_node = free_blocks_list;
	while (cur_node && cur_node -> next) {
		if ((cur_node -> start + cur_node -> size) == (cur_node -> next -> start)) {
			block_info_t *del_entry = cur_node -> next;
			cur_node -> size += del_entry -> size;
			sglib_block_dl_delete(&free_blocks_list, cur_node -> next);

			// swap last entry with del_entry, and reinsert into tree or list
			block_info_t *last_entry = remove_last_entry();
			if (del_entry != last_entry) {
				del_entry -> start = last_entry -> start;
				del_entry -> size = last_entry -> size;
				if (sglib_block_dl_delete_if_member(&free_blocks_list, last_entry, NULL)) {
					sglib_block_dl_add_after(&free_blocks_list, del_entry);
				} else if (sglib_block_tree_delete_if_member(&allocated_tree_root, last_entry, NULL)) {
					sglib_block_tree_add(&allocated_tree_root, del_entry);
				} else {
					fprintf(stderr, "error while tring to merge list\n");
					exit(EXIT_FAILURE);
				}
			}

		} else {
			cur_node = cur_node -> next;
		}
	}
	last = free_blocks_list;
}

/* next fit */
block_info_t *alloc_in_list(size_t size) {
    // find the first matched block in free block list, allocate and save remains
    block_info_t *l;
    if (last == NULL) {
        last = free_blocks_list;
    }
    for (l = last; l!=NULL; l = l -> next) {
        if (l -> size >= size) {
			last = l -> next;
            sglib_block_dl_delete(&free_blocks_list, l);
            size_t remain = l -> size - size;
            if (remain) {
                block_info_t *entry = get_new_entry();
                entry -> size = remain;
                entry -> start = l -> start + size;
                sglib_block_dl_add_after(&free_blocks_list, entry);
            }
            l -> size = size;
            sglib_block_tree_add(&allocated_tree_root, l);
			break;
        }
    }

    return l;
}

void *mm_malloc(size_t size) {
    if (pagesize == 0) {
        pagesize = sysconf(_SC_PAGE_SIZE);
        max_indx = (pagesize - sizeof(page_node_t)) / sizeof(block_info_t);
        if (pagesize == -1)
            handle_error("pagesize");

        block_info_t tmp_info;
        align_to_page(&tmp_info);
        if (tmp_info.start != NULL) {
            block_info_t *entry = get_new_entry();
            entry -> left = NULL;
            entry -> right = NULL;
            entry -> start = tmp_info.start;
            entry -> size = tmp_info.size;
            sglib_block_dl_add_after(&free_blocks_list, entry);
        }
    }

    if (size == 0)
        return NULL;

    block_info_t *l = alloc_in_list(size);
    if (l == NULL) {
        try_to_merge_list();
        l = alloc_in_list(size);
    } else {
		return l -> start;
	}
    //if not found allocate a new page and allocate, add left mem block to free list;
    if (l == NULL) {
        void *new_block = get_n_pages((size + pagesize-1)/pagesize);
        if (new_block) {
            size_t remain = ((size + pagesize-1)/pagesize)*pagesize - size;
            if (remain) {
                block_info_t *remain_block_entry = get_new_entry();
                if (! remain_block_entry)
                    handle_error("get new entry");
                remain_block_entry -> start = new_block + size;
                remain_block_entry -> size = remain;
                sglib_block_dl_add_after(&free_blocks_list, remain_block_entry);
            }

            block_info_t *new_block_entry = get_new_entry();
            if (! new_block_entry)
                handle_error("get new entry");
            new_block_entry -> start = new_block;
            new_block_entry -> size = size;
            sglib_block_tree_add(&allocated_tree_root, new_block_entry);
            return new_block;
        }
    }

    /* YOUR CODE HERE */
    return NULL;
}

void *mm_realloc(void *ptr, size_t size) {
	if (size == 0) {
		mm_free(ptr);
		return ptr;
	}

	block_info_t elem;
    elem.start = ptr;
    block_info_t *found = sglib_block_tree_find_member(allocated_tree_root, &elem);
    if (found) {
        if(found -> size > size) {
            block_info_t *new_entry = get_new_entry();
            if (new_entry) {
				new_entry -> start = found -> start + size;
				new_entry -> size = found -> size - size;
				sglib_block_dl_add_after(&free_blocks_list, new_entry);
				found -> size = size;
            } else {
				fprintf(stderr, "cannot get new entry\n");
				exit(EXIT_FAILURE);
			}
			return ptr;

        } else if (found -> size == size) {
            return ptr;

        } else {
            void *new_ptr = mm_malloc(size);
            if (!new_ptr)
                return NULL;
            memcpy(new_ptr, ptr, found -> size);
            mm_free(ptr);
            return new_ptr;
        }
    }

    return NULL;
}

void mm_free(void *ptr) {
    block_info_t elem;
    elem.start = ptr;
    block_info_t *found;
    if (sglib_block_tree_delete_if_member(&allocated_tree_root, &elem, &found)) {
        sglib_block_dl_add_after(&free_blocks_list, found);

    } else {
        /* error */
        fprintf(stderr, "free error\n");
        exit(EXIT_FAILURE);
    }
}
