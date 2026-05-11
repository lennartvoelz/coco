#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>

constexpr int ARRAY_MIN = 1024;
constexpr int ARRAY_MAX = 4096 * 512;
constexpr int NUM_THREADS = 4;

int x[ARRAY_MAX]; /* Array to stride through */
std::atomic<double> total_steps(0.0); /* Atomic variable for thread-safe step counting */
std::mutex print_mutex; /* Mutex for thread-safe printing */

long get_seconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec;
}

int label(int i) {
    if (i < 1e3) printf("%1dB,", i);
    else if (i < 1e6) printf("%1dK,", i / 1024);
    else if (i < 1e9) printf("%1dM,", i / 1048576);
    else printf("%1dG,", i / 1073741824);
    return 0;
}

void worker_thread(int stride) {
    int nextstep, i;
    double steps = 0.0;
    long lastsec, sec0, sec1;

    /* Wait for timer to roll over */
    lastsec = get_seconds();
    do sec0 = get_seconds();
    while (sec0 == lastsec);

    /* Walk through path in array for ten seconds */
    do {
        for (i = stride; i != 0; i = i - 1) {
            nextstep = 0;
            do nextstep = x[nextstep];
            while (nextstep != 0);
        }
        steps += 1.0;
        sec1 = get_seconds();
    } while ((sec1 - sec0) < 5);

    total_steps.fetch_add(steps);
}

int main() {
    int csize, stride;
    std::vector<std::thread> threads;

    /* Initialize output */
    printf(" ,");
    for (stride = 1; stride <= ARRAY_MAX / 2; stride = stride * 2) {
        label(stride * sizeof(int));
    }
    printf("\n");

    /* Main loop for each configuration */
    for (csize = ARRAY_MIN; csize <= ARRAY_MAX; csize = csize * 2) {
        label(csize * sizeof(int));

        for (stride = 1; stride <= csize / 2; stride = stride * 2) {
            /* Lay out linked path through array */
            for (int index = 0; index < csize - stride; index += stride)
                x[index] = index + stride;
            x[csize - stride] = 0;

            /* Reset total steps for this configuration */
            total_steps = 0.0;

            /* Launch worker threads */
            threads.clear();
            for (int i = 0; i < NUM_THREADS; ++i) {
                threads.emplace_back(worker_thread, stride);
            }

            /* Wait for all threads to complete */
            for (auto &thread : threads) {
                thread.join();
            }

            /* ns per load: total_time / total_loads across all threads */
            double steps = total_steps.load();
            double loadtime = (20.0 * 1e9) / (steps / NUM_THREADS * csize);

            /* Thread-safe printing */
            {
              std::lock_guard<std::mutex> lock(print_mutex);
              printf("%4.1f,", (loadtime < 0.1) ? 0.1 : loadtime);
            }
        }
        printf("\n");
    }

    return 0;
}
