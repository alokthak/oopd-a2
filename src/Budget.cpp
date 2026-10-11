#include "bookmgmt/Budget.h"
#include "bookmgmt/Pricing.h"
#include <limits>

#include <iomanip>
#include <ostream>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

namespace {
// Every category, in the order Budget::print() lists them.
const ResourceCategory kAllCategories[] = {ResourceCategory::Book,
                                           ResourceCategory::ElectronicResource,
                                           ResourceCategory::Journal,
                                           ResourceCategory::EBook,
                                           ResourceCategory::AudioBook,
                                           ResourceCategory::Thesis};
}

Budget::Budget(Money total) : total_(total), spent_(Money::fromMinor(0, total.currency())) {
    if (total_.isNegative()) throw std::invalid_argument("budget must not be negative");
}

void Budget::setQuota(ResourceCategory c, Quota q) {
    if (q.maxUnits < 0 || q.maxSpend.isNegative() || (q.maxTitles && *q.maxTitles < 0))
        throw std::invalid_argument("quota limits must not be negative");
    if (q.maxSpend.currency() != total_.currency())
        throw CurrencyMismatchError("quota and budget currencies differ");
    quotas_[c] = q;
}

void Budget::removeQuota(ResourceCategory c) { quotas_.erase(c); }

std::optional<Quota> Budget::quotaFor(ResourceCategory c) const {
    auto it = quotas_.find(c);
    if (it == quotas_.end()) return std::nullopt;
    return it->second;
}

Usage Budget::usageFor(ResourceCategory c) const {
    auto it = usage_.find(c);
    return it == usage_.end() ? Usage{0, Money::fromMinor(0, total_.currency()), 0} : it->second;
}

std::optional<int> Budget::unitsRemaining(ResourceCategory c) const {
    auto q = quotaFor(c);
    if (!q) return std::nullopt;
    return q->maxUnits - usageFor(c).units;
}

std::optional<Money> Budget::spendRemaining(ResourceCategory c) const {
    auto q = quotaFor(c);
    if (!q) return std::nullopt;
    return q->maxSpend - usageFor(c).spent;
}

Budget::Failure Budget::evaluate(ResourceCategory c, int units, Money cost,
                                 const std::string& resourceId, std::string& why) const {
    if (cost.currency() != total_.currency()) {
        why = "purchase and budget currencies differ";
        return Failure::BadInput;
    }
    if (units <= 0) {
        why = "quantity must be positive";
        return Failure::BadInput;
    }
    if (cost.isNegative()) {
        why = "cost must not be negative";
        return Failure::BadInput;
    }
    if (auto left = unitsRemaining(c); left && units > *left) {
        why = std::string(categoryName(c)) + " unit quota exceeded: requested " +
              std::to_string(units) + ", " + std::to_string(*left) + " remaining";
        return Failure::Quota;
    }
    if (auto left = spendRemaining(c); left && cost > *left) {
        why = std::string(categoryName(c)) + " spend quota exceeded: cost " +
              cost.toString() + ", " + left->toString() + " remaining";
        return Failure::Quota;
    }
    const auto quota = quotaFor(c);
    if (quota && quota->maxTitles) {
        if (resourceId.empty()) {
            why = "resource id required for a title quota";
            return Failure::BadInput;
        }
        const auto category = titleUsage_.find(c);
        const bool existing = category != titleUsage_.end() && category->second.count(resourceId);
        if (!existing && usageFor(c).titles >= *quota->maxTitles) {
            why = std::string(categoryName(c)) + " title quota exceeded";
            return Failure::Quota;
        }
    }
    if (cost > remaining()) {
        why = "overall budget exceeded: cost " + cost.toString() + ", " +
              remaining().toString() + " remaining";
        return Failure::Overall;
    }
    why.clear();
    return Failure::None;
}

std::string Budget::check(ResourceCategory c, int units, Money cost,
                          const std::string& resourceId) const {
    std::string why;
    evaluate(c, units, cost, resourceId, why);
    return why;
}

void Budget::commit(ResourceCategory c, int units, Money cost, const std::string& resourceId) {
    std::string why;
    switch (evaluate(c, units, cost, resourceId, why)) {
        case Failure::None: break;
        case Failure::BadInput: throw std::invalid_argument(why);
        case Failure::Quota: throw QuotaExceededError(why);
        case Failure::Overall: throw BudgetExceededError(why);
    }
    Usage& u = usage_.try_emplace(c, Usage{0, Money::fromMinor(0, total_.currency()), 0}).first->second;
    u.units += units;
    u.spent += cost;
    spent_ += cost;
    auto& titles = titleUsage_[c];
    if (!resourceId.empty() && !titles.count(resourceId)) ++u.titles;
    auto& title = titles.try_emplace(resourceId,
        TitleUsage{0, Money::fromMinor(0, total_.currency())}).first->second;
    title.units += units;
    title.spent += cost;
}

std::optional<int> Budget::titlesRemaining(ResourceCategory c) const {
    const auto q = quotaFor(c);
    if (!q || !q->maxTitles) return std::nullopt;
    return *q->maxTitles - usageFor(c).titles;
}

void Budget::refund(ResourceCategory c, int units, Money cost, const std::string& resourceId) {
    if (cost.currency() != total_.currency())
        throw CurrencyMismatchError("refund and budget currencies differ");
    const auto category = titleUsage_.find(c);
    if (units <= 0 || cost.isNegative() || category == titleUsage_.end())
        throw std::invalid_argument("invalid refund");
    const auto title = category->second.find(resourceId);
    if (title == category->second.end() || units > title->second.units || cost > title->second.spent
        || (units == title->second.units && cost != title->second.spent))
        throw std::invalid_argument("refund exceeds or disagrees with active purchases");
    auto& u = usage_.at(c);
    u.units -= units;
    u.spent -= cost;
    spent_ -= cost;
    title->second.units -= units;
    title->second.spent -= cost;
    if (title->second.units == 0) {
        if (!resourceId.empty()) --u.titles;
        category->second.erase(title);
    }
}

Budget Budget::rollover(Money nextYearAllocation, int carryPercent) const {
    if (nextYearAllocation.isNegative() || carryPercent < 0 || carryPercent > 100)
        throw std::invalid_argument("rollover requires non-negative allocation and 0..100 percent");
    if (nextYearAllocation.currency() != total_.currency())
        throw CurrencyMismatchError("rollover allocation and current budget currencies differ");
    const Money carry = scalePrice(remaining(), carryPercent, 100);
    if (nextYearAllocation.minorUnits() > std::numeric_limits<std::int64_t>::max() - carry.minorUnits())
        throw std::overflow_error("next year's budget is too large");
    Budget next(nextYearAllocation + carry);
    next.quotas_ = quotas_;
    return next;
}

void Budget::print(std::ostream& os) const {
    os << "Budget: total " << total_ << ", spent " << spent_ << ", remaining "
       << remaining() << "\nCurrency: " << total_.currency() << "\n";
    os << std::left << std::setw(22) << "  Category" << std::setw(18) << "Units used/max"
       << std::setw(24) << "Spend used/max" << "Titles used/max\n";
    for (ResourceCategory c : kAllCategories) {
        const Usage u = usageFor(c);
        const auto q = quotaFor(c);
        const std::string units =
            std::to_string(u.units) + "/" + (q ? std::to_string(q->maxUnits) : "-");
        const std::string spend =
            u.spent.toString() + "/" + (q ? q->maxSpend.toString() : "-");
        const std::string titles = std::to_string(u.titles) + "/" +
            (q && q->maxTitles ? std::to_string(*q->maxTitles) : "-");
        os << "  " << std::setw(20) << categoryName(c) << std::setw(18) << units << std::setw(24) << spend << titles
           << "\n";
    }
}

}  // namespace bookmgmt
