#pragma once



#include <atomic>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>



// Simple lightweight thread pool.
// To init, provide number of workers + function to run with arg_Ts input
template<class... arg_Ts>
class thread_pool {
	struct task_t {
		std::tuple<arg_Ts...> args;

		std::condition_variable &task_cv;
		std::atomic<bool> &has_finished;
	};

	public:
		thread_pool(const size_t worker_count, std::function<void(arg_Ts...)> func) : func(func), worker_count(worker_count) {

			if (worker_count == 0)
				throw std::runtime_error("worker_count must be > 0");

			workers.reserve(worker_count);

			for (size_t i = 0; i < worker_count; i++) {
				workers.emplace_back([this] { worker_loop(); });
			}
		}
		
		void emplace_task(std::condition_variable &task_cv, std::atomic<bool> &has_finished, arg_Ts... args) {
			{
				std::lock_guard<std::mutex> lock(tasks_mtx);
				tasks.push(task_t{std::forward_as_tuple(args...), task_cv, has_finished});
			}

			tasks_cv.notify_one();
		}

		~thread_pool() {
			stop = true;
			tasks_cv.notify_all();
			for (auto &worker : workers) worker.join();
		}



	private:
		std::condition_variable tasks_cv;
		std::queue<task_t> tasks;
		std::mutex tasks_mtx;
		std::function<void(arg_Ts...)> func;

		std::vector<std::thread> workers;

		const size_t worker_count;

		// Flag for all threads to READ to determine whether or not they should take on more tasks
		std::atomic<bool> stop = false;
		
		void worker_loop() {
			while (1) {
				std::unique_lock<std::mutex> lock(tasks_mtx);

				tasks_cv.wait(lock, [this] () {
					return stop || !tasks.empty();
				});

				if (stop && tasks.empty()) break;

				auto t = std::move(tasks.front());
				
				tasks.pop();

				lock.unlock();

				std::apply(func, t.args);
				t.has_finished.store(true);
				t.task_cv.notify_all();
			}
		}



};
