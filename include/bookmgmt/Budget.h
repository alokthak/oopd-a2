#pragma once
// Budget: an overall spending limit plus optional per-category purchase quotas.
//
// A quota caps how many units (copies/seats) and how much money may be spent on
// one category, optionally also limiting distinct resource IDs. Categories with
// no quota are limited only by the total budget.

#include <iosfwd>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "bookmgmt/Money.h"
#include "bookmgmt/Resource.h"

namespace bookmgmt {

struct Quota {
    int maxUnits;    // maximum copies/seats that may be bought
    Money maxSpend;  // maximum money that may be spent
    std::optional<int> maxTitles = std::nullopt; // no title limit by default
};

struct Usage {
    int units = 0;
    Money spent;
    int titles = 0;
};

class Budget {
public:
    explicit Budget(Money total);

    Money total() const { return total_; }
    Money spent() const { return spent_; }
    Money remaining() const { return total_ - spent_; }

    void setQuota(ResourceCategory c, Quota q);
    void removeQuota(ResourceCategory c);
    std::optional<Quota> quotaFor(ResourceCategory c) const;
    Usage usageFor(ResourceCategory c) const;

    // Remaining allowance in a category; nullopt means "no quota set".
    std::optional<int> unitsRemaining(ResourceCategory c) const;
    std::optional<Money> spendRemaining(ResourceCategory c) const;
    std::optional<int> titlesRemaining(ResourceCategory c) const;

    // Returns an empty string if the purchase fits, otherwise the reason it
    // does not. Does not change state.
    std::string check(ResourceCategory c, int units, Money cost,
                      const std::string& resourceId = {}) const;

    // Records a purchase. Throws QuotaExceededError / BudgetExceededError
    // (and changes nothing) if it would not fit.
    // IDs are needed for title limits; legacy calls may omit them otherwise.
    void commit(ResourceCategory c, int units, Money cost, const std::string& resourceId = {});

    // Refund an active purchase using its recorded cost, including tax.
    void refund(ResourceCategory c, int units, Money cost, const std::string& resourceId = {});

    // New allocation + carryPercent of unspent money; copy quotas, reset usage.
    Budget rollover(Money nextYearAllocation, int carryPercent) const;
    std::vector<std::string> warnings() const;

    void print(std::ostream& os) const;

private:
    enum class Failure { None, BadInput, Quota, Overall };
    Failure evaluate(ResourceCategory c, int units, Money cost,
                     const std::string& resourceId, std::string& why) const;
    struct TitleUsage { int units = 0; Money spent; };
    std::map<ResourceCategory, std::map<std::string, TitleUsage>> titleUsage_;

    Money total_;
    Money spent_;
    std::map<ResourceCategory, Quota> quotas_;
    std::map<ResourceCategory, Usage> usage_;
};

}  // namespace bookmgmt
