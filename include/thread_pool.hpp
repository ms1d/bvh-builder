#pragma once



#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <thread>
#include <vector>



// Simple, lightweight bounded thread pool.
// To init, provide number of workers + function to run with arg_Ts input
// Use try_emplace_task() to add a task. if it returns false, the task could not be added
template<class... arg_Ts>
class thread_pool {
	struct task_t {
		std::tuple<arg_Ts...> args;
		std::promise<void> promise;
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
		
		std::optional<std::future<void>> try_emplace_task(arg_Ts... args) {
			if (stop.load()) return std::nullopt;
			auto curr = busy_workers.fetch_add(1);
			if (curr >= worker_count) {
				busy_workers.fetch_sub(1);
				return std::nullopt;
			}

			task_t task{
				std::tuple<arg_Ts...>(args...),
				std::promise<void>()
			};
			auto fut = task.promise.get_future();

			{
				std::lock_guard<std::mutex> lock(tasks_mtx);
				tasks.push(std::move(task));
			}

			tasks_cv.notify_one();
			return fut;
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

		std::atomic<uint> busy_workers = 0;

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
				t.promise.set_value();
				busy_workers--;
			}
		}



};
