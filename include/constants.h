#ifndef ATG_ENGINE_SIM_CONSTANTS_H
#define ATG_ENGINE_SIM_CONSTANTS_H

// PORT NOTE (M0 macOS bring-up): these were declared `extern constexpr`,
// which -- since they are not `inline` -- makes each translation unit that
// includes this header emit its own definition of the same externally
// linked symbol (constexpr requires an initializer wherever it's declared,
// and `extern` forces external rather than the usual internal linkage for a
// non-inline const/constexpr namespace-scope variable). That's an ODR
// violation: Apple's linker (unlike Windows') rejects it outright as a
// duplicate symbol at link time. `inline constexpr` (C++17) is the correct,
// portable idiom for a header-defined constant used across many
// translation units -- exactly one definition is kept regardless of how
// many TUs include the header.
namespace constants {

    inline constexpr double pi = 3.14159265359;
    inline constexpr double R = 8.31446261815324;
    inline constexpr double root_2 = 1.41421356237309504880168872420969807856967187537694807317667973799;
    inline constexpr double e = 2.71828182845904523536028747135266249775724709369995;

} /* namespace Constants */

#endif /* ATG_ENGINE_SIM_CONSTANTS_H */
