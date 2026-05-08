#pragma once



#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>
#include "structs.hpp"



using build_bvh_node_t = std::function<
void(bvh_node*, vec<3>*, std::atomic<uint16_t>&)
	>;



typedef struct {
	bvh_node *node;
	vec<3> *verts;
	std::atomic<uint16_t> &nodes_len;
} build_bvh_node_params;



typedef struct {
	build_bvh_node_params params;
	std::condition_variable &task_cv;
	std::atomic<bool> &has_finished;
} task_t;



class thread_pool {
	public:
		thread_pool(const size_t worker_count, build_bvh_node_t func);
		
		void emplace_task(build_bvh_node_params p, std::condition_variable &task_cv, std::atomic<bool> &has_finished);

		~thread_pool();



	private:
		std::condition_variable tasks_cv;
		std::queue<task_t> tasks;
		std::mutex tasks_mtx;
		build_bvh_node_t func;

		std::vector<std::thread> workers;

		const size_t worker_count;

		void worker_loop();

		// Flag for all threads to READ to determine whether or not they should take on more tasks
		std::atomic<bool> stop = false;



};
