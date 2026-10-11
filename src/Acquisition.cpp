#include "bookmgmt/Acquisition.h"
#include "bookmgmt/Pricing.h"
#include <limits>
#include <algorithm>

#include <iomanip>
#include <ostream>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

AcquisitionManager::AcquisitionManager(Catalog& catalog, Budget& budget, TaxRates taxes)
    : catalog_(catalog), budget_(budget) {
    setTaxRates(taxes);
}

void AcquisitionManager::addDepartment(const std::string& name, Budget& budget) {
    if (name.empty() || departments_.count(name) || &budget == &budget_)
        throw std::invalid_argument("department name must be unique and budget independent");
    for (const auto& entry : departments_)
        if (entry.second == &budget)
            throw std::invalid_argument("department budgets must be independent");
    departments_.emplace(name, &budget);
}

Budget& AcquisitionManager::departmentBudget(const std::string& name) const {
    if (name.empty()) return budget_;
    const auto found = departments_.find(name);
    if (found == departments_.end()) throw std::invalid_argument("unknown department: " + name);
    return *found->second;
}

void AcquisitionManager::setTaxRates(TaxRates taxes) {
    if (taxes.printBasisPoints < 0 || taxes.electronicBasisPoints < 0)
        throw std::invalid_argument("tax rates must not be negative");
    taxes_ = taxes;
}

Money AcquisitionManager::taxedPrice(const Resource& resource, Money preTaxCost) const {
    const int rate = resource.isDigital() ? taxes_.electronicBasisPoints : taxes_.printBasisPoints;
    const Money tax = scalePrice(preTaxCost, rate, 10000);
    if (preTaxCost.minorUnits() > std::numeric_limits<std::int64_t>::max() - tax.minorUnits())
        throw std::overflow_error("post-tax price is too large");
    return preTaxCost + tax;
}

Money AcquisitionManager::quote(const std::string& id, int quantity) const {
    const Resource& r = catalog_.get(id);
    return taxedPrice(r, r.costFor(quantity));
}

Money AcquisitionManager::quoteBeforeTax(const std::string& id, int quantity) const {
    return catalog_.get(id).costFor(quantity);
}

bool AcquisitionManager::canPurchase(const std::string& id, int quantity,
                                     std::string* reason, const std::string& department) const {
    std::string why;
    if (const Resource* r = catalog_.find(id)) {
        if (quantity <= 0)
            why = "quantity must be positive";
        else {
            try {
                why = departmentBudget(department).check(r->category(), quantity,
                    taxedPrice(*r, r->costFor(quantity)), id);
            } catch (const std::invalid_argument& error) {
                why = error.what();
            }
        }
    } else {
        why = "resource not found: " + id;
    }
    if (reason) *reason = why;
    return why.empty();
}

PurchaseRecord& AcquisitionManager::record(const Resource* r, const std::string& id,
                                           int qty, Money cost, bool approved,
                                           std::string reason, Money preTaxCost,
                                           const std::string& department) {
    history_.push_back(PurchaseRecord{
        nextOrderNo_++, id, r ? r->title() : std::string("(unknown)"),
        r ? r->category() : ResourceCategory::Book, qty, cost, approved,
        std::move(reason), preTaxCost, department});
    return history_.back();
}

const PurchaseRecord& AcquisitionManager::purchase(const std::string& id, int quantity,
                                                   const std::string& department) {
    const Resource& r = catalog_.get(id);        // may throw NotFoundError
    const Money preTaxCost = r.costFor(quantity); // may throw invalid_argument
    const Money cost = taxedPrice(r, preTaxCost);
    departmentBudget(department).commit(r.category(), quantity, cost, id);  // may throw quota/budget errors
    catalog_.addHoldings(id, quantity);
    return record(&r, id, quantity, cost, true, {}, preTaxCost, department);
}

const PurchaseRecord& AcquisitionManager::cancel(int orderNo) {
    const auto original = std::find_if(history_.begin(), history_.end(),
        [orderNo](const PurchaseRecord& r) { return r.orderNo == orderNo; });
    if (original == history_.end() || !original->approved || original->cancellation)
        throw std::invalid_argument("only approved purchase orders can be cancelled");
    for (const auto& r : history_)
        if (r.cancellation && r.cancelledOrderNo == orderNo)
            throw std::invalid_argument("order already cancelled");
    // Copy before appending: vector growth may invalidate the original iterator.
    const PurchaseRecord purchase = *original;
    if (catalog_.holdings(purchase.resourceId) < purchase.quantity)
        throw std::invalid_argument("not enough holdings to cancel this order");
    Budget& charged = departmentBudget(purchase.department);
    // Validate/refund on a copy first, so a rejected cancellation changes nothing.
    Budget refunded = charged;
    refunded.refund(purchase.category, purchase.quantity, purchase.cost, purchase.resourceId);
    PurchaseRecord cancellation = purchase;
    cancellation.orderNo = nextOrderNo_;
    cancellation.cancellation = true;
    cancellation.cancelledOrderNo = orderNo;
    cancellation.reason.clear();
    history_.push_back(std::move(cancellation));
    charged = std::move(refunded);
    catalog_.addHoldings(purchase.resourceId, -purchase.quantity);
    ++nextOrderNo_;
    return history_.back();
}

std::vector<PurchaseRecord> AcquisitionManager::processBatch(
    const std::vector<PurchaseRequest>& reqs) {
    std::vector<PurchaseRecord> results;
    results.reserve(reqs.size());
    for (const auto& req : reqs) {
        const Resource* r = catalog_.find(req.resourceId);
        Money cost;
        Money preTaxCost;
        std::string why;
        if (!r) {
            why = "resource not found: " + req.resourceId;
        } else if (req.quantity <= 0) {
            why = "quantity must be positive";
        } else {
            preTaxCost = r->costFor(req.quantity);
            cost = taxedPrice(*r, preTaxCost);
            try {
                why = departmentBudget(req.department).check(r->category(), req.quantity,
                                                              cost, req.resourceId);
            } catch (const std::invalid_argument& error) {
                why = error.what();
            }
        }

        if (why.empty()) {
            results.push_back(purchase(req.resourceId, req.quantity, req.department));
        } else {
            results.push_back(record(r, req.resourceId, req.quantity, cost, false, why, preTaxCost, req.department));
        }
    }
    return results;
}

Money AcquisitionManager::totalSpent() const {
    Money sum;
    for (const auto& rec : history_)
        if (rec.approved) {
            if (rec.cancellation) sum -= rec.cost;
            else sum += rec.cost;
        }
    return sum;
}

Money AcquisitionManager::totalBeforeTax() const {
    Money sum;
    for (const auto& rec : history_)
        if (rec.approved) {
            if (rec.cancellation) sum -= rec.preTaxCost;
            else sum += rec.preTaxCost;
        }
    return sum;
}

void AcquisitionManager::printReport(std::ostream& os) const {
    os << "Order history (" << history_.size() << " orders)\n";
    for (const auto& rec : history_) {
        os << "  #" << std::setw(3) << std::left << rec.orderNo << " "
           << (rec.cancellation ? "CANCELLED" : (rec.approved ? "APPROVED" : "REJECTED")) << "  " << std::setw(6)
           << rec.resourceId << " x" << std::setw(3) << rec.quantity << " "
           << std::setw(12) << std::right << rec.cost.toString() << std::left << "  "
           << rec.title << "  [pre-tax: " << rec.preTaxCost
           << ", post-tax: " << rec.cost << "]";
        if (!rec.department.empty()) os << "  [department: " << rec.department << "]";
        if (rec.cancellation) os << "  [refund of order #" << rec.cancelledOrderNo << "]";
        if (!rec.approved) os << "\n        reason: " << rec.reason;
        os << "\n";
    }
    os << "Total pre-tax: " << totalBeforeTax() << "\n"
       << "Total post-tax (spent): " << totalSpent() << "\n";
}

}  // namespace bookmgmt
