#include "bookmgmt/Acquisition.h"
#include "bookmgmt/Pricing.h"
#include <limits>

#include <iomanip>
#include <ostream>
#include <stdexcept>

#include "bookmgmt/Exceptions.h"

namespace bookmgmt {

AcquisitionManager::AcquisitionManager(Catalog& catalog, Budget& budget, TaxRates taxes)
    : catalog_(catalog), budget_(budget) {
    setTaxRates(taxes);
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
                                     std::string* reason) const {
    std::string why;
    if (const Resource* r = catalog_.find(id)) {
        if (quantity <= 0)
            why = "quantity must be positive";
        else
            why = budget_.check(r->category(), quantity, taxedPrice(*r, r->costFor(quantity)));
    } else {
        why = "resource not found: " + id;
    }
    if (reason) *reason = why;
    return why.empty();
}

PurchaseRecord& AcquisitionManager::record(const Resource* r, const std::string& id,
                                           int qty, Money cost, bool approved,
                                           std::string reason, Money preTaxCost) {
    history_.push_back(PurchaseRecord{
        nextOrderNo_++, id, r ? r->title() : std::string("(unknown)"),
        r ? r->category() : ResourceCategory::Book, qty, cost, approved,
        std::move(reason), preTaxCost});
    return history_.back();
}

const PurchaseRecord& AcquisitionManager::purchase(const std::string& id, int quantity) {
    const Resource& r = catalog_.get(id);        // may throw NotFoundError
    const Money preTaxCost = r.costFor(quantity); // may throw invalid_argument
    const Money cost = taxedPrice(r, preTaxCost);
    budget_.commit(r.category(), quantity, cost);  // may throw quota/budget errors
    catalog_.addHoldings(id, quantity);
    return record(&r, id, quantity, cost, true, {}, preTaxCost);
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
            why = budget_.check(r->category(), req.quantity, cost);
        }

        if (why.empty()) {
            results.push_back(purchase(req.resourceId, req.quantity));
        } else {
            results.push_back(record(r, req.resourceId, req.quantity, cost, false, why, preTaxCost));
        }
    }
    return results;
}

Money AcquisitionManager::totalSpent() const {
    Money sum;
    for (const auto& rec : history_)
        if (rec.approved) sum += rec.cost;
    return sum;
}

Money AcquisitionManager::totalBeforeTax() const {
    Money sum;
    for (const auto& rec : history_)
        if (rec.approved) sum += rec.preTaxCost;
    return sum;
}

void AcquisitionManager::printReport(std::ostream& os) const {
    os << "Order history (" << history_.size() << " orders)\n";
    for (const auto& rec : history_) {
        os << "  #" << std::setw(3) << std::left << rec.orderNo << " "
           << (rec.approved ? "APPROVED" : "REJECTED") << "  " << std::setw(6)
           << rec.resourceId << " x" << std::setw(3) << rec.quantity << " "
           << std::setw(12) << std::right << rec.cost.toString() << std::left << "  "
           << rec.title << "  [pre-tax: " << rec.preTaxCost
           << ", post-tax: " << rec.cost << "]";
        if (!rec.approved) os << "\n        reason: " << rec.reason;
        os << "\n";
    }
    os << "Total pre-tax: " << totalBeforeTax() << "\n"
       << "Total post-tax (spent): " << totalSpent() << "\n";
}

}  // namespace bookmgmt
