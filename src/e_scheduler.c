#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "e_lib.h"
#include "proto.h"

volatile int *service_id = (int *)0x7000;
volatile int *execution_counter = (int *)0x7020;

int main(void) {
    const char	  shm_name[] = "services_shm"; 
    e_memseg_t    emem;

    *service_id = -1;
    *execution_counter = 0;

    if ( E_OK != e_shm_attach(&emem, shm_name) ) {
        return EXIT_FAILURE;
    }

    while (1) {
        // wait for a new service id to process
        if (*service_id == -1) {
            continue;
        }

        // read service state
        service_t service;
        e_read((void*) &emem, (void*) &service, (*service_id) * sizeof(service_t), 0, 0, sizeof(service_t));

        int i, j;

        // reset state
        for(i = 0; i < HOSTS_PER_SERVICE; i++){
            service.host_list[i].curr_ram = service.host_list[i].max_ram;
        }

        // sort hosts by amount of available ram
        for(i = 0; i < HOSTS_PER_SERVICE; i++) {
            for(j = i+1; j < HOSTS_PER_SERVICE; j++) {
                if(service.host_list[i].curr_ram < service.host_list[j].curr_ram) {
                    host_t tmp_host = service.host_list[i];
                    service.host_list[i] = service.host_list[j];
                    service.host_list[j] = tmp_host;
                }
            }
        }

        // allocate a task to a host (worst fit)
        for(i = 0; i < TASKS_PER_SERVICE; i++){
            for(j = 0; j < HOSTS_PER_SERVICE; j++){
                if(
                        service.host_list[j].curr_ram >= service.task_list[i].reserved_ram
                  ){
                    service.host_list[j].curr_ram -= service.task_list[i].reserved_ram;
                    service.task_list[i].allocated_host_id = service.host_list[j].id;
                    break;              
                }  
            }

            if(j == HOSTS_PER_SERVICE) {
                service.task_list[i].allocated_host_id = -1;
                break;
            }
        }

        // reset state
        *service_id = -1;
        *execution_counter += 1;

        // update service state
        e_write((void*)&emem, (void*) &service, 0, 0, (*service_id) * sizeof(service_t), sizeof(service_t));
    }

    return EXIT_SUCCESS;
}
