/**
  * Assignment: memory
  * Operating Systems
  */

// function/class definitions you are going to use
#include <sys/resource.h>
#include <sys/time.h>
#include <unistd.h>
#include <cstdint>
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <vector>
// MOD: add a monotonic wall-clock timer
#include <time.h>

// although it is good habit, you don't have to type 'std::' before many objects by including this line
using namespace std;

// size of the image
const int64_t SIZE = 16384ULL;
const int64_t REPEAT = 1ULL;

// MOD: helper to get wall time in seconds
static inline double now_sec() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}
int main(int argc, char* argv[]) {
  // MOD: start wall timer
  double t0 = now_sec();
  float* img = new float[SIZE * SIZE];

  // fill with dummy data
  //MOD: changed to row major just as described by the book
  for (int64_t j = 0; j < SIZE; j++) {
    for (int64_t i= 0; i < SIZE; i++) {
      img[j * SIZE + i] = (2 * j + i) % 32768;
    }
  }

  // this dummy value is needed to avoid compilers eliminating the loop as part of a optimisation
  uint64_t dummy = 0;

  // ADJUST BELOW, BUT keep writing to the dummy variable
  float* res = new float[SIZE * SIZE];  // result of mean filter

  // Apply an averaging imaging filter to some input image, and write in to an output image.
  // A pixel in the output image is calculated by averaging 9 pixels: the pixel at the same
  // coordinates in the input image, and the adjecent pixels.
  // MOD: precompute reciprocal of 9 to avoid repeated divisions
  const float INV9 = 1.0f / 9.0f;
  // MOD: switch to row major just as by filling the image with zeroes
  for (int64_t r = 0; r < REPEAT; ++r) {
    for (int64_t j = 1; j < SIZE - 1; j++) {
      // MOD: row pointers to avoid recomputing base index
      const float* row_m1 = &img[(j - 1) * SIZE];
      const float* row_0  = &img[j * SIZE];
      const float* row_p1 = &img[(j + 1) * SIZE];
      float*       row_out = &res[j * SIZE];
      for (int64_t i = 1; i < SIZE - 1; i++) {
        // MOD: replace inner 3x3 loop with explicit sum of 9 neighbours
        float sum =
            row_m1[i - 1] + row_m1[i] + row_m1[i + 1] +
            row_0 [i - 1] + row_0 [i] + row_0 [i + 1] +
            row_p1[i - 1] + row_p1[i] + row_p1[i + 1];

        // MOD: multiply by precomputed 1/9
        row_out[i] = sum * INV9;
      }
    }
    // MOD: Row major dummy
    for (int64_t j = 1; j < SIZE - 1; j++) {
      for (int64_t i = 1; i < SIZE - 1; i++) {
        dummy += res[j * SIZE + i];
      }
    }
  }

  delete[] res;

  // ADJUST ABOVE, BUT keep writing to the dummy variable
  delete[] img;

  // MOD: stop wall timer
  double t1 = now_sec();

  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  std::cout << std::fixed << std::setprecision(6);
  std::cout << "wall time:                    " << (t1 - t0) << " s\n";
  std::cout << "user time:                    " << usage.ru_utime.tv_sec << "." << std::fixed << std::setw(6) << std::setprecision(6) << std::setfill('0') << usage.ru_utime.tv_usec << " s" << std::endl;
  std::cout << "system time:                  " << usage.ru_stime.tv_sec << "."<< std::setw(6) << std::setfill('0') << usage.ru_stime.tv_usec << " s\n";
  std::cout << "soft page faults:             " << usage.ru_minflt << std::endl;
  std::cout << "hard page faults:             " << usage.ru_majflt << std::endl;
#ifdef __APPLE__
  std::cout << "max memory:                   " << usage.ru_maxrss/1024 << " KiB" << std::endl;
#else
  std::cout << "max memory:                   " << usage.ru_maxrss << " KiB" << std::endl;
#endif
  std::cout << "voluntary context switches:   " << usage.ru_nvcsw << std::endl;
  std::cout << "involuntary context switches: " << usage.ru_nivcsw << std::endl;
  std::cout << "dummy value (ignore):         " << dummy << std::endl; // this value is printed to avoid optimisations
  std::cout << "typical page size:            " << sysconf(_SC_PAGESIZE) << std::endl;
  // ...
  return 0;
}
