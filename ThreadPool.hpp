/*
 *Copyright (c) 2012 Jakob Progsch
 *
 *This software is provided 'as-is', without any express or implied
 *warranty. In no event will the authors be held liable for any damages
 *arising from the use of this software.
 *
 *Permission is granted to anyone to use this software for any purpose,
 *including commercial applications, and to alter it and redistribute it
 *freely, subject to the following restrictions:
 *
 *  1. The origin of this software must not be misrepresented; you must not
 *  claim that you wrote the original software. If you use this software
 *  in a product, an acknowledgment in the product documentation would be
 *  appreciated but is not required.
 *
 *  2. Altered source versions must be plainly marked as such, and must not be
 *  misrepresented as being the original software.

 *  3. This notice may not be removed or altered from any source
 *  distribution.
 */

#ifndef THREAD_POOL_H
#define THREAD_POOL_H

#include <stack>
#include <mutex>
#include <queue>
#include <memory>
#include <thread>
#include <vector>
#include <future>
#include <stdexcept>
#include <functional>
#include <condition_variable>

struct pTask{
	const int priority;
	std::function<void()> mTask;
	pTask(int p, std::function<void()> f) : priority(p), mTask(f) {}
	bool operator< (const pTask& other){ return this->priority < other.priority;}
};

typedef std::queue<std::function<void()>> 	FIFO_POLICY;
typedef std::stack<std::function<void()>> 	LIFO_POLICY;
//typedef std::stack<std::function<void()>> 	PRIORITY_POLICY;

template <typename policy_type = FIFO_POLICY>
class ThreadPool {
private:
    std::vector<std::thread> mWorkers;
	policy_type mTasks;

    std::mutex queue_mutex;
    std::condition_variable condition;
	std::atomic<bool> isActive;

public:
	ThreadPool (size_t numThreads = std::thread::hardware_concurrency() ){
	    for(size_t i = 0 ; i < numThreads; i++){
	        mWorkers.emplace_back(std::thread(
	            [this] {
    	            while(true){
    	                std::unique_lock<std::mutex> lock(this->queue_mutex);
    	                while( this->isActive.load() && this->mTasks.empty())
    	                    this->condition.wait(lock);
    	                if( ! this->isActive.load() && this->mTasks.empty())
    	                    return;
    	                std::function<void()> lNextTask(this->mTasks.front());
    	                this->mTasks.pop();
    	                lock.unlock();
    	                lNextTask();
    	            }
    	        }
    	    ));
		}
	}

	template<class F, class... Args>
	auto enqueue(F&& f, Args&&... args) -> std::future<decltype(std::forward<F>(f)(std::forward<Args>(args)...))> {
	    typedef decltype(std::forward<F>(f)(std::forward<Args>(args)...)) return_type;
	    // Don't allow enqueueing after stopping the pool
	    if ( ! isActive.load() ) {
	        throw std::runtime_error("enqueue on stopped ThreadPool");
		}

	    auto task = std::make_shared<std::packaged_task<return_type()>>( std::bind(std::forward<F>(f), std::forward<Args>(args)...) );

		std::future<return_type> res = task->get_future();
		{
			std::unique_lock<std::mutex> lock(queue_mutex);
			mTasks.push([task](){ (*task)(); });
		}
		condition.notify_one();
		return res;
	}


	int pending(void) {
		std::unique_lock<std::mutex> lock(queue_mutex);
		return this->mTasks.size();
	}

	~ThreadPool(void) {
		this->isActive = false;
		condition.notify_all();
		for(std::thread& t : mWorkers)
			t.join();
	}
};

template<>
ThreadPool<LIFO_POLICY>::ThreadPool (size_t numThreads){
    for(size_t i = 0 ; i < numThreads; i++){
        mWorkers.emplace_back(std::thread(
            [this] {
   	            while(true){
   	                std::unique_lock<std::mutex> lock(this->queue_mutex);
   	                while( this->isActive.load() && this->mTasks.empty())
   	                    this->condition.wait(lock);
   	                if( ! this->isActive.load() && this->mTasks.empty())
   	                    return;
   	                std::function<void()> lNextTask(this->mTasks.top());
   	                this->mTasks.pop();
   	                lock.unlock();
   	                lNextTask();
   	            }
   	        }
   	    ));
	}
}

ThreadPool<PRIORITY_POLICY>::ThreadPool (size_t numThreads){
    for(size_t i = 0 ; i < numThreads; i++){
        mWorkers.emplace_back(std::thread(
            [this] {
                while(true){
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    while( this->isActive.load() && this->mTasks.empty())
                        this->condition.wait(lock);
                    if( ! this->isActive.load() && this->mTasks.empty())
                        return;
                    std::function<void()> lNextTask(this->mTasks.top());
                    this->mTasks.pop();
                    lock.unlock();
                    lNextTask();
                }
            }
        ));
    }
}
#endif
