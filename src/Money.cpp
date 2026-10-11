#include "bookmgmt/Money.h"

#include <ostream>
#include <stdexcept>

namespace bookmgmt {

Money::Money(std::int64_t minor, const std::string& currency) : minor_(minor), currency_(currency) {
    if (currency_.size() != 3)
        throw std::invalid_argument("currency code must contain three uppercase letters");
    for (char ch : currency_)
        if (ch < 'A' || ch > 'Z')
            throw std::invalid_argument("currency code must contain three uppercase letters");
}

void Money::requireSameCurrency(const Money& other) const {
    if (currency_ != other.currency_)
        throw CurrencyMismatchError("currency mismatch: " + currency_ + " and " + other.currency_);
}

Money Money::fromMinor(std::int64_t minor, const std::string& currency) {
    return Money(minor, currency);
}

Money Money::of(std::int64_t major, int minor, const std::string& currency) {
    if (minor < 0 || minor > 99)
        throw std::invalid_argument("minor part must be in 0..99");
    const std::int64_t sign = major < 0 ? -1 : 1;
    return Money(major * 100 + sign * minor, currency);
}

std::string Money::toString() const {
    const std::int64_t absMinor = minor_ < 0 ? -minor_ : minor_;
    std::string s = std::to_string(absMinor / 100);
    const auto frac = absMinor % 100;
    s += '.';
    if (frac < 10) s += '0';
    s += std::to_string(frac);
    return minor_ < 0 ? "-" + s : s;
}

std::ostream& operator<<(std::ostream& os, Money m) { return os << m.toString(); }

}  // namespace bookmgmt
