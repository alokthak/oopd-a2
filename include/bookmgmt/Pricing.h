#pragma once
#include <limits>
#include <stdexcept>
#include "bookmgmt/Money.h"

namespace bookmgmt {
// Non-negative pricing amounts; round half a minor unit upward. Splitting the
// quotient/remainder avoids overflowing an intermediate amount * numerator.
inline Money scalePrice(Money amount, int numerator, int denominator) {
    if (amount.isNegative() || numerator < 0 || denominator <= 0)
        throw std::invalid_argument("invalid price ratio");
    const auto value = amount.minorUnits();
    const auto whole = value / denominator;
    const auto fraction = ((value % denominator) * numerator + denominator / 2) / denominator;
    const auto maximum = std::numeric_limits<std::int64_t>::max();
    if (numerator != 0 && whole > (maximum - fraction) / numerator)
        throw std::overflow_error("scaled price is too large");
    return Money::fromMinor(whole * numerator + fraction);
}
}  // namespace bookmgmt
