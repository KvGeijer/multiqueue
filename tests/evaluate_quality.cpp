#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <numeric>
#include <set>
#include <unordered_map>
#include <vector>

#include "cxxopts.hpp"
#include "tlx/container/btree_map.hpp"

struct deletion_log {
    uint64_t tick;
    std::optional<std::pair<unsigned int, uint32_t>> value;
};

std::istream& operator>>(std::istream& in, deletion_log& line) {
    std::pair<unsigned int, uint32_t> value;
    in >> line.tick >> value.first >> value.second;
    line.value = value;
    return in;
}

std::ostream& operator<<(std::ostream& out, deletion_log const& line) {
    if (line.value) {
        out << "Tick: " << line.tick << " Other thread id: " << line.value->first << " Value: " << line.value->second;
    } else {
        out << "Tick: " << line.tick << " Failed";
    }
    return out;
}

struct insertion_log {
    uint64_t tick;
    uint32_t key;
};

std::istream& operator>>(std::istream& in, insertion_log& line) {
    in >> line.tick >> line.key;
    return in;
}

std::ostream& operator<<(std::ostream& out, insertion_log const& line) {
    out << "Tick: " << line.tick << " Key: " << line.key;
    return out;
}

struct heap_entry {
    uint32_t key;
    unsigned int ins_thread_id;
    uint32_t elem_id;
};

bool operator<(heap_entry const& lhs, heap_entry const& rhs) {
    if (lhs.key < rhs.key) {
        return true;
    }
    if (lhs.key == rhs.key) {
        if (lhs.ins_thread_id < rhs.ins_thread_id) {
            return true;
        }
        if (lhs.ins_thread_id == rhs.ins_thread_id) {
            if (lhs.elem_id < rhs.elem_id) {
                return true;
            }
        }
    }
    return false;
}

int main(int argc, char* argv[]) {
    std::filesystem::path out_rank;
    std::filesystem::path out_delay;
    std::filesystem::path out_top_delay;

    cxxopts::Options options(argv[0], "Parses the logs generated by the quality benchmarks");
    options.positional_help("INSERT DELETION");
    // clang-format off
    options.add_options()
      ("r,out-rank", "The output of the rank histogram", cxxopts::value<std::filesystem::path>(out_rank)->default_value("rank_histogram.txt"), "PATH")
      ("d,out-delay", "The output of the delay histogram", cxxopts::value<std::filesystem::path>(out_delay)->default_value("delay_histogram.txt"), "PATH")
      ("t,out-top-delay", "The output of the top delay histogram", cxxopts::value<std::filesystem::path>(out_top_delay)->default_value("top_delay_histogram.txt"), "PATH")
      ("h,help", "Print this help");
    // clang-format on

    try {
        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cerr << options.help() << std::endl;
            std::exit(0);
        }
    } catch (cxxopts::OptionParseException const& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    std::vector<std::vector<insertion_log>> insertions;
    std::vector<deletion_log> deletions;
    std::clog << "Reading quality log file from stdin..." << std::endl;
    unsigned int num_threads;
    std::cin >> num_threads;
    if (!std::cin || num_threads == 0) {
        std::cerr << "Invalid number of threads" << std::endl;
        return 1;
    }
    insertions.resize(num_threads);
    char op;
    unsigned int thread_id;
    bool deleting = false;
    int line = 0;
    while (std::cin >> op >> thread_id) {
        if (thread_id >= num_threads) {
            std::cerr << "Thread id " << thread_id << " too high (Max: " << num_threads - 1 << ')' << std::endl;
            return 1;
        }
        if (op == 'i') {
            if (deleting) {
                std::cerr << "Insertion following a deletion" << std::endl;
                return 1;
            }
            insertion_log ins;
            std::cin >> ins;
            if (!insertions[thread_id].empty() && ins.tick < insertions[thread_id].back().tick) {
                std::cerr << "Insertion\n\t" << ins << "\nhappens before previous insertion" << std::endl;
                return 1;
            }
            insertions[thread_id].push_back(ins);
        } else if (op == 'd') {
            deleting = true;
            deletion_log del;
            std::cin >> del;
            if (del.value->first >= num_threads) {
                std::cerr << "Line " << line << ": Other thread id " << del.value->first
                          << " too high (Max: " << num_threads - 1 << ')' << std::endl;
                return 1;
            }
            if (del.value->second >= insertions[del.value->first].size()) {
                std::cerr << "No insertion corresponding to deletion\n\t" << del << std::endl;
                return 1;
            }
            if (del.tick < insertions[del.value->first][del.value->second].tick) {
                std::cerr << "Deletion of \n\t" << del << "\nhappens before its insertion" << std::endl;
                return 1;
            }
            deletions.push_back(del);
        } else if (op == 'f') {
            deleting = true;
            uint64_t tick;
            std::cin >> tick;
            deletions.push_back(deletion_log{tick, std::nullopt});
        } else {
            std::cerr << "Invalid operation: " << op << std::endl;
            return 1;
        }
        ++line;
    }

    if (deletions.size() < 100'000) {
        std::cerr << "Too few deletions" << std::endl;
        return 1;
    }
    std::clog << "Sorting deletions...\n";
    std::sort(deletions.begin(), deletions.end(), [](auto const& lhs, auto const& rhs) { return lhs.tick < rhs.tick; });
    std::cout << std::flush;
    std::vector<size_t> rank_histogram;
    std::vector<size_t> delay_histogram;
    std::vector<size_t> top_delay_histogram;
    rank_histogram.resize(5'000);
    delay_histogram.resize(5'000);
    top_delay_histogram.resize(5'000);
    std::clog << "Replaying operations...\n";
    tlx::btree_map<heap_entry, std::pair<size_t, size_t>> replay_heap{};
    std::vector<size_t> insert_index(insertions.size(), 0);
    uint64_t failed_deletions = 0;
    for (size_t i = 0; i < 100'000; ++i) {
        // Inserting everything before next deletion
        for (unsigned int t = 0; t < insertions.size(); ++t) {
            while (insert_index[t] < insertions[t].size() && insertions[t][insert_index[t]].tick < deletions[i].tick) {
                replay_heap.insert(
                    {{insertions[t][insert_index[t]].key, t, static_cast<uint32_t>(insert_index[t])}, {0, 0}});
                ++insert_index[t];
            }
        }

        if (!deletions[i].value) {
            if (!replay_heap.empty()) {
                ++failed_deletions;
                size_t rank_error = std::min(replay_heap.size(), static_cast<size_t>(4999));
                ++rank_histogram[rank_error];
                for (auto& [entry, delays] : replay_heap) {
                    if (entry.key == replay_heap.begin()->first.key) {
                        ++delays.first;
                    }
                    ++delays.second;
                }
            }
            continue;
        }

        auto key = insertions[deletions[i].value->first][deletions[i].value->second].key;

        if (auto it = replay_heap.find({key, deletions[i].value->first, deletions[i].value->second});
            it != replay_heap.end()) {
            ++top_delay_histogram[std::min<size_t>(it->second.first, 4999)];
            ++delay_histogram[std::min<size_t>(it->second.second, 4999)];
            auto smaller = replay_heap.begin();
            for (; smaller != it && smaller->first.key < key; ++smaller) {
                if (smaller->first.key == replay_heap.begin()->first.key) {
                    ++smaller->second.first;
                }
                ++smaller->second.second;
            }
            size_t rank_error =
                std::min(static_cast<size_t>(std::distance(replay_heap.begin(), smaller)), static_cast<size_t>(4999));
            ++rank_histogram[rank_error];
            replay_heap.erase(it);
        } else {
            std::cerr << "Element\n\t" << insertions[deletions[i].value->first][deletions[i].value->second]
                      << " Value: " << deletions[i].value->second << " Deletion tick: " << deletions[i].tick
                      << "\nis not in the heap at deletion time" << std::endl;
            return 1;
        }
        if (i % 10'000 == 0) {
            std::clog << "\rProcessed " << std::setprecision(3)
                      << 100. * static_cast<double>(i) / static_cast<double>(deletions.size()) << "%";
            std::clog << "\rProcessed " << std::setprecision(3) << 100. * static_cast<double>(i) / 100'000.0 << "%";
        }
    }
    std::clog << "\rProcessed 100.0%" << std::endl;
    {
        auto out_f = std::ofstream{out_rank};
        for (size_t i = 0; i < rank_histogram.size(); ++i) {
            if (rank_histogram[i] > 0) {
                out_f << i << " " << rank_histogram[i] << '\n';
            }
        }
        out_f.close();
    }
    {
        auto out_f = std::ofstream{out_delay};
        for (size_t i = 0; i < delay_histogram.size(); ++i) {
            if (delay_histogram[i] > 0) {
                out_f << i << " " << delay_histogram[i] << '\n';
            }
        }
        out_f.close();
    }
    {
        auto out_f = std::ofstream{out_top_delay};
        for (size_t i = 0; i < top_delay_histogram.size(); ++i) {
            if (top_delay_histogram[i] > 0) {
                out_f << i << " " << top_delay_histogram[i] << '\n';
            }
        }
        out_f.close();
    }
    std::clog << "Histograms have been written" << std::endl;
    std::clog << "Failed deletions with nonempty queue: " << failed_deletions << std::endl;
    return 0;
}