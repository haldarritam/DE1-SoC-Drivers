#ifndef AUX_FUNCTIONS_H
#define AUX_FUNCTIONS_H

int map_virtual(void);
void unmap_virtual(void);
int open_physical (int fd);
void close_physical (int fd);
void* map_physical(int fd, unsigned int base, unsigned int span);
int unmap_physical(void* virtual_base, unsigned int span);

#endif
