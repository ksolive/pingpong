#pragma once

#include <cstdint>
#include <string>
#include <cassert>
#include <random>
#include <sstream>
#include <ostream>
#include <cstring>

#define PKT_SIZE 288  // TODO
#define MAC_SIZE 16  // TODO
#define DUMMY_KEY "aaaaaaaaaaaaaaaa"  // TODO

struct uint128_t {
    uint64_t high;
    uint64_t low;
    uint128_t() :high(0), low(0) {
    }
    uint128_t(uint64_t h, uint64_t l) :high(h), low(l) {
    }
    bool operator==(const uint128_t& other) {
        return other.high == this->high && other.low == this->low;
    }
    bool operator!=(const uint128_t& other) {
        return other.high != this->high || other.low != this->low;
    }
};

#pragma pack(push,1)
struct PktHead
{
    uint64_t signal; // hash(counter|id1|id2)
    bool read_write;
    bool is_dummy;
    bool is_exceed;
    uint8_t enode_id;
    uint32_t from;
    PktHead() :signal(0), read_write(true), is_dummy(false), is_exceed(false), enode_id(0) {
    }

    PktHead(const std::string& s) {
        assert(s.size() == sizeof(PktHead));
        memcpy(reinterpret_cast<char*>(this), s.data(), sizeof(PktHead));
    }
    operator std::string() {
        return std::string(reinterpret_cast<char*>(this), reinterpret_cast<char*>(this) + sizeof(PktHead));
    }
    uint64_t get_digest() {
        return signal ^ read_write ^ is_dummy ^ is_exceed ^ enode_id;
    }


};
struct Pkt {
    PktHead head;
    uint8_t content[PKT_SIZE - MAC_SIZE - sizeof(PktHead)]{};  // TODO
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
};
#pragma pack(pop)
