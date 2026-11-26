#include "src.hpp"
#include <cassert>
#include <cstdio>
#include <memory>
#include <random>
#include <string>

int main() {
    std::mt19937 random(2995);
    for (int n = 3; n <= 10; ++n) {
        for (int size : {1, 7, 4096}) {
            const int stripes = 23;
            std::vector<std::unique_ptr<sjtu::fstream>> owners;
            std::vector<sjtu::fstream *> drives;
            std::vector<std::string> names;
            auto reset = [&](int disk) {
                std::ofstream file(names[disk], std::ios::binary | std::ios::trunc);
                std::vector<char> zeros(stripes * size);
                file.write(zeros.data(), zeros.size());
            };
            for (int disk = 0; disk < n; ++disk) {
                names.push_back("/tmp/raid5-local-test-" + std::to_string(disk));
                reset(disk);
                owners.emplace_back(new sjtu::fstream);
                drives.push_back(owners.back().get());
            }
            auto open = [&](int missing) {
                for (int disk = 0; disk < n; ++disk) {
                    if (disk == missing) continue;
                    drives[disk]->clear();
                    drives[disk]->open(names[disk], std::ios::binary | std::ios::in | std::ios::out);
                    assert(drives[disk]->is_open());
                }
            };
            RAID5Controller raid(drives, stripes, size);
            assert(raid.Capacity() == (n - 1) * stripes);
            std::vector<std::vector<char>> expected(raid.Capacity(), std::vector<char>(size));
            std::vector<char> actual(size);
            auto verify = [&]() {
                for (int id = 0; id < raid.Capacity(); ++id) {
                    raid.ReadBlock(id, actual.data());
                    assert(actual == expected[id]);
                }
            };
            auto write = [&]() {
                for (int step = 0; step < 500; ++step) {
                    int id = random() % raid.Capacity();
                    for (char &byte : expected[id]) byte = static_cast<char>(random());
                    raid.WriteBlock(id, expected[id].data());
                    raid.ReadBlock(id, actual.data());
                    assert(actual == expected[id]);
                }
                verify();
            };
            open(-1);
            raid.Start(EventType::NORMAL, 0);
            verify();
            write();
            raid.Shutdown();
            for (int disk = 0; disk < n; ++disk) {
                std::remove(names[disk].c_str());
                open(disk);
                raid.Start(EventType::FAILED, disk);
                verify();
                write();
                raid.Shutdown();
                reset(disk);
                open(-1);
                raid.Start(EventType::REPLACED, disk);
                verify();
                write();
                raid.Shutdown();
                reset((disk + 1) % n);
                open(-1);
                raid.Start(EventType::REPLACED, (disk + 1) % n);
                verify();
                raid.Shutdown();
            }
            for (const auto &name : names) std::remove(name.c_str());
        }
    }
    std::puts("Randomized RAID5 tests passed.");
}
