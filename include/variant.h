#pragma once

#include <cstdint>

namespace logF {

// 紧凑的LogVariant，使用packed减少大小
struct __attribute__((packed)) LogVariant {
    enum Type : uint8_t {
        INT = 0,
        DOUBLE = 1, 
        CSTR = 2
    };
    
    union {
        int32_t i;
        double d;
        const char* s;
    } data;
    
    Type type;
    
    LogVariant() : type(INT) { data.i = 0; }
    LogVariant(int val) : type(INT) { data.i = val; }
    LogVariant(long val) : type(INT) { data.i = static_cast<int32_t>(val); }
    LogVariant(double val) : type(DOUBLE) { data.d = val; }
    LogVariant(const char* val) : type(CSTR) { data.s = val; }
    
    // 访问方法
    int32_t as_int() const { return data.i; }
    double as_double() const { return data.d; }
    const char* as_cstr() const { return data.s; }
    Type get_type() const { return type; }
};

}