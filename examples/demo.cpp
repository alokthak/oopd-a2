// Demo: builds a small catalog, sets a budget with per-category quotas,
// and runs a batch of purchase requests through the acquisition manager.

#include <iostream>

#include "bookmgmt/bookmgmt.h"

using namespace bookmgmt;

int main() {
    Catalog catalog;

    catalog.emplace<Book>("B001", "Clean Code", std::vector<std::string>{"Robert C. Martin"},
                          "978-0132350884", "Prentice Hall", 2008, Money::of(450));
    catalog.emplace<Book>("B002", "The C++ Programming Language",
                          std::vector<std::string>{"Bjarne Stroustrup"}, "978-0321563842",
                          "Addison-Wesley", 2013, Money::of(1200), 4, Binding::Hardcover);
    catalog.emplace<ElectronicResource>("R001", "IEEE Xplore Digital Library", "IEEE", 2026,
                                        Money::of(150), "https://ieeexplore.example",
                                        LicenseModel::AnnualSubscription, Money::of(2000));
    catalog.emplace<ElectronicResource>("R002", "MATLAB Campus Licence", "MathWorks", 2026,
                                        Money::of(400), "https://licensing.example/matlab",
                                        LicenseModel::Perpetual);

    std::cout << "=== Catalog ===\n";
    for (const Resource* r : catalog.all()) std::cout << r->summary() << "\n";

    std::cout << "\n=== Details of R001 ===\n" << catalog.get("R001");

    Budget budget(Money::of(20000));
    budget.setQuota(ResourceCategory::Book, {10, Money::of(8000)});
    budget.setQuota(ResourceCategory::ElectronicResource, {40, Money::of(12000)});

    AcquisitionManager acq(catalog, budget);

    std::cout << "\n=== Quotes ===\n";
    std::cout << "5 copies of B002  = " << acq.quote("B002", 5) << "\n";
    std::cout << "20 seats of R001  = " << acq.quote("R001", 20) << "  (incl. platform fee)\n";

    acq.processBatch({
        {"B001", 4},   // 1800  ok
        {"B002", 5},   // 7200 rejected: hardcover surcharge exceeds remaining book spend quota
        {"B001", 1},   // 450 approved: the preceding hardcover request was rejected
        {"R001", 20},  // 5000  ok
        {"R002", 25},  // 10000 rejected: e-resource unit quota (20 seats left)
        {"R002", 15},  // 6000  ok  -> e-resource spend 11000
        {"R002", 5},   // 2000  rejected: e-resource spend quota (1000 left)
        {"X999", 1},   // rejected: unknown id
    });

    std::cout << "\n=== Acquisition report ===\n";
    acq.printReport(std::cout);

    std::cout << "\n=== Budget ===\n";
    budget.print(std::cout);

    std::cout << "\n=== Holdings ===\n";
    for (const Resource* r : catalog.all())
        std::cout << "  " << r->id() << ": " << catalog.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";

    // Direct purchase: errors are reported with exceptions
    std::cout << "\n=== Direct purchase that breaks a quota ===\n";
    try {
        acq.purchase("B002", 10);
    } catch (const QuotaExceededError& e) {
        std::cout << "QuotaExceededError: " << e.what() << "\n";
    }
    // Part A uses a separate budget so the original walkthrough stays intact.
    std::cout << "\n=== Part A: new resource types ===\n";
    Catalog extended;
    extended.emplace<Journal>("J001", "Computing Journal", "1234-5678", 12,
                              "Publisher", 2026, Money::of(100), 3);
    extended.emplace<EBook>("E001", "Digital C++", std::vector<std::string>{"Author"},
                            "978-example", "Publisher", 2026, Money::of(20), "ebook-url",
                            FileFormat::EPUB, true, LicenseModel::Perpetual, Money::of(50));
    extended.emplace<ElectronicResource>("DB001", "Database", "Publisher", 2026,
                                         Money::of(10), "database-url");
    extended.emplace<AudioBook>("A001", "Audio C++", "Publisher", 2026, Money::of(15),
                                "audio-url", "Narrator", 180);
    extended.emplace<Thesis>("T001", "Library Systems", "IIITD", "PhD", "Supervisor", 2026);
    for (const Resource* r : extended.all()) std::cout << *r << "\n";
    Budget partABudget(Money::of(2000));
    partABudget.setQuota(ResourceCategory::Journal, {2, Money::of(600)});
    partABudget.setQuota(ResourceCategory::EBook, {3, Money::of(110)});
    partABudget.setQuota(ResourceCategory::ElectronicResource, {1, Money::of(10)});
    partABudget.setQuota(ResourceCategory::AudioBook, {2, Money::of(30)});
    partABudget.setQuota(ResourceCategory::Thesis, {1, Money{}});
    AcquisitionManager partA(extended, partABudget);
    std::cout << "Journal: 2 copies for 3 years = " << partA.quote("J001", 2) << "\n"
              << "EBook: 3 seats including platform fee = " << partA.quote("E001", 3) << "\n";
    partA.processBatch({{"J001", 2}, {"J001", 1}, {"E001", 3}, {"E001", 1},
                        {"DB001", 1}, {"A001", 2}, {"T001", 1}});
    std::cout << "T001 is intentionally free: one copy consumes a unit, but no money.\n";
    partA.printReport(std::cout);
    partABudget.print(std::cout);
    for (const Resource* r : extended.all())
        std::cout << r->id() << ": " << extended.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";
    std::cout << "\n=== Part B: hardcover and bulk pricing (pre-tax) ===\n";
    std::cout << "Hardcover B002, listed per-copy price: " << catalog.get("B002").unitPrice()
              << ", one copy: " << acq.quoteBeforeTax("B002", 1)
              << ", ten copies after discount: " << acq.quoteBeforeTax("B002", 10) << "\n";
    std::cout << "Paperback B001: 9 copies = " << acq.quoteBeforeTax("B001", 9)
              << ", 10 copies = " << acq.quoteBeforeTax("B001", 10) << "\n";
    std::cout << "Journal J001: 10 copies for 3 years = " << partA.quoteBeforeTax("J001", 10)
              << "\nElectronic R001: 50 seats = " << acq.quoteBeforeTax("R001", 50)
              << ", 60 seats = " << acq.quoteBeforeTax("R001", 60) << "\n";

    std::cout << "\n=== Part B: purchases across all categories ===\n";
    // A fresh catalogue and budget make these holdings independent of Part A.
    Catalog priced;
    priced.emplace<Book>("B001", "Hardcover C++", std::vector<std::string>{"Author"},
                         "isbn", "Publisher", 2026, Money::of(100), 1, Binding::Hardcover);
    priced.emplace<Journal>("J001", "Computing Journal", "1234-5678", 12,
                            "Publisher", 2026, Money::of(100), 2);
    priced.emplace<ElectronicResource>("R001", "Database", "Publisher", 2026,
                                       Money::of(10), "database-url",
                                       LicenseModel::Perpetual, Money::of(100));
    priced.emplace<EBook>("E001", "Digital C++", std::vector<std::string>{"Author"},
                          "isbn", "Publisher", 2026, Money::of(20), "ebook-url",
                          FileFormat::EPUB, true, LicenseModel::Perpetual, Money::of(50));
    priced.emplace<AudioBook>("A001", "Audio C++", "Publisher", 2026, Money::of(15),
                              "audio-url", "Narrator", 180,
                              LicenseModel::Perpetual, Money::of(30));
    priced.emplace<Thesis>("T001", "Paid thesis copies", "IIITD", "PhD", "Supervisor",
                           2026, Money::of(10));
    priced.emplace<Thesis>("T002", "Free deposited thesis", "IIITD", "PhD", "Supervisor", 2026);
    Budget taxedBudget(Money::of(10000));
    taxedBudget.setQuota(ResourceCategory::Book, {20, Money::of(1200)});
    taxedBudget.setQuota(ResourceCategory::Journal, {20, Money::of(2500)});
    taxedBudget.setQuota(ResourceCategory::ElectronicResource, {70, Money::of(1000)});
    taxedBudget.setQuota(ResourceCategory::EBook, {70, Money::of(1500)});
    taxedBudget.setQuota(ResourceCategory::AudioBook, {70, Money::of(1200)});
    taxedBudget.setQuota(ResourceCategory::Thesis, {20, Money::of(200)});
    AcquisitionManager taxed(priced, taxedBudget, {1000, 2000});
    std::cout << "Tax rates: print 10%, electronic 20%.\n"
              << "Bulk purchases: 10 print copies or 60 electronic seats.\n"
              << "T002 is intentionally free; T001 demonstrates paid thesis copies.\n";
    taxed.processBatch({{"B001", 10}, {"J001", 10}, {"R001", 60}, {"E001", 60},
                         {"A001", 60}, {"T001", 10}, {"T002", 1}});
    std::cout << "\nIntentional rejection: one more hardcover costs "
              << taxed.quoteBeforeTax("B001", 1) << " before tax and "
              << taxed.quote("B001", 1) << " after tax. Only "
              << *taxedBudget.spendRemaining(ResourceCategory::Book)
              << " remains in the book spending quota.\n";
    taxed.processBatch({{"B001", 1}});
    taxed.printReport(std::cout);
    taxedBudget.print(std::cout);
    std::cout << "\nPart B holdings (approved purchases only):\n";
    for (const Resource* r : priced.all())
        std::cout << r->id() << ": " << priced.holdings(r->id())
                  << (r->isDigital() ? " seats" : " copies") << "\n";
    return 0;
}
