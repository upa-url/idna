// Copyright 2017-2024 Rimas Misevičius
// Distributed under the BSD-style license that can be
// found in the LICENSE file.
//
#include "upa/idna/punycode.h"
#include <algorithm>
#include <type_traits>

namespace upa {
namespace idna {
namespace punycode {

namespace {

// punycode_uint needs to be unsigned and needs to be
// at least 26 bits wide.

using punycode_uint = std::uint32_t;

// Bootstring parameters for Punycode

constexpr punycode_uint base = 36;
constexpr punycode_uint tmin = 1;
constexpr punycode_uint tmax = 26;
constexpr punycode_uint skew = 38;
constexpr punycode_uint damp = 700;
constexpr punycode_uint initial_bias = 72;
constexpr punycode_uint initial_n = 0x80;
constexpr char delimiter = 0x2D;

// basic(cp) tests whether cp is a basic code point:
template <class T>
constexpr bool basic(T cp) {
    return static_cast<typename std::make_unsigned<T>::type>(cp) < 0x80;
}

// decode_digit(cp) returns the numeric value of a basic code
// point (for use in representing integers) in the range 0 to
// base-1, or base if cp does not represent a value.

constexpr punycode_uint decode_digit(punycode_uint cp) {
    return cp - 48 < 10 ? cp - 22 : cp - 65 < 26 ? cp - 65 :
        cp - 97 < 26 ? cp - 97 : base;
}

// encode_digit(d) returns the basic code point whose value
// (when used for representing integers) is d, which needs to be in
// the range 0 to base-1. The lowercase form is used.

constexpr char encode_digit(punycode_uint d) {
    return d + 22 + 75 * (d < 26);
    /*  0..25 map to ASCII a..z */
    /* 26..35 map to ASCII 0..9 */
}

// Platform-specific constants

// maxint is the maximum value of a punycode_uint variable:
constexpr punycode_uint maxint = -1;
constexpr std::size_t kMaxCodePoints = maxint;

// Bias adaptation function

inline punycode_uint adapt(punycode_uint delta, punycode_uint numpoints, bool firsttime)
{
    delta = firsttime ? delta / damp : delta >> 1; // delta >> 1 is a faster way of doing delta / 2
    delta += delta / numpoints;

    punycode_uint k = 0;
    while (delta > ((base - tmin) * tmax) / 2) {
        delta /= base - tmin;
        k += base;
    }

    return k + (base - tmin + 1) * delta / (delta + skew);
}

// for decoder

template <typename CharT>
inline const CharT* find_delim(const CharT* first, const CharT* last) {
    for (auto it = last; it != first;) {
        if (*--it == delimiter) return it;
    }
    return nullptr;
}

} // namespace


// Main encode function

status encode(std::string& output, const char32_t* first, const char32_t* last) {

    // The Punycode spec assumes that the input length is the same type
    // of integer as a code point, so we need to convert the size_t to
    // a punycode_uint, which could overflow.

    if (last - first > kMaxCodePoints)
        return status::overflow;

    const auto input_length = static_cast<punycode_uint>(last - first);

    // Handle the basic code points:

    const std::size_t len0 = output.length();
    output.reserve(len0 + input_length);
    for (auto it = first; it != last; ++it) {
        const auto ch = *it;
        if (basic(ch)) {
            output.push_back(static_cast<char>(ch));
        } else if (ch > 0x10FFFF) {
            // invalid codepoint
            return status::bad_input;
        }
    }

    // b is the number of basic code points
    const auto b = static_cast<punycode_uint>(output.length() - len0);
    if (b > 0) output.push_back(delimiter);

    // Initialize the state:

    punycode_uint n = initial_n;
    punycode_uint delta = 0;
    punycode_uint bias = initial_bias;

    // Main encoding loop:

    // h is the number of code points that have been handled
    for (punycode_uint h = b; h < input_length;) {
        // All non-basic code points < n have been
        // handled already. Find the next larger one:
        punycode_uint m = maxint;
        for (auto it = first; it != last; ++it) {
            const auto ch = *it;
            if (ch >= n && ch < m) m = ch;
        }

        // Increase delta enough to advance the decoder's
        // <n,i> state to <m,0>, but guard against overflow:
        if (m - n > (maxint - delta) / (h + 1))
            return status::overflow;
        delta += (m - n) * (h + 1);
        n = m;

        for (auto it = first; it != last; ++it) {
            const auto ch = *it;
            if (ch < n) {
                if (++delta == 0)
                    return status::overflow;
            }
            if (ch == n) {
                // Represent delta as a generalized variable-length integer:
                punycode_uint q = delta;
                for (punycode_uint k = base; ; k += base) {
                    const punycode_uint t = k <= bias ? tmin :
                        k >= bias + tmax ? tmax : k - bias;
                    if (q < t) break;
                    output.push_back(encode_digit(t + (q - t) % (base - t)));
                    q = (q - t) / (base - t);
                }

                output.push_back(encode_digit(q));
                bias = adapt(delta, h + 1, h == b);
                delta = 0;
                ++h;
            }
        }

        ++delta, ++n;
    }
    return status::success;
}

// Main decode function

status decode(std::u32string& output, const char32_t* first, const char32_t* last) {

    // Handle the basic code points:  Let b be the number of input code
    // points before the last delimiter, or 0 if there is none, then
    // copy the first b code points to the output.

    auto bp = find_delim(first, last);
    if (bp) {
        // has delimiter, but hasn't basic code points
        if (bp == first) return status::bad_input;
        if (bp - first > kMaxCodePoints)
            return status::big_output;
    }
    const std::size_t len0 = output.length();
    output.reserve(len0 + (last - first));
    if (bp) {
        // append basic code points to output
        for (auto it = first; it != bp; ++it) {
            const auto ch = *it;
            if (!basic(ch)) return status::bad_input;
            output.push_back(ch);
        }
        // skip delimiter
        first = bp + 1;
    }

    // Initialize the state:

    punycode_uint n = initial_n;
    punycode_uint out = static_cast<punycode_uint>(output.length() - len0); // basic code points count
    punycode_uint i = 0;
    punycode_uint bias = initial_bias;

    // Main decoding loop:

    for (auto inp = first; inp != last; ++out) {
        // in is the index of the next ASCII code point to be consumed,
        // and out is the number of code points in the output array.

        // Decode a generalized variable-length integer into delta,
        // which gets added to i.  The overflow checking is easier
        // if we increase i as we go, then subtract off its starting
        // value at the end to obtain delta.

        const punycode_uint oldi = i;
        punycode_uint w = 1;
        for (punycode_uint k = base; ; k += base) {
            if (inp == last) return status::bad_input;
            const punycode_uint digit = decode_digit(*inp++);
            if (digit >= base) return status::bad_input;
            if (digit > (maxint - i) / w) return status::overflow;
            i += digit * w;
            const punycode_uint t = k <= bias ? tmin :
                k >= bias + tmax ? tmax : k - bias;
            if (digit < t) break;
            if (w > maxint / (base - t)) return status::overflow;
            w *= (base - t);
        }

        bias = adapt(i - oldi, out + 1, oldi == 0);

        // i was supposed to wrap around from out+1 to 0,
        // incrementing n each time, so we'll fix that now:

        if (i / (out + 1) > maxint - n) return status::overflow;
        n += i / (out + 1);
        i %= (out + 1);

        // Insert n at position i of the output:
        
        if (out >= kMaxCodePoints)
            return status::big_output;

        output.insert(len0 + i, 1, n);
        //output.insert(output.begin() + len0 + i, n);
        ++i;
    }
    return status::success;
}


} // namespace punycode
} // namespace idna
} // namespace upa
