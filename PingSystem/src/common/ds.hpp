#pragma once

#include <cstdint>
#include <string>
#include <cassert>
#include <random>
#include <sstream>
#include <ostream>
#include <cstring>
#include <unistd.h>

// #define PKT_SIZE 256  // TODO
// 65 + 44 + 16 + 8 // 512
// 126 + 44 + 16 + 8 // 1000
// 251 + 44 + 16 + 8 // 2000
// 376 + 44 + 16 + 8 // 3000
// 501 + 44 + 16 + 8 // 4000
// 626 + 44 + 16 + 8 // 5000

// #define PKT_SIZE 694 // new?
#define PKT_SIZE 694
#define MAX_BUDDY 5000 //new 
#define MAC_SIZE 16  // TODO
#define DUMMY_KEY "This is a dummy key"  // TODO

struct uint256_t{
    uint64_t highest;
    uint64_t midhigh;
    uint64_t midlow;
    uint64_t lowest;

    uint256_t(uint64_t highest, uint64_t midhigh, uint64_t midlow, uint64_t lowest) : 
        highest(highest), midhigh(midhigh), midlow(midlow), lowest(lowest){
    }
    uint256_t() : 
        highest(0), midhigh(0), midlow(0), lowest(0){
    }
    bool operator==(const uint256_t& other) {
        return other.highest == this->highest && other.midhigh == this->midhigh && other.midlow == this->midlow&& other.lowest == this->lowest;
    }
    bool operator!=(const uint256_t& other) {
        return other.highest != this->highest && other.midhigh != this->midhigh && other.midlow != this->midlow&& other.lowest != this->lowest;
    }
};

#pragma pack(push,1)

struct PktConfig{
    uint32_t from;
    uint32_t to;
    uint32_t notf;
    bool is_sentail;
};

struct PktHead
{
    uint256_t dd_id;
    uint32_t to_R;
    uint32_t from_S;
    uint8_t enode_id;
    uint8_t bnode_id;
    uint8_t tag;
    uint8_t is_sential;
    PktHead() :dd_id(0,0,0,0), to_R(0), from_S(0), enode_id(0), bnode_id(0), tag(0), is_sential(0) {
    }

    PktHead(const std::string& s) {
        assert(s.size() == sizeof(PktHead));
        memcpy(reinterpret_cast<char*>(this), s.data(), sizeof(PktHead));
    }
    operator std::string() {
        return std::string(reinterpret_cast<char*>(this), reinterpret_cast<char*>(this) + sizeof(PktHead));
    }
    uint64_t get_digest() {
        return dd_id.highest ^ dd_id.midhigh ^ dd_id.midlow ^ dd_id.lowest ^ to_R ^ from_S ^ enode_id ^ bnode_id ^ tag ^ is_sential;
    }
    void set_dd_id(uint32_t seed){
        std::default_random_engine e(seed);
        std::uniform_int_distribution<uint64_t> u(0, UINT64_MAX);
        uint64_t id_higest = u(e);
        uint64_t id_lowest = u(e);
        uint64_t id_midhigh = u(e);
        uint64_t id_midlow = u(e);
        uint256_t id = uint256_t(id_higest, id_midhigh, id_midlow, id_lowest);
        dd_id = id;
        return;
    }
};


struct Pkt {
    PktHead head;
    uint8_t notf[MAX_BUDDY / 8 + 1]{0};
    uint8_t content[PKT_SIZE - MAC_SIZE - sizeof(PktHead) - sizeof(notf)]{};  // TODO
    Pkt() {
        memset(this, 0, sizeof(Pkt));
    }
    Pkt(uint8_t* data_p, size_t size) {
        memcpy(this, data_p, size);
    }

    uint64_t get_digest() {
        uint64_t ret = 0;
        uint64_t tmp = 0;
        for (size_t k = 0;k < sizeof(content);++k) {
            tmp &= content[k];
            tmp << 8;
            if (k % sizeof(uint64_t) == 0) {
                ret ^= tmp;
                tmp = 0;
            }
        }
        ret ^= head.get_digest();
        return  ret;
    }

    void set_notf(uint32_t int_notf){
        notf[int_notf / 8] |= (1 << (int_notf % 8));
    }

    std::vector<uint32_t> retrieve_notf_from_bit(){
        std::vector<uint32_t> ret = {};
        for(int i = 0; i < sizeof(notf); i++){
            for(int j = 0; j < 8; j++){
                if((notf[i] >> j) & 1){
                    ret.push_back(8 * i + j);
                }
            }
        }
        return ret;
    }

    bool check_notf_is_one(){
        int size = sizeof(notf) / sizeof(notf[0]);
        bool check_flag = true;
        for(int i = 0; i < size; i++){
            for(int j = 0; j < 8; j++){
                if(notf[i] >> j & 1){
                    if(check_flag == true){
                        check_flag = false;
                    } else {
                        return false;
                    }
                }
            }
        }
        return true;
    }
};
#pragma pack(pop)