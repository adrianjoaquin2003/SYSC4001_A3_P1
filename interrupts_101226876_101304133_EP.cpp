/**
 * @file interrupts_student1_student2_EP.cpp
 * @author Sasisekhar Govind
 * @brief EP (external priority, no preemption) scheduler simulation
 */

#include "interrupts_101226876_101304133.hpp"

void EP_order_ready_queue(std::vector<PCB> &ready_queue) {
    std::sort(
        ready_queue.begin(),
        ready_queue.end(),
        [](const PCB &a, const PCB &b) {
            return a.priority < b.priority;
        }
    );
}

std::string record_memory_state(unsigned int current_time)
{
    std::stringstream ss;

    ss << "\nMemory State at time " << current_time << " \n";

    unsigned int total_used = 0;
    unsigned int total_free = 0;

    for (int i = 0; i < 6; i++) {
        if (memory_paritions[i].occupied != -1) {
            ss << "Partition " << memory_paritions[i].partition_number
               << ": USED by PID " << memory_paritions[i].occupied
               << " (size " << memory_paritions[i].size << ")\n";

            total_used += memory_paritions[i].size;
        }
        else {
            ss << "Partition " << memory_paritions[i].partition_number
               << ": FREE (size " << memory_paritions[i].size << ")\n";

            total_free += memory_paritions[i].size;
        }
    }

    ss << "Total memory used: " << total_used << "\n";
    ss << "Total free memory: " << total_free << "\n";
    ss << "Total usable memory: " << total_free << "\n\n";

    return ss.str();
}


std::tuple<std::string>
run_simulation(std::vector<PCB> list_processes) {

    std::vector<PCB> ready_queue;   
    std::vector<PCB> wait_queue;    
    std::vector<PCB> job_list;      

    unsigned int current_time = 0;
    PCB running;
    idle_CPU(running);

    std::string execution_status;
    execution_status = print_exec_header();

    while (!all_process_terminated(job_list) || job_list.empty()) {

        for (auto &process : list_processes) {
            if (process.arrival_time <= current_time &&
                process.state == NOT_ASSIGNED) {

                if (assign_memory(process)) {
                    states old_state = NEW;

                    execution_status += record_memory_state(current_time);

                    process.state = READY;
                    process.time_to_next_io = (process.io_freq > 0) ? process.io_freq : 0;
                    process.io_completion_time = -1;

                    ready_queue.push_back(process);
                    job_list.push_back(process);

                    execution_status += print_exec_status(
                        current_time,
                        process.PID,
                        old_state,
                        READY
                    );
                }

            }
        }

        for (auto it = wait_queue.begin(); it != wait_queue.end();) {
            if (it->io_completion_time != -1 &&
                it->io_completion_time <= (int)current_time) {

                PCB proc = *it;
                states old_state = WAITING;

                proc.state = READY;
                proc.time_to_next_io =
                    (proc.io_freq > 0) ? proc.io_freq : 0;

                sync_queue(job_list, proc);
                ready_queue.push_back(proc);

                execution_status += print_exec_status(
                    current_time,
                    proc.PID,
                    old_state,
                    READY
                );

                it = wait_queue.erase(it);
            } else {
                ++it;
            }
        }

        if (running.PID == -1 && !ready_queue.empty()) {
            EP_order_ready_queue(ready_queue);

            states old_state = READY;
            run_process(running, job_list, ready_queue, current_time);

            execution_status += print_exec_status(
                current_time,
                running.PID,
                old_state,
                RUNNING
            );
        }

        if (running.PID != -1) {
            if (running.remaining_time > 0) {
                running.remaining_time--;
            }

            if (running.io_freq > 0 && running.time_to_next_io > 0) {
                running.time_to_next_io--;
            }

            bool did_transition = false;

            if (running.remaining_time == 0) {
                states old_state = RUNNING;

                terminate_process(running, job_list);

                execution_status += print_exec_status(
                    current_time,
                    running.PID,
                    old_state,
                    TERMINATED
                );

                idle_CPU(running);
                did_transition = true;
            }
            else if (running.io_freq > 0 && running.time_to_next_io == 0) {
                states old_state = RUNNING;

                running.state = WAITING;
                running.io_completion_time =
                    current_time + running.io_duration;
                sync_queue(job_list, running);

                execution_status += print_exec_status(
                    current_time,
                    running.PID,
                    old_state,
                    WAITING
                );

                wait_queue.push_back(running);
                idle_CPU(running);
                did_transition = true;
            }

            if (!did_transition && running.PID != -1) {
                sync_queue(job_list, running);
            }
        }

        current_time++;
    }

    execution_status += print_exec_footer();
    return std::make_tuple(execution_status);
}


int main(int argc, char** argv) {
    if (argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received " << argc - 1 << std::endl;
        std::cout << "To run the program, do: ./interrupts <your_input_file.txt>" << std::endl;
        return -1;
    }

    auto file_name = argv[1];
    std::ifstream input_file(file_name);

    if (!input_file.is_open()) {
        std::cerr << "Error: Unable to open file: " << file_name << std::endl;
        return -1;
    }

    std::string line;
    std::vector<PCB> list_process;
    while (std::getline(input_file, line)) {
        auto input_tokens = split_delim(line, ", ");
        auto new_process = add_process(input_tokens);
        list_process.push_back(new_process);
    }
    input_file.close();

    auto [exec] = run_simulation(list_process);
    write_output(exec, "output_files/execution.txt");

    return 0;
}
