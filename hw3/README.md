## simple implementation of malloc realloc and free
- using brk and sbrk to get memory from os
- seperated block-description and allocated-block address
- use linked list to store allocated pages for allocation information, every pages are page aligned, which can be set to proteced mode with mprotect() function, so that user cannot modify the important information
- allocated blocks and free blocks are described with block_infos, wich are stored in the pages in the form of linked list
- free memory block_infos are stored in the form of double linked list
- allocated memory block_infos are stored in the form of RB tree
