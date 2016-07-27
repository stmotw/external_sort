#include <cstdio>
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <iterator>
#include <future>
#include <exception>

#include "ExternalSort.h"
#include "ThreadPool.hpp"
#include "AntiGccUtils.hpp"

namespace ExternalSort {

SortParameters::SortParameters(int argc, char** argv)
  : threads_count(DEFAULT_THREADS_COUNT),
    memory_limit(DEFAULT_MEMORY_LIMIT_MB),
    no_of_ways_for_merging(DEFAULT_NO_OF_WAYS_FOR_MERGING)
{
    if (argc < 2) {
        throw SortParametersException("Input filename not specified");
    } else {
        input_filename = std::string(argv[1]);
    }

    if (argc < 3) {
        throw SortParametersException("Output filename not specified");
    } else {
        output_filename = std::string(argv[2]);
    }

    if (argc > 3) {
        std::istringstream iss(argv[3]);
        iss >> memory_limit;
        if ((memory_limit < MIN_MEMORY_LIMIT) || (memory_limit > MAX_MEMORY_LIMIT)) {
            throw SortParametersException("Incorrect memory limit");
        }
    }
    memory_limit *= 1 << 20; // MB -> bytes

    if (argc > 4) {
        threads_count = atoi(argv[4]);
        if ((threads_count < 1) || (threads_count > MAX_THREADS_COUNT)) {
            throw SortParametersException("Incorrect threads count");
        }
    }

    if (argc > 5) {
        no_of_ways_for_merging = atoi(argv[5]);
        /**
         * In fact that's not right, because we are storing streams + their iterators for
         * each of files that are merging. But in practice buffer for sorting a chunk is
         * way bigger than this requirement.
         */
        if (no_of_ways_for_merging * sizeof(std::int32_t) > memory_limit || no_of_ways_for_merging < 2) {
            throw SortParametersException("Can not use this number of ways for merging");
        }
    }

    // Each thread must be able to hold at least two numbers for sort to work
    if (memory_limit < 2 * sizeof(std::int32_t) * threads_count) {
        throw SortParametersException("Insufficient memory for 1 thread");
    }

    chunk_size = memory_limit / threads_count;
    chunk_size = chunk_size - chunk_size % sizeof(std::int32_t);
}

void sort(const SortParameters& parameters) {
    std::uint32_t chunk_no = split_into_chunks(parameters.input_filename, parameters.chunk_size);
    const std::uint32_t initial_chunks_count = chunk_no;
    std::cout << "Split into " << initial_chunks_count << " chunks." << std::endl;

    ThreadPool pool(parameters.threads_count);
    std::vector<std::future<bool>> schedule;

    // Add tasks for sorting initial chunks
    for (std::uint32_t i = 0; i < initial_chunks_count; ++i) {
        schedule.emplace_back(pool.enqueue(
            [i, &parameters]() {
                sort_chunk(get_chunk_filename(i), parameters.chunk_size);
                std::cout << "Chunk #" << i << " sorted" << std::endl;
                return true;
            }
        ));
    }
    // Add tasks for merging chunks
    for (std::uint32_t i = 0; i * (parameters.no_of_ways_for_merging - 1) + 1 < initial_chunks_count; ++i) {
        schedule.emplace_back(pool.enqueue(
            [i, chunk_no, &schedule, &parameters]() mutable {
                std::cout << "Merging [";
                std::vector<std::string> chunk_filenames;
                for (std::uint32_t delta = 0; delta < parameters.no_of_ways_for_merging; ++delta) {
                    std::uint32_t needed_chunk_no = parameters.no_of_ways_for_merging * i + delta;
                    if (needed_chunk_no + 1 < schedule.size()) {
                        schedule[needed_chunk_no].wait();
                        chunk_filenames.emplace_back(get_chunk_filename(needed_chunk_no));
                        std::cout << needed_chunk_no << " ";
                    }
                }
                std::cout << "] into " << chunk_no << std::endl;

                merge_sorted_chunks(chunk_filenames, get_chunk_filename(chunk_no));
                delete_files(chunk_filenames);

                return true;
            }
        ));
        ++chunk_no;
    }

    schedule.back().get();
    if (std::rename(get_chunk_filename(chunk_no - 1).c_str(), parameters.output_filename.c_str()) != 0) {
        throw std::runtime_error("Cannot rename last chunk into output file");
    }
}

std::uint32_t split_into_chunks(const std::string& filename, std::uint64_t chunk_size) {
    std::ifstream input_file(filename, std::ios::binary);
    uint32_t chunks_count = 0;
    char* buffer = new char[chunk_size];

    while (!input_file.eof()) {
        input_file.read(buffer, chunk_size);
        auto read_length = input_file.gcount();
        if (read_length > 0) {
            auto chunk_filename = get_chunk_filename(chunks_count++);
            std::ofstream chunk_file(chunk_filename, std::ios::binary | std::ios::trunc);
            chunk_file.write(buffer, read_length);
            chunk_file.close();
        }
    }

    delete [] buffer;
    return chunks_count;
}

void sort_chunk(const std::string& chunk_filename, std::uint64_t chunk_size) {
    std::vector<std::int32_t> data(chunk_size / sizeof(std::int32_t));
    std::fstream chunk;

    chunk.open(chunk_filename, std::ios::binary | std::ios::in);
    chunk.read((char*) & data[0], chunk_size);
    chunk.close();

    auto values_count = chunk.gcount() / sizeof(std::int32_t);
    std::sort(data.begin(), data.begin() + values_count, std::less<std::int32_t>());

    chunk.open(chunk_filename, std::ios::binary | std::ios::out | std::ios::trunc);
    chunk.write((char*) & data[0], values_count * sizeof(std::int32_t));
    chunk.close();
}

void merge_sorted_chunks(const std::vector<std::string>& chunk_filenames, std::string output_filename) {
    std::vector<std::unique_ptr<std::ifstream>> chunks;
    std::vector<BinaryStreamWalker<std::int32_t>> heap;
    std::ofstream output_file(output_filename, std::ios::binary | std::ios::trunc);

    for (auto it = chunk_filenames.begin(); it != chunk_filenames.end(); ++it) {
        chunks.emplace_back(make_up<std::ifstream>(*it, std::ios::binary));
        heap.emplace_back(*(chunks.back()));
        if (!heap.back().move()) {
            heap.pop_back();
        }
    }

    auto heap_cmp = [](const BinaryStreamWalker<std::int32_t>& a, const BinaryStreamWalker<std::int32_t>& b) {
        return a.value() > b.value();
    };

    /**
     * Create heap for merging. Basic loop:
     * - get smallest
     * - try to read next value from the file samllest originated from
     *   - if succes: push new element to heap
     *   - if not:    reduce heap size by 1
     */
    std::make_heap(heap.begin(), heap.end(), heap_cmp);
    while (heap.size() > 1) {
        std::pop_heap(heap.begin(), heap.end(), heap_cmp);
        std::int32_t smallest = heap.back().value();
        output_file.write((char*) & smallest, sizeof(std::int32_t) / sizeof(char));
        if (heap.back().move()) {
            std::push_heap(heap.begin(), heap.end(), heap_cmp);
        }
        else {
            heap.pop_back();
        }
    }
    // Read up last file
    do {
        std::int32_t smallest = heap.back().value();
        output_file.write((char*) & smallest, sizeof(std::int32_t) / sizeof(char));
    } while(heap.back().move());

    output_file.close();
}


std::string get_chunk_filename(std::uint32_t chunk_no) {
    std::ostringstream oss;
    oss << "chunk" << chunk_no << ".bin";
    return oss.str();
}

void delete_files(const std::vector<std::string>& filenames) {
    std::for_each(filenames.begin(), filenames.end(), [](std::string filename) {
        if (std::remove(filename.c_str()) != 0) {
            throw std::runtime_error("Cannot remove file: " + filename);
        }
    });
}

} // namespace ExternalSort
