#include "thread_pool.hpp"
#include <condition_variable>
#include <mutex>



thread_pool::thread_pool(const size_t worker_count, build_bvh_node_t func)
	: func(func), worker_count(worker_count) {

	if (worker_count == 0)
		throw std::runtime_error("worker_count must be > 0");

	workers.reserve(worker_count);

	for (size_t i = 0; i < worker_count; i++) {
		workers[i] = std::thread([this] { worker_loop(); });
	}
}



thread_pool::~thread_pool() {
	stop = true;
	tasks_cv.notify_all();
	for (auto &worker : workers) worker.join();
}



void thread_pool::emplace_task(build_bvh_node_params p, std::condition_variable &task_cv, std::atomic<bool> &has_finished) {
	{
		std::lock_guard<std::mutex> lock(tasks_mtx);
		tasks.push(task_t{p, task_cv, has_finished});
	}
	
	tasks_cv.notify_one();
}



void thread_pool::worker_loop() {
	while (!stop) {
		std::unique_lock<std::mutex> lock(tasks_mtx);

		tasks_cv.wait(lock, [this] () {
			return stop || !tasks.empty();
		});

		if (stop && tasks.empty()) break;

		auto t = std::move(tasks.front());
		auto params = t.params;
		
		tasks.pop();

		lock.unlock();

		func(params.node, params.verts, params.nodes_len);
		t.has_finished = true;
		t.task_cv.notify_all();
	}
}
