#include <stdbool.h>

#define HOSTS_PER_SERVICE 50
#define TASKS_PER_SERVICE 100

typedef struct {
    int id;
    int max_ram;
    int curr_ram;
} __attribute__((__packed__)) host_t ;

typedef struct {
    int id;
    int reserved_ram;
    int reserved_hdd;
    int allocated_host_id;
} __attribute__((__packed__)) task_t;

typedef struct {
    int id;

    host_t host_list[HOSTS_PER_SERVICE];
    task_t task_list[TASKS_PER_SERVICE];
} __attribute__((__packed__)) service_t;
