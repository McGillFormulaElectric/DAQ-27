#pragma once

#include <iostream>

namespace sil_check {
inline void record(bool result, int& failures, const char* function, int line, const char* expression) {
    if (!result) {
        std::cerr << function << ':' << line << " CHECK failed: " << expression << '\n';
        ++failures;
    }
}

inline void xfail_if(bool observed, int& failures, int& known_defects,
                     const char* id, const char* defect) {
    if (!observed) {
        ++failures;
        std::cerr << "XPASS " << id << ' ' << defect << " (retire marker and CHAR)\n";
        return;
    }
    ++known_defects;
    std::cout << "XFAIL " << id << ' ' << defect << '\n';
}
}

#define CHECK(expression) \
    do { ::sil_check::record(static_cast<bool>(expression), failures, __func__, __LINE__, #expression); } while (false)
