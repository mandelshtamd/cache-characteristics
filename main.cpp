#include <iostream>
#include <vector>
#include <chrono>
#include <numeric>
#include <set>
#include <algorithm>

static const int MEMORY_SIZE = 1024 * 1024 * 1024;
static char *memoryBlock;

int measureTime(long long stride, int span) {
    volatile char **currentPointer = (volatile char **)(&memoryBlock[0]);

    for (long long index = (span - 1) * stride; index >= 0; index -= stride) {
        currentPointer = (volatile char **)(&memoryBlock[index]);
        *currentPointer = (index >= stride) ? &memoryBlock[index - stride] : &memoryBlock[(span - 1) * stride];
        asm volatile("" ::: "memory");
    }
    std::vector<long long> timingResults;

    for (int i = 0; i < 20; ++i) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int j = 0; j < 1000000; ++j) {
            currentPointer = (volatile char **)(*currentPointer);
        }
        asm volatile("" ::: "memory");
        auto end = std::chrono::high_resolution_clock::now();
        timingResults.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
    }
    return std::accumulate(timingResults.begin(), timingResults.end(), 0LL) / timingResults.size();
}


int main() {
    memoryBlock = new char[MEMORY_SIZE];

    long long stride = 16;
    std::vector<std::set<int>> jumpSizes;

    for (; stride < MEMORY_SIZE / 16; stride *= 2) {
        int previousTime = -1;
        std::set<int> newJumps;

        for (int span = 1; span <= 16; ++span) {
            int currentTime = measureTime(stride, span);
            if (previousTime != -1 && static_cast<double>(currentTime - previousTime) / currentTime > 0.1) {
                newJumps.insert(span - 1);
            }
            previousTime = currentTime;
        }
        bool hasSameJumps = jumpSizes.empty() || jumpSizes.back() == newJumps;

        if (hasSameJumps && stride >= 256 * 1024) {
            break;
        }

        jumpSizes.push_back(std::move(newJumps));
    }

    std::vector<std::pair<int, int>> caches;
    std::set<int> toProcess = jumpSizes[jumpSizes.size() - 1];

    std::reverse(jumpSizes.begin(), jumpSizes.end());
    for (auto &jump: jumpSizes) {
        std::set<int> toDelete;
        for (int s: toProcess) {
            if (!jump.count(s)) {
                caches.push_back({stride * s, s});
                toDelete.insert(s);
            }
        }
        for (int s: toDelete) {
            toProcess.erase(s);
        }
        stride /= 2;
    }

    std::sort(caches.begin(), caches.end());

    int cacheSize = caches[0].first;
    int cacheAssociativity = caches[0].second;
    int cacheLineSize = -1;
    int previousFirstJump = 1025;

    for (int L = 1; L <= cacheSize; L *= 2) {
        int previousTime = -1;
        int firstJump = -1;

        for (int S = 1; S <= 1024; S *= 2) {
            int currentTime = measureTime(cacheSize / cacheAssociativity + L, S + 1);
            if (previousTime != -1 && (double) (currentTime - previousTime) / currentTime > 0.1) {
                if (firstJump <= 0) {
                    firstJump = S;
                }
            }
            previousTime = currentTime;
        }

        if (firstJump > previousFirstJump || previousFirstJump != 1025 * firstJump == -1) {
            cacheLineSize = L * cacheAssociativity;
            break;
        }
        previousFirstJump = firstJump;
    }

    delete[] memoryBlock;

    std::cout << "L1 cache: "
              << "size = " << cacheSize
              << ", associativity = " << cacheAssociativity
              << ", line size = " << cacheLineSize
              << std::endl;
}