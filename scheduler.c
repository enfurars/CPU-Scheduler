// necessary headers
#include <unistd.h>
#include <string.h> 
#include <stdio.h>
#include <stdlib.h> 
#include <sys/wait.h>
#include <sys/types.h>
#include <time.h>
#include <sys/sysinfo.h>
#include <dirent.h>
#include <stdbool.h> 
#include <fcntl.h> 
#include <math.h>


// lets define instruction burst times
#define instr1 90;
#define instr2 80;
#define instr3 70;
#define instr4 60;
#define instr5 50;
#define instr6 40;
#define instr7 30;
#define instr8 20;
#define instr9 30;
#define instr10 40;
#define instr11 50;
#define instr12 60;
#define instr13 70;
#define instr14 80;
#define instr15 90;
#define instr16 80;
#define instr17 70;
#define instr18 60;
#define instr19 50;
#define instr20 40;
#define instr_exit 10;

// define context switch time
int context_switch = 10; 

// lets define process types
int p1_len = 12; 
int p1[12] = {90, 50, 90, 60, 70, 80, 40, 90, 60, 70, 80, 10}; // 790
int p2_len = 9;
int p2[9] = {60, 80, 50, 40, 50, 40, 50, 40, 10}; // 420 
int p3_len = 10;
int p3[10] = {20, 30, 60, 50, 70, 80, 50, 20, 30, 10} ; // 420
int p4_len = 6;
int p4[6] = {30, 80, 50, 30, 80, 10}; // 280
int p5_len = 11;
int p5[11] = {30, 80, 50, 30, 80, 80, 50, 30, 80, 50, 10}; // 570
int p6_len = 15;
int p6[15] = {40, 30, 40, 50, 60, 50, 30, 40, 30, 40, 50, 60, 50, 30, 10}; // 610
int p7_len = 6;
int p7[6] = {20, 90, 40, 50, 80, 10}; // 290
int p8_len = 5;
int p8[5] = {80, 60, 70, 90, 10}; // 310
int p9_len = 6;
int p9[6] = {50, 60, 30, 90, 30, 10}; // 270
int p10_len = 10; 
int p10[10] = {40, 70, 50, 50, 80, 50, 20, 70, 80, 10}; // 520 

// lets define quantum times for different types (GOLD, SILVER)
#define silver_quantum 80;
#define gold_quantum 120;

// Process structure
typedef struct {
    char name[10]; // P1, P2, ... P10
    int priority; // priority of the process
    int arrival_time; // arrival to system
    int secondary_arrival; // in case that process escalates (silver -> gold or gold -> platinum)
    int completion_time; // termination time
    char type[10]; // silver, gold or platinum
    int PC; // program counter
    int quantum_counter; // number of times the process entered to CPU
    int duration; // total time process is executed (equals to sum of all instruction times when terminated)
    int enter_to_ready; // time of entering to ready queue, it is updated during execution and used to handle round robin
} Process;

Process processes[10]; // process array, max 10 processes are expected
int process_count = 0; // number of processes in processes array

Process exited_processes[10]; // terminated processes
int exited_process_count = 0; // number of terminated processes
Process ready_processes[10]; // ready queue
int ready_process_count = 0; // number of ready processes in ready queue

int global_time = 0; // current time

char lep[10] = "";  // last executed process name
int ongoing_quantum = 0; // stores the execution time during last quantum in the system, it is used to update quantum counter and enter_to_ready field of processes

// prints some fields of processes for debugging purposes
void printProcess(Process *process) {
    printf("Name: %s, Pri: %d, Quantum: %d, Arrival: %d, Type: %s PC: %d Duration: %d\n", process->name, process->priority, process->quantum_counter, process->enter_to_ready, process->type, process->PC, process->duration); 
}

// comparison function used in qsort function, priorities are arranged in order -> (platinum - high priority - early arrival to ready queue - name(str comparison))
int cmp(const void *left, const void*right) {
    
    const Process *a = (const Process *)left;
    const Process *b = (const Process *)right;

    // platinum process has higher priority over other types
    if (strcmp(a->type, "PLATINUM") == 0 && strcmp(b->type, "PLATINUM") != 0 ) {

        return -1; 

    // platinum process has higher priority over other types
    } else if (strcmp(a->type, "PLATINUM") != 0 && strcmp(b->type, "PLATINUM") == 0 ){

        return 1;

    } else { // both not platinum or both platinum 

        // high priority comes before low priority
        if (a->priority > b->priority) {

            return -1;

        // high priority comes before low priority
        } else if (a->priority < b->priority) {

            return 1;

        } else { // equal priority

            // if priorities are equal, process came to ready queue first should be scheduled first 
            // (!!! note that not arrival to system, after a process run for a quantum, its enter to ready queue is updated)
            if (a->enter_to_ready < b->enter_to_ready) {

                return -1;

            // if priorities are equal, process came to ready queue first should be scheduled first 
            } else if (a->enter_to_ready > b->enter_to_ready) {

                return 1;

            }  else { // equal arrival to ready queue

                // if everything is equal check for process names with string comparison
                return strcmp(a->name, b->name);  

            }
        } 
    } 
}

// this function checks if any new process entered to system, if so it updated the ready queue
void update_ready() {
    // iterate all processes (that did not arrive yet)
    for(int c = 0; c < process_count; ++c) {
        // if a process's arrival is happened add it to ready queue and delete it from processes array
        if (processes[c].arrival_time <= global_time) {
            ready_processes[ready_process_count++] = processes[c];
            for (int i = c; i < process_count - 1; ++i) {
                processes[i] = processes[i + 1];
            }
            process_count--; // decrement process count
            c--; 
        }  
    } 
}

// this function handles executions and necessary updates on processes after executions
void execute_process() {

    // take the scheduled process from the top of the queue
    Process scheduled = ready_processes[0]; 
    
    // if this is the first process in the system or a new process is allowed to enter CPU, make a context switch
    if (strcmp(lep, "") == 0 || strcmp(lep, scheduled.name) != 0) {
        global_time += context_switch; // context switch  
        ongoing_quantum = 0; 
    } 

    // update the last executed process name
    strcpy(lep, scheduled.name); 

    //printf("SCHEDULED: %s, TIME: %d\n", scheduled.name, global_time); 

    // reset execution time to 0 
    int execution_time = 0; 

    // handle platinum process case
    if(strcmp(scheduled.type, "PLATINUM") == 0) {
        
        // if process name is P1
        if (strcmp(scheduled.name, "P1") == 0) {
            
            // since this is a platinum process it will execute in an atomic fashion
            // execute all instructions
            while(scheduled.PC < p1_len) {
                execution_time += p1[scheduled.PC]; // uddate execution time
                scheduled.duration += p1[scheduled.PC]; // update duration
                scheduled.PC++; // increment PC 
            } 

        // if process name is P2
        } else if (strcmp(scheduled.name, "P2") == 0) {
            
            // since this is a platinum process it will execute in an atomic fashion
            // execute all instructions
            while(scheduled.PC < p2_len) {
                execution_time += p2[scheduled.PC]; // uddate execution time
                scheduled.duration += p2[scheduled.PC]; // update duration
                scheduled.PC++;// increment PC 
            }

        // if process name is P3
        } else if (strcmp(scheduled.name, "P3") == 0) {
            
            // since this is a platinum process it will execute in an atomic fashion
            // execute all instructions
            while(scheduled.PC< p3_len) {
                execution_time += p3[scheduled.PC]; // uddate execution time
                scheduled.duration += p3[scheduled.PC]; // update duration
                scheduled.PC++; // increment PC 
            }

        // if process name is P4
        } else if (strcmp(scheduled.name, "P4") == 0) {

            // since this is a platinum process it will execute in an atomic fashion
            // execute all instructions
            while(scheduled.PC < p4_len) {
                execution_time += p4[scheduled.PC]; // uddate execution time
                scheduled.duration += p4[scheduled.PC];  // update duration
                scheduled.PC++; // increment PC 
            }

        // if process name is P5
        } else if (strcmp(scheduled.name, "P5") == 0) {

            // since this is a platinum process it will execute in an atomic fashion
            // execute all instructions
            while(scheduled.PC < p5_len) {
                execution_time += p5[scheduled.PC]; // uddate execution time
                scheduled.duration += p5[scheduled.PC]; // update duration
                scheduled.PC++; // increment PC 
            }

        // if process name is P6
        } else if (strcmp(scheduled.name, "P6") == 0) {

            // since this is a platinum process it will execute in an atomic fashion
            // execute all instructions
            while(scheduled.PC < p6_len) {
                execution_time += p6[scheduled.PC]; // uddate execution time
                scheduled.duration += p6[scheduled.PC]; // update duration
                scheduled.PC++; // increment PC 
            }

        // if process name is P7
        } else if (strcmp(scheduled.name, "P7") == 0) {

            // since this is a platinum process it will execute in an atomic fashion
            // execute all instructions
            while(scheduled.PC < p7_len) {
                execution_time += p7[scheduled.PC]; // uddate execution time
                scheduled.duration += p7[scheduled.PC];// update duration
                scheduled.PC++;  // increment PC 
            }

        // if process name is P8
        } else if (strcmp(scheduled.name, "P8") == 0) {

            // since this is a platinum process it will execute in an atomic fashion
            // execute all instructions
            while(scheduled.PC < p8_len) {
                execution_time += p8[scheduled.PC]; // uddate execution time
                scheduled.duration += p8[scheduled.PC]; // update duration
                scheduled.PC++; // increment PC 
            }

        // if process name is P9
        } else if (strcmp(scheduled.name, "P9") == 0) {

            // since this is a platinum process it will execute in an atomic fashion
            // execute all instructions
            while(scheduled.PC < p9_len) {
                execution_time += p9[scheduled.PC]; // uddate execution time
                scheduled.duration += p9[scheduled.PC]; // update duration
                scheduled.PC++; // increment PC 
            }
        
        // if process name is P10
        } else if (strcmp(scheduled.name, "P10") == 0) {

            // since this is a platinum process it will execute in an atomic fashion
            // execute all instructions
            while(scheduled.PC < p10_len) {
                execution_time += p10[scheduled.PC]; // uddate execution time
                scheduled.duration += p10[scheduled.PC];  // update duration
                scheduled.PC++; // increment PC 
            }

        } 

        global_time += execution_time; // update global time 
        scheduled.completion_time = global_time; // update completion time of the process
        exited_processes[exited_process_count++] = scheduled; // add process to exited processes list

        // delete the process from ready queue
        for (int i = 0; i < ready_process_count - 1; ++i) {
            ready_processes[i] = ready_processes[i + 1]; 
        }
        ready_process_count--; // decrement ready process count

    // handle the processes with type gold
    } else if (strcmp(scheduled.type, "GOLD") == 0) {

        // if process name is P1
        if (strcmp(scheduled.name, "P1") == 0) {

            execution_time += p1[scheduled.PC]; // uddate execution time
            scheduled.duration += p1[scheduled.PC]; // update duration
            ongoing_quantum += execution_time; // update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 120) {
                scheduled.quantum_counter++;  // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC
            
            // check if this was a silver process earlier
            // if arrival and secondary arrival are equal then this process was gold earlier too
            if(scheduled.secondary_arrival == scheduled.arrival_time) {

                // if quantum counter reaches 5, promote to platinum
                if(scheduled.quantum_counter >= 5) {
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time; // update its secondary arrival
                }

            // it was a silver process and promoted to gold
            } else { 

                // if quantum counter reaches 8, promote to platinum (8 because 3 of them are used to promote to gold from silver)
                if(scheduled.quantum_counter >= 8) { 
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time; // update its secondary arrival
                }
            }

            // if exit instruction is executed
            if(p1[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled; // add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--; // decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P2
        } else if (strcmp(scheduled.name, "P2") == 0) {

            execution_time += p2[scheduled.PC]; // uddate execution time
            scheduled.duration += p2[scheduled.PC];// update duration
            ongoing_quantum += execution_time; // update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 120) {
                scheduled.quantum_counter++;  // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++;// increment PC

            // check if this was a silver process earlier
            // if arrival and secondary arrival are equal then this process was gold earlier too
            if(scheduled.secondary_arrival == scheduled.arrival_time) {

                // if quantum counter reaches 5, promote to platinum
                if(scheduled.quantum_counter >= 5) {
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time; // update its secondary arrival
                }

            // it was a silver process and promoted to gold
            } else {

                // if quantum counter reaches 8, promote to platinum (8 because 3 of them are used to promote to gold from silver)
                if(scheduled.quantum_counter >= 8) { 
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time; // update its secondary arrival
                }
            }

            // if exit instruction is executed
            if(p2[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P3
        } else if (strcmp(scheduled.name, "P3") == 0) {

            execution_time += p3[scheduled.PC]; // uddate execution time
            scheduled.duration += p3[scheduled.PC];// update duration
            ongoing_quantum += execution_time; // update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 120) {
                scheduled.quantum_counter++;  // increment quantum counter 
                scheduled.enter_to_ready = global_time;// update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC
            
            // check if this was a silver process earlier
            // if arrival and secondary arrival are equal then this process was gold earlier too
            if(scheduled.secondary_arrival == scheduled.arrival_time) {

                // if quantum counter reaches 5, promote to platinum
                if(scheduled.quantum_counter >= 5) {
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }

            // it was a silver process and promoted to gold
            } else {
                if(scheduled.quantum_counter >= 8) { 

                    // if quantum counter reaches 8, promote to platinum (8 because 3 of them are used to promote to gold from silver)
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            }

            // if exit instruction is executed
            if(p3[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P4
        } else if (strcmp(scheduled.name, "P4") == 0) {

            execution_time += p4[scheduled.PC]; // uddate execution time
            scheduled.duration += p4[scheduled.PC];// update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 120) {
                scheduled.quantum_counter++;  // increment quantum counter 
                scheduled.enter_to_ready = global_time;// update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC
            
            // check if this was a silver process earlier
            // if arrival and secondary arrival are equal then this process was gold earlier too
            if(scheduled.secondary_arrival == scheduled.arrival_time) {

                // if quantum counter reaches 5, promote to platinum
                if(scheduled.quantum_counter >= 5) {
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            
            // it was a silver process and promoted to gold
            } else {

                // if quantum counter reaches 8, promote to platinum (8 because 3 of them are used to promote to gold from silver)
                if(scheduled.quantum_counter >= 8) { 
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            }

            // if exit instruction is executed
            if(p4[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P5
        } else if (strcmp(scheduled.name, "P5") == 0) {

            execution_time += p5[scheduled.PC]; // uddate execution time
            scheduled.duration += p5[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 120) {
                scheduled.quantum_counter++;  // increment quantum counter 
                scheduled.enter_to_ready = global_time;// update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC
            
            // check if this was a silver process earlier
            // if arrival and secondary arrival are equal then this process was gold earlier too
            if(scheduled.secondary_arrival == scheduled.arrival_time) {

                // if quantum counter reaches 5, promote to platinum
                if(scheduled.quantum_counter >= 5) {
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            
            // it was a silver process and promoted to gold
            } else {

                // if quantum counter reaches 8, promote to platinum (8 because 3 of them are used to promote to gold from silver)
                if(scheduled.quantum_counter >= 8) { 
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            }

            // if exit instruction is executed
            if(p5[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P6
        } else if (strcmp(scheduled.name, "P6") == 0) {

            execution_time += p6[scheduled.PC]; // uddate execution time
            scheduled.duration += p6[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 120) {
                scheduled.quantum_counter++;  // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // check if this was a silver process earlier
            // if arrival and secondary arrival are equal then this process was gold earlier too
            if(scheduled.secondary_arrival == scheduled.arrival_time) {

                // if quantum counter reaches 5, promote to platinum
                if(scheduled.quantum_counter >= 5) {
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            
            // it was a silver process and promoted to gold
            } else {

                // if quantum counter reaches 8, promote to platinum (8 because 3 of them are used to promote to gold from silver)
                if(scheduled.quantum_counter >= 8) { 
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            }

            // if exit instruction is executed
            if(p6[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P7
        } else if (strcmp(scheduled.name, "P7") == 0) {

            execution_time += p7[scheduled.PC]; // uddate execution time
            scheduled.duration += p7[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 120) {
                scheduled.quantum_counter++;  // increment quantum counter 
                scheduled.enter_to_ready = global_time;// update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // check if this was a silver process earlier
            // if arrival and secondary arrival are equal then this process was gold earlier too
            if(scheduled.secondary_arrival == scheduled.arrival_time) {

                // if quantum counter reaches 5, promote to platinum
                if(scheduled.quantum_counter >= 5) {
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            
            // it was a silver process and promoted to gold
            } else {

                // if quantum counter reaches 8, promote to platinum (8 because 3 of them are used to promote to gold from silver)
                if(scheduled.quantum_counter >= 8) { 
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            }
            
            // if exit instruction is executed
            if(p7[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P8
        } else if (strcmp(scheduled.name, "P8") == 0) {

            execution_time += p8[scheduled.PC]; // uddate execution time
            scheduled.duration += p8[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 120) {
                scheduled.quantum_counter++;  // increment quantum counter 
                scheduled.enter_to_ready = global_time;// update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++;// increment PC

            // check if this was a silver process earlier
            // if arrival and secondary arrival are equal then this process was gold earlier too
            if(scheduled.secondary_arrival == scheduled.arrival_time) {

                // if quantum counter reaches 5, promote to platinum
                if(scheduled.quantum_counter >= 5) {
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            
            // it was a silver process and promoted to gold
            } else {

                // if quantum counter reaches 8, promote to platinum (8 because 3 of them are used to promote to gold from silver)
                if(scheduled.quantum_counter >= 8) { 
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            }

            // if exit instruction is executed
            if(p8[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time; // set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }
        // if process name is P9
        } else if (strcmp(scheduled.name, "P9") == 0) {

            execution_time += p9[scheduled.PC]; // uddate execution time
            scheduled.duration += p9[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 120) {
                scheduled.quantum_counter++;  // increment quantum counter 
                scheduled.enter_to_ready = global_time;// update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // check if this was a silver process earlier
            // if arrival and secondary arrival are equal then this process was gold earlier too
            if(scheduled.secondary_arrival == scheduled.arrival_time) {

                // if quantum counter reaches 5, promote to platinum
                if(scheduled.quantum_counter >= 5) {
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            
            // it was a silver process and promoted to gold
            } else {

                // if quantum counter reaches 8, promote to platinum (8 because 3 of them are used to promote to gold from silver)
                if(scheduled.quantum_counter >= 8) { 
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            }

            // if exit instruction is executed
            if(p9[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time; // set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P10
        } else if (strcmp(scheduled.name, "P10") == 0) {

            execution_time += p10[scheduled.PC]; // uddate execution time
            scheduled.duration += p10[scheduled.PC];  // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 120) {
                scheduled.quantum_counter++;  // increment quantum counter 
                scheduled.enter_to_ready = global_time;// update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // check if this was a silver process earlier
            // if arrival and secondary arrival are equal then this process was gold earlier too
            if(scheduled.secondary_arrival == scheduled.arrival_time) {

                // if quantum counter reaches 5, promote to platinum
                if(scheduled.quantum_counter >= 5) {
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }

            // it was a silver process and promoted to gold
            } else {

                // if quantum counter reaches 8, promote to platinum (8 because 3 of them are used to promote to gold from silver)
                if(scheduled.quantum_counter >= 8) { 
                    strcpy(scheduled.type, "PLATINUM"); 
                    scheduled.secondary_arrival = global_time;// update its secondary arrival
                }
            }

            // if exit instruction is executed
            if(p10[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time; // set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }
        }

    // handle process type is silver
    } else { // silver 

        // if process name is P1
        if (strcmp(scheduled.name, "P1") == 0) {

            execution_time += p1[scheduled.PC]; // uddate execution time
            scheduled.duration += p1[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 80) {
                scheduled.quantum_counter++; // increment quantum counter 
                scheduled.enter_to_ready = global_time;// update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // if quantum counter reaches 3, promote to gold
            if(scheduled.quantum_counter >= 3) {
                strcpy(scheduled.type, "GOLD"); 
                scheduled.secondary_arrival = global_time; // update its secondary arrival
            }
            
            // if exit instruction is executed
            if(p1[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P2
        } else if (strcmp(scheduled.name, "P2") == 0) {

            execution_time += p2[scheduled.PC]; // uddate execution time
            scheduled.duration += p2[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 80) {
                scheduled.quantum_counter++; // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // if quantum counter reaches 3, promote to gold
            if(scheduled.quantum_counter >= 3) {
                strcpy(scheduled.type, "GOLD"); 
                scheduled.secondary_arrival = global_time;// update its secondary arrival
            }

            // if exit instruction is executed
            if(p2[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P3
        } else if (strcmp(scheduled.name, "P3") == 0) {

            execution_time += p3[scheduled.PC]; // uddate execution time
            scheduled.duration += p3[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 80) {
                scheduled.quantum_counter++; // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // if quantum counter reaches 3, promote to gold
            if(scheduled.quantum_counter >= 3) {
                strcpy(scheduled.type, "GOLD"); 
                scheduled.secondary_arrival = global_time;// update its secondary arrival
            }

            // if exit instruction is executed
            if(p3[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P4
        } else if (strcmp(scheduled.name, "P4") == 0) {

            execution_time += p4[scheduled.PC]; // uddate execution time
            scheduled.duration += p4[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 80) {
                scheduled.quantum_counter++; // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // if quantum counter reaches 3, promote to gold
            if(scheduled.quantum_counter >= 3) {
                strcpy(scheduled.type, "GOLD"); 
                scheduled.secondary_arrival = global_time;// update its secondary arrival
            }

            // if exit instruction is executed
            if(p4[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P5
        } else if (strcmp(scheduled.name, "P5") == 0) { 

            execution_time += p5[scheduled.PC]; // uddate execution time
            scheduled.duration += p5[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 80) {
                scheduled.quantum_counter++; // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++;// increment PC

            // if quantum counter reaches 3, promote to gold
            if(scheduled.quantum_counter >= 3) {
                strcpy(scheduled.type, "GOLD"); 
                scheduled.secondary_arrival = global_time;// update its secondary arrival
            }

            // if exit instruction is executed
            if(p5[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P6
        } else if (strcmp(scheduled.name, "P6") == 0) {

            execution_time += p6[scheduled.PC]; // uddate execution time
            scheduled.duration += p6[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 80) {
                scheduled.quantum_counter++; // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // if quantum counter reaches 3, promote to gold
            if(scheduled.quantum_counter >= 3) {
                strcpy(scheduled.type, "GOLD"); 
                scheduled.secondary_arrival = global_time;// update its secondary arrival
            }

            // if exit instruction is executed
            if(p6[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P7
        } else if (strcmp(scheduled.name, "P7") == 0) {

            execution_time += p7[scheduled.PC]; // uddate execution time
            scheduled.duration += p7[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 80) {
                scheduled.quantum_counter++; // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // if quantum counter reaches 3, promote to gold
            if(scheduled.quantum_counter >= 3) {
                strcpy(scheduled.type, "GOLD"); 
                scheduled.secondary_arrival = global_time;// update its secondary arrival
            }

            // if exit instruction is executed
            if(p7[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P8
        } else if (strcmp(scheduled.name, "P8") == 0) {

            execution_time += p8[scheduled.PC]; // uddate execution time
            scheduled.duration += p8[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 80) {
                scheduled.quantum_counter++; // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // if quantum counter reaches 3, promote to gold
            if(scheduled.quantum_counter >= 3) {
                strcpy(scheduled.type, "GOLD"); 
                scheduled.secondary_arrival = global_time;// update its secondary arrival
            }

            // if exit instruction is executed
            if(p8[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }
        
        // if process name is P9
        } else if (strcmp(scheduled.name, "P9") == 0) {

            execution_time += p9[scheduled.PC]; // uddate execution time
            scheduled.duration += p9[scheduled.PC]; // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 80) {
                scheduled.quantum_counter++; // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // if quantum counter reaches 3, promote to gold
            if(scheduled.quantum_counter >= 3) {
                strcpy(scheduled.type, "GOLD"); 
                scheduled.secondary_arrival = global_time;// update its secondary arrival
            }

            // if exit instruction is executed
            if(p9[scheduled.PC - 1] == 10) { 
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }

        // if process name is P10
        } else if (strcmp(scheduled.name, "P10") == 0) {

            execution_time += p10[scheduled.PC]; // uddate execution time
            scheduled.duration += p10[scheduled.PC];  // update duration
            ongoing_quantum += execution_time;// update current quantum time
            global_time += execution_time;  // update global time 

            // check if process completed its allowed quantum time 
            if (ongoing_quantum >= 80) {
                scheduled.quantum_counter++; // increment quantum counter 
                scheduled.enter_to_ready = global_time; // update enter_to_ready for round robin
                ongoing_quantum = 0; // reset current quantum time
            }
            scheduled.PC++; // increment PC

            // if quantum counter reaches 3, promote to gold
            if(scheduled.quantum_counter >= 3) {
                strcpy(scheduled.type, "GOLD"); 
                scheduled.secondary_arrival = global_time; // update its secondary arrival
            }

            // if exit instruction is executed
            if(p10[scheduled.PC - 1] == 10) {  
                scheduled.completion_time = global_time;// set completion time of the process
                exited_processes[exited_process_count++] = scheduled;// add the process to exited process list

                // delete it from ready queue
                for (int i = 0; i < ready_process_count - 1; ++i) {
                    ready_processes[i] = ready_processes[i + 1]; 
                }      
                ready_process_count--;// decrement ready queue count
            
            // process is not terminated
            } else {
                ready_processes[0] = scheduled; // update changes on process
            }
        } 
    }
}

/* main function reads definition.txt file and fills processes array 
then while there exist a process that is not exited, it updates ready queue and sorts it based on priorities
it calls the execute function above to get the scheduled process executed, after execution it checks if a preemption occurred and 
makes necessary changes on preempted process and sorts the ready queue again and calls execute function*/

int main() {

    // input reading
    FILE *filepointer;
    char *line = NULL;
    size_t len = 0;
    ssize_t line_len; 

    char *process_info[128]; 
    char *token = NULL;

    filepointer = fopen("definition.txt", "r");
    if (filepointer == NULL) {
        exit(EXIT_FAILURE); }

    // while there exist a line to read
    while ((line_len = getline(&line, &len, filepointer)) != -1) {

        // parse the current line from spaces and new line characters and store tokens in process_info 
        token = strtok(line, " \n");
        int i = 0;
        while(token) {
            process_info[i++] = token;
            token = strtok(NULL, " \n");
        }
        process_info[i] = NULL; // null end the array
        

        strcpy(processes[process_count].name, process_info[0]); // name P1, P2, P3 ... P10
        processes[process_count].priority = atoi(process_info[1]); // priority
        processes[process_count].arrival_time = atoi(process_info[2]);  // arrival to system
        processes[process_count].enter_to_ready = atoi(process_info[2]); // enter time to ready queue
        processes[process_count].secondary_arrival = atoi(process_info[2]);  // secondary arrival (in case of promotion)
        strcpy(processes[process_count].type, process_info[3]); // type PLATINUM, GOLD, SILVER
        processes[process_count].completion_time = -1; // completion time of process, initially 0
        processes[process_count].PC = 0; // program counter
        processes[process_count].quantum_counter = 0; // number of times the process entered to CPU
        processes[process_count].duration = 0; // total execution time of the process

        process_count++; // increment process count
    }
    
    // close file
    fclose(filepointer); 
    if (line)
        free(line);
    
    // while there exist a process that is not terminated (either in ready queue or not arrived to system yet)
    while(ready_process_count > 0 || process_count > 0) {   
        
        // update ready queue
        update_ready(); 
        
        // sort ready queue using cmp function 
        qsort(ready_processes, ready_process_count, sizeof(Process), cmp);  

        // if a new process is scheduled and it is not the first process in the system
        if(strcmp(ready_processes[0].name, lep) != 0 && strcmp(lep, "") != 0) {
            
            char typ[10] = ""; // to store type of the last executed process
            int idx = 0; // to store index(in ready queue) of the last executed process

            // iterate over ready queue and find last executed process
            for (int i = 0; i < ready_process_count; i++) {
                if (strcmp(ready_processes[i].name, lep) == 0) {
                    strcpy(typ, ready_processes[i].type); // store its type
                    idx = i; // store its index
                    break ; // break
                }
            }

            // if it was a gold process and preempted before its allowed quantum time
            if (strcmp(typ, "GOLD") == 0 && ongoing_quantum < 120 && ongoing_quantum > 0) {

                // set its enter to ready field to current time
                ready_processes[idx].enter_to_ready = global_time; 

                // increment its quantum counter
                ready_processes[idx].quantum_counter++;

                // check if this was a silver process earlier
                // if arrival and secondary arrival are equal then this process was gold earlier 
                if(ready_processes[idx].secondary_arrival == ready_processes[idx].arrival_time) {

                    // if quantum counter reaches 5, promote to platinum
                    if(ready_processes[idx].quantum_counter >= 5) {
                        strcpy(ready_processes[idx].type, "PLATINUM"); 
                        ready_processes[idx].secondary_arrival = global_time; // update its secondary arrival
                    }
                
                // it was a silver process and promoted to gold
                } else {

                    // if quantum counter reaches 8, promote to platinum (8 because 3 of them are used to promote to gold from silver)
                    if(ready_processes[idx].quantum_counter >= 8) { 
                        strcpy(ready_processes[idx].type, "PLATINUM"); 
                        ready_processes[idx].secondary_arrival = global_time; // update its secondary arrival
                    }
                }

                // sort ready queue again as updates on preempted process may change things
                qsort(ready_processes, ready_process_count, sizeof(Process), cmp);  
            
            // if it was a silver process and preempted before its allowed quantum time
            } else if (strcmp(typ, "SILVER") == 0 && ongoing_quantum < 80 && ongoing_quantum > 0) {

                // set its enter to ready field to current time
                ready_processes[idx].enter_to_ready = global_time; 

                // increment its quantum counter
                ready_processes[idx].quantum_counter++;

                // if quantum counter reaches 3, promote to gold
                if(ready_processes[idx].quantum_counter >= 3) {
                    strcpy(ready_processes[idx].type, "GOLD"); 
                    ready_processes[idx].secondary_arrival = global_time; // update its secondary arrival
                } 

                // sort ready queue again as updates on preempted process may change things
                qsort(ready_processes, ready_process_count, sizeof(Process), cmp);
            }

        }
        
        // if there exist a process in ready queue
        if (ready_process_count > 0) {

            // excute first process in the sorted ready queue
            execute_process(); 
        
        // else increment current time 
        } else {
            global_time += 1; 
        } 
        
    }  
    
    // after all processes in the system terminated 

    int turnaround_time = 0; // total turaround time of all processes
    int waiting_time = 0; // total waiting time of all processes

    // iterate over exited processes array
    for(int i = 0; i < exited_process_count; i++) {
        
        // Turnaround time = Time of Completion - Time Of Arrival
        turnaround_time += (exited_processes[i].completion_time - exited_processes[i].arrival_time);

        // Waiting Time = Turnaround Time - Burst Time
        waiting_time += (exited_processes[i].completion_time - exited_processes[i].arrival_time) - exited_processes[i].duration; 
    }

    // take averages
    float avg_waiting_time = (float)waiting_time / exited_process_count;
    float avg_turnaround_time = (float)turnaround_time / exited_process_count;

    
    // print them as integer or if floating number use 1 digit after decimal point
    if (fmod(avg_waiting_time, 1) == 0) {
        printf("%d\n", (int)avg_waiting_time);
    } else {
        printf("%.1f\n", avg_waiting_time);
    }

    if (fmod(avg_turnaround_time, 1) == 0) {
        printf("%d\n", (int)avg_turnaround_time);
    } else {
        printf("%.1f\n", avg_turnaround_time);
    }


    /* for(int i = 0; i < process_count; i++) {
        printProcess(&processes[i]); 
    } */ 

    return 0; 
}