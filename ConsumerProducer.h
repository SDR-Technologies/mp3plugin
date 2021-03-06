#ifndef __CONSUMERPRODUCERQUEUE_H__
#define __CONSUMERPRODUCERQUEUE_H__

#include <stdio.h>
#include <queue>
#include <mutex>
#include <condition_variable>

/*
 * Some references in order
 *
 * Some code I wrote a long time before C++ 11 to do consumer producer buffers, using 2 condition variables
 * https://github.com/mdaus/coda-oss/blob/master/modules/c%2B%2B/mt/include/mt/RequestQueue.h
 *
 * A great article explaining both 2 condition variable and 1 condition variable buffers
 * https://en.wikipedia.org/wiki/Monitor_%28synchronization%29#Condition_variables
 *
 * C++ 11 thread reference:
 * http://en.cppreference.com/w/cpp/thread
 */
template<typename T>
class ConsumerProducerQueue
{
    std::condition_variable cond;
    std::mutex mutex;
    std::queue<T> cpq;
    std::string queueName ;
    unsigned int maxSize;

public:

    ConsumerProducerQueue(int mxsz) : maxSize(mxsz)
    { }

    ConsumerProducerQueue(std::string qName, int mxsz) : queueName(qName), maxSize(mxsz)
    { }

    std::string& name() {
        return( queueName );
    }

    void setName( std::string qName ) {
        queueName.assign( qName );
    }

    int add(T request)
    {
        int L = 0 ;
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock, [this]() { 
                return !isFull(); 
        });
        cpq.push(request); 
        L = cpq.size();
        lock.unlock();
        cond.notify_all();
        return(L);
    }

    int consume(T &request)
    {
        int L = 0 ;
        std::unique_lock<std::mutex> lock(mutex);
        cond.wait(lock, [this]() { 
            return !isEmpty(); 
        });
        request = cpq.front();
        cpq.pop();
        L = cpq.size();
        lock.unlock();
        cond.notify_all();
        return(L);
    }

    bool isFull() const
    {
        return cpq.size() >= maxSize;
    }

    bool isEmpty() const
    {
        return cpq.size() == 0;
    }

    int length() const
    {
        return cpq.size();
    }

    void clear()
    {
        std::unique_lock<std::mutex> lock(mutex);
        while (!isEmpty())
        {
            cpq.pop();
        }
        lock.unlock();
        cond.notify_all();
    }
};

#endif
