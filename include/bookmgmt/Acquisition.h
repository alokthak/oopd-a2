#pragma once
// AcquisitionManager: turns purchase requests into orders, enforcing the
// Budget's quotas, updating Catalog holdings and keeping an order history.

#include <iosfwd>
#include <map>
#include <string>
#include <vector>

#include "bookmgmt/Budget.h"
#include "bookmgmt/Catalog.h"

namespace bookmgmt {

// Rates in basis points: 100 = 1%, 750 = 7.5%. Both default to zero.
struct TaxRates {
    int printBasisPoints = 0;
    int electronicBasisPoints = 0;
};

struct PurchaseRequest {
    std::string resourceId;
    int quantity;  // copies for print, seats for electronic
    std::string department = {}; // empty selects the original default budget
};

struct PurchaseRecord {
    int orderNo;
    std::string resourceId;
    std::string title;
    ResourceCategory category;
    int quantity;
    Money cost;  // post-tax amount used for quotas and budget
    bool approved;
    std::string reason;  // why it was rejected; empty if approved
    Money preTaxCost;
    std::string department = {};
    bool cancellation = false; // cost/preTaxCost are positive refund amounts
    int cancelledOrderNo = 0;
};

class AcquisitionManager {
public:
    AcquisitionManager(Catalog& catalog, Budget& budget, TaxRates taxes = {});
    void setTaxRates(TaxRates taxes);
    TaxRates taxRates() const { return taxes_; }
    // Budgets must outlive this manager. A budget may belong to one department only.
    void addDepartment(const std::string& name, Budget& budget);
    Budget& departmentBudget(const std::string& name) const;

    // Post-tax price without buying anything. Throws NotFoundError.
    Money quote(const std::string& id, int quantity) const;
    Money quoteBeforeTax(const std::string& id, int quantity) const;

    // True if the purchase would be approved; if not, `reason` explains why.
    bool canPurchase(const std::string& id, int quantity,
                     std::string* reason = nullptr, const std::string& department = {}) const;

    // Buys immediately. Throws NotFoundError, QuotaExceededError,
    // BudgetExceededError or std::invalid_argument. On success the budget
    // and holdings are updated and the record is added to history.
    const PurchaseRecord& purchase(const std::string& id, int quantity,
                                   const std::string& department = {});
    // Full-order cancellation; original purchase stays unchanged in history.
    const PurchaseRecord& cancel(int orderNo);

    // Processes requests in order; each is approved or rejected on its own
    // (never throws for a rejected request). Every outcome is recorded.
    // EXTENSION POINT: priority ordering, all-or-nothing batches, ...
    std::vector<PurchaseRecord> processBatch(const std::vector<PurchaseRequest>& reqs);

    const std::vector<PurchaseRecord>& history() const { return history_; }
    // Net approved amounts after subtracting cancellation refunds.
    Money totalSpent() const;
    Money totalBeforeTax() const;

    void printReport(std::ostream& os) const;

private:
    PurchaseRecord& record(const Resource* r, const std::string& id, int qty,
                           Money cost, bool approved, std::string reason, Money preTaxCost,
                           const std::string& department);
    Money taxedPrice(const Resource& resource, Money preTaxCost) const;

    std::map<std::string, Budget*> departments_;
    TaxRates taxes_;
    Catalog& catalog_;
    Budget& budget_;
    std::vector<PurchaseRecord> history_;
    int nextOrderNo_ = 1;
};

}  // namespace bookmgmt
