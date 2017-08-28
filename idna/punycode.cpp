#include "punycode.h"
#include "iterate_utf16.h"
#include <algorithm>
#include <type_traits>
#include <vector>


// punycode_uint needs to be unsigned and needs to be
// at least 26 bits wide.

typedef uint32_t punycode_uint;

// Bootstring parameters for Punycode
enum {
    base = 36, tmin = 1, tmax = 26, skew = 38, damp = 700,
    initial_bias = 72, initial_n = 0x80, delimiter = 0x2D
};

// basic(cp) tests whether cp is a basic code point:
template <class T>
inline bool basic(T cp) {
    return static_cast<std::make_unsigned<T>::type>(cp) < 0x80;
}

// delim(cp) tests whether cp is a delimiter:
template <class T>
inline bool delim(T cp) {
    return cp == delimiter;
}

// decode_digit(cp) returns the numeric value of a basic code
// point (for use in representing integers) in the range 0 to
// base-1, or base if cp does not represent a value.

inline punycode_uint decode_digit(punycode_uint cp) {
    return cp - 48 < 10 ? cp - 22 : cp - 65 < 26 ? cp - 65 :
        cp - 97 < 26 ? cp - 97 : base;
}

// encode_digit(d) returns the basic code point whose value
// (when used for representing integers) is d, which needs to be in
// the range 0 to base-1. The lowercase form is used.

inline char encode_digit(punycode_uint d) {
    return d + 22 + 75 * (d < 26);
    /*  0..25 map to ASCII a..z */
    /* 26..35 map to ASCII 0..9 */
}

// Platform-specific constants

// maxint is the maximum value of a punycode_uint variable:
static const punycode_uint maxint = -1;

// TODO: or = maxint
static const size_t kCodePointIndexMask = 0x7FF;
static const size_t kMaxCodePoints = kCodePointIndexMask + 1;

typedef uint32_t punycode_item;

inline punycode_item to_punycode_item(punycode_uint cp, size_t ind) {
    return (static_cast<punycode_item>(cp) << 11) | ind;
}
inline punycode_uint get_item_cp(punycode_item item) {
    return static_cast<punycode_uint>(item >> 11);
}
inline punycode_uint get_item_ind(punycode_item item) {
    return static_cast<punycode_uint>(item & kCodePointIndexMask);
}

// Bias adaptation function

static punycode_uint adapt(punycode_uint delta, punycode_uint numpoints, bool firsttime)
{
    punycode_uint k;

    delta = firsttime ? delta / damp : delta >> 1; // delta >> 1 is a faster way of doing delta / 2
    delta += delta / numpoints;

    for (k = 0; delta > ((base - tmin) * tmax) / 2; k += base) {
        delta /= base - tmin;
    }

    return k + (base - tmin + 1) * delta / (delta + skew);
}

// for decoder

inline const char16_t* find_delim(const char16_t* first, const char16_t* last) {
    for (auto it = last; it != first;) {
        if (delim(*--it)) return it;
    }
    return nullptr;
}

inline void appendCodePoint(std::u16string& output, uint32_t cp) {
    if (cp <= 0xFFFF) {
        output.push_back(static_cast<char16_t>(cp));
    } else if (cp <= 0x10FFFF) {
        // http://unicode.org/faq/utf_bom.html#utf16-4
        // https://en.wikipedia.org/wiki/UTF-16#Description
        const uint32_t cc = cp - 0x10000;
        char16_t cu1 = static_cast<char16_t>(0xD800 | (cc >> 10)); // high surrogate
        char16_t cu2 = static_cast<char16_t>(0xDC00 | (cc & 0x03FF)); // low surrogate
        output.push_back(cu1);
        output.push_back(cu2);
    }
}


namespace punycode {

// Main encode function

status encode(std::u16string& output, const char16_t* first, const char16_t* last) {
    // ???
    // The Punycode spec assumes that the input length is the same type
    // of integer as a code point, so we need to convert the size_t to
    // a punycode_uint, which could overflow.

    if (last - first > kMaxCodePoints) // TODO: code points!!!
        return status::overflow;

    // code point/index array
    std::vector<punycode_item> arrCpInd;

    // Handle the basic code points and fill code point/index array
    size_t len0 = output.length();
    size_t ind = 0;
    for (auto it = first; it != last; it++, ind++) {
        const char16_t ch = *it;
        if (basic(ch)) {
            output.push_back(ch);
            // for basic cp's only index-es
            arrCpInd.push_back(to_punycode_item(0, ind));
        } else if (!is_surrogate(ch)) {
            arrCpInd.push_back(to_punycode_item(ch, ind));
        } else if (is_surrogate_lead(ch) && last - it > 1 && is_surrogate_trail(it[1])) {
            const punycode_uint cp = get_suplementary(ch, it[1]);
            arrCpInd.push_back(to_punycode_item(cp, ind));
            it++;
        } else {
            // unmatched surrogate
            return status::bad_input;
        }
    }

    // b is the number of basic code points
    punycode_uint b = output.length() - len0;
    if (b > 0) output.push_back(delimiter);

    // sort items by code point/index
    std::sort(arrCpInd.begin(), arrCpInd.end());

    // Initialize the state:
    punycode_uint n = initial_n;
    punycode_uint delta = 0;
    punycode_uint bias = initial_bias;

    // Main encoding loop:

    // h is the number of code points that have been handled
    std::vector<punycode_item> deltas;
    for (punycode_uint h = b; h < arrCpInd.size();) {
        const punycode_uint m = get_item_cp(arrCpInd[h]);
        const punycode_uint ind = get_item_ind(arrCpInd[h]);
        // end of m code points
        punycode_uint next_h = h + 1;
        while (next_h < arrCpInd.size() && get_item_cp(arrCpInd[next_h]) == m)
            next_h++;
        // fill deltas
        const punycode_uint count_m = next_h - h;
        deltas.assign(count_m, 0);

        // Increase delta enough to advance the decoder's
        // <n,i> state to <m,0>, but guard against overflow:
        deltas[0] = delta + (m - n) * (h + 1); // TODO: overflow

        // calculate deltas
        punycode_uint next_delta = 0;
        for (size_t j = 0; j < h; j++) {
            const punycode_uint i = get_item_ind(arrCpInd[j]);
            if (i < ind) {
                deltas[0]++; // TODO: overflow
            } else {
                // TODO: use binary search
                next_delta++; // gal galima optimaliau?
                for (punycode_uint hh = h + 1; hh < next_h; hh++) {
                    if (i < get_item_ind(arrCpInd[hh])) {
                        deltas[hh - h]++; // TODO: overflow
                        next_delta--;
                        break;
                    }
                }
            }
        }

        // output
        for (const punycode_uint h0 = h; h < next_h; h++) {
            delta = deltas[h - h0];

            // Represent delta as a generalized variable-length integer:
            punycode_uint q = delta;
            for (punycode_uint k = base;; k += base) {
                const punycode_uint t = k <= bias ? tmin :
                    k >= bias + tmax ? tmax : k - bias;
                if (q < t) break;
                output.push_back(encode_digit(t + (q - t) % (base - t)));
                q = (q - t) / (base - t);
            }
            output.push_back(encode_digit(q));
            bias = adapt(delta, h + 1, h == b);
        }

        // initial delta for next char
        delta = next_delta + 1;
        n = m + 1;
    }

    return status::success;
}

// Main decode function

status decode(std::u16string& output, const char16_t* first, const char16_t* last) {

    // code points list
    std::vector<punycode_item> arrCpPtr;

    // Handle the basic code points:  Let b be the number of input code
    // points before the last delimiter, or 0 if there is none, then
    // copy the first b code points to the output.

    auto pb = find_delim(first, last);
    if (pb) {
        // has delimiter, but hasn't basic code points
        if (pb == first) return status::bad_input;
        if (pb - first > kMaxCodePoints)
            return status::big_output;

        // basic code points to list
        size_t ptr = 0;
        for (auto it = first; it != pb; it++) {
            const auto ch = *it;
            if (!basic(ch)) return status::bad_input;
            // ++ptr - index to the next char in the list
            arrCpPtr.push_back(to_punycode_item(ch, ++ptr));
        }
        // skip delimiter
        first = pb + 1;
    }

    // Initialize the state:

    punycode_uint n = initial_n;
    punycode_uint out = arrCpPtr.size(); // basic code points count
    punycode_uint i = 0;
    punycode_uint bias = initial_bias;

    // Main decoding loop:  Start just after the last delimiter if any
    // basic code points were copied; start at the beginning otherwise.

    size_t start_ptr = 0;
    for (auto inp = first; inp != last; ++out) {

        // Decode a generalized variable-length integer into delta,
        // which gets added to i.  The overflow checking is easier
        // if we increase i as we go, then subtract off its starting
        // value at the end to obtain delta.

        const punycode_uint oldi = i;
        punycode_uint w = 1;
        for (punycode_uint k = base;; k += base) {
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
        // output[i++] = n;

        if (out >= kMaxCodePoints)
            return status::big_output;

        if (i == 0) {
            arrCpPtr.push_back(to_punycode_item(n, start_ptr));
            start_ptr = arrCpPtr.size() - 1; // index of just inserted
        } else {
            const size_t ptr = arrCpPtr.size();
            size_t prev_ptr = start_ptr;
            for (punycode_uint count = 1; count < i; count++) {
                prev_ptr = get_item_ind(arrCpPtr[prev_ptr]);
            }
            // insert
            const size_t next_ptr = get_item_ind(arrCpPtr[prev_ptr]);
            arrCpPtr.push_back(to_punycode_item(n, next_ptr));
            arrCpPtr[prev_ptr] = to_punycode_item(get_item_cp(arrCpPtr[prev_ptr]), ptr);
        }
        i++;
    }

    // convert list to utf-16
    size_t ptr = start_ptr;
    for (size_t count = arrCpPtr.size(); count > 0; count--) {
        appendCodePoint(output, get_item_cp(arrCpPtr[ptr]));
        ptr = get_item_ind(arrCpPtr[ptr]);
    }

    return status::success;
}


} // namespace punycode
