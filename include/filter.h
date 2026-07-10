#ifndef ATG_ENGINE_SIM_FILTER_H
#define ATG_ENGINE_SIM_FILTER_H

// PORT NOTE (M0 macOS bring-up): __forceinline is an MSVC-only keyword, used
// by several Filter subclasses (low_pass_filter.h, butterworth_low_pass_filter.h,
// jitter_filter.h, preemphasis_filter.h); a plain `inline` is a safe portable
// substitute (it's only ever a hint either way). Defined here since every one
// of those headers includes this one.
#if !defined(_WIN32)
#define __forceinline inline
#endif

class Filter {
    public:
        Filter();
        virtual ~Filter();

        virtual float f(float sample);
        virtual void destroy();
};

#endif /* ATG_ENGINE_SIM_FILTER_H */
