#include "caffe2/utils/threadpool/pthreadpool.h"
#include "caffe2/utils/threadpool/ThreadPool.h"


//
// External API
//

void pthreadpool_compute_1d(
    pthreadpool_t threadpool,
    pthreadpool_function_1d_t function,
    void* argument,
    size_t range) {
  if (threadpool == nullptr) {
      /* No thread pool provided: execute function sequentially on the calling
       * thread */
      for (size_t i = 0; i < range; i++) {
          function(argument, i);
      }
      return;
  }
  reinterpret_cast<caffe2::ThreadPool*>(threadpool)
      ->run(
          [function, argument](int threadId, size_t workId) {
            function(argument, workId);
          },
          range);
}

size_t pthreadpool_get_threads_count(pthreadpool_t threadpool) {
  return reinterpret_cast<caffe2::ThreadPool*>(threadpool)->getNumThreads();
}
