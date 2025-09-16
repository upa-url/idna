# Upa IDNA

Upa IDNA is the [Unicode IDNA Compatibility Processing (UTS #46)](https://www.unicode.org/reports/tr46/) C++ library. It is compliant with the 17.0.0 version of the Unicode standard.

This library implements two functions from [UTS #46](https://www.unicode.org/reports/tr46/): [`to_ascii`](https://www.unicode.org/reports/tr46/#ToASCII) and [`to_unicode`](https://www.unicode.org/reports/tr46/#ToUnicode), and two functions from the [WHATWG URL Standard](https://url.spec.whatwg.org/): [`domain_to_ascii`](https://url.spec.whatwg.org/#concept-domain-to-ascii) and [`domain_to_unicode`](https://url.spec.whatwg.org/#concept-domain-to-unicode). It has no dependencies and requires C++17 or later.

## License

This library is licensed under the [BSD 2-Clause License](https://opensource.org/license/bsd-2-clause/).
