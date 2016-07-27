#ifndef EXTERNAL_SORT_H
#define EXTERNAL_SORT_H

#include <vector>
#include <string>
#include <cstdint>
#include <stdexcept>

namespace ExternalSort {
    const std::uint64_t DEFAULT_MEMORY_LIMIT_MB = 10;
    const std::uint64_t MIN_MEMORY_LIMIT = 1;
    const std::uint64_t MAX_MEMORY_LIMIT = 1 << 8;
    const std::uint64_t DEFAULT_THREADS_COUNT = 4;
    const std::uint64_t MAX_THREADS_COUNT = 100;
    const std::uint32_t DEFAULT_NO_OF_WAYS_FOR_MERGING = 2;

    class SortParametersException: public std::runtime_error {
    public:
        SortParametersException(const std::string& message)
          : std::runtime_error(message){};
    };

    class SortParameters {
    public:
        std::string input_filename;
        std::string output_filename;
        std::uint64_t threads_count;
        std::uint64_t memory_limit;
        std::uint64_t chunk_size;
        std::uint32_t no_of_ways_for_merging;

        SortParameters(int argc, char** argv);
    };

    template<typename T>
    class BinaryStreamWalker {
        std::istreambuf_iterator<char> it;
        std::istreambuf_iterator<char> end;
        std::vector<char> data;
    public:
        BinaryStreamWalker(std::ifstream& stream)
          : it(stream),
            end(),
            data(sizeof(T) / sizeof(char))
        {}
        const T& value() const
        {
            return *reinterpret_cast<T const * const>(& data[0]);
        }
        bool move() {
            for (auto i = 0; i < data.size(); ++i) {
                if (it == end) {
                    return false;
                }
                else {
                    data[i] = *it;
                    ++it;
                }
            }
            return true;
        }
    };

    void sort(const SortParameters& parameters);
    /**
     * Splits file into chunks, saves them on disk and returns number of chunks created
     */
    std::uint32_t split_into_chunks(const std::string& filename, std::uint64_t chunk_size);
    void sort_chunk(const std::string& chunk_filename, std::uint64_t chunk_size);
    void merge_sorted_chunks(const std::vector<std::string>& chunk_filenames, std::string output_filename);

    std::string get_chunk_filename(std::uint32_t chunk_no);
    void delete_files(const std::vector<std::string>& filenames);
}

#endif
