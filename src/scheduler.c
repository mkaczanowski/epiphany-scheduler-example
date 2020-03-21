#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <e-hal.h>
#include <e-loader.h>
#include "proto.h"

const unsigned total_iterations = 10000;
const unsigned total_services = 300;
const unsigned shm_size = sizeof(service_t)*total_services;
const char shm_name[] = "services_shm"; 

void initialize_services(e_mem_t* mbuf, int total_services) {
    int i, j;

    for (i = 0; i < total_services; i++) {
        service_t service = { .id = i };

        memset( &(service.host_list), sizeof(host_t) * HOSTS_PER_SERVICE, 0 );
        for (j = 0; j < HOSTS_PER_SERVICE; j++ ) {
            service.host_list[j].id = j;
            service.host_list[j].max_ram = 1024;
            service.host_list[j].curr_ram = 1024;
        }

        memset( &(service.task_list), sizeof(task_t) * TASKS_PER_SERVICE, 0 );

        for (j = 0; j < TASKS_PER_SERVICE; j++ ) {
            service.task_list[j].id = j;
            service.task_list[j].reserved_ram = 512;
            service.task_list[j].reserved_hdd = 512;
            service.task_list[j].allocated_host_id = -1;
        }

        e_write(mbuf, 0, 0, sizeof(service_t)*i, (void*) &service, sizeof(service_t));
    }
}

void simulate_hosts_ram_change(e_mem_t* mbuf, int service_id, int total_hosts, int ram) {
    service_t service;
    e_read(mbuf, 0, 0, service_id * sizeof(service_t), &service, sizeof(service_t));

    int host_index, i;
    for (i = 0; i < total_hosts; i++) {
        host_index = rand() % HOSTS_PER_SERVICE;
        service.host_list[host_index].max_ram = ram;
    }

    e_write(mbuf, 0, 0, sizeof(service_t)*service_id, (void*) &service, sizeof(service_t));
}

void update_service(e_epiphany_t* dev, unsigned row, unsigned col, int service_id) {
    e_write(dev, row, col, 0x7000, &service_id, sizeof(int));
}

// highly inefficient, but shall do for learning purposes
int find_free_cpu(e_epiphany_t* dev, int rows, int cols) {
    int i, j;

    for(i = 0; i < rows; i++) {
        for (j = 0; j < cols; j++) {
            int service_id;
            e_read(dev, i, j, 0x7000, &service_id, sizeof(int));
            if (service_id == -1) {
                return (int) ((i << 3) | j); // being lazy: for Epiphany III we assume there are 4x4 cores
            }
        }
    }

    return -1;
}

void dump_service(e_mem_t* mbuf, int service_id) {
    int i, j;
    service_t service;
    e_read(mbuf, 0, 0, service_id * sizeof(service_t), &service, sizeof(service_t));

    // sort hosts by id (not required but convinient)
    for(i = 0; i < HOSTS_PER_SERVICE; i++) {
        for(j = i+1; j < HOSTS_PER_SERVICE; j++) {
            if(service.host_list[i].id > service.host_list[j].id) {
                host_t tmp_host = service.host_list[i];
                service.host_list[i] = service.host_list[j];
                service.host_list[j] = tmp_host;
            }
        }
    }

    printf("TASK_ID,ALLOCATED_HOST,TASK_RAM,CURR_RAM,MAX_RAM\n");
    for (i = 0; i < TASKS_PER_SERVICE; i++) {
        int host_id = service.task_list[i].allocated_host_id;
        host_t host = service.host_list[host_id];

        printf("%d,%d,%d,%d,%d\n", service.task_list[i].id, host_id, service.task_list[i].reserved_ram, host.curr_ram, host.max_ram);
    }
}

int main(int argc, char *argv[]) {
    e_set_loader_verbosity(H_D0);
    e_set_host_verbosity(H_D0);

    unsigned i, j;
    int rc;

    e_platform_t platform;
    e_epiphany_t dev;
    e_mem_t   mbuf;

    srand(1);

    e_init(NULL);
    e_reset_system();
    e_get_platform_info(&platform);

    // allocate shared memory buffer to store all services
    rc = e_shm_alloc(&mbuf, shm_name, shm_size);
    if (rc != E_OK)
        rc = e_shm_attach(&mbuf, shm_name);

    if (rc != E_OK) {
        fprintf(stderr, "Failed to allocate shared memory. Error is %s\n",
                strerror(errno));
        return EXIT_FAILURE;
    }

    // open 16-core workgroup
    e_open(&dev, 0, 0, platform.rows, platform.cols);

    // load epiphany program
    if ( E_OK != e_load_group("e_scheduler.elf", &dev, 0, 0, platform.rows, platform.cols, E_TRUE) ) {
        fprintf(stderr, "Failed to load e_scheduler.elf\n");
        return EXIT_FAILURE;
    }

    // initialize each service struct
    initialize_services(&mbuf, total_services);

    int counter = 0;
    while(1) {
        int service_to_be_updated = counter % total_services;
        int free_cpu = find_free_cpu(&dev, platform.rows, platform.cols);

        if (free_cpu == -1) {
            continue;
        }

        int row = (free_cpu >> 3) & 0x7;
        int col = free_cpu & 0x7;

        // simulate 1 host going down
        simulate_hosts_ram_change(&mbuf, service_to_be_updated, 1, 0);

        // simulate 4 hosts ram changing to 768
        simulate_hosts_ram_change(&mbuf, service_to_be_updated, 4, 768);

        // simulate 2 hosts ram restoring to 1024
        simulate_hosts_ram_change(&mbuf, service_to_be_updated, 2, 1024);

        // dispatch the service
        update_service(&dev, row, col, service_to_be_updated);

        counter++;

        if (counter > total_iterations) {
            break;
        }
    }

    // print tasks for service id = 0
    dump_service(&mbuf, 0);

    // cleanup all
    e_close(&dev);
    e_shm_release(shm_name);
    e_finalize();

    return 0;
}
