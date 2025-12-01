/**
 * @file interrupts_student1_student2_EP_RR.cpp
 * @author
 * @brief External Priority + Round Robin (100 ms) scheduler simulation
 */

#include "interrupts_101226876_101304133.hpp"

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
        } else {
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

void EP_order_ready_queue(std::vector<PCB> &ready_queue) {
    std::sort(
        ready_queue.begin(),
        ready_queue.end(),
        [](const PCB &a, const PCB &b) {
            return a.priority < b.priority;  
        }
    );
}

std::tuple<std::string>
run_simulation(std::vector<PCB> list_processes)
{
    std::vector<PCB> ready_queue;
    std::vector<PCB> wait_queue;
    std::vector<PCB> job_list;

    unsigned int current_time = 0;
    PCB running;
    idle_CPU(running);

    const unsigned int TIME_QUANTUM = 100;
    unsigned int time_in_quantum = 0;

    std::string execution_status;
    execution_status = print_exec_header();

    while (!all_process_terminated(job_list) || job_list.empty())
    {
        for (auto &process : list_processes)
        {
            if (process.arrival_time <= current_time &&
                process.state == NOT_ASSIGNED)
            {
                if (assign_memory(process))
                {
                    states old_state = NEW;

                    execution_status += record_memory_state(current_time);

                    process.state = READY;
                    process.time_to_next_io =
                        (process.io_freq > 0 ? process.io_freq : 0);
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

        for (auto it = wait_queue.begin(); it != wait_queue.end();)
        {
            if (it->io_completion_time != -1 &&
                it->io_completion_time <= (int)current_time)
            {
                PCB proc = *it;
                states old_state = WAITING;

                proc.state = READY;
                proc.time_to_next_io =
                    (proc.io_freq > 0 ? proc.io_freq : 0);
                proc.io_completion_time = -1;

                sync_queue(job_list, proc);
                ready_queue.push_back(proc);

                execution_status += print_exec_status(
                    current_time,
                    proc.PID,
                    old_state,
                    READY
                );

                it = wait_queue.erase(it);
            }
            else {
                ++it;
            }
        }

        if (running.PID != -1 && !ready_queue.empty())
        {
            EP_order_ready_queue(ready_queue);
            PCB best = ready_queue.back();

            if (best.priority > running.priority)
            {
                states old_state = RUNNING;
                PCB preempted = running;

                preempted.state = READY;
                sync_queue(job_list, preempted);
                ready_queue.push_back(preempted);

                execution_status += print_exec_status(
                    current_time,
                    preempted.PID,
                    old_state,
                    READY
                );

                EP_order_ready_queue(ready_queue);

                states old_state_new = READY;
                run_process(running, job_list, ready_queue, current_time);

                time_in_quantum = 0;

                execution_status += print_exec_status(
                    current_time,
                    running.PID,
                    old_state_new,
                    RUNNING
                );
            }
        }

        if (running.PID == -1 && !ready_queue.empty())
        {
            EP_order_ready_queue(ready_queue);

            states old_state = READY;

            run_process(running, job_list, ready_queue, current_time);
            time_in_quantum = 0;

            execution_status += print_exec_status(
                current_time,
                running.PID,
                old_state,
                RUNNING
            );
        }

        if (running.PID != -1)
        {
            if (running.remaining_time > 0)
                running.remaining_time--;

            if (running.io_freq > 0 && running.time_to_next_io > 0)
                running.time_to_next_io--;

            bool transitioned = false;

            if (running.remaining_time == 0)
            {
                states old_state = RUNNING;
                int pid = running.PID;

                terminate_process(running, job_list);

                execution_status += print_exec_status(
                    current_time,
                    pid,
                    old_state,
                    TERMINATED
                );

                idle_CPU(running);
                time_in_quantum = 0;
                transitioned = true;
            }

            else if (running.io_freq > 0 &&
                     running.time_to_next_io == 0)
            {
                states old_state = RUNNING;
                PCB proc = running;

                proc.state = WAITING;
                proc.io_completion_time =
                    current_time + proc.io_duration;

                sync_queue(job_list, proc);
                wait_queue.push_back(proc);

                execution_status += print_exec_status(
                    current_time,
                    proc.PID,
                    old_state,
                    WAITING
                );

                idle_CPU(running);
                time_in_quantum = 0;
                transitioned = true;
            }

            else
            {
                time_in_quantum++;

                if (time_in_quantum == TIME_QUANTUM &&
                    !ready_queue.empty())
                {
                    states old_state = RUNNING;
                    PCB proc = running;

                    proc.state = READY;
                    sync_queue(job_list, proc);
                    ready_queue.push_back(proc);

                    execution_status += print_exec_status(
                        current_time,
                        proc.PID,
                        old_state,
                        READY
                    );

                    idle_CPU(running);
                    time_in_quantum = 0;
                    transitioned = true;
                }
            }

            if (!transitioned && running.PID != -1)
                sync_queue(job_list, running);
        }

        current_time++;
    }

    execution_status += print_exec_footer();
    return std::make_tuple(execution_status);
}


int main(int argc, char** argv)
{
    if (argc != 2) {
        std::cout << "ERROR!\nExpected 1 argument, received "
                  << (argc - 1) << std::endl;
        std::cout << "Usage: ./interrupts <input_file.txt>" << std::endl;
        return -1;
    }

    auto file_name = argv[1];
    std::ifstream input_file(file_name);

    if (!input_file.is_open()) {
        std::cerr << "Error opening file: " << file_name << std::endl;
        return -1;
    }

    std::string line;
    std::vector<PCB> list_process;

    while (std::getline(input_file, line))
    {
        auto tokens = split_delim(line, ", ");
        auto proc = add_process(tokens);
        list_process.push_back(proc);
    }
    input_file.close();

    auto [exec] = run_simulation(list_process);
    write_output(exec, "output_files/execution.txt");

    return 0;
}
