#include <iostream>
#include "ExternalSort.h"


int main (int argc, char** argv) {
    try {
        ExternalSort::SortParameters sort_parameters(argc, argv);
        ExternalSort::sort(sort_parameters);
    }
    catch (ExternalSort::SortParametersException& e) {
        std::cout
            << e.what() << std::endl
            << std::endl
            << "Usage: " << argv[0]
                << " input_file output_file [memory_limit_mb [threads_count [no_of_ways_for_merging]]]"
            << std::endl;
    }
    catch (std::runtime_error& e) {
        std::cout << e.what() << std::endl;
    }
}
