#ifndef RAID5_SRC_HPP
#define RAID5_SRC_HPP

#include "fstream.h"
#include <algorithm>
#include <vector>

enum class EventType { NORMAL, FAILED, REPLACED };

class RAID5Controller {
private:
    std::vector<sjtu::fstream *> drives_;
    int blocks_per_drive_;
    int block_size_;
    int num_disks_;
    int failed_drive_ = -1;

    void ReadPhysical(int disk, int stripe, char *data) {
        drives_[disk]->seekg(static_cast<std::streamoff>(stripe) * block_size_);
        drives_[disk]->read(data, block_size_);
    }

    void WritePhysical(int disk, int stripe, const char *data) {
        drives_[disk]->seekp(static_cast<std::streamoff>(stripe) * block_size_);
        drives_[disk]->write(data, block_size_);
    }

    void Reconstruct(int missing, int stripe, char *result) {
        std::fill(result, result + block_size_, 0);
        std::vector<char> buffer(block_size_);
        for (int disk = 0; disk < num_disks_; ++disk) {
            if (disk == missing) continue;
            ReadPhysical(disk, stripe, buffer.data());
            for (int i = 0; i < block_size_; ++i) result[i] ^= buffer[i];
        }
    }

public:
    RAID5Controller(std::vector<sjtu::fstream *> drives, int blocks_per_drive,
                    int block_size = 4096)
        : drives_(drives), blocks_per_drive_(blocks_per_drive),
          block_size_(block_size), num_disks_(static_cast<int>(drives.size())) {}

    void Start(EventType event_type, int drive_id) {
        failed_drive_ = event_type == EventType::FAILED ? drive_id : -1;
        if (event_type == EventType::REPLACED) {
            std::vector<char> buffer(block_size_);
            for (int stripe = 0; stripe < blocks_per_drive_; ++stripe) {
                Reconstruct(drive_id, stripe, buffer.data());
                WritePhysical(drive_id, stripe, buffer.data());
            }
        }
    }

    void Shutdown() {
        for (auto *drive : drives_) {
            if (drive->is_open()) drive->close();
        }
    }

    void ReadBlock(int block_id, char *result) {
        const int stripe = block_id / (num_disks_ - 1);
        const int parity = num_disks_ - 1 - stripe % num_disks_;
        int disk = block_id % (num_disks_ - 1);
        if (disk >= parity) ++disk;
        if (disk == failed_drive_) Reconstruct(disk, stripe, result);
        else ReadPhysical(disk, stripe, result);
    }

    void WriteBlock(int block_id, const char *data) {
        const int stripe = block_id / (num_disks_ - 1);
        const int parity = num_disks_ - 1 - stripe % num_disks_;
        int disk = block_id % (num_disks_ - 1);
        if (disk >= parity) ++disk;

        if (parity != failed_drive_) {
            std::vector<char> updated_parity(block_size_);
            std::vector<char> buffer(block_size_);
            if (disk == failed_drive_) {
                // Recompute parity from the new missing block and surviving data.
                std::copy(data, data + block_size_, updated_parity.begin());
                for (int other = 0; other < num_disks_; ++other) {
                    if (other == disk || other == parity) continue;
                    ReadPhysical(other, stripe, buffer.data());
                    for (int i = 0; i < block_size_; ++i)
                        updated_parity[i] ^= buffer[i];
                }
            } else {
                // P_new = P_old XOR D_old XOR D_new.
                ReadPhysical(parity, stripe, updated_parity.data());
                ReadPhysical(disk, stripe, buffer.data());
                for (int i = 0; i < block_size_; ++i)
                    updated_parity[i] ^= buffer[i] ^ data[i];
            }
            WritePhysical(parity, stripe, updated_parity.data());
        }
        if (disk != failed_drive_) WritePhysical(disk, stripe, data);
    }

    int Capacity() { return (num_disks_ - 1) * blocks_per_drive_; }
};

#endif
