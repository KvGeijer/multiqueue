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

static constexpr std::size_t num_deletions = 10'000'000;

struct log_entry {
    unsigned int thread_id;
    uint64_t tick;
    bool failed;
    uint32_t key;
    unsigned int insert_thread_id;
    uint32_t value;
    bool deleted;
};

std::istream& operator>>(std::istream& in, log_entry& entry) {
    in >> entry.thread_id >> entry.tick >> entry.key >> entry.insert_thread_id >> entry.value;
    entry.failed = false;
    entry.deleted = false;
    return in;
}

std::ostream& operator<<(std::ostream& out, log_entry const& entry) {
    out << entry.thread_id << ' ' << entry.tick << ' ' << entry.key << ' ' << entry.insert_thread_id << ' '
        << entry.value;
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

    cxxopts::Options options(argv[0], "Parses the logs generated by the generic test");
    options.positional_help("INSERT DELETION");
    // clang-format off
    options.add_options()
      ("v,verify", "Only verify the log")
      ("r,out-rank", "The output of the rank histogram", cxxopts::value<std::filesystem::path>(out_rank)->default_value("rank_histogram.txt"), "PATH")
      ("d,out-delay", "The output of the delay histogram", cxxopts::value<std::filesystem::path>(out_delay)->default_value("delay_histogram.txt"), "PATH")
      ("t,out-top-delay", "The output of the top delay histogram", cxxopts::value<std::filesystem::path>(out_top_delay)->default_value("top_delay_histogram.txt"), "PATH")
      ("h,help", "Print this help");
    // clang-format on

    auto verify_only = false;
    try {
        auto result = options.parse(argc, argv);
        if (result.count("help")) {
            std::cerr << options.help() << std::endl;
            std::exit(0);
        }
        if (result.count("verify")) {
            verify_only = true;
        }
    } catch (cxxopts::OptionParseException const& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    std::vector<std::vector<log_entry>> insertions;
    std::vector<log_entry> deletions;

    std::clog << "Reading quality log file from stdin..." << std::endl;
    unsigned int num_threads;
    std::cin >> num_threads;
    if (!std::cin || num_threads == 0) {
        std::cerr << "Line 1: "
                  << "Invalid number of threads" << std::endl;
        return 1;
    }
    insertions.resize(num_threads);

    char op;
    bool deleting = false;
    int line = 2;

    while (std::cin >> op) {
        if (op == 'i') {
            if (deleting) {
                std::cerr << "Line " << line << ": "
                          << "Insertion following a deletion" << std::endl;
                return 1;
            }
            log_entry entry;
            std::cin >> entry;
            if (entry.thread_id >= num_threads) {
                std::cerr << "Line " << line << ": "
                          << "Thread id " << entry.thread_id << " too high (Max: " << num_threads - 1 << ')'
                          << std::endl;
                return 1;
            }
            if (entry.insert_thread_id >= num_threads) {
                std::cerr << "Line " << line << ": "
                          << "Insert thread id " << entry.insert_thread_id << " too high (Max: " << num_threads - 1
                          << ')' << std::endl;
                return 1;
            }
            if (!insertions[entry.thread_id].empty() && entry.tick < insertions[entry.thread_id].back().tick) {
                std::cerr << "Line " << line << ": "
                          << "Insertion\n\t" << entry << "\nhappens before previous insertion of same thread"
                          << std::endl;
                return 1;
            }
            if (entry.value != insertions[entry.thread_id].size() || entry.thread_id != entry.insert_thread_id) {
                std::cerr << "Line " << line << ": "
                          << "Inconsistent insertion:\n\t" << entry << std::endl;
                return 1;
            }
            insertions[entry.thread_id].push_back(entry);
        } else if (op == 'd') {
            deleting = true;
            log_entry entry;
            std::cin >> entry;
            if (entry.thread_id >= num_threads) {
                std::cerr << "Line " << line << ": "
                          << "Thread id " << entry.thread_id << " too high (Max: " << num_threads - 1 << ')'
                          << std::endl;
                return 1;
            }
            if (entry.insert_thread_id >= num_threads) {
                std::cerr << "Line " << line << ": "
                          << "Insert thread id " << entry.insert_thread_id << " too high (Max: " << num_threads - 1
                          << ')' << std::endl;
                return 1;
            }
            if (entry.value >= insertions[entry.insert_thread_id].size()) {
                std::cerr << "Line " << line << ": "
                          << "No insertion corresponding to deletion\n\t" << entry << std::endl;
                return 1;
            }
            if (entry.key != insertions[entry.insert_thread_id][entry.value].key) {
                std::cerr << "Line " << line << ": "
                          << "Deletion \n\t" << entry << "\ninconsistent to insertion\n\t"
                          << insertions[entry.insert_thread_id][entry.value] << std::endl;
                return 1;
            }
            if (entry.tick < insertions[entry.insert_thread_id][entry.value].tick) {
                std::cerr << "Line " << line << ": "
                          << "Deletion of \n\t" << entry << "\nhappens before its insertion" << std::endl;
                return 1;
            }
            if (insertions[entry.insert_thread_id][entry.value].deleted) {
                std::cerr << "Line " << line << ": "
                          << "Insertion\n\t" << insertions[entry.insert_thread_id][entry.value] << "\n extracted twice"
                          << std::endl;
                return 1;
            }
            deletions.push_back(entry);
            insertions[entry.insert_thread_id][entry.value].deleted = true;
        } else if (op == 'f') {
            deleting = true;
            log_entry entry;
            std::cin >> entry.thread_id >> entry.tick;
            if (entry.thread_id >= num_threads) {
                std::cerr << "Line " << line << ": "
                          << "Thread id " << entry.thread_id << " too high (Max: " << num_threads - 1 << ')'
                          << std::endl;
                return 1;
            }
            entry.failed = true;
            deletions.push_back(entry);
        } else {
            std::cerr << "Line " << line << ": "
                      << "Invalid operation: " << op << std::endl;
            return 1;
        }
        ++line;
    }
    if (verify_only) {
        std::clog << "Log is consistent" << std::endl;
        return 0;
    }
    if (deletions.size() < num_deletions) {
        std::clog << "Too few deletions!" << std::endl;
    }
    std::clog << "Sorting deletions..." << std::flush;
    std::sort(deletions.begin(), deletions.end(), [](auto const& lhs, auto const& rhs) { return lhs.tick < rhs.tick; });
    std::clog << "done\n";
    std::vector<size_t> rank_histogram;
    std::vector<size_t> delay_histogram;
    std::vector<size_t> top_delay_histogram;

    std::clog << "Replaying operations...\n";
    tlx::btree_map<heap_entry, std::pair<size_t, size_t>> replay_heap{};
    std::vector<size_t> insert_index(insertions.size(), 0);
    std::size_t failed_deletions = 0;
    for (size_t i = 0; i < std::min(deletions.size(), num_deletions); ++i) {
        // Inserting everything before next deletion
        for (unsigned int t = 0; t < insertions.size(); ++t) {
            while (insert_index[t] < insertions[t].size() && insertions[t][insert_index[t]].tick < deletions[i].tick) {
                replay_heap.insert(
                    {{insertions[t][insert_index[t]].key, t, static_cast<uint32_t>(insert_index[t])}, {0, 0}});
                ++insert_index[t];
            }
        }

        if (deletions[i].failed) {
            if (!replay_heap.empty()) {
                ++failed_deletions;
                size_t rank_error = replay_heap.size();
                if (rank_histogram.size() <= rank_error) {
                    rank_histogram.resize(rank_error + 1, 0);
                }
                ++rank_histogram[rank_error];
                auto top_key = replay_heap.begin()->first.key;
                for (auto& [entry, delays] : replay_heap) {
                    if (entry.key == top_key) {
                        ++delays.first;
                    }
                    ++delays.second;
                }
            }
            continue;
        }

        auto key = deletions[i].key;

        if (auto it = replay_heap.find({key, deletions[i].insert_thread_id, deletions[i].value});
            it != replay_heap.end()) {
            if (top_delay_histogram.size() <= it->second.first) {
                top_delay_histogram.resize(it->second.first + 1, 0);
            }
            ++top_delay_histogram[it->second.first];
            if (delay_histogram.size() <= it->second.second) {
                delay_histogram.resize(it->second.second + 1, 0);
            }
            ++delay_histogram[it->second.second];
            auto smaller = replay_heap.begin();
            for (; smaller != it && smaller->first.key < key; ++smaller) {
                if (smaller->first.key == replay_heap.begin()->first.key) {
                    ++smaller->second.first;
                }
                ++smaller->second.second;
            }
            size_t rank_error = static_cast<size_t>(std::distance(replay_heap.begin(), smaller));
            if (rank_histogram.size() <= rank_error) {
                rank_histogram.resize(rank_error + 1, 0);
            }
            ++rank_histogram[rank_error];
            replay_heap.erase(it);
        } else {
            std::cerr << "Element\n\t" << deletions[i] << "\nis not in the heap at deletion time" << std::endl;
            return 1;
        }
        if (i % (num_deletions / 100) == 0) {
            std::clog << "\rProcessed " << std::setprecision(3)
                      << 100. * static_cast<double>(i) / static_cast<double>(num_deletions) << "%";
        }
    }
    std::clog << "\rProcessing done         " << std::endl;
    std::clog << "Failed deletions: " << failed_deletions << std::endl;
    std::clog << "Writing histograms..." << std::flush;
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
    std::clog << "done" << std::endl;
    return 0;
}
